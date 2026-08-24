# Phase 020 / PI1 计划：production integration plan

## 阶段意图和边界

本阶段只做 PI1 production integration plan（生产接入计划）。目标是把 Phase 010 的
production-shaped diagnostic（生产形态诊断）收益落到可审查的 production（生产源码）接入边界：
候选范围、fallback（回退路径）、helper 结构、测试和证据计划必须先闭合；本阶段计划文件本身
不修改 `io/include/pcl/io/impl/pcd_io.hpp`。

当前用户 prompt 已授权持续推进 topic-local RVV 优化和板卡验证，但 `AGENTS.md` 默认不允许在
未明确进入 production integration loop（生产接入闭环）前直接改 production。因此 PI1 的
completion criteria（完成判据）是形成可执行生产接入合同；PI2 production patch 需要后续明确授权或
下一轮 prompt 指向生产接入闭环。

## 候选范围

validated diagnostic scope（来自 Phase 010）：

- public entry candidate：`PCDWriter::writeBinaryCompressed<PointT>(file_name, cloud)`。
- candidate stage：过滤 `_` 字段后，把 `PointCloud<PointT>` 的 point-major layout（按点连续布局）
  打包为 field-major buffer（按字段连续缓冲区），随后交给 `pcl::lzfCompress`。
- field gate：所有保留字段的 `field.count * pcl::getFieldSize(field.datatype)` 都是 4 字节；
  `field.offset` 和 `sizeof(PointT)` 满足 4 字节对齐。
- row source：contiguous PointCloud rows（连续点云行），不含 indices。
- evidence：Phase 010 board repeated positive，Doctor clean。

不接入范围：

- `writeBinary<PointT>`、`writeASCII<PointT>` 和 `io/src/pcd_io.cpp` 的 PCLPointCloud2 路径。
- `writeBinaryCompressed(file_name, cloud, indices)`，因为 indexed row source（索引行来源）没有当前证据。
- 非 4 字节字段、字段 offset 未对齐、`sizeof(PointT)` 不能作为 4 字节 stride 的点类型。
- `_WIN32` 分支生产证据，除非后续单独补平台验证；PI2 如实现应保证非 RVV 或 unsupported 平台自然走原标量 helper。

## 生产 helper 形态候选

PI2 若获授权，应优先采用窄 helper 结构：

1. 保留 public entry 的语义检查、header 生成、file open、lock、LZF 和 mmap/write 主流程。
2. 将现有字段打包循环抽成标量 helper，例如 `copyValidFieldsForBinaryCompressedStd` 或所在文件风格允许的更短命名。
3. 在 `__RVV10__` 下新增 RVV helper，例如 `copyValidFourByteFieldsForBinaryCompressedRVV`，只负责
   4 字节字段 pack，不负责 LZF 或 file write。
4. public entry 在完成 fields / fields_sizes / data_size 后尝试 RVV pack；helper 返回 false 时自然调用 Std helper。

不建议在 public entry 里直接塞入一大段 RVV intrinsic。公开入口应保持“准备元数据 -> 尝试 RVV pack -> Std fallback -> LZF -> 写文件”的可审查形状。

## Fallback 矩阵

| fallback item | gate / evidence needed | expected behavior |
| --- | --- | --- |
| 非 RVV 构建 | `__RVV10__` 未定义 | 只编译和调用 Std helper。 |
| 空 cloud | 现有语义仍保留 header + 8 字节 compressed header | 不触发 RVV helper，保持原行为。 |
| 非 4 字节字段 | 任一 `fields_sizes[i] != 4` | Std helper。 |
| 未对齐字段 offset | 任一 `fields[i].offset % 4 != 0` | Std helper。 |
| `sizeof(PointT)` 未按 4 字节对齐 | `sizeof(PointT) % 4 != 0` | Std helper。 |
| data_size overflow gate | 保留现有 `data_size * 3 / 2` 检查 | 先返回原错误，不进入 RVV。 |
| indices overload | 当前 topic 不接入 | 保持现有标量路径。 |
| Windows branch | 生产证据暂缺 | 若 helper 是平台无关 pack helper，可编译；否则保守保持标量。 |

