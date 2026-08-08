# transformation_estimation_point_to_plane_lls_weighted 优化证据索引

## 本文职责

本文把“当前采用了什么 RVV 优化方式、它落在哪些代码、有哪些对应测试 target、有哪些 bench / board 证据、边界在哪里”放在一处。

本文只提供优化方式的证据索引。最终接入判断放在 `transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md`。
函数调用关系和 helper 组织放在 `test-support-code-map.zh.md`。

## 读者入口

| 读者问题 | 先读文档 | 作用 |
| --- | --- | --- |
| 我想知道 RVV 到底改了什么 | 本文 | 先看优化方式和证据边界。 |
| 我想知道某个 gtest 名字是什么意思 | `correctness-tests.zh.md` | 看测试名、输入、断言和代码位置。 |
| 我想知道 bench label 和 checksum 怎么来的 | `benchmark-and-evidence.zh.md` | 看 case-filter、label 语法、checksum 和提交边界。 |
| 我想知道 `include/impl` 和 `src` 的调用关系 | `test-support-code-map.zh.md` | 看函数族、调用链和 production/test 边界。 |
| 我想知道为什么最后接入了当前 production 方案 | `transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md` | 看 EvidenceDecision、历史候选和风险。 |
| 我想知道 production 代码长期怎么写 | `doc-rvv/registration/transformation_estimation_point_to_plane_lls_weighted-RVV.zh.md` | 看长期实现说明。 |

## 当前结论摘要

当前 production 只批准 full-cloud public overload、`Scalar=float`、连续 `weights_`、source xyz f32 AoS layout、target xyz+normal f32 AoS layout、规模 / VLEN / byte-offset gate 都满足的路径。

这条路径的 RVV 组织方式是：

```text
public full-cloud overload
  -> RVV gate
  -> AoS load + finite mask
  -> block-reduction A/B/C/N
  -> solve
```

source-indexed、dual-indices、correspondences、`Scalar=double` 和 layout miss 路径保持标量。

## 优化方式总表

| 优化方式 | 当前状态 | 生产代码位置 | 主要 test target | 主要 bench / board 证据 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- | --- | --- | --- |
| full-cloud RVV dispatch 与 generic layout gate | adopted | `estimateRigidTransformation(full-cloud)`、`estimatePointToPlaneLLSWeightedFullCloudRVV`、`buildPointToPlaneLLSWeightedFullCloudBlockRVV`、`canUsePointToPlaneLLSWeightedFullCloudRVV` | `run_test_public_semantics`、`run_test_input_semantics`、`run_test_production_direct`、`run_board_test_public_semantics`、`run_board_test_input_semantics`、`run_board_test_production_direct` | `run_bench_production_dispatch`、`run_board_bench_production_dispatch`、`run_bench_production_default_fused_abcd_ilp_rvv`、`collect_board_production_dispatch_repeated`、`collect_board_production_default_fused_abcd_ilp` | 真实 public full-cloud 入口能在满足 gate 时走 RVV；fallback 保持标量语义。 | source-indexed、dual-indices、correspondences 的 production RVV。 |
| block-reduction A/B/C/N | adopted | `loadPointToPlaneLLSWeightedFullReductionVectors`、`accumulatePointToPlaneLLSWeightedBlockGroupA/B/C/N` | `run_test_candidates`、`run_board_test_candidates`、`run_test_production_direct`、`run_board_test_production_direct` | `run_bench_fused_formula`、`run_board_bench_fused_formula`、`run_bench_production_shaped_fused_formula`、`run_board_bench_production_shaped_fused_formula`、`trace_summary.md`、`checksum_validation.md` | 逐块规约树、accepted points、`ATA/ATb` 和 matrix 在预算内。 | solver 优化或 board 性能对所有点型都一样。 |
| fused formula code shape | adopted / accepted-risk | `WeightedFusedFormulaMode`、`accumulate_candidate_full_block_fused_*`、`estimate_candidate_full_block_fused_*` | `run_test_candidates`、`run_board_test_candidates` | `run_bench_fused_formula`、`run_board_bench_fused_formula`、`run_bench_generic_fused_abc`、`run_board_bench_generic_fused_abc`、`run_bench_generic_fused_formula`、`run_board_bench_generic_fused_formula`、`run_bench_generic_fused_abc_trace`、`run_board_bench_generic_fused_abc_trace`、`asm_production_symbol_attribution.md` | `abc`、`d-six-term`、`d-displacement`、`abcd` 及其 ILP 变体在 correctness 和预算内对齐。 | 每个 ILP 变体都一定有独立机器码收益。 |
| production default fused-abcd-ilp | adopted | `estimateRigidTransformation(full-cloud)`、`estimatePointToPlaneLLSWeightedFullCloudRVV`、`buildPointToPlaneLLSWeightedFullCloudDefault` | `run_test_production_direct`、`run_board_test_production_direct`、`run_test_input_semantics` | `run_bench_production_dispatch`、`run_board_bench_production_dispatch`、`run_bench_production_default_fused_abcd_ilp_rvv`、`run_board_bench_production_default_fused_abcd_ilp`、`production_dispatch_fused_abcd_ilp/summary.md`、`production_default_fused_abcd_ilp/trace_summary.md` | 当前默认 RVV path 的 correctness、trace、checksum 和 asm 归因已闭合到当前边界。 | indexed / correspondences 也能直接接入 production。 |
| source-indexed / dual-indices / correspondences | deferred | `ConstCloudIterator` clone 路径和公开 indexed / correspondence overload | `run_test_row_sources`、`run_board_test_row_sources`、`run_test_public_semantics`、`run_board_test_public_semantics` | `run_bench_row_sources`、`run_board_bench_row_sources`、`collect_board_row_sources_repeated`、`run_bench_default_diagnostic`（只作诊断形状） | 这些入口的 row source 语义、有限值语义和有效输入对拍。 | production RVV 接入、fused-abcd-ilp 同构收益和 repeated board 性能结论。 |
| `Scalar=double` 与 layout miss fallback | deferred / fallback only | public overload 的标量 clone 路径 | `run_test_production_direct`、`run_board_test_production_direct`、`run_test_row_sources`、`run_board_test_row_sources` | `run_bench_default_diagnostic`、`run_board_bench_default_diagnostic`（只作诊断形状） | gate miss 后会回到标量语义；public matrix 不应被破坏。 | double 路径的 RVV 结论。 |

