# registration/transformation_estimation_point_to_plane_lls_weighted 函数级 RVV 评估

## 范围与结论

- 主题：`transformation_estimation_point_to_plane_lls_weighted`
- production 文件：`registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp`
- 专项测试目录：`test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/`
- 公开入口：`TransformationEstimationPointToPlaneLLSWeighted::estimateRigidTransformation`

当前 EvidenceDecision：

```text
production-candidate/full-cloud-f32-aos-layout-gated-weighted-block-dispatch-representative-pointtypes
```

生产候选只覆盖 full-cloud public overload（全云公开入口）、`Scalar=float`、连续 `weights_`、source `RVVXYZAoSFloatLayout`、target `RVVXYZNormalFloatLayout`、size/VLEN/byte-offset gate 均满足的路径。source-indexed（源索引路径）、dual-indices（双索引路径）、correspondences（对应关系路径）、`Scalar=double` 和非连续权重都保持标量。

主文档负责长期算法说明和设计理由：`doc-rvv/registration/transformation_estimation_point_to_plane_lls_weighted-RVV.zh.md`。本评估文档只记录测试矩阵、bench 边界、证据索引和决策审计。

目录怎么跑、点型怎么来的、统计脚本怎么用，先看 [README.zh.md](./README.zh.md)。

## Production patch scope

| 项 | 当前状态 | 证据 |
| --- | --- | --- |
| dispatch | 只在 full-cloud overload 中尝试 RVV；命中后提前返回，否则进入原 iterator 标量 helper。 | production direct tests、源码审查。 |
| generic gate | source 用 `RVVXYZAoSFloatLayout`，target 用 `RVVXYZNormalFloatLayout`，两侧分别 offset/stride。 | `PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal` tests。 |
| weights | 只读成员 `weights_` 连续 vector。 | public overload tests 和 production-dispatch bench 都调用 `setCorrespondenceWeights(weights)`。 |
| reduction | 采用 weighted full-cloud block-reduction；每个 chunk 用 `vfmacc` 更新 A/B/C/N partial sums，每组扫完 block 后显式 `vfredosum`。 | normal-equation tests、asm attribution、board 5-run。 |
| fallback | 非 RVV、非 float、小规模、layout miss、VL miss、byte-offset miss、indexed/correspondences 均标量。 | fallback tests 和源码审查。 |

## 标量流程与 RVV 流程对照

| 阶段 | 标量路径 | 当前 RVV production |
| --- | --- | --- |
| 入口检查 | full-cloud overload 检查 source/target 点数和 `weights_.size()`。 | 保留原检查。 |
| row source | `ConstCloudIterator` 同步读取 `source[k]` 和 `target[k]`。 | 只接 full-cloud，source/target 直接按 AoS stride load。 |
| weight | `weights_it` 顺序读取。 | `vle32.v` 连续加载 `weights_[k]`。 |
| finite mask | 检查 source xyz、target xyz 和 target normal，不检查 weight。 | 同语义，invalid lane merge to zero。 |
| formula | `normal *= weight` 后计算 `a/b/c/d`。 | 同公式，当前不使用 `a/b/c/d` 逐点公式树 fused contraction。 |
| accumulation | 按 row 顺序用 double 累加 `ATA/ATb`。 | 每个 block 用 A/B/C/N vector partial sums；chunk 内用 `vfmacc` 累加，组结束后再 `vfredosum` 到 double normal-equation。 |
| solve / matrix | Eigen 6x6 inverse solve，构造 4x4 matrix。 | 保留标量。 |

## 函数族评估表

| 路径 | 当前决策 | 证据边界 |
| --- | --- | --- |
| full-cloud current diagnostic | 保留为 baseline | `vcompress + fixed buffer + tail` 可对拍，但不是 production 默认。 |
| full-cloud block-reduction | production candidate | 已有 production direct/fallback、numeric stress、asm 和 representative board 5-run。 |
| source-indexed | 仅保留诊断证据 | correctness 已建，board single-run 弱正向，缺 repeated board 和 production direct/fallback。 |
| dual-indices | 仅保留诊断证据 | correctness 已建，board single-run 负向。 |
| correspondences | 仅保留诊断证据 | correctness 已建，board single-run 负向，且混入 index/weight 展开和 gather 多个成本源。 |
| fused formula | attempted / no-production | test-only diagnostic/component candidates 已通过 QEMU/board correctness、数值预算和 diagnostic helper asm attribution；`abc`、D 项和 `abcd` 已泛型化并补 representative warm-up 5-run。真实 production-symbol `abcd-ilp` helper 已实现并补板卡 trace，但 `PointNormal -> PointNormal` avg/median B/A 仍不稳定，因此不接 production。 |
| `Scalar=double` | fallback | 有 fallback smoke；不进入 RVV。 |

