# registration/transformation_estimation_point_to_plane_lls_weighted 函数级 RVV 评估

## 范围与结论

- 主题：`transformation_estimation_point_to_plane_lls_weighted`
- production 文件：`registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp`
- 专项测试目录：`test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/`
- 公开入口：`TransformationEstimationPointToPlaneLLSWeighted::estimateRigidTransformation`

当前 EvidenceDecision：

```text
production-adopted/full-cloud-f32-aos-layout-gated-weighted-block-dispatch-fused-abcd-ilp-accepted-risk
```

生产接入只覆盖 full-cloud public overload（全云公开入口）、`Scalar=float`、连续 `weights_`、source `RVVXYZAoSFloatLayout`、target `RVVXYZNormalFloatLayout`、size/VLEN/byte-offset gate 均满足的路径。该路径保留 weighted block-reduction，并在逐点 `a/b/c/d` 公式块中采用 `fused-abcd-ilp` code shape。source-indexed（源索引路径）、dual-indices（双索引路径）、correspondences（对应关系路径）、`Scalar=double` 和非连续权重都保持标量。

主文档负责长期算法说明和设计理由：`doc-rvv/registration/transformation_estimation_point_to_plane_lls_weighted-RVV.zh.md`。本评估文档只记录测试矩阵、bench 边界、证据索引和决策审计。

目录怎么跑、点型怎么来的、统计脚本怎么用，先看 [README.zh.md](../README.zh.md)。

## Production patch scope

| 项 | 当前状态 | 证据 |
| --- | --- | --- |
| dispatch | 只在 full-cloud overload 中尝试 RVV；命中后提前返回，否则进入原 iterator 标量 helper。 | production direct tests、源码审查。 |
| generic gate | source 用 `RVVXYZAoSFloatLayout`，target 用 `RVVXYZNormalFloatLayout`，两侧分别 offset/stride。 | `PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal` tests。 |
| weights | 只读成员 `weights_` 连续 vector。 | public overload tests 和 production-dispatch bench 都调用 `setCorrespondenceWeights(weights)`。 |
| reduction | 采用 weighted full-cloud block-reduction；每个 chunk 用 `vfmacc` 更新 A/B/C/N partial sums，每组扫完 block 后显式 `vfredosum`。 | normal-equation tests、asm attribution、board 5-run。 |
| formula | `a/b/c` 使用 seed multiply + `vfmsac`；`d` 使用三组点差后 `vfmul + vfmacc + vfmacc`。 | production source、production-symbol asm attribution、production default trace。 |
| fallback | 非 RVV、非 float、小规模、layout miss、VL miss、byte-offset miss、indexed/correspondences 均标量。 | fallback tests 和源码审查。 |

## 标量流程与 RVV 流程对照

| 阶段 | 标量路径 | 当前 RVV production |
| --- | --- | --- |
| 入口检查 | full-cloud overload 检查 source/target 点数和 `weights_.size()`。 | 保留原检查。 |
| row source | `ConstCloudIterator` 同步读取 `source[k]` 和 `target[k]`。 | 只接 full-cloud，source/target 直接按 AoS stride load。 |
| weight | `weights_it` 顺序读取。 | `vle32.v` 连续加载 `weights_[k]`。 |
| finite mask | 检查 source xyz、target xyz 和 target normal，不检查 weight。 | 同语义，invalid lane merge to zero。 |
| formula | `normal *= weight` 后计算 `a/b/c/d`。 | `a/b/c` 使用 `vfmsac`，`d` 使用位移差和 `vfmacc`；源码形态对应 `fused-abcd-ilp`。 |
| accumulation | 按 row 顺序用 double 累加 `ATA/ATb`。 | 每个 block 用 A/B/C/N vector partial sums；chunk 内用 `vfmacc` 累加，组结束后再 `vfredosum` 到 double normal-equation。 |
| solve / matrix | Eigen 6x6 inverse solve，构造 4x4 matrix。 | 保留标量。 |

## 函数族评估表

| 路径 | 当前决策 | 证据边界 |
| --- | --- | --- |
| full-cloud staged-row diagnostic | 保留为测试专用 baseline | `include/impl/teptplw_candidate_full_cloud.hpp` 提供早期 staged-row accumulate，`include/impl/teptplw_candidate_estimates.hpp` 提供 `estimate_candidate_full` wrapper；该路径使用 `vcompress + fixed buffer + tail`，可作为 full-cloud 消融对照，production 默认路径使用 block-reduction。 |
| full-cloud block-reduction | adopted | 已有 production direct/fallback、numeric stress、asm 和 representative board 5-run。 |
| source-indexed | 仅保留诊断证据 | correctness 已建，board single-run 弱正向，缺 repeated board 和 production direct/fallback。 |
| dual-indices | 仅保留诊断证据 | correctness 已建，board single-run 负向。 |
| correspondences | 仅保留诊断证据 | correctness 已建，board single-run 负向，且混入 index/weight 展开和 gather 多个成本源。 |
| fused formula | production adopted / accepted-risk | test-only diagnostic/component candidates 已通过 QEMU/board correctness、数值预算和 diagnostic helper asm attribution；`abc`、D 项和 `abcd` 已泛型化并补 representative warm-up 5-run。真实 production-symbol `abcd-ilp` helper 接入前已补 asm、5/10/20-run 板卡数据；虽然 `PointNormal -> PointNormal` 20-run fused-vs-block avg B/A median 为 `0.976x` 且 avg 低于 `1.0x` 为 `12/20`，但按新的接入优先级人工接受该风险，默认 full-cloud RVV path 已采用 fused 公式。 |
| `Scalar=double` | fallback | 有 fallback smoke；不进入 RVV。 |

