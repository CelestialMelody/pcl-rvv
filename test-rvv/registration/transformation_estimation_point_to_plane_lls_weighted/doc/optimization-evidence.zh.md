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

当前 production 批准 full-cloud public overload 和 source-indexed public overload。两条路径都要求 `Scalar=float`、连续 `weights_`、source xyz f32 AoS layout、target xyz+normal f32 AoS layout、规模 / VLEN / byte-offset gate 都满足。source-indexed 还要求 source index stream 全部有效。

两条路径的 RVV 组织方式不同：

```text
public full-cloud overload
  -> RVV gate
  -> AoS load + finite mask
  -> block-reduction A/B/C/N
  -> solve

public source-indexed overload
  -> RVV gate
  -> valid-index scan
  -> uint32_t staging
  -> source gather + target stride load
  -> vcompress + scalar tail accumulation
  -> solve
```

dual-indices、correspondences、`Scalar=double` 和 layout miss 路径保持标量。

这里的 source-indexed adopted 只表示 staged-gather / compressed-tail family 已有真实 production direct 证据。full-cloud 已采纳的 block-reduction + A/B/C/N block groups + fused formula / ILP family 已按 source-indexed、dual-indices 和 correspondences 做过同边界 family carry-over audit（实现族迁移审计）；其中 source-indexed repeated diagnostic 已闭合为当前 no-production，dual-indices / correspondences 仍为 diagnostic negative。不能把 full-cloud positive 或 source-indexed 当前 family positive 直接外推成其它入口的最优结论。

## 优化方式总表