## RowSourcePolicy + WeightPolicy 测试矩阵

| 入口形态 | RowSourcePolicy 取点 | WeightPolicy 取权重 | 关键测试 | Bench case | 当前判断 |
| --- | --- | --- | --- | --- | --- |
| full-cloud | `source[k] + target[k]` | `weights_[k]` 连续读取 | `StdDiagnosticMatchesPublicEstimator`、`FullCloudCandidateMatchesStd` | `weighted lls full-cloud pointnormal` | diagnostic baseline。 |
| full-cloud block-reduction | 同 full-cloud | `weights_[k]` 连续读取 | `FullCloudBlockReductionMatchesStdWithinBudget`、production direct/fallback tests | `weighted lls full-cloud block-reduction pointnormal`、production-dispatch representative rows | production candidate。 |
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
| `ProductionFullCloudDoubleNormalTargetFallsBackToScalar` | layout fallback | target normal 不是单个 float 字段时回标量。 |
| `ProductionFullCloudScalarDoubleFallsBackToScalar` | scalar fallback | `Scalar=double` 回标量。 |

完整专项矩阵还保留 diagnostic tests：四条 row source 对拍、小规模 isolated gate、invalid lane mask、非有限 weight 语义和 block-reduction A/B。保留这些测试是为了把 row source、weight source、mask、reduction tree 和 production dispatch 分层审查。

## Fused formula diagnostic 测试

fused formula（融合公式）follow-up 只在 test-rvv diagnostic 层实现，不改变 production selector。已有 `abc-fused`、`d-six-term-fma`、`d-displacement-fused` 和 `abcd-fused` 四类公式树候选；本轮补齐 `abc-fused-ilp`、`d-six-term-fma-ilp`、`d-displacement-fused-ilp` 和 `abcd-fused-ilp`，用于观察源码调度/ILP 是否保留到机器码。它们都保留 full-cloud、连续 `weights_`、finite point/normal mask、非有限 weight 传播和 A/B/C/N block-reduction 组织，只改变或诊断 `a/b/c/d` 逐点公式树。

| 测试 | 层级 | 覆盖内容 |
| --- | --- | --- |
| `FullCloudBlockFusedFormulaCandidatesMatchBlockAndStdWithinBudget` | diagnostic same-chain | 四类 fused candidate 对拍当前 block baseline 和标量 reference，覆盖 `accepted_points`、`ATA/ATb` 和 matrix。 |
| `FullCloudBlockFusedFormulaNearCancellationStressMatchesBlockAndStd` | numerical consistency | 大坐标、小位移的 near-cancellation 样本，保护 `d-six-term-fma` 和 `d-displacement-fused` 的舍入风险。 |
| `FullCloudBlockFusedFormulaScaleStressMatchesBlockAndStd` | numerical consistency | normal、weight 和坐标高动态范围输入，检查 normal-equation 和最终 matrix 预算。 |
| `FullCloudBlockFusedFormulaPreservesNonFiniteWeightSemantics` | numerical consistency | 非有限 point/normal 仍由 finite mask 剔除；非有限 weight 不参与 mask，但继续传播到 normal-equation。 |
| `FullCloudGenericAbcFusedRepresentativePointTypesMatchStd` | generic representative correctness | `abc-fused` 和 `abc-fused-ilp` 覆盖 `PointNormal -> PointNormal`、`PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal`。 |
| `FullCloudGenericDAndAbcdFusedRepresentativePointTypesMatchStd` | generic representative correctness | `d-six-term`、`d-six-term-ilp`、`d-displacement`、`d-displacement-ilp`、`abcd`、`abcd-ilp` 覆盖三类代表点型。 |

## Bench 边界