## RowSourcePolicy + WeightPolicy 测试矩阵

| 入口形态 | RowSourcePolicy 取点 | WeightPolicy 取权重 | 关键测试 | Bench case | 当前判断 |
| --- | --- | --- | --- | --- | --- |
| full-cloud staged-row diagnostic | `source[k] + target[k]` | `weights_[k]` 连续读取 | `StdDiagnosticMatchesPublicEstimator`、`FullCloudCandidateMatchesStd` | `weighted lls full-cloud pointnormal` | 测试专用 staged-row baseline。 |
| full-cloud block-reduction | 同 full-cloud | `weights_[k]` 连续读取 | `FullCloudBlockReductionMatchesStdWithinBudget`、production direct/fallback tests | `weighted lls full-cloud block-reduction pointnormal`、production-dispatch repeated rows | adopted production path。 |
| source-indexed | `source[indices_src[k]] + target[k]` | `weights_[k]` 连续读取 | `StdSourceIndexedMatchesPublicEstimator`、`SourceIndexedCandidateMatchesStd` | `weighted lls source-indices pointnormal` | 仅保留诊断证据。 |
| dual-indices | `source[indices_src[k]] + target[indices_tgt[k]]` | `weights_[k]` 连续读取 | `StdDualIndicesMatchesPublicEstimator`、`DualIndicesCandidateMatchesStd` | `weighted lls dual-indices pointnormal` | 仅保留诊断证据。 |
| correspondences | 展开 `index_query/index_match` 后 gather | 展开 `correspondence.weight` | `StdCorrespondencesMatchesPublicEstimator`、`CorrespondenceCandidateMatchesStd` | `weighted lls correspondences pointnormal` | 仅保留诊断证据。 |

Weight finite semantics（权重有限性语义）由 `NonFiniteWeightsAreNotMaskedWhenPointsAreFinite` 和 `ProductionFullCloudPreservesNonFiniteWeightSemantics` 保护：point/normal 非有限会被剔除，weight 非有限但 point/normal 有限时仍参与计算。

## Production direct 与 fallback 测试

| 测试 | 层级 | 覆盖内容 |
| --- | --- | --- |
| `ProductionFullCloudPublicOverloadMatchesStdWithinBudget` | production direct | 真实 public full-cloud overload 的 matrix 对拍。 |
| `ProductionFullCloudNormalEquationMatchesStdWithinBudget` | production direct / normal-equation | `accepted_points`、`ATA/ATb` 和 matrix 预算。 |
| `ProductionFullCloudScaleStressMatchesStdWithinBudget` | numeric stress | 大尺度输入和权重动态范围下的 matrix / normal-equation。 |
| `ProductionFullCloudPreservesNonFiniteWeightSemantics` | numerical consistency | point/normal finite mask 与非有限 weight 语义。 |
| `ProductionFullCloudSmallInputFallsBackToScalar` | fallback | `n < 64` 回标量。 |
| `ProductionFullCloudPredicateGatesAreNarrow` | gate | size、weights size、VL 和 byte-offset predicate。 |
| `ProductionFullCloudPointXYZSourceMatchesStdWithinBudget` | generic source | `PointXYZ -> PointNormal` 命中 source xyz f32 AoS gate。 |
| `ProductionFullCloudPointXYZToPointXYZINormalMatchesStdWithinBudget` | generic target | `PointXYZ -> PointXYZINormal` 命中 target xyz+normal f32 AoS gate。 |
| `ProductionFullCloudDoubleNormalTargetFallsBackToScalar` | layout fallback | target normal 不满足单个 float 字段条件时回标量。 |
| `ProductionFullCloudScalarDoubleFallsBackToScalar` | scalar fallback | `Scalar=double` 回标量。 |

完整专项矩阵还保留 diagnostic tests：四条 row source 对拍、小规模 isolated gate、invalid lane mask、非有限 weight 语义和 block-reduction A/B。保留这些测试是为了把 row source、weight source、mask、reduction tree 和 production dispatch 分层审查。

## Fused formula diagnostic 测试

fused formula（融合公式）follow-up 先在 test-rvv diagnostic 层筛选公式树候选，再补 production-symbol 和默认 production 证据。当前 production selector 已采用 `abcd-fused-ilp` 公式块；其它 `abc`、D 项和非 ILP 变体仍只作为 test_support 消融候选。所有候选都保留 full-cloud、连续 `weights_`、finite point/normal mask、非有限 weight 传播和 A/B/C/N block-reduction 组织，只改变或诊断 `a/b/c/d` 逐点公式树。