## Generic point type 策略

本 topic 的生产候选不是只读 `x/y/z`，而是按 `pcl::getFields<PointT>()` 动态枚举所有非 `_` 字段。
因此 PI2 不应使用 `PointXYZ` exact-type gate，也不应假设某个固定字段集合。可采用 field-metadata
runtime gate（运行期字段元数据准入）：

- 点类型仍由 `pcl::getFields<PointT>()` 决定。
- RVV helper 只要求每个保留字段都是 4 字节、offset 对齐、`sizeof(PointT)` 对齐。
- 非覆盖点类型自然 fallback 到 Std helper。

这不是完整泛型点类型采纳：它只批准“当前字段元数据满足 4 字节 pack 条件”的模板实例。非 4 字节字段、
多元素字段或特殊输出语义仍由 Std helper 保持原行为。

## Production direct test 计划

PI3 至少需要新增或扩展真实公开入口测试：

| test | purpose | completion criteria |
| --- | --- | --- |
| public compressed writer hit case | 构造 PointXYZRGB-like cloud，调用真实 `PCDWriter::writeBinaryCompressed` 写临时 PCD | RVV build 能证明 RVV helper hit，文件可由 PCL 读取或 header / compressed payload 结构正确。 |
| Std/RVV file equivalence | 同一输入分别运行 Std/RVV build | compressed payload 或完整文件在可控边界内一致；若 LZF 输出依赖实现但 deterministic，应要求 byte-equal。 |
| fallback non-4-byte field | 使用含 2 字节或 1 字节字段的测试点型或 test-only registered point | RVV build 不命中 RVV helper，输出与 Std 一致。 |
| non-RVV build compile/run | `USE_PCL_RVV10=0` | 不引用 RVV intrinsic，测试通过。 |

若无法在 upstream test 中安全注册特殊点型，fallback case 可先放在 `test-rvv/io/pcd_io_templated_writer`
production-direct test harness（真实生产入口测试框架）里，并在 Handoff 说明它不是 PCL upstream test。

## Bench / asm / board 计划

PI4 production evidence rerun（生产证据重跑）需要：

- QEMU correctness：真实 public entry direct tests；QEMU 不写性能结论。
- asm attribution：生产 helper 或 public entry binary 的 RVV 指令归属，至少能看到 `vlse32.v` / `vse32.v`
  属于 production pack helper 或其内联范围。
- board production bench：计时边界应包含真实 `writeBinaryCompressed` public entry 的 temp file 写入；
  若文件系统噪声过大，允许新增 production-detail A/B（生产细节对照）bench，但必须标清 evidence role。
- Evidence Doctor：生产 direct 或 production-detail summary 必须生成 manifest、Doctor 和 registry。

## PI1 Gate

PI1 完成需要 reviewer 能回答：

| gate | required answer |
| --- | --- |
| pi2_scope | 只覆盖 `writeBinaryCompressed<PointT>(file_name, cloud)` 的 4 字节字段 pack helper。 |
| forbidden_expansion | 不碰 indices overload、ASCII writer、PCLPointCloud2 path、public API 或跨 topic common API。 |
| fallback_matrix | 非 RVV、空 cloud、非 4 字节字段、未对齐、overflow、indices 都有明确 fallback。 |
| entry_structure | public entry 只做准备、RVV 短路和 Std fallback，不堆大段 RVV 主体。 |
| production_direct_test_plan | public entry hit、fallback、Std/RVV file equivalence 和 non-RVV build 都有测试入口。 |
| evidence_commands | QEMU correctness、asm、board production bench、Doctor、registry 有目标命令或新增 target 计划。 |

## Continue / Stop Conditions

- 若用户明确授权进入 production integration loop，且本 PI1 计划没有被 reviewer 发现范围 / fallback / 测试缺口，
  下一阶段可进入 `PI2 production_patch`。
- 若用户只要求继续诊断或不授权 production，默认停在 PI1 plan complete，保留 test-only 证据和计划。
- 若 production direct test 无法设计出真实 RVV hit / fallback 可观测信号，PI1 必须 blocked，不进入 PI2。