输入在计时前构造：source 是确定性曲面，target 由 `transformPointCloudWithNormals` 生成，full/source/dual 权重是周期序列，indices 和 correspondences 是确定性有效输入。

测量包含 normal-equation 构造、Eigen solve 和 matrix 构造。full-cloud 不包含权重生成；source-indexed 和 dual-indices 的 `pcl::Indices` 输入在计时前构造，但 candidate 内部 valid-index scan、`uint32_t` staging、byte-offset prepare、index load/gather 和连续 weight load 计入；correspondences case 包含 candidate 内部 index/weight 展开。

| case | 入口 | 证明点 | 不能证明什么 |
| --- | --- | --- | --- |
| `weighted lls full-cloud pointnormal` | 测试专用 `estimate_candidate_full` | full-cloud current baseline 的 stride load、contiguous weight、mask/staging。 | production dispatch、generic 点型、indexed/correspondences。 |
| `weighted lls full-cloud block-reduction pointnormal` | 测试专用 block helper | A/B/C/N partial sums 和显式 `vfredosum`。 | production dispatch；生产性能由 production-dispatch 5-run 证明。 |
| `weighted lls component full-cloud block-fused-* no-solve pointnormal` | 测试专用 fused component helper | 只测 normal-equation 构造，不含 Eigen solve / matrix；用于逐点公式树 A/B 归因。 | production dispatch、端到端收益、代表点型。 |
| `weighted lls full-cloud block-fused-* pointnormal` | 测试专用 fused estimate helper | 包含 normal-equation、solve 和 matrix，用来观察 component 变化进入完整 estimate 后的形状。 | production dispatch、generic 点型、indexed/correspondences。 |
| `weighted lls production-shaped full-cloud block-baseline pointnormal` | 真实 public overload | 当前 production block baseline 的 PointNormal A 侧。 | fused production、generic 代表点型。 |
| `weighted lls production-shaped full-cloud block-fused-* pointnormal` | 测试专用 fused estimate helper | 用 public production A 侧和 fused helper B 侧做 production-shaped B/A。 | B 侧不是真实 production fused path；不覆盖 generic 点型。 |
| `weighted lls component/production-shaped full-cloud block-fused-abc* <representative>` | 测试专用 generic fused helper | `generic-fused-abc` case-filter 覆盖 component no-solve 和 production-shaped full estimate 的三类代表点型。 | B 侧仍不是真实 production fused path；`abc-fused-ilp` 只是源码调度诊断。 |
| `weighted lls component/production-shaped full-cloud block-fused-* <representative>` | 测试专用 generic fused helper | `generic-fused-formula` case-filter 覆盖 `abc`、D 项和 `abcd` 候选的 component no-solve 与 production-shaped full estimate。 | B 侧仍不是真实 production fused path；当前 `*-ilp` 在 asm 中与对应非 ILP mode 等价。 |
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
| direct diagnostic A/B | test-rvv block-reduction helper | test-rvv fused helper | `B/A = A_rvv_ms / B_rvv_ms`，`>1` 表示 B 更快 | 只用于 helper 消融，不是 production evidence。 |
| production-shaped A/B | 真实 full-cloud public overload，经当前 production block dispatch | test-rvv fused helper | `B/A = A_rvv_ms / B_rvv_ms`，`>1` 表示 B 更快 | 只说明 fused 是否值得继续进入 production loop；B 侧不是生产 fused dispatch。 |
| std/RVV speedup | 同一 bench case 的 std 构建 | 同一 bench case 的 RVV 构建 | `std_ms / rvv_ms` | 只说明该 case 自身 RVV 形状，不能替代 fused-vs-block B/A。 |

## 证据索引与结果