| 测试 | 层级 | 覆盖内容 |
| --- | --- | --- |
| `FullCloudBlockFusedFormulaCandidatesMatchBlockAndStdWithinBudget` | diagnostic same-chain | 四类 fused candidate 对拍同边界 block baseline 和标量 reference，覆盖 `accepted_points`、`ATA/ATb` 和 matrix。 |
| `FullCloudBlockFusedFormulaNearCancellationStressMatchesBlockAndStd` | numerical consistency | 大坐标、小位移的 near-cancellation 样本，保护 `d-six-term-fma` 和 `d-displacement-fused` 的舍入风险。 |
| `FullCloudBlockFusedFormulaScaleStressMatchesBlockAndStd` | numerical consistency | normal、weight 和坐标高动态范围输入，检查 normal-equation 和最终 matrix 预算。 |
| `FullCloudBlockFusedFormulaPreservesNonFiniteWeightSemantics` | numerical consistency | 非有限 point/normal 仍由 finite mask 剔除；非有限 weight 不参与 mask，但继续传播到 normal-equation。 |
| `FullCloudGenericAbcFusedRepresentativePointTypesMatchStd` | generic representative correctness | `abc-fused` 和 `abc-fused-ilp` 覆盖 `PointNormal -> PointNormal`、`PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal`。 |
| `FullCloudGenericDAndAbcdFusedRepresentativePointTypesMatchStd` | generic representative correctness | `d-six-term`、`d-six-term-ilp`、`d-displacement`、`d-displacement-ilp`、`abcd`、`abcd-ilp` 覆盖三类代表点型。 |

## Bench 边界

输入在计时前构造：source 是确定性曲面，target 由 `transformPointCloudWithNormals` 生成，full/source/dual 权重是周期序列，indices 和 correspondences 是确定性有效输入。

测量包含 normal-equation 构造、Eigen solve 和 matrix 构造。full-cloud 不包含权重生成；source-indexed 和 dual-indices 的 `pcl::Indices` 输入在计时前构造，但 candidate 内部 valid-index scan、`uint32_t` staging、byte-offset prepare、index load/gather 和连续 weight load 计入；correspondences case 包含 candidate 内部 index/weight 展开。

早期 staged-row diagnostic 位于 `include/impl/teptplw_candidate_full_cloud.hpp`，外层仍通过 `include/impl/teptplw_candidates.hpp` 聚合暴露。`vcompress + fixed buffer + tail` 的含义是：RVV 先用 finite mask 保留有效 lane，把每个有效 lane 的 `a/b/c/d/nx/ny/nz` 压缩写入固定大小 buffer，然后由尾段循环把 buffer 中的行按标量方式累加到 normal-equation。这个设计便于审查 mask 和 accepted-point 顺序，也带来额外 store/load 和固定 buffer 成本。该路径保留为消融 baseline，不参与当前 production default dispatch。

| case | 入口 | 证明点 | 不能证明什么 |
| --- | --- | --- | --- |
| `weighted lls full-cloud pointnormal` | 测试专用 `estimate_candidate_full` | 早期 staged-row baseline 的 stride load、contiguous weight、mask、`vcompress` 和 buffer tail。 | production dispatch、generic 点型、indexed/correspondences。 |
| `weighted lls full-cloud block-reduction pointnormal` | 测试专用 block helper | A/B/C/N partial sums 和显式 `vfredosum`。 | 生产性能由 production-dispatch 5-run 证明。 |
| `weighted lls component full-cloud block-fused-* no-solve pointnormal` | 测试专用 fused component helper | 只测 normal-equation 构造，不含 Eigen solve / matrix；用于逐点公式树 A/B 归因。 | production dispatch、端到端收益、代表点型。 |
| `weighted lls full-cloud block-fused-* pointnormal` | 测试专用 fused estimate helper | 包含 normal-equation、solve 和 matrix，用来观察 component 变化进入完整 estimate 后的形状。 | production dispatch、generic 点型、indexed/correspondences。 |
| `weighted lls production-shaped full-cloud block-baseline pointnormal` | 测试专用 layout-gated block helper | 接入前非 fused 公式的同边界 A 侧，包含 normal-equation、solve 和 matrix。 | production dispatch；生产性能由 `production-dispatch` 证明。 |
| `weighted lls production-shaped full-cloud block-fused-* pointnormal` | 测试专用 fused helper | 用同一 test_support estimate 外壳比较 fused 公式 B 侧。 | 只支持 helper A/B 和公式消融，不作为 production direct。 |
| `weighted lls component/production-shaped full-cloud block-fused-abc* <representative>` | 测试专用 generic helper | `generic-fused-abc` case-filter 覆盖 component no-solve 和 full estimate 的三类代表点型，baseline 与 fused 均走 layout-gated test_support helper。 | `abc-fused-ilp` 只作为源码调度诊断。 |
| `weighted lls component/production-shaped full-cloud block-fused-* <representative>` | 测试专用 generic helper | `generic-fused-formula` case-filter 覆盖 `abc`、D 项和 `abcd` 候选的 component no-solve 与 full estimate。 | 当前 `*-ilp` 在 asm 中与对应非 ILP mode 等价。 |
| `weighted lls source-indices pointnormal` | 测试专用 source-indexed helper | 单侧 source gather + target stride + continuous weight。 | dual gather、correspondence weight 展开、生产收益。 |
| `weighted lls dual-indices pointnormal` | 测试专用 dual helper | 双侧 gather 与连续 weight stream。 | correspondence parsing 和生产收益。 |
| `weighted lls correspondences pointnormal` | 测试专用 correspondences helper | index/weight 展开 + gather 语义。 | 单因归因到 gather、任意真实 correspondence 分布。 |
| `weighted lls production-dispatch full-cloud pointnormal` | 真实 public overload | `PointNormal -> PointNormal` 子集的 public dispatch 性能。 | 所有泛型点型、indexed/correspondences。 |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointnormal` | 真实 public overload | generic source representative 性能。 | 所有 source 点型。 |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointxyzinormal` | 真实 public overload | generic target representative 性能。 | 所有 target normal 点型。 |