| 优化方式 | 当前状态 | 生产代码位置 | 主要 test target | 主要 bench / board 证据 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- | --- | --- | --- |
| full-cloud RVV dispatch 与 generic layout gate | adopted | `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)`、`estimatePointToPlaneLLSWeightedFullCloudRVV`、`buildPointToPlaneLLSWeightedFullCloudBlockRVV`、`canUsePointToPlaneLLSWeightedFullCloudRVV` | `run_test_public_semantics`、`run_test_input_semantics`、`run_test_production_direct`、`run_board_test_public_semantics`、`run_board_test_input_semantics`、`run_board_test_production_direct` | `run_bench_production_dispatch`、`run_board_bench_production_dispatch`、`run_bench_production_default_fused_abcd_ilp_rvv`、`collect_board_production_dispatch_repeated`、`collect_board_production_default_fused_abcd_ilp` | 真实 public full-cloud 入口能在满足 gate 时走 RVV；fallback 保持标量语义。 | source-indexed 的性能结论、dual-indices 和 correspondences 的 production RVV。 |
| source-indexed RVV dispatch 与 staged gather gate | adopted | `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)`、`estimatePointToPlaneLLSWeightedSourceIndicesRVV`、`buildPointToPlaneLLSWeightedSourceIndicesStagedRVV`、`loadPointToPlaneLLSWeightedSourceIndexedVectors`、`accumulatePointToPlaneLLSWeightedCompressedRowsF32M2`、`buildPointToPlaneLLSWeightedSourceIndicesDefault`、`canUsePointToPlaneLLSWeightedSourceIndicesRVV` | `run_test_public_semantics`、`run_test_input_semantics`、`run_test_production_direct`、`run_test_source_indices`、`run_board_test_source_indices` | `run_bench_production_source_indices`、`run_board_bench_production_source_indices`、`collect_board_production_source_indices_repeated`、`run_bench_row_sources`、`run_board_bench_row_sources` | 真实 public source-indexed 入口能在有效 source index、连续 weights 和 generic layout gate 时走 RVV；fallback 保持标量语义。当前路径是 staged-gather / compressed-tail adopted path，不等于已经比较过 source-indexed block-reduction / fused-formula family。 | dual-indices、correspondences、非法 index public API 行为；也不能证明 source-indexed 其它实现族已比较完毕。 |
| source-indexed block-reduction / fused-formula carry-over audit | attempted / no-production for current evidence | 暂无 production 代码；candidate 只在 test-rvv。 | `run_test_source_indexed_family_compare` | `run_bench_source_indexed_family`、`run_board_bench_source_indexed_family`、`collect_board_source_indexed_family_repeated`、`log/board/source_indexed_family_repeated/summary.md`、`evidence_doctor.md` | 用来回答 full-cloud adopted family 是否适合 source-indexed gather ingress；当前 5-run 说明 `block-fused-abcd-ilp` 不适合直接推进 production-candidate。 | 不能把当前 `PointNormal` / `float` / 262144 点 diagnostic 写成所有未来 source-indexed formula variant 永久拒绝；也不能替代 production direct / asm。 |
| dual-indices / correspondences family carry-over | diagnostic attempted / no-production | `DualIndicesBlockReductionMatchesStdWithinBudget`、`DualIndicesBlockFusedAbcdIlpMatchesBlockAndStdWithinBudget`、`CorrespondenceBlockReductionMatchesStdWithinBudget`、`CorrespondenceBlockFusedAbcdIlpMatchesBlockAndStdWithinBudget` | `run_test_dual_correspondence_family` | `run_bench_dual_correspondence_family` | `run_board_bench_dual_correspondence_family`、`evidence_manifest.json`、`evidence_doctor.md` | board diagnostic 20 comparisons、`Errors=20`、`Warnings=21`、`Suggestions=0`；只能支持 no-production closeout。 |
| block-reduction A/B/C/N | adopted | `loadPointToPlaneLLSWeightedFullReductionVectors`、`accumulatePointToPlaneLLSWeightedBlockGroupA/B/C/N` | `run_test_candidates`、`run_board_test_candidates`、`run_test_production_direct`、`run_board_test_production_direct` | `run_bench_fused_formula`、`run_board_bench_fused_formula`、`run_bench_production_shaped_fused_formula`、`run_board_bench_production_shaped_fused_formula`、`trace_summary.md`、`checksum_validation.md` | 逐块规约树、accepted points、`ATA/ATb` 和 matrix 在预算内。 | solver 优化或 board 性能对所有点型都一样。 |
| fused formula code shape | adopted / accepted-risk | `WeightedFusedFormulaMode`、`accumulate_candidate_full_block_fused_*`、`estimate_candidate_full_block_fused_*` | `run_test_candidates`、`run_board_test_candidates` | `run_bench_fused_formula`、`run_board_bench_fused_formula`、`run_bench_generic_fused_abc`、`run_board_bench_generic_fused_abc`、`run_bench_generic_fused_formula`、`run_board_bench_generic_fused_formula`、`run_bench_generic_fused_abc_trace`、`run_board_bench_generic_fused_abc_trace`、`asm_production_symbol_attribution.md` | `abc`、`d-six-term`、`d-displacement`、`abcd` 及其 ILP 变体在 correctness 和预算内对齐。 | 每个 ILP 变体都一定有独立机器码收益。 |
| production default fused-abcd-ilp | adopted | `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)`、`estimatePointToPlaneLLSWeightedFullCloudRVV`、`buildPointToPlaneLLSWeightedFullCloudDefault` | `run_test_production_direct`、`run_board_test_production_direct`、`run_test_input_semantics` | `run_bench_production_dispatch`、`run_board_bench_production_dispatch`、`run_bench_production_default_fused_abcd_ilp_rvv`、`run_board_bench_production_default_fused_abcd_ilp`、`production_dispatch_fused_abcd_ilp/summary.md`、`production_default_fused_abcd_ilp/trace_summary.md` | full-cloud 默认 RVV path 的 correctness、trace、checksum 和 asm 归因已闭合到当前边界。 | source-indexed 也采用 full-cloud block-reduction；dual-indices / correspondences 也能直接接入 production。 |
| dual-indices / correspondences | diagnostic attempted / no-production | `ConstCloudIterator` clone 路径和公开 dual-index / correspondence overload；family compare 候选来自 `run_test_dual_correspondence_family`。 | `run_test_row_sources`、`run_board_test_row_sources`、`run_test_public_semantics`、`run_board_test_public_semantics`、`run_test_dual_correspondence_family`、`run_board_test_dual_correspondence_family` | `run_bench_row_sources`、`run_board_bench_row_sources`、`run_bench_dual_correspondence_family`、`run_board_bench_dual_correspondence_family`、`collect_board_row_sources_repeated`、`run_bench_default_diagnostic`（只作诊断形状） | 这些入口的 row source 语义、有限值语义和有效输入对拍，以及 dual/correspondence family carry-over 的单次板卡负向信号。 | production RVV 接入、repeated board、asm 闭合和 board clean pass。 |
| `Scalar=double` 与 layout miss fallback | deferred / fallback only | public overload 的标量 clone 路径 | `run_test_production_direct`、`run_board_test_production_direct`、`run_test_row_sources`、`run_board_test_row_sources` | `run_bench_default_diagnostic`、`run_board_bench_default_diagnostic`（只作诊断形状） | gate miss 后会回到标量语义；public matrix 不应被破坏。 | double 路径的 RVV 结论。 |