| 证据 | 路径 / 命令 | 结果 |
| --- | --- | --- |
| QEMU tests | `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_test_compare` | std/RVV 各 31 tests passed，包含 generic abc 与 D/ABCD fused representative correctness。 |
| board tests | `output/board/run_test.log` | board 31 tests passed，包含 generic abc 与 D/ABCD fused representative correctness。 |
| QEMU bench shape | `make -C ... run_bench_compare` 和 `production-shaped-fused-formula` case-filter | 可解析，checksum 基本对齐；QEMU timing 不作性能结论。 |
| asm | `make -C ... dump_bench_rvv` | production helper 独立符号内确认 stride load、contiguous weight load、A/B/C/N partial sums、`vcpop/vmerge` 和显式 `vfredosum`。 |
| 10-case board diagnostic | 文档 summary；顶层 log 会被 case-filter 覆盖 | single-run diagnostic signal，不是 production performance evidence。 |
| representative board 5-run | `output/board/production_dispatch_weighted_generic_representative_5run_summary.md` | 三类代表点型 64K/256K median 均正向。 |
| fused formula direct PointNormal 5-run | `output/board/weighted_fused_formula_pointnormal_5run_summary.md` | test-only diagnostic/component A/B；文件内区分 std/RVV speedup 和 fused-vs-block B/A。 |
| fused formula production-shaped PointNormal 5-run | `output/board/weighted_fused_formula_production_shaped_pointnormal_5run_summary.md` | A 为真实 public production block baseline，B 为测试专用 fused helper；结论为 `no-production-for-fused/production-shaped-pointnormal-only`。 |
| fused formula generic abc representative 5-run | `output/board/weighted_fused_formula_generic_abc_representative_5run_summary.md` | `abc-fused` / `abc-fused-ilp` 泛型 test_support candidate 的三类代表点型 B/A；结论为 `no-production-for-fused/generic-abc-diagnostic-or-production-shaped-a-b-only`。 |
| fused formula generic formula representative 5-run | `output/board/weighted_fused_formula_generic_formula_representative_5run_summary.md` | `abc`、D 项和 `abcd` 泛型 test_support candidates 的三类代表点型 warm-up RVV-vs-RVV B/A；结论为 `no-production-for-fused/generic-formula-diagnostic`。 |
| fused formula current asm attribution | `output/board/generic_fused_formula_warmup_5run_20260805/asm_current_fused_formula_attribution.md` | 当前二进制确认 D/ABCD 公式收缩进入机器码；`*-ilp` 与对应非 ILP mode 的 RVV 指令序列相同；`PointNormal` D/ABCD 有 out-of-line `group_n` 和 vector save/restore。 |

重复上板采集建议使用 `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/script/collect_teptplw_board_rvv_ba.py`，不要再手工拼 run / copy / analyze 步骤。该脚本会在 `output/board/<case>_<runs>run_<timestamp>/` 下保留 `runNN_rvv.log`、`checksum_validation.md`、`analyze_rvv_ba.md`、`ba_below_1_frequency.txt`、板卡环境日志和 asm 归因；这套目录是后续判断异常频率、是否可剔除异常值、以及 production-symbol 归因是否闭合的标准证据包。

production-dispatch representative 5-run 摘要：

| case | runs | 64K median/min | 256K median/min | 说明 |
| --- | ---: | ---: | ---: | --- |
| `pointnormal` | 5 | `2.69x / 2.66x` | `2.71x / 2.11x` | `PointNormal -> PointNormal` 子集；256K 有一轮低谷但仍正向。 |
| `pointxyz-to-pointnormal` | 5 | `2.81x / 2.79x` | `2.83x / 2.69x` | source generic gate representative。 |
| `pointxyz-to-pointxyzinormal` | 5 | `2.81x / 2.80x` | `2.85x / 2.83x` | source + target generic gate representative。 |

QEMU 10-case diagnostic table 和 board 10-case diagnostic table 只用于日志形状和诊断信号。当前性能结论只使用 representative 5-run board summary。

fused formula PointNormal direct diagnostic 5-run 摘要中的 std/RVV speedup：

| candidate | 64K full median/min | 256K full median/min | 结论 |
| --- | ---: | ---: | --- |
| `abc-fused` | `1.49x / 1.46x` | `1.45x / 1.35x` | median 正向，但只有 diagnostic direct evidence。 |
| `d-six-term-fma` | `1.55x / 1.55x` | `1.47x / 1.01x` | 最强 isolated 信号，256K 仍有低谷。 |
| `d-displacement-fused` | `1.55x / 0.91x` | `1.24x / 0.94x` | 出现负向 run，不进入 production。 |
| `abcd-fused` | `1.34x / 1.16x` | `1.43x / 0.93x` | 组合后不稳定，不进入 production。 |