## Fused Formula 对比口径

fused formula follow-up 同时报告三类数值，含义不同：

| 口径 | A 侧 | B 侧 | 指标 | 结论边界 |
| --- | --- | --- | --- | --- |
| direct diagnostic A/B | test-rvv block-reduction helper | test-rvv fused helper | `B/A = A_rvv_ms / B_rvv_ms`，`>1` 表示 B 更快 | 只用于 helper 消融。 |
| production-shaped helper A/B | test-rvv layout-gated block helper | test-rvv fused helper | `B/A = A_rvv_ms / B_rvv_ms`，`>1` 表示 B 更快 | 同边界比较 full estimate，不代表 production direct。 |
| production dispatch | 真实 full-cloud public overload | 同一个 public overload 的 RVV 构建 | `std_ms / rvv_ms` | 真实生产入口性能证据。 |
| std/RVV speedup | 同一 bench case 的 std 构建 | 同一 bench case 的 RVV 构建 | `std_ms / rvv_ms` | 只说明该 case 自身 RVV 形状，不能替代 fused-vs-block B/A。 |

## 证据索引与结果

| 证据 | 路径 / 命令 | 结果 |
| --- | --- | --- |
| QEMU tests | `log/qemu/run_test_std.log`；`log/qemu/run_test_rvv.log` | `run_test_compare` 生成；std 31 passed + 1 skipped，RVV 32 passed，包含 generic abc 与 D/ABCD fused representative correctness。 |
| board tests | `board run_test` | board 32 tests passed，包含 generic abc 与 D/ABCD fused representative correctness。 |
| QEMU bench shape | `make -C ... run_bench_compare` 和 `production-shaped-fused-formula` case-filter | 可解析；QEMU timing 不作性能结论。 |
| asm | `make -C ... dump_bench_rvv` | `production-default-fused-abcd-ilp` profile 按 `boundary` 记录实际归因边界；`detail` 或 `estimate-rvv-helper` 边界内确认 stride load、contiguous weight load、A/B/C/N partial sums、`vcpop/vmerge` 和显式 `vfredosum`。 |
| 10-case board diagnostic | 文档 summary；顶层 log 会被 case-filter 覆盖 | single-run diagnostic signal。 |
| production-dispatch repeated board | `log/board/production_dispatch_fused_abcd_ilp/summary.md` | 当前 production default 的三类代表点型在 262144 点上 repeated std/RVV speedup 均正向。 |
| production-default trace | `log/board/production_default_fused_abcd_ilp/trace_summary.md` | 5 runs、20 iterations、5 warm-up；三类点型 checksum 序列一致。 |
| checksum validation | `log/board/production_default_fused_abcd_ilp/checksum_validation.md` | 5 轮 checksum 行齐全，序列一致。 |
| production asm | `log/board/production_default_fused_abcd_ilp/asm_production_symbol_attribution.md` | 当前默认 production helper 的 RVV 指令归属。 |

重复上板采集使用 `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/script/collect_teptplw_board_rvv_ba.py`。生产默认证据使用固定目录 `log/board/production_default_fused_abcd_ilp/`。脚本会生成 raw log、checksum、trace summary 和 asm attribution；提交边界只保留固定目录中的摘要文件，raw log 由脚本重新生成。

当前 production-dispatch repeated 5-run 摘要（262144 点）：

| case | runs | 64K median/min | 256K median/min | 说明 |
| --- | ---: | ---: | ---: | --- |
| `pointnormal` | 5 | n/a | `2.76x / 2.73x` | `PointNormal -> PointNormal`；5 轮均正向。 |
| `pointxyz-to-pointnormal` | 5 | n/a | `2.98x / 2.95x` | source generic gate representative；5 轮均正向。 |
| `pointxyz-to-pointxyzinormal` | 5 | n/a | `3.00x / 2.96x` | source + target generic gate representative；5 轮均正向。 |

QEMU 10-case diagnostic table 和 board 10-case diagnostic table 只用于日志形状和诊断信号。当前性能结论使用 `log/board/production_dispatch_fused_abcd_ilp/summary.md` 和 `log/board/production_default_fused_abcd_ilp/trace_summary.md`。

下列 fused diagnostic 数值来自本轮修正对比边界前的历史日志。历史 `production-shaped` 行曾使用真实 public baseline 与 test_support fused helper 做混合边界比较。当前代码已改为同边界 layout-gated helper A/B；这些表只保留候选筛选过程，当前 production 结论使用固定目录里的 production-dispatch summary、production-default trace 和默认 asm 归因。

fused formula PointNormal direct diagnostic 5-run 摘要中的 std/RVV speedup：