## 标量路径与 RVV 路径差异

| 阶段 | 标量路径 | RVV 路径 | 直接相关的证据 |
| --- | --- | --- | --- |
| 输入检查 | public overload 先检查 source / target / weights 长度。 | 同样保留检查。 | `run_test_input_semantics`。 |
| 行来源 | `ConstCloudIterator` 逐行同步前进。 | full-cloud 用连续权重和 AoS load；source-indexed 用 valid-index staging、source gather 和 target stride load。 | `run_test_public_semantics`、`run_test_row_sources`、`run_test_source_indices`。 |
| 取数 | 逐点读 source、target、weight。 | full-cloud 读 source / target AoS 字段并连续读 weights；source-indexed 额外先把索引展开成 `uint32_t` staging。 | `run_test_production_direct`、`run_bench_production_dispatch`、`run_test_source_indices`、`run_bench_production_source_indices`。 |
| 有限值检查 | 对 source xyz、target xyz 和 target normal 做 `continue`。 | 同样构造 mask，invalid lane merge to zero。 | `InvalidLaneMaskMatchesStd`、`NonFiniteWeightsAreNotMaskedWhenPointsAreFinite`。 |
| 公式 | 逐点计算 `a/b/c/d`。 | 在 `loadPointToPlaneLLSWeightedFullReductionVectors` 中生成向量 `a/b/c/d`。 | `run_test_candidates`。 |
| 规约 | 逐点累加到 normal-equation。 | A/B/C/N block groups 维护向量 partial sums，再显式 `vfredosum`。 | `run_test_candidates`、`trace_summary.md`。 |
| 求解 | Eigen 6x6 solve + matrix 构造。 | full-cloud 和 source-indexed 都复用同一 solve 和 matrix 构造。 | `run_test_production_direct`、`run_test_source_indices`。 |

## 代码级证据索引

### 1. 生产入口与 RVV 分流

| 代码 | 作用 | 对应 target | 证据文件 |
| --- | --- | --- | --- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | 真实 public full-cloud 入口；只在 full-cloud 上尝试 RVV。 | `run_test_production_direct`、`run_test_input_semantics`、`run_test_public_semantics` | `run_test_std.log`、`run_test_rvv.log`、`summary.md` |
| `canUsePointToPlaneLLSWeightedFullCloudRVV(...)` | gate：点数、weights、VLEN 和 byte-offset 是否允许 RVV。 | `run_test_production_direct` | `run_test_rvv.log`、`trace_summary.md` |
| `estimatePointToPlaneLLSWeightedFullCloudRVV(...)` | float-only RVV wrapper。 | `run_test_production_direct`、`run_bench_production_default_fused_abcd_ilp_rvv` | `trace_summary.md`、`asm_production_symbol_attribution.md` |
| `buildPointToPlaneLLSWeightedFullCloudBlockRVV(...)` | 默认 RVV normal-equation 构造。 | `run_test_production_direct`、`run_test_candidates` | `checksum_validation.md`、`trace_summary.md` |
| `buildPointToPlaneLLSWeightedFullCloudDefault(...)` | RVV miss 时回标量。 | `run_test_production_direct` | `run_test_std.log`、`run_test_rvv.log` |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | 真实 public source-indexed 入口；只在有效 source index 和 gate 成立时尝试 RVV。 | `run_test_source_indices`、`run_test_input_semantics`、`run_test_public_semantics` | `run_test_source_indices_std.log`、`run_test_source_indices_rvv.log`、`production_source_indices_staged_gather/summary.md` |
| `canUsePointToPlaneLLSWeightedSourceIndicesRVV(...)` | gate：source size、index count、target size、weights size、VLEN 和 byte-offset 是否允许 RVV。 | `run_test_source_indices` | `run_test_source_indices_rvv.log` |
| `estimatePointToPlaneLLSWeightedSourceIndicesRVV(...)` | source-indexed RVV wrapper。 | `run_test_source_indices`、`run_bench_production_source_indices` | `run_test_source_indices_rvv.log`、`production_source_indices_staged_gather/summary.md` |
| `buildPointToPlaneLLSWeightedSourceIndicesStagedRVV(...)` | valid-index scan、`uint32_t` staging、source gather、target stride load、weight load、`vcompress` 和 scalar tail。 | `run_test_source_indices`、`run_bench_production_source_indices` | `production_source_indices_staged_gather/summary.md` |
| `buildPointToPlaneLLSWeightedSourceIndicesDefault(...)` | source-indexed RVV miss 时回标量。 | `run_test_source_indices` | `run_test_source_indices_std.log`、`run_test_source_indices_rvv.log` |

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