这些 fused rows 的 std/RVV 双侧调用 test-rvv helpers，不是真实 public production overload；它们不能替代 production-dispatch representative 5-run。

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
| `abcd-fused` | `1.05x / 1.05x` | `1.13x / 1.09x` | PointNormal 正向，但组合收益不可单独归因；缺代表点型和 production-symbol 证据。 |

generic abc representative 5-run B/A 摘要：

| candidate / layer | `pointnormal` 64K / 256K median-min | `pointxyz-to-pointnormal` 64K / 256K median-min | `pointxyz-to-pointxyzinormal` 64K / 256K median-min | 结论 |
| --- | ---: | ---: | ---: | --- |
| `abc-fused` component no-solve | `1.01x / 1.00x`、`0.96x / 0.89x` | `1.02x / 1.02x`、`1.33x / 0.73x` | `1.02x / 1.01x`、`1.02x / 0.40x` | component 层有 256K 退化，不闭合。 |
| `abc-fused` production-shaped full | `1.03x / 1.03x`、`1.05x / 1.05x` | `1.04x / 1.04x`、`1.05x / 1.04x` | `1.04x / 1.03x`、`0.85x / 0.63x` | generic target 256K 负向，不接 production。 |
| `abc-fused-ilp` production-shaped full | `1.03x / 1.03x`、`1.05x / 0.96x` | `1.04x / 1.04x`、`1.04x / 0.76x` | `1.04x / 1.03x`、`1.22x / 1.05x` | 有正向项，但 asm 不是独立公式形状。 |

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

`buildPointToPlaneLLSWeightedFullCloudBlockRVV` 是当前 production helper 的关键符号。该符号范围内确认：

- `vlse32.v`：source xyz、target xyz 和 target normal 的 AoS stride load。
- `vle32.v`：连续 `weights_` load。
- `vcpop.m` / `vmerge`：finite mask 和 invalid lane 归零。
- `vfmacc.vv`：每个 chunk 更新 A/B/C/N partial sums。
- `vfredosum.vs`：每个 A/B/C/N group 扫完 block 后，对该组 partial sums 做显式横向规约。

全二进制中的其它 `vfmadd/vfmacc` 或 `vfred*` 可能来自 Eigen、bench harness 或自动向量化，不能归因到当前 helper。当前 production 行为已经包含 normal-equation partial-sum `vfmacc`；fused formula follow-up 评估的是 `a/b/c/d` 逐点公式树 contraction，不是禁止或缺失所有 FMA 指令。

fused diagnostic helper 模板符号范围内另行确认逐点公式树的 `vfmsac/vfmacc`。本轮 generic `abc-fused` / `abc-fused-ilp` 六个 helper 实例（Mode0/1 × 三类代表点型）均确认 `vlse32.v`、`vle32.v`、`vfmsac.vv`、partial-sum `vfmacc.vv`、`vcpop/vmerge` 和 `vfredosum.vs`。Mode0 与 Mode1 的 normalized core formula asm sequence 相同，因此 `abc-fused-ilp` 只能作为源码调度诊断，不能作为独立 production 候选。`vfredosum` 仍只归属于每个 A/B/C/N group 扫完 block 后的横向规约。

当前 generic fused-formula 二进制的 asm 归因进一步显示：D/ABCD 公式收缩确实进入机器码，但所有 `*-ilp` 与对应非 ILP mode 的 RVV 指令序列相同。两个 `PointXYZ` 代表组合中，`abcd` 相对 baseline 少 17 条 effective RVV 指令；`PointNormal -> PointNormal` 中 D/ABCD 被编译器切出 out-of-line `group_n`，并出现 `vs1r.v` / `vl1re32.v`，导致 `d-six-term` 实际比 baseline 多 6 条、`d-displacement` 基本持平、`abcd` 只少 5 条。这个结果解释了为什么同一理论公式形态会随 layout 改变收益。

production-symbol `abcd-ilp` helper 的当前 asm 归因位于 `output/board/production_symbol_fused_abcd_ilp_trace_warmup_5run_20260806/asm_production_symbol_attribution.md`。该归因显示三类代表点型中 fused helper 相对当前 block helper 均少 26 条 RVV 指令，`vfadd` 8->0、`vfsub` 18->12、`vfmsac` 0->6、`vfmacc` 27->35，且没有新增 `vs1r.v` / `vl1re32.v` spill/reload 或 `jal` 调用。也就是说，生产符号里公式收缩已经发生；性能不闭合不是因为 fused 公式没有进机器码。