| candidate | 64K full median/min | 256K full median/min | 结论 |
| --- | ---: | ---: | --- |
| `abc-fused` | `1.49x / 1.46x` | `1.45x / 1.35x` | median 正向，但只有 diagnostic direct evidence。 |
| `d-six-term-fma` | `1.55x / 1.55x` | `1.47x / 1.01x` | 最强 isolated 信号，256K 仍有低谷。 |
| `d-displacement-fused` | `1.55x / 0.91x` | `1.24x / 0.94x` | 出现负向 run，不进入 production。 |
| `abcd-fused` | `1.34x / 1.16x` | `1.43x / 0.93x` | direct diagnostic 单独不足以作为接入证据；后续判断依赖 generic、production-symbol 和 production-dispatch 证据。 |

这些 fused rows 的 std/RVV 双侧调用 test-rvv helpers。真实 public production overload 的性能结论来自 production-dispatch repeated summary。

direct diagnostic 的主判断还需要看 fused-vs-block B/A（RVV ms 计算的 block baseline / fused candidate）。该表显示 256K 相对 block baseline 多数不稳定或负向：

| candidate | 64K B/A median/min | 256K B/A median/min | 结论 |
| --- | ---: | ---: | --- |
| `abc-fused` | `1.08x / 0.99x` | `0.96x / 0.88x` | direct diagnostic 不支持 production。 |
| `d-six-term-fma` | `1.14x / 1.03x` | `0.98x / 0.65x` | 256K 低谷明显。 |
| `d-displacement-fused` | `1.12x / 1.00x` | `0.81x / 0.61x` | 256K 负向。 |
| `abcd-fused` | `1.11x / 0.78x` | `0.92x / 0.60x` | 组合后不稳定。 |

production-shaped PointNormal 5-run B/A 摘要：

| candidate | 64K B/A median/min | 256K B/A median/min | 结论 |
| --- | ---: | ---: | --- |
| `abc-fused` | `1.03x / 1.02x` | `1.11x / 1.09x` | PointNormal 正向，值得迁移 generic test_support helper 后继续复核。 |
| `d-six-term-fma` | `1.06x / 1.05x` | `1.13x / 0.80x` | 256K 有明显退化 run，不进 production。 |
| `d-displacement-fused` | `1.05x / 1.05x` | `1.10x / 0.72x` | 256K 有明显退化 run，不进 production。 |
| `abcd-fused` | `1.05x / 1.05x` | `1.13x / 1.09x` | PointNormal 正向；该历史表只说明继续复核价值，后续已补代表点型和 production-symbol 证据。 |

generic abc representative 5-run B/A 摘要：

| candidate / layer | `pointnormal` 64K / 256K median-min | `pointxyz-to-pointnormal` 64K / 256K median-min | `pointxyz-to-pointxyzinormal` 64K / 256K median-min | 结论 |
| --- | ---: | ---: | ---: | --- |
| `abc-fused` component no-solve | `1.01x / 1.00x`、`0.96x / 0.89x` | `1.02x / 1.02x`、`1.33x / 0.73x` | `1.02x / 1.01x`、`1.02x / 0.40x` | component 层有 256K 退化，不闭合。 |
| `abc-fused` production-shaped full | `1.03x / 1.03x`、`1.05x / 1.05x` | `1.04x / 1.04x`、`1.05x / 1.04x` | `1.04x / 1.03x`、`0.85x / 0.63x` | generic target 256K 负向，不接 production。 |
| `abc-fused-ilp` production-shaped full | `1.03x / 1.03x`、`1.05x / 0.96x` | `1.04x / 1.04x`、`1.04x / 0.76x` | `1.04x / 1.03x`、`1.22x / 1.05x` | 有正向项；asm 与非 ILP 公式形状相同。 |

generic fused-formula warm-up 5-run B/A 摘要（262144，RVV-only，`--warmup-iterations 5`）：

| candidate | component `pointnormal` median/min | component `pointxyz->pointnormal` median/min | component `pointxyz->xyzinormal` median/min | production-shaped `pointnormal` median/min | production-shaped `pointxyz->pointnormal` median/min | production-shaped `pointxyz->xyzinormal` median/min | 结论 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `d-six-term` | `0.947x / 0.655x` | `1.031x / 1.019x` | `1.025x / 0.955x` | `1.023x / 1.020x` | `1.042x / 0.936x` | `1.054x / 1.039x` | `PointNormal` component 不闭合。 |
| `d-six-term-ilp` | `0.982x / 0.925x` | `1.031x / 1.029x` | `1.028x / 1.003x` | `1.021x / 1.017x` | `1.050x / 1.015x` | `1.051x / 0.988x` | asm 与非 ILP 等价。 |
| `d-displacement` | `0.995x / 0.585x` | `1.041x / 1.035x` | `1.042x / 1.016x` | `1.023x / 0.983x` | `1.067x / 0.988x` | `1.070x / 1.062x` | 两个 `PointXYZ` 代表组合强，`PointNormal` component 不稳。 |
| `d-displacement-ilp` | `0.972x / 0.605x` | `1.044x / 1.030x` | `1.045x / 1.003x` | `1.027x / 0.862x` | `1.065x / 1.036x` | `1.086x / 1.065x` | asm 与非 ILP 等价。 |
| `abcd` | `1.010x / 1.002x` | `1.044x / 1.041x` | `1.044x / 1.041x` | `1.021x / 1.016x` | `1.067x / 1.064x` | `1.098x / 1.061x` | 本轮最稳，适合进入下一轮 production-loop 评估。 |
| `abcd-ilp` | `1.013x / 1.010x` | `1.044x / 1.041x` | `1.040x / 1.012x` | `1.026x / 1.021x` | `1.064x / 1.051x` | `1.081x / 1.058x` | asm 与非 ILP 等价。 |