### 4. Row source 入口

| 代码 | 当前状态 | 对应 target | 证据边界 |
| --- | --- | --- | --- |
| `ConstCloudIterator` 公开 overload clone | 保持标量 | `run_test_row_sources`、`run_test_public_semantics` | 只证明语义，不证明 production RVV。 |
| source-indexed production helper | adopted | `run_test_source_indices`、`run_bench_production_source_indices`、`run_board_bench_production_source_indices`、`collect_board_production_source_indices_repeated` | 已有 production direct、fallback、代表点型 board summary 和 dedicated bench。 |
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
| `run_test_production_direct` | 真实 production full-cloud 与 source-indexed 入口。 | dispatch / fallback / layout gate / representative point types。 | dual-indices、correspondences 的 production 接入。 |
| `run_test_source_indices` | 真实 production source-indexed 入口。 | source-indexed dispatch / fallback / layout gate / representative point types。 | dual-indices、correspondences 的 production 接入。 |

### bench target

| target | case-filter | 含义 | 它证明什么 | 它不能证明什么 |
| --- | --- | --- | --- | --- |
| `run_bench_fused_formula` | `fused-formula` | fused 候选诊断子集。 | `block-fused-*` 的 formula 形状和 checksum。 | 真实 public dispatch。 |
| `run_bench_row_sources` | `row-sources` | 数据源候选诊断子集。 | full-cloud/source-indexed/dual-indices/correspondences candidate 的耗时和 checksum 形状。 | dedicated production dispatch；dual-indices / correspondences 的生产接入。 |
| `run_bench_production_source_indices` | `production-source-indices` | 真实 source-indexed 生产路径。 | source-indexed std / RVV production direct speedup。 | dual-indices / correspondences。 |
| `run_bench_production_shaped_fused_formula` | `production-shaped-fused-formula` | layout-gated helper 对拍。 | 同边界 full estimate 候选筛选。 | 真实 public overload。 |
| `run_bench_generic_fused_abc` | `generic-fused-abc` | 三类代表点型的 abc 诊断。 | generic layout gate 和代表点型正确性。 | D 项和 abcd 全候选。 |
| `run_bench_generic_fused_formula` | `generic-fused-formula` | 全部 fused 候选诊断。 | `abc`、D 项、`abcd` 及 ILP 变体。 | production dispatch。 |
| `run_bench_generic_fused_abc_trace` | `generic-fused-abc-trace` | 逐 iteration 追踪。 | 长尾和顺序变化。 | 默认 production trace。 |
| `run_bench_production_dispatch` | `production-dispatch` | 真实 public full-cloud overload。 | std / RVV production direct speedup。 | source-indexed / dual-indices / correspondences / all point types 逐类型性能。 |
| `run_bench_production_default_fused_abcd_ilp` | `production-default-fused-abcd-ilp` | QEMU std/RVV 形状检查。 | 默认 production case-filter 在两种构建下都能输出日志。 | QEMU 性能结论。 |
| `run_bench_production_default_fused_abcd_ilp_rvv` | `production-default-fused-abcd-ilp` | RVV-only 形状检查。 | 默认 RVV path 的输出和日志形状。 | repeated board 性能。 |

### board target

| target | 含义 | 证据角色 |
| --- | --- | --- |
| `collect_board_row_sources_repeated` | 多轮采集行来源诊断。 | 行来源重复板卡摘要候选。 |
| `collect_board_production_dispatch_repeated` | 多轮采集真实 public overload。 | 生产路径重复板卡性能摘要。 |
| `collect_board_production_source_indices_repeated` | 多轮采集真实 public source-indexed overload。 | source-indexed 生产路径重复板卡性能摘要。 |
| `collect_board_production_default_fused_abcd_ilp` | 多轮采集默认 RVV path。 | 追踪、校验和、反汇编归因。 |
| `summarize_board_production_default_fused_abcd_ilp` | 汇总 trace。 | 运行稳定性和 checksum 序列。 |
| `asm_production_default_fused_abcd_ilp` | 重新导出 asm attribution。 | 路径和指令归因。 |