## 标量路径与 RVV 路径差异

| 阶段 | 标量路径 | RVV 路径 | 直接相关的证据 |
| --- | --- | --- | --- |
| 输入检查 | public overload 先检查 source / target / weights 长度。 | 同样保留检查。 | `run_test_input_semantics`。 |
| 行来源 | `ConstCloudIterator` 逐行同步前进。 | 只在 full-cloud public overload 上用连续权重和 AoS load。 | `run_test_public_semantics`、`run_test_row_sources`。 |
| 取数 | 逐点读 source、target、weight。 | `vlse32.v` 读 source / target AoS 字段，`vle32.v` 读连续 weights。 | `run_test_production_direct`、`run_bench_production_dispatch`。 |
| 有限值检查 | 对 source xyz、target xyz 和 target normal 做 `continue`。 | 同样构造 mask，invalid lane merge to zero。 | `InvalidLaneMaskMatchesStd`、`NonFiniteWeightsAreNotMaskedWhenPointsAreFinite`。 |
| 公式 | 逐点计算 `a/b/c/d`。 | 在 `loadPointToPlaneLLSWeightedFullReductionVectors` 中生成向量 `a/b/c/d`。 | `run_test_candidates`。 |
| 规约 | 逐点累加到 normal-equation。 | A/B/C/N block groups 维护向量 partial sums，再显式 `vfredosum`。 | `run_test_candidates`、`trace_summary.md`。 |
| 求解 | Eigen 6x6 solve + matrix 构造。 | 同一 solve 和 matrix 构造。 | `run_test_production_direct`。 |

## 代码级证据索引

### 1. 生产入口与 RVV 分流

| 代码 | 作用 | 对应 target | 证据文件 |
| --- | --- | --- | --- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | 真实 public full-cloud 入口；只在 full-cloud 上尝试 RVV。 | `run_test_production_direct`、`run_test_input_semantics`、`run_test_public_semantics` | `run_test_std.log`、`run_test_rvv.log`、`summary.md` |
| `canUsePointToPlaneLLSWeightedFullCloudRVV(...)` | gate：点数、weights、VLEN 和 byte-offset 是否允许 RVV。 | `run_test_production_direct` | `run_test_rvv.log`、`trace_summary.md` |
| `estimatePointToPlaneLLSWeightedFullCloudRVV(...)` | float-only RVV wrapper。 | `run_test_production_direct`、`run_bench_production_default_fused_abcd_ilp_rvv` | `trace_summary.md`、`asm_production_symbol_attribution.md` |
| `buildPointToPlaneLLSWeightedFullCloudBlockRVV(...)` | 默认 RVV normal-equation 构造。 | `run_test_production_direct`、`run_test_candidates` | `checksum_validation.md`、`trace_summary.md` |
| `buildPointToPlaneLLSWeightedFullCloudDefault(...)` | RVV miss 时回标量。 | `run_test_production_direct` | `run_test_std.log`、`run_test_rvv.log` |