当前 `AbcdFused` 与 `AbcdFusedIlp` 的审计口径是：二者都应作为源码候选进入拆分消融，但当前二进制中 asm 等价，不能写成 `Ilp` 有独立机器码收益。若后续生产实现没有负面证据，可以优先采用 `AbcdFusedIlp` 这种源码层面更显式暴露独立 multiply 和依赖链的 code shape（代码形态），但文档必须说明这是 code-shape preference（代码形态偏好），不是已经由当前 asm 证实的 ILP 性能收益；接 production 前仍要补 production-symbol asm attribution 和 warm-up 多轮 RVV-vs-RVV B/A。

## 正确性与高效性证据链

| 维度 | 证据 | 结论 | 边界 |
| --- | --- | --- | --- |
| correctness | 31 tests QEMU；board 31 tests；production direct/fallback；numeric stress；非有限语义。 | `accepted_points`、`ATA/ATb`、matrix 和 fallback 均有覆盖；generic abc 与 D/ABCD fused representative tests 已由 QEMU 和 board 覆盖。 | 不覆盖非法 index、所有 correspondence 分布或 `Scalar=double` RVV。 |
| path/asm | production helper 符号内指令归属；fused diagnostic helper 符号内 formula FMA 归属。 | production 热点来自 weighted block-reduction helper；fused formula 只是 diagnostic candidate。 | 不把全二进制 FMA/reduction 当作当前公式证据，也不把 diagnostic formula FMA 写成 production-symbol evidence。 |
| performance | representative production-dispatch board 5-run。 | 支持 full-cloud f32 AoS layout-gated weighted block dispatch。 | 只覆盖三类代表点型，不证明所有 gate-allowed 点型逐类型性能。 |
| boundary | RowSourcePolicy / WeightPolicy 矩阵和 fallback tests。 | source/dual/correspondences、`Scalar=double`、非连续权重保持标量。 | diagnostic 10-case 不能替代 production evidence。 |

## EvidenceDecision 审计

支持当前 production candidate 的条件：

- full-cloud public overload 已有真实 direct tests。
- size、VL、layout、byte-offset、`Scalar` 和 small-input fallback 均有测试或源码审查。
- production helper 独立符号内完成 asm attribution。
- board gtest 和 QEMU gtest 均是 31 tests，包含 generic abc 与 D/ABCD fused representative correctness。
- representative production-dispatch 5-run board summary 在三类点型、64K/256K 上正向。

证据不支持的扩展：

- source-indexed production：缺 repeated board、production direct/fallback 和符号级生产归属。
- dual-indices production：single-run board diagnostic 负向，缺 production direct/fallback。
- correspondences production：single-run board diagnostic 负向，且成本源包含 index/weight 展开、gather、压缩和 tail；缺消融。
- fused formula：逐点公式树 contraction 已完成 diagnostic/component A/B，QEMU/board correctness 和 diagnostic asm 通过；`abc`、D 项和 `abcd` 已补 generic representative pointtypes。production-symbol `abcd-ilp` helper 已补 asm、10-run avg B/A 和 5-run trace B/A，但 `PointNormal -> PointNormal` avg/median 仍不稳定，因此结论是 `no-production-for-fused/production-symbol-abcd-ilp-trace-not-stable`。
- `Scalar=double` 和未逐类型上板的点型性能。

因此 current EvidenceDecision 保持为 `production-candidate/full-cloud-f32-aos-layout-gated-weighted-block-dispatch-representative-pointtypes`。

## 遗留风险

- 代表点型不是逐类型证明。新增 gate-allowed 点型的性能结论需要对应 board evidence。
- `PointNormal 262144` representative row 有一轮 `2.11x` 低谷，仍为正向，但不应写成零波动稳定性。
- source/dual/correspondences 的负向归因尚未消融，不能写成“唯一主因是 gather”。
- current diagnostic 的 `vcompress + buffer + tail` 仍可作为消融 baseline，但 production 默认已采用 block-reduction。

## Fused formula follow-up 结果