## Source-Indexed 采纳结果

source-indexed 先由 `log/board/run_board_bench_row_sources/analyze_bench_compare.log` 的 row-source 单次板卡诊断触发 production integration loop。采纳结论来自 dedicated production target，不来自默认综合 bench。summary 路径是：

```text
log/board/production_source_indices_staged_gather/summary.md
```

| case | 262144 median/min | 65536 median/min | 结论 |
| --- | ---: | ---: | --- |
| `pointnormal` | `2.33x / 1.99x` | `2.59x / 2.47x` | 两个规模均正向。 |
| `pointxyz-to-pointnormal` | `2.22x / 2.18x` | `2.57x / 2.49x` | 两个规模均正向。 |
| `pointxyz-to-pointxyzinormal` | `2.37x / 2.28x` | `2.78x / 2.70x` | 两个规模均正向。 |

该结果只支持 valid source-indexed public overload。它不支持 dual-indices、correspondences、invalid index public API 行为、`Scalar=double` 或非连续权重。

source-indexed repeated board summary 已补 machine-readable Evidence Doctor 边界：

```text
log/board/production_source_indices_staged_gather/evidence_manifest.json
log/board/production_source_indices_staged_gather/evidence_doctor.md
```

doctor 结果为 0 Errors / 7 Warnings / 6 Suggestions。Warnings 主要说明 source-indexed-specific asm boundary 还没有闭合，并记录 `pointnormal 262144` 的 long-tail / variance；Suggestions 主要是 binary identity 缺口。这些 finding 不推翻当前 staged-gather / compressed-tail adopted 结论，但禁止把 source-indexed 写成完整 doctor clean pass。source-indexed block-reduction / fused-formula / ILP family carry-over audit 已由 Phase 030 的 `3E / 6W / 13S` repeated diagnostic 闭合为当前 no-production。

## Source-Indexed 实现族状态

source-indexed 当前 production 的函数链路是：

```text
estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)
  -> estimatePointToPlaneLLSWeightedSourceIndicesRVV(...)
  -> buildPointToPlaneLLSWeightedSourceIndicesStagedRVV(...)
  -> loadPointToPlaneLLSWeightedSourceIndexedVectors(...)
  -> accumulatePointToPlaneLLSWeightedCompressedRowsF32M2(...)
  -> solvePointToPlaneLLSWeightedNormalEquation(...)
```

这条链路说明当前采用的是 staged-gather + compressed-tail family。它不是 full-cloud block-reduction / fused-formula family 的简单搬运。