### 2. Block-reduction

| 代码 | 作用 | 对应 target | 证据文件 |
| --- | --- | --- | --- |
| `loadPointToPlaneLLSWeightedFullReductionVectors(...)` | 取 source / target / weight，构造 `keep` mask、`a/b/c/d/nx/ny/nz`。 | `run_test_candidates`、`run_bench_fused_formula` | `asm_production_symbol_attribution.md`、`trace_summary.md` |
| `accumulatePointToPlaneLLSWeightedBlockGroupA/B/C/N(...)` | A/B/C/N 四组累加 21 个 `ATA` 上三角项和 6 个 `ATb` 项。 | `run_test_candidates`、`run_test_production_direct` | `checksum_validation.md`、`trace_summary.md` |
| `solvePointToPlaneLLSWeightedNormalEquation(...)` | 补三角并求解。 | `run_test_production_direct` | `run_test_rvv.log`、`run_test_std.log` |

### 3. Fused formula

| 代码 | 作用 | 对应 target | 证据文件 |
| --- | --- | --- | --- |
| `WeightedFusedFormulaMode` | 枚举 `BlockBaseline`、`AbcFused`、`DSixTermFma`、`DDisplacementFused`、`AbcdFused` 及 ILP 变体。 | `run_test_candidates`、`run_bench_fused_formula` | `run_bench_fused_formula_*.log`、`summary.md` |
| `accumulate_candidate_full_block_fused_*` | fused 候选的 normal-equation 构造。 | `run_test_candidates`、`run_bench_generic_fused_formula` | `trace_summary.md`、`checksum_validation.md` |
| `estimate_candidate_full_block_fused_*` | fused 候选加 solver 的 full estimate。 | `run_test_candidates`、`run_bench_production_shaped_fused_formula` | `summary.md`、`trace_summary.md` |

### 4. 暂缓的 row source 入口

| 代码 | 当前状态 | 对应 target | 证据边界 |
| --- | --- | --- | --- |
| `ConstCloudIterator` 公开 overload clone | 保持标量 | `run_test_row_sources`、`run_test_public_semantics` | 只证明语义，不证明 production RVV。 |
| source-indexed helper | 仅诊断 | `run_test_row_sources`、`run_bench_row_sources`、`run_board_bench_row_sources` | 没有 production direct、repeated board 和 production asm。 |
| dual-indices helper | 仅诊断 | `run_test_row_sources`、`run_bench_row_sources`、`run_board_bench_row_sources` | 没有 production direct、repeated board 和 production asm。 |
| correspondences helper | 仅诊断 | `run_test_row_sources`、`run_bench_row_sources`、`run_board_bench_row_sources` | 没有 production direct、repeated board 和 production asm。 |

## 细粒度 target 字典

本节列出复核入口。target 运行后可能生成 QEMU 或 board 日志；只有进入“当前可提交证据”表的文件才是当前提交候选。

### gtest target

| target | 含义 | 它证明什么 | 它不能证明什么 |
| --- | --- | --- | --- |
| `run_test_public_semantics` | 公开入口的标量语义对拍。 | test-only reference 复刻有效输入语义。 | production RVV 是否命中。 |
| `run_test_input_semantics` | 公开入口输入语义。 | 数量不匹配、0 权重、负权重的 public 行为。 | 非法 index / correspondences 的 public 合同。 |
| `run_test_row_sources` | row source 与 fallback 语义。 | full/source/dual/correspondences 数据来源差异。 | production RVV 接入。 |
| `run_test_candidates` | staged-row、block-reduction、fused formula、代表点型。 | 候选链路与标量 reference 在预算内对齐。 | 板卡性能。 |
| `run_test_production_direct` | 真实 production full-cloud 入口。 | dispatch / fallback / layout gate / representative point types。 | source-indexed、dual-indices、correspondences 的 production 接入。 |

### bench target

