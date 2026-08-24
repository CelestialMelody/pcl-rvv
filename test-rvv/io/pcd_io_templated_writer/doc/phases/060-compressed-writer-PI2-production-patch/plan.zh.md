# Phase 060: compressed writer PI2 production patch 计划

## 阶段意图和边界

本阶段进入 production integration loop（生产接入闭环）的 PI2-PI5。目标是在
`io/include/pcl/io/impl/pcd_io.hpp` 的
`PCDWriter::writeBinaryCompressed<PointT>(file_name, cloud)` 中接入 4 字节字段
RVV pack（把 AoS 点结构转换成 field-major 临时缓冲），并用真实公开入口重新验证是否值得保留。

本阶段只覆盖：

- 入口：`writeBinaryCompressed<PointT>(const std::string&, const PointCloud<PointT>&)`。
- row source（行来源）：contiguous `PointCloud<PointT>` rows。
- 点类型 / 布局：所有有效字段的 `field.count * getFieldSize(field.datatype) == 4`，字段 offset 和
  `sizeof(PointT)` 都满足 4 字节对齐；以 `PointXYZRGB` / `PointXYZRGBA` 类 4 字节字段布局作为板卡 case。
- 数据流：原有 header、file lock、LZF、mmap/write、empty cloud 和错误返回路径保持原语义。

本阶段不覆盖：

- `writeBinary<PointT>` binary writer；它保留 Phase 050 的 PI1 计划，后续可单独进入 PI2。
- indices overload、ASCII writer、PCLPointCloud2 writer / reader、`io/src/pcd_io.cpp`。
- 1 / 2 / 8 字节字段或 offset / stride 不满足 4 字节对齐的布局；这些必须 fallback（回退）到标量 pack。
- PI5 后的最终采纳。即使 production-public 板卡结果为 positive，也先保留补丁并暂停给用户确认。

## 当前状态清单

| item | current state |
| --- | --- |
| diagnostic component | Phase 000 pack-only board mean speedup `1.4892x / 1.4466x / 1.5739x`，Doctor clean。 |
| production-shaped diagnostic | Phase 010 pack+LZF board mean speedup `1.3904x / 1.3526x / 1.3493x`，Doctor clean。 |
| PI1 plan | Phase 020 已冻结压缩 writer PI1 范围和 fallback 矩阵。 |
| production source | 当前无 diff；本阶段会首次修改 `io/include/pcl/io/impl/pcd_io.hpp`。 |
| formal doc-rvv | 尚不创建；只有 PI5 evidence positive 且用户确认采纳后才创建长期 production 文档。 |
| board availability | 当前会话用户确认板卡可用；本阶段需要执行 production-public repeated board。 |

## 候选族和假设

候选族是 `compressed writer 4-byte field RVV stride pack`。假设是：生产路径中 field-major
临时缓冲的构造仍占足够成本，RVV `vlse32.v` 跨步加载和 `vse32.v` 连续写入能减少每点每字段小
`memcpy`、内层指针推进和循环开销；LZF 与文件写入会稀释收益，但 Phase 010 显示仍可能保持 positive。

实现形态：

- 抽出 `packBinaryCompressedFieldsStd` 标量 helper，复刻当前双重循环。
- 在 `__RVV10__ && __riscv_vector` 下新增 `packBinaryCompressedFieldsRVV`，只在 4 字节字段和 4 字节对齐 stride / offset 下返回 true。
- 公开入口只保留“准备字段和缓冲 -> RVV 尝试 -> Std fallback -> LZF / file write”的短分流形状。
- 新增仅测试宏 hook 记录 `Std` / `RVV` 路径，参考同模块 `point_cloud_image_extractors` 的测试 hook 形态，不改变公开 API。

## 优化矩阵

| candidate family | row source | point type / layout | entry | test | bench / board | asm | Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| compressed writer 4-byte RVV production pack | contiguous rows | 4 字节字段，offset / stride 4 字节对齐 | public `writeBinaryCompressed<PointT>` | production-direct hit + Std/RVV file equivalence | production-public repeated board | production helper 或 public entry 范围含 `vlse32.v` / `vse32.v` | required | pending PI5 |
| scalar fallback for mixed field sizes | contiguous rows | 非 4 字节字段或非对齐布局 | same public entry | fallback hook + return/output equivalence | not_applicable | no RVV requirement | not_applicable | pending |
| non-RVV build fallback | contiguous rows | same 4-byte layout | same public entry | Std build compile/run | not_applicable | no RVV requirement | not_applicable | pending |

## TDD 和实现动作