| 维度 | 当前状态 | 当前证据 | 不能外推 |
| --- | --- | --- | --- |
| source-indexed staged-gather / compressed-tail | adopted | `run_test_source_indices_compare`、`run_bench_production_source_indices`、`collect_board_production_source_indices_repeated`、`production_source_indices_staged_gather/summary.md` | source-indexed 其它 family 的优劣。 |
| source-indexed block-reduction / fused-formula family | attempted / no-production for current evidence | `run_test_source_indexed_family_compare`、`run_board_bench_source_indexed_family`、`source_indexed_family_repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | 新 repeated summary 显示 `block-fused-abcd-ilp` full estimate median `0.90x` 且 `4/5` 低于 `1.0x`；不能直接改 production，也不进入 production-candidate。 |

如果未来要替换当前 source-indexed helper family，必须先补同边界 candidate、bench 和 board 证据，再更新本表和 evaluation。这个 audit 至少要回答：gather 后是否还能复用 A/B/C/N block groups、fused-abcd-ilp 是否改变误差预算或寄存器压力、index staging 成本是否进入计时边界、以及 source-indexed 专属 asm attribution 是否闭合。

当前 source-indexed repeated board summary 已有独立 Evidence Doctor / manifest 边界，但仍缺 source-indexed-specific asm attribution 和 binary identity。row-source diagnostic trigger 也已补 diagnostic manifest / doctor：

```text
log/board/run_board_bench_row_sources/evidence_manifest.json
log/board/run_board_bench_row_sources/evidence_doctor.md
```

该 row-source manifest 只记录 single-run observed speedup 和 pre-production diagnostic 边界：full-cloud / source-indexed 正向，dual-indices / correspondences 负向。它不能替代当前 production summary，也不能把 dual-indices / correspondences 直接推进 production。

## 当前可提交证据

这些文件已经是当前 topic 的稳定证据入口。

| 文件 | 证据角色 |
| --- | --- |
| `log/qemu/run_test_std.log` | QEMU std correctness；`45 passed + 1 skipped`。 |
| `log/qemu/run_test_rvv.log` | QEMU RVV correctness；`47 passed`。 |
| `log/qemu/run_test_source_indices_std.log` | source-indexed production direct std correctness；`6 passed`。 |
| `log/qemu/run_test_source_indices_rvv.log` | source-indexed production direct RVV correctness；`7 passed`。 |
| `log/board/run_board_bench_row_sources/analyze_bench_compare.log` | row-source 诊断触发原始日志；source-indexed 65536 / 262144 正向，dual-indices 和 correspondences 负向。 |
| `log/board/run_board_bench_row_sources/evidence_manifest.json` | row-source diagnostic manifest；记录 observed speedup 和 diagnostic evidence role。 |
| `log/board/run_board_bench_row_sources/evidence_doctor.md` | row-source diagnostic doctor；0 Errors / 0 Warnings / 0 Suggestions。 |
| `log/board/production_dispatch_fused_abcd_ilp/summary.md` | production-dispatch repeated std/RVV speedup summary。 |
| `log/board/production_source_indices_staged_gather/summary.md` | source-indexed repeated std/RVV speedup summary。 |
| `log/board/production_source_indices_staged_gather/evidence_manifest.json` | source-indexed production repeated board manifest。 |
| `log/board/production_source_indices_staged_gather/evidence_doctor.md` | source-indexed Evidence Doctor；0 Errors / 7 Warnings / 6 Suggestions。 |
| `log/board/source_indexed_family_repeated/summary.md` | source-indexed implementation-family repeated diagnostic summary；当前 `block-fused-abcd-ilp` full estimate median `0.90x`。 |
| `log/board/source_indexed_family_repeated/evidence_manifest.json` | source-indexed implementation-family repeated diagnostic manifest。 |
| `log/board/source_indexed_family_repeated/evidence_doctor.md` | source-indexed implementation-family repeated doctor；3 Errors / 6 Warnings / 13 Suggestions。 |
| `log/board/source_indexed_family_repeated/evidence_doctor.json` | source-indexed implementation-family repeated doctor 机器可读输出。 |
| `log/board/production_default_fused_abcd_ilp/trace_summary.md` | 默认 RVV path 多轮 trace summary。 |
| `log/board/production_default_fused_abcd_ilp/checksum_validation.md` | 默认 RVV path checksum 序列一致性。 |
| `log/board/production_default_fused_abcd_ilp/asm_production_symbol_attribution.md` | 默认 production helper 反汇编归因。 |

## 结论边界

当前证据支持以下结论：

- full-cloud public overload 的 RVV dispatch 可以在当前 gate 下命中。
- source-indexed public overload 的 RVV dispatch 可以在有效 source index、连续权重和当前 gate 下命中。
- `Scalar=float`、连续 `weights_`、source xyz f32 AoS、target xyz+normal f32 AoS 的路径已经有 correctness、trace、checksum 和 asm 归因证据。
- source-indexed 路径已经有 correctness、bench 和 repeated board speedup 证据。
- full-cloud block-reduction 与 fused formula 的候选已经通过 test target 和 bench target 闭环。
- source-indexed block-reduction / fused-formula / ILP family comparison 已在当前 `PointNormal` / `float` /
  262144 点 diagnostic 下闭合为 no-production；`block-fused-abcd-ilp` 不进入 production-candidate。
- dual-indices、correspondences 仍然是标量 production 边界，不应被写成 production RVV 已接入。

当前证据不支持以下扩展：

- 把 dual-indices / correspondences 路径写成 production RVV。
- 把 `Scalar=double` 写成 RVV 接入。
- 把 QEMU timing 写成性能结论。
- 把 representative point types 的结果外推成所有 gate-allowed 点型逐类型性能。
