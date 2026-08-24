# Phase 050 / PI1 计划：binary writer production integration plan

## 阶段意图和边界

本阶段只做 `PCDWriter::writeBinary<PointT>(file_name, cloud)` 的 PI1 production integration plan
（生产接入计划）。Phase 040 已证明 binary packed output loop（按点连续的有效字段输出循环）在
test-only component ablation（测试专用组件消融）中为 positive，但它仍不包含 header、mmap、write、
sync、file lock 或 error handling。因此本阶段只冻结生产接入合同，不修改
`io/include/pcl/io/impl/pcd_io.hpp`。

当前 production 修改需要用户明确授权。PI1 的完成判据是让 reviewer 能清楚回答：如果后续进入 PI2，
生产 patch 应改哪条入口、哪些路径必须保持标量、如何观测 RVV hit / fallback，以及需要哪些 production
direct（真实生产路径证据）。

## 候选范围

validated diagnostic scope（来自 Phase 040）：

- public entry candidate：`PCDWriter::writeBinary<PointT>(file_name, cloud)`，不含 indices overload。
- candidate stage：过滤 `_` 字段后，把 `PointCloud<PointT>` 的有效字段按 point-major packed output
  写入 `map + data_idx`。
- field gate：所有保留字段的 `field.count * pcl::getFieldSize(field.datatype)` 都是 4 字节；
  `field.offset` 和 `sizeof(PointT)` 满足 4 字节对齐。
- row source：contiguous PointCloud rows（连续点云行）。
- evidence：Phase 040 board repeated positive；large / padding case positive，small case outlier-scoped。

不接入范围：

- `writeBinary(file_name, cloud, indices)`，因为 indexed row source（索引行来源）需要 gather 或逐 index
  stride 语义，Phase 040 没有覆盖。
- `writeBinaryCompressed<PointT>`，它已有独立 Phase 020 PI1 计划，生产优先级仍高于本 binary writer 计划。
- `writeASCII<PointT>` 和 `io/src/pcd_io.cpp` 的 PCLPointCloud2 路径。
- 非 4 字节字段、字段 offset 未对齐、`sizeof(PointT)` 未按 4 字节对齐的点类型。

## 生产 helper 形态候选

PI2 若获授权，应优先采用窄 helper 结构：

1. 保留 public entry 的 header 生成、file open、lock、raw_fallocate、mmap、msync、munmap 和 close 流程。
2. 将 `for point -> for field -> memcpy(out, ...)` 抽成 Std helper，例如
   `copyValidFieldsForBinaryStd`。
3. 在 `__RVV10__` 下新增 RVV helper，例如 `copyValidFourByteFieldsForBinaryRVV`，只负责写入
   `map + data_idx` 的 binary data 区域。
4. helper 返回 false 或 gate 不满足时自然调用 Std helper。

不建议直接把 RVV intrinsic 放进 public entry 主体。公开入口应保持“准备 metadata -> mmap -> 尝试 RVV
pack -> Std fallback -> sync / unmap”的可审查形状。

## Fallback 矩阵

| fallback item | gate / evidence needed | expected behavior |
| --- | --- | --- |
| 非 RVV 构建 | `__RVV10__` 未定义 | 只编译和调用 Std helper。 |
| 空 cloud | 现有 warning 语义保持 | 不触发 RVV helper，保持原行为。 |
| 非 4 字节字段 | 任一 `fields_sizes[i] != 4` | Std helper。 |
| 未对齐字段 offset | 任一 `fields[i].offset % 4 != 0` | Std helper。 |
| `sizeof(PointT)` 未按 4 字节对齐 | `sizeof(PointT) % 4 != 0` | Std helper。 |
| indices overload | 当前不接入 | 保持现有标量路径。 |
| `_WIN32` 分支 | 当前板卡证据不覆盖 | 若 helper 平台无关可编译；否则保守保持标量。 |
| file open / mmap / raw_fallocate / msync failure | 保留现有错误处理 | 错误路径不进入 RVV helper 或保持原抛错。 |

## Generic point type 策略

本候选按 `pcl::getFields<PointT>()` 动态枚举所有非 `_` 字段，不使用 `PointXYZ` exact-type gate。
RVV helper 只批准运行期字段元数据满足 4 字节和对齐条件的模板实例。非覆盖点类型自然 fallback 到
Std helper；这不是完整泛型点类型采纳。

## Production direct test 计划

PI3 至少需要新增或扩展真实公开入口测试：

| test | purpose | completion criteria |
| --- | --- | --- |
| public binary writer hit case | 构造 PointXYZRGB-like cloud，调用真实 `PCDWriter::writeBinary` 写临时 PCD | RVV build 能证明 RVV helper hit；输出文件可读或 binary data 区域与 Std 等价。 |
| Std/RVV file equivalence | 同一输入分别运行 Std/RVV build | header 和 binary payload byte-equal，或明确说明可接受差异。 |
| fallback non-4-byte field | 使用含 2 字节或 1 字节字段的测试点型或 test-only registered point | RVV build 不命中 RVV helper，输出与 Std 一致。 |
| indices overload unchanged | 调用 `writeBinary(file_name, cloud, indices)` | 仍走标量路径，输出与既有行为一致。 |
| non-RVV build compile/run | `USE_PCL_RVV10=0` | 不引用 RVV intrinsic，测试通过。 |

## Bench / asm / board 计划

PI4 production evidence rerun（生产证据重跑）需要：

- QEMU correctness：真实 public entry direct tests；QEMU 不写性能结论。
- asm attribution：生产 helper 或 public entry binary 的 RVV 指令归属，至少能看到 `vlse32.v` / `vsse32.v`
  属于 binary writer production helper 或其内联范围。
- board production bench：计时边界应包含真实 `writeBinary` public entry 的临时文件写入；若文件系统噪声过大，
  允许新增 production-detail A/B，但必须标清 evidence role。
- Evidence Doctor：production direct 或 production-detail summary 必须生成 manifest、Doctor 和 registry。

## PI1 Gate

| gate | required answer |
| --- | --- |
| pi2_scope | 只覆盖 `writeBinary<PointT>(file_name, cloud)` 的 contiguous cloud、4 字节字段 packed output helper。 |
| forbidden_expansion | 不碰 indices overload、compressed writer、ASCII writer、PCLPointCloud2 path、public API 或跨 topic common API。 |
| fallback_matrix | 非 RVV、空 cloud、非 4 字节字段、未对齐、indices、文件错误路径都有明确 fallback / 保持原行为要求。 |
| entry_structure | public entry 只做准备、mmap、RVV 短路和 Std fallback，不堆大段 RVV 主体。 |
| production_direct_test_plan | public binary writer hit、Std/RVV file equivalence、fallback、indices unchanged 和 non-RVV build 都有测试入口。 |
| evidence_commands | QEMU correctness、asm、board production bench、Doctor、registry 有目标命令或新增 target 计划。 |

## Continue / Stop Conditions

- 若用户明确授权进入 production integration loop，且本 PI1 计划没有被 reviewer 发现范围 / fallback / 测试缺口，
  可进入 binary writer PI2 production patch。
- 若用户优先授权 compressed writer PI2，则 binary writer 计划保持 `PI1-plan-complete / pending-production-authorization`。
- 未授权 production 前，不修改 `io/include/pcl/io/impl/pcd_io.hpp`。