## 反汇编归属

`buildPointToPlaneLLSWeightedFullCloudBlockRVV` 是当前 production detail helper 的首选归因符号。默认 profile 会先找这个 detail 符号；如果某个点型被编译器内联或合并，就回退到 `estimatePointToPlaneLLSWeightedFullCloudRVV`、public full-cloud overload 或 iterator clone，并在 `boundary` 列记录实际边界。归因边界内需要确认：

- `vlse32.v`：source xyz、target xyz 和 target normal 的 AoS stride load。
- `vle32.v`：连续 `weights_` load。
- `vcpop.m` / `vmerge`：finite mask 和 invalid lane 归零。
- `vfmacc.vv`：每个 chunk 更新 A/B/C/N partial sums。
- `vfredosum.vs`：每个 A/B/C/N group 扫完 block 后，对该组 partial sums 做显式横向规约。

全二进制中的其它 `vfmadd/vfmacc` 或 `vfred*` 可能来自 Eigen、bench harness 或自动向量化，不能归因到当前 helper。当前 production 行为已经包含 normal-equation partial-sum `vfmacc`；fused formula follow-up 评估 `a/b/c/d` 逐点公式树 contraction，和 partial-sum FMA 是两类证据。

fused diagnostic helper 模板符号范围内另行确认逐点公式树的 `vfmsac/vfmacc`。本轮 generic `abc-fused` / `abc-fused-ilp` 六个 helper 实例（Mode0/1 × 三类代表点型）均确认 `vlse32.v`、`vle32.v`、`vfmsac.vv`、partial-sum `vfmacc.vv`、`vcpop/vmerge` 和 `vfredosum.vs`。Mode0 与 Mode1 的 normalized core formula asm sequence 相同，因此 `abc-fused-ilp` 只能作为源码调度诊断，不能作为独立 production 候选。`vfredosum` 仍只归属于每个 A/B/C/N group 扫完 block 后的横向规约。

当前 generic fused-formula 二进制的 asm 归因进一步显示：D/ABCD 公式收缩确实进入机器码，但所有 `*-ilp` 与对应非 ILP mode 的 RVV 指令序列相同。两个 `PointXYZ` 代表组合中，`abcd` 相对 baseline 少 17 条 effective RVV 指令；`PointNormal -> PointNormal` 中 D/ABCD 被编译器切出 out-of-line `group_n`，并出现 `vs1r.v` / `vl1re32.v`，导致 `d-six-term` 实际比 baseline 多 6 条、`d-displacement` 基本持平、`abcd` 只少 5 条。这个结果解释了为什么同一理论公式形态会随 layout 改变收益。

接入前 production-symbol `abcd-ilp` helper 的 asm 归因显示，三类代表点型中 fused helper 相对 block helper 均少 26 条 RVV 指令，`vfadd` 8->0、`vfsub` 18->12、`vfmsac` 0->6、`vfmacc` 27->35，且没有新增 `vs1r.v` / `vl1re32.v` spill/reload 或 `jal` 调用。当前默认 production 的固定目录归因文件按 `boundary` 列记录实际统计边界：`PointNormal -> PointNormal` 落在 `estimate-rvv-helper`，RVV 指令数为 `357`；另外两类代表点型落在 `detail`，RVV 指令数为 `338`。三行都包含 `vfmsac=6`、`vfmacc=35`、`vfadd=0`、`vfsub=12`，且没有 vector spill/reload。不同 `boundary` 的总指令数不能直接横向比较。