unweighted TEPTPL 的 fused formula 已经迁移到 production：它保留 A/B/C/N block-reduction，逐点 `a/b/c` 使用 `vfmsac`，`d` 使用 `nx*(dx-sx) + ny*(dy-sy) + nz*(dz-sz)` 后用 `vfmacc` 累加。weighted TEPTPLW 不能直接采用同一结论，因为 weighted 先执行 `normal *= weight`，非有限 weight 不参与 finite mask，并且 current production evidence 只批准 block-reduction partial sums。

本轮已经按 diagnostic/component A/B 实现，并新增 production-symbol `abcd-ilp` helper 用于接入前验证；默认 production dispatch 仍不切换：

| 候选 | 变化 | 测试预算 | bench 结果 |
| --- | --- | --- | --- |
| `abc-fused` | `a/b/c` 使用 `vfmsac`，`d` 保持当前展开；本轮已迁移为 generic layout-gated test_support candidate。 | weighted same-chain、scale-stress、`ATA/ATb`、matrix；三类代表点型 correctness。 | production-shaped generic `PointXYZ -> PointXYZINormal` 256K B/A 负向，不接 production。 |
| `abc-fused-ilp` | 只重排独立 `a/b/c` seed multiply 的源码顺序，`d` 保持当前展开。 | 同 `abc-fused`。 | 当前二进制与 `abc-fused` RVV 指令序列相同；只保留诊断。 |
| `d-six-term-fma` / `d-six-term-fma-ilp` | 保留 `d` 六项形态，但用 FMA 累加；ILP 版交错 target/source accumulator。 | 非有限 weight、near-cancellation、`accepted_points`、`ATA/ATb`；三类代表点型 correctness。 | `PointNormal` component no-solve 不稳；ILP 版 asm 与非 ILP 等价。 |
| `d-displacement-fused` / `d-displacement-fused-ilp` | 使用 `dx-sx`、`dy-sy`、`dz-sz` 后 `vfmacc`；ILP 版交错独立差值与 abc 项。 | 非有限 weight 传播、near-cancellation、scale-stress、matrix；三类代表点型 correctness。 | 两个 `PointXYZ` 代表组合强，但 `PointNormal` component no-solve 不稳；ILP 版 asm 与非 ILP 等价。 |
| `abcd-fused` / `abcd-fused-ilp` | 组合 `abc-fused` 和 `d-displacement-fused`。 | 上述预算全量覆盖；三类代表点型 correctness、generic warm-up 5-run、production-symbol asm、production-symbol board trace。 | generic test_support 层最稳；真实 production-symbol 层两个 `PointXYZ` 组合正向，但 `PointNormal -> PointNormal` trace 不稳，不接 production。 |

测试已显式检查非有限 weight 语义、near-cancellation、scale-stress、`accepted_points`、`ATA/ATb` 和 matrix。非有限 point/normal 仍由 finite mask 排除；非有限 weight 不能改变 `accepted_points`，但会继续影响 normal-equation，这一点已和标量 reference 对齐。

test_support 已在 `teptplw_reductions.hpp` 增加 fused formula lane helper，在 `teptplw_candidates.hpp` 增加 direct diagnostic candidates。bench 已增加 component no-solve 和 full estimate cases：

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

asm gate 已在 fused diagnostic helper 符号范围内区分两类 FMA：逐点公式树里的 `vfmsac/vfmacc`，以及当前 already-adopted A/B/C/N partial-sum `vfmacc`。`vfredosum` 仍只归属于每组结束后的横向规约。本轮新增 generic abc helper 符号归属覆盖三类代表点型；QEMU timing 不作为性能结论。

最终 fused 结论是 `no-production-for-fused/production-symbol-abcd-ilp-trace-not-stable`。当前 block-reduction production candidate 的 correctness、fallback、asm 和 representative board 证据仍成立；fused formula 会改变逐点舍入树和非有限 weight 传播。`abcd` / `abcd-ilp` 在 generic warm-up 5-run 中最稳，且 production-symbol helper 已经补齐，但板卡 trace 5-run 显示 `PointNormal -> PointNormal` avg B/A 和 iter-median B/A 都有 3/5 轮低于 `1.0x`。两个 `PointXYZ` 代表组合稳定正向还不足以覆盖 `PointNormal` 负向风险，因此不建议本轮直接接 production。