| action | artifact / command | expected evidence | completion |
| --- | --- | --- | --- |
| A1 写 production-direct RED test | `src/test_pcdtw.cpp`，调用 `PCDWriter::writeBinaryCompressed<PointT>` 写临时文件 | RVV build 在生产补丁前无法命中 RVV hook，测试失败 | fail for expected missing path |
| A2 写 fallback RED test | mixed-size 自定义点类型或现有非 4 字节字段路径 | 生产补丁前 hook 不存在或只能标量 | fail/compile-gap explained |
| A3 最小 production patch | `io/include/pcl/io/impl/pcd_io.hpp` | helper 分层、公开入口短路、fallback 清晰 | compiles |
| A4 GREEN correctness | `make -C test-rvv/io/pcd_io_templated_writer run_test_compare` | Std/RVV gtest 通过，RVV hit case 命中 RVV，fallback case 命中 Std | pass |
| A5 production-public bench harness | `src/bench_pcdtw.cpp` + Make target | 新增 `production_compressed_*` case 通过 public writer 计时 | local/QEMU smoke only for shape |
| A6 asm attribution | `make ... dump_bench_rvv` + grep | RVV bench asm 中可归属 `vlse32.v` / `vse32.v` | recorded |
| A7 board repeated production evidence | 新增 `run_board_pcdtw_production_compressed_repeated` | 5 runs，summary/manifest/Doctor/registry fresh | decision bucket stable |
| A8 PI5 report and docs | result、matrix、roadmap、handoff、screening | 写清 production diff、证据和是否建议采纳；不自行创建 doc-rvv | pause for user confirmation |

## Evidence Doctor 和 registry

本阶段新增 production-public 证据目录：

```text
test-rvv/io/pcd_io_templated_writer/log/board/production_compressed_repeat_5/
```

新增 summary / manifest / Doctor 需要登记到
`test-rvv/io/pcd_io_templated_writer/log/evidence_registry.json`，`doc-ref` 指向本阶段
`result.zh.md` 和 topic evaluation。Doctor（证据体检）出现 Error 时先修复或降级为 blocked；
Warning 必须解释对 PI5 的影响。

## 板卡复跑预算和决策桶

- run count：5。
- warm-up：每 run 3 iterations。
- timed iterations：20，除非生产 public file I/O 太慢或板卡空间异常；调整必须写入 result。
- positive：大规模和 padding case mean / median 均大于 `1.05x`，且 Doctor 无 Error。
- weak-positive：主要 case `1.00x`-`1.05x` 或只有一个大规模 case positive；可建议保留但需用户判断。
- neutral / negative：主要 case <= `1.00x`；不建议采纳，但 PI5 不自动回滚。
- unstable：预算用尽后 min/max 跨越 decision bucket 或 Doctor 指出 group outlier 影响主要 case。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 000/010 是 diagnostic / production-shaped diagnostic；本阶段新增 production-public。 |
| A/B boundary | 前期是 test helper；本阶段是 public overload（真实 `PCDWriter` 公开入口）。 |
| 当前决策问题 | RVV-vs-scalar：当前 public RVV path 是否快于当前 public scalar path。 |
| diagnostic 是否可外推到 production | 只能作为 bounded probe 的理由，不能直接当 production 结论；本阶段用公开入口重跑。 |
| comparison-boundary / baseline mismatch 风险 | 有。生产 public case 包含 header、LZF、临时文件和 mmap/write，可能稀释 pack 收益。 |
| 弱 / 负 / 中性 / 不稳定 diagnostic 是否允许 bounded production probe | 允许，因为 Phase 010 为 positive 且用户已授权；若本阶段 public evidence 不支持，PI5 暂停报告而不自动回滚。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；当前 production 无既有 RVV family，决策是 public RVV-vs-scalar。 |

## 继续 / 停止条件

继续条件：

- TDD RED 能定位到 production RVV hook 缺失或路径未命中。
- production patch 能保持公开 API、错误路径和 fallback 语义。
- 板卡 target 可用且 Doctor / registry 能生成。

停止条件：

- 生产 patch 要求扩大到 header 公共 API、其它入口或公共 RVV helper，超出 PI1 范围。
- 真实公开入口测试无法可靠区分 RVV / fallback，且没有低风险 hook 方式。
- 板卡不可达、远端空间或工具链失败，或 evidence registry 出现无法归属的冲突。
- PI5 production evidence 已生成；无论 positive 还是 negative，都暂停给用户确认采纳或回滚。

## 文档更新清单

- 必写：本阶段 `result.zh.md`、phase README、optimization matrix、optimization roadmap、current handoff。
- 若生产 evidence positive：screening 状态更新为 PI5 evidence pending user adoption，不创建长期 `doc-rvv`。
- 只有用户确认采纳后，才创建 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`，并使用本阶段 production-public 板卡数据。