当前 `AbcdFused` 与 `AbcdFusedIlp` 的 asm 统计相同。`AbcdFusedIlp` 的采用理由是源码显式保留独立 seed multiply、点差和累加阶段，便于维护者审查数据依赖；这属于 code-shape preference（代码形态偏好），不声明独立机器码收益。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `loadPointToPlaneLLSWeightedFullReductionVectors` | production RVV helper | 当前默认 production 的 `a/b/c/d` 公式和 A/B/C/N partial sums。 | production patch、公式实现和 asm 归属 | `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp` |
| `estimatePointToPlaneLLSWeightedFullCloudRVV` | production dispatch / fallback | 检查 layout、size、VL 和 byte-offset gate；失败时回标量。 | production boundary、fallback coverage | 同上 |
| `WeightedFusedFormulaMode` | candidate formula | `abc`、D 项、`abcd` 及各自源码调度候选。 | RVV-vs-RVV 消融和 code-shape 审计 | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/include/impl/teptplw_reductions.hpp` |
| `FullCloudGeneric*RepresentativePointTypesMatchStd` | production-shaped diagnostic correctness | 对三类代表点型做候选与 std 对拍。 | representative correctness | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/src/test_teptplw_candidates.cpp` |
| `ProductionFullCloud*` | production direct correctness | 真实 public full-cloud overload、layout gate、fallback 和 Scalar gate。 | production direct correctness / fallback coverage | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/src/test_teptplw_production_direct.cpp` |
| `run_case` / `run_case_trace` | bench harness | 分别测完整 estimate 和逐 iteration trace。 | bench input、warm-up 和 checksum | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/include/impl/teptplw_bench_harness.hpp` |
| `bench_teptplw.cpp` | bench thin entry | 解析参数、设置 warm-up 并调用 bench case registry。 | bench entry stability | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/src/bench_teptplw.cpp` |
| `teptplw_bench_cases.hpp` | bench case registry | 维护 case-filter 到具体 bench case 的映射。 | bench boundary / label stability | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/include/impl/teptplw_bench_cases.hpp` |
| `collect_teptplw_board_rvv_ba.py` | analysis script | 采集板卡多轮日志，校验 checksum，生成摘要和 asm 归因。 | repeated board evidence | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/script/` |
| `production_default_fused_abcd_ilp/trace_summary.md` | evidence output summary | 当前默认 RVV trace 的 5-run 统计。 | production default timing stability | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_default_fused_abcd_ilp/` |
| `production_dispatch_fused_abcd_ilp/summary.md` | evidence output summary | 当前默认 production 的 repeated std/RVV speedup。 | production direct performance | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_dispatch_fused_abcd_ilp/` |

## 正确性与高效性证据链

| 维度 | 证据 | 结论 | 边界 |
| --- | --- | --- | --- |
| correctness | 32 tests QEMU；board 32 tests；production direct/fallback；numeric stress；非有限语义。 | `accepted_points`、`ATA/ATb`、matrix 和 fallback 均有覆盖；generic abc 与 D/ABCD fused representative tests 已由 QEMU 和 board 覆盖。 | 不覆盖非法 index、所有 correspondence 分布或 `Scalar=double` RVV。 |
| path/asm | 当前 production-default asm attribution 的 `boundary` 列；接入前 production-symbol fused helper 的差异统计。 | 当前默认 production helper 确实包含 fused-abcd-ilp 公式形态，归因表内没有 vector spill/reload。 | 不把不同 `boundary` 的总指令数直接横向比较，也不把 generic diagnostic helper 的统计外推到未覆盖的 production 入口。 |
| performance | `production_dispatch_fused_abcd_ilp/summary.md` 与 `production_default_fused_abcd_ilp/trace_summary.md`。 | 当前 production direct board repeated std/RVV speedup 正向；默认 RVV trace 的三类点型 checksum 一致。 | 只覆盖三类代表点型和 262144 点；不证明所有 gate-allowed 点型逐类型性能。 |
| boundary | RowSourcePolicy / WeightPolicy 矩阵和 fallback tests。 | source/dual/correspondences、`Scalar=double`、非连续权重保持标量。 | diagnostic 10-case 不能替代 production evidence。 |

## EvidenceDecision 审计

支持当前 production adopted 的条件：

- full-cloud public overload 已有真实 direct tests。
- size、VL、layout、byte-offset、`Scalar` 和 small-input fallback 均有测试或源码审查。
- production-default asm attribution 已记录实际 `boundary`，覆盖默认 RVV path 的归因边界。
- board gtest 和 QEMU gtest 均是 32 tests，包含 generic abc 与 D/ABCD fused representative correctness。
- 当前 production-dispatch repeated board summary 在三类代表点型、262144 点上正向。
- 当前 production-default trace 经过 5 轮，每轮 20 次测量和 5 次 warm-up，三类点型 checksum 序列一致。

证据不支持的扩展：

- source-indexed production：缺 repeated board、production direct/fallback 和符号级生产归属。
- dual-indices production：single-run board diagnostic 负向，缺 production direct/fallback。
- correspondences production：single-run board diagnostic 负向，且成本源包含 index/weight 展开、gather、压缩和 tail；缺消融。
- fused formula：逐点公式树 contraction 已完成 diagnostic/component A/B，QEMU/board correctness 和 asm 归属通过；`abc`、D 项和 `abcd` 已补 generic representative pointtypes。接入前 20-run 中 `PointNormal -> PointNormal` 的 fused-vs-block avg B/A median 为 `0.976x`，低于 `1.0x` 为 `12/20`。当前生产默认的 repeated std/RVV summary 在 5 轮中三类代表点型均正向；默认 trace 的 checksum 序列一致。按用户确定的接入优先级，RVV 相对 std 和静态实现质量是主要依据，运行态波动作为 accepted risk 记录。
- `Scalar=double` 和未逐类型上板的点型性能。

因此当前 EvidenceDecision 为 `production-adopted/full-cloud-f32-aos-layout-gated-weighted-block-dispatch-fused-abcd-ilp-accepted-risk`。

## 遗留风险

- 代表点型只证明三类组合。新增 gate-allowed 点型的性能结论需要对应 board evidence。
- 接入前 `PointNormal -> PointNormal` 的 production-symbol fused-vs-block 20-run 存在 `B/A < 1` 高频风险；当前 production-dispatch 5-run std/RVV summary 均正向，但不证明长期无波动。
- source/dual/correspondences 的负向归因尚未消融，不能写成“唯一主因是 gather”。
- `include/impl/teptplw_candidate_full_cloud.hpp` 中早期 staged-row diagnostic 的 `vcompress + buffer + tail` 仍可作为消融 baseline；production 默认已采用 block-reduction。

## Fused formula follow-up 结果

unweighted TEPTPL 的 fused formula 已经迁移到 production：它保留 A/B/C/N block-reduction，逐点 `a/b/c` 使用 `vfmsac`，`d` 使用 `nx*(dx-sx) + ny*(dy-sy) + nz*(dz-sz)` 后用 `vfmacc` 累加。weighted TEPTPLW 不能直接采用同一结论，因为 weighted 先执行 `normal *= weight`，非有限 weight 不参与 finite mask；因此本 topic 先补 test_support 消融、production-symbol 归因和板卡复测，再把 `abcd-fused-ilp` 接入默认 production。

本轮已经按 diagnostic/component A/B 实现，并曾新增 production-symbol `abcd-ilp` helper 用于接入前验证；正式接入后，production 默认 helper 直接采用 `abcd-fused-ilp` 公式块，重复的 production-symbol fused helper 已删除：

| 候选 | 变化 | 测试预算 | bench 结果 |
| --- | --- | --- | --- |
| `abc-fused` | `a/b/c` 使用 `vfmsac`，`d` 保持当前展开；本轮已迁移为 generic layout-gated test_support candidate。 | weighted same-chain、scale-stress、`ATA/ATb`、matrix；三类代表点型 correctness。 | production-shaped generic `PointXYZ -> PointXYZINormal` 256K B/A 负向，不接 production。 |
| `abc-fused-ilp` | 只重排独立 `a/b/c` seed multiply 的源码顺序，`d` 保持当前展开。 | 同 `abc-fused`。 | 当前二进制与 `abc-fused` RVV 指令序列相同；只保留诊断。 |
| `d-six-term-fma` / `d-six-term-fma-ilp` | 保留 `d` 六项形态，但用 FMA 累加；ILP 版交错 target/source accumulator。 | 非有限 weight、near-cancellation、`accepted_points`、`ATA/ATb`；三类代表点型 correctness。 | `PointNormal` component no-solve 不稳；ILP 版 asm 与非 ILP 等价。 |
| `d-displacement-fused` / `d-displacement-fused-ilp` | 使用 `dx-sx`、`dy-sy`、`dz-sz` 后 `vfmacc`；ILP 版交错独立差值与 abc 项。 | 非有限 weight 传播、near-cancellation、scale-stress、matrix；三类代表点型 correctness。 | 两个 `PointXYZ` 代表组合强，但 `PointNormal` component no-solve 不稳；ILP 版 asm 与非 ILP 等价。 |
| `abcd-fused` / `abcd-fused-ilp` | 组合 `abc-fused` 和 `d-displacement-fused`。 | 上述预算全量覆盖；三类代表点型 correctness、generic warm-up 5-run、production-symbol asm、production-symbol board trace。 | generic test_support 层最稳；真实 production-symbol 层两个 `PointXYZ` 组合正向，`PointNormal -> PointNormal` trace 不稳；按人工接入标准接受风险后，默认 production 采用 `abcd-fused-ilp` code shape。 |

测试已显式检查非有限 weight 语义、near-cancellation、scale-stress、`accepted_points`、`ATA/ATb` 和 matrix。非有限 point/normal 仍由 finite mask 排除；非有限 weight 不能改变 `accepted_points`，但会继续影响 normal-equation，这一点已和标量 reference 对齐。

test support 已在 `include/impl/teptplw_reductions.hpp` 增加 fused formula lane helper，在 `include/impl/teptplw_candidate_full_cloud.hpp` 和 `include/impl/teptplw_candidate_estimates.hpp` 增加 direct diagnostic candidates。bench 已在 `include/impl/teptplw_bench_cases.hpp` 增加 component no-solve 和 full estimate cases：

```text
weighted lls component full-cloud block-fused-abc no-solve pointnormal
weighted lls full-cloud block-fused-abc pointnormal
weighted lls component full-cloud block-fused-d-six-term no-solve pointnormal
weighted lls full-cloud block-fused-d-six-term pointnormal
weighted lls component full-cloud block-fused-d-displacement no-solve pointnormal
weighted lls full-cloud block-fused-d-displacement pointnormal
weighted lls component full-cloud block-fused-abcd no-solve pointnormal
weighted lls full-cloud block-fused-abcd pointnormal
```

asm gate 已在 fused diagnostic helper 符号范围内区分两类 FMA：逐点公式树里的 `vfmsac/vfmacc`，以及已采用的 A/B/C/N partial-sum `vfmacc`。`vfredosum` 仍只归属于每组结束后的横向规约。本轮新增 generic abc helper 符号归属覆盖三类代表点型；QEMU timing 不作为性能结论。

最终 fused 结论是 `production-adopted/fused-abcd-ilp-default-accepted-risk`。当前 production 仍使用 full-cloud A/B/C/N block-reduction 框架、layout gate、fallback 和 finite mask；变化点是逐点 `a/b/c/d` 公式块切为 `abcd-fused-ilp`。接入理由是 RVV 相对 std 仍正向，静态实现质量更高：旧 production-symbol asm 显示 RVV 指令数少 `26`，`vfadd` `8 -> 0`，`vfsub` `18 -> 12`，`vfmsac` `0 -> 6`，`vfmacc` `27 -> 35`，且无新增 vector spill/reload 或 `jal`。旧 20-run 复测中 `PointNormal -> PointNormal` avg B/A median 为 `0.976x`，avg 低于 `1.0x` 为 `12/20`；当前判断将它记录为运行态不稳定风险，并按人工接入标准接受。