| target | case-filter | 含义 | 它证明什么 | 它不能证明什么 |
| --- | --- | --- | --- | --- |
| `run_bench_fused_formula` | `fused-formula` | fused 候选诊断子集。 | `block-fused-*` 的 formula 形状和 checksum。 | 真实 public dispatch。 |
| `run_bench_row_sources` | `row-sources` | 数据源候选诊断子集。 | full-cloud/source-indexed/dual-indices/correspondences candidate 的耗时和 checksum 形状。 | production dispatch；当前 production fused-abcd-ilp 对 indexed/correspondences 的收益。 |
| `run_bench_production_shaped_fused_formula` | `production-shaped-fused-formula` | layout-gated helper 对拍。 | 同边界 full estimate 候选筛选。 | 真实 public overload。 |
| `run_bench_generic_fused_abc` | `generic-fused-abc` | 三类代表点型的 abc 诊断。 | generic layout gate 和代表点型正确性。 | D 项和 abcd 全候选。 |
| `run_bench_generic_fused_formula` | `generic-fused-formula` | 全部 fused 候选诊断。 | `abc`、D 项、`abcd` 及 ILP 变体。 | production dispatch。 |
| `run_bench_generic_fused_abc_trace` | `generic-fused-abc-trace` | 逐 iteration 追踪。 | 长尾和顺序变化。 | 默认 production trace。 |
| `run_bench_production_dispatch` | `production-dispatch` | 真实 public full-cloud overload。 | std / RVV production direct speedup。 | indexed / correspondences / all point types 逐类型性能。 |
| `run_bench_production_default_fused_abcd_ilp` | `production-default-fused-abcd-ilp` | QEMU std/RVV 形状检查。 | 默认 production case-filter 在两种构建下都能输出日志。 | QEMU 性能结论。 |
| `run_bench_production_default_fused_abcd_ilp_rvv` | `production-default-fused-abcd-ilp` | RVV-only 形状检查。 | 默认 RVV path 的输出和日志形状。 | repeated board 性能。 |

### board target

| target | 含义 | 证据角色 |
| --- | --- | --- |
| `collect_board_row_sources_repeated` | 多轮采集行来源诊断。 | 行来源重复板卡摘要候选。 |
| `collect_board_production_dispatch_repeated` | 多轮采集真实 public overload。 | 生产路径重复板卡性能摘要。 |
| `collect_board_production_default_fused_abcd_ilp` | 多轮采集默认 RVV path。 | 追踪、校验和、反汇编归因。 |
| `summarize_board_production_default_fused_abcd_ilp` | 汇总 trace。 | 运行稳定性和 checksum 序列。 |
| `asm_production_default_fused_abcd_ilp` | 重新导出 asm attribution。 | 路径和指令归因。 |

## 当前可提交证据

这些文件已经是当前 topic 的稳定证据入口。

| 文件 | 证据角色 |
| --- | --- |
| `log/qemu/run_test_std.log` | QEMU std correctness；`39 passed + 1 skipped`。 |
| `log/qemu/run_test_rvv.log` | QEMU RVV correctness；`40 passed`。 |
| `log/board/production_dispatch_fused_abcd_ilp/summary.md` | production-dispatch repeated std/RVV speedup summary。 |
| `log/board/production_default_fused_abcd_ilp/trace_summary.md` | 默认 RVV path 多轮 trace summary。 |
| `log/board/production_default_fused_abcd_ilp/checksum_validation.md` | 默认 RVV path checksum 序列一致性。 |
| `log/board/production_default_fused_abcd_ilp/asm_production_symbol_attribution.md` | 默认 production helper 反汇编归因。 |

## 结论边界

当前证据支持以下结论：

- full-cloud public overload 的 RVV dispatch 可以在当前 gate 下命中。
- `Scalar=float`、连续 `weights_`、source xyz f32 AoS、target xyz+normal f32 AoS 的路径已经有 correctness、trace、checksum 和 asm 归因证据。
- block-reduction 与 fused formula 的候选已经通过 test target 和 bench target 闭环。
- source-indexed、dual-indices、correspondences 仍然是标量 production 边界，不应被写成 production RVV 已接入。

当前证据不支持以下扩展：

- 把 indexed / correspondences 路径写成 production RVV。
- 把 `Scalar=double` 写成 RVV 接入。
- 把 QEMU timing 写成性能结论。
- 把 representative point types 的结果外推成所有 gate-allowed 点型逐类型性能。
