# transformation_estimation_point_to_plane_lls Correctness Tests

## 本文职责

本文解释拆分后的 `src/test_teptpl_*.cpp` 中每个 gtest 的输入、被测路径、断言和证据边界。
这些源码共同 include `include/test_teptpl.h`，再通过 `include/impl/teptpl_test_helpers.hpp`
使用 gtest-only fixtures、assertions 和 production helper bridge。本文只说明 correctness
（正确性）和 fallback（回退路径）证据，不写性能结论。

运行入口：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_compare
```

当前 Phase 030 记录的 QEMU correctness logs 为：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls/log/qemu/run_test_std.log
test-rvv/registration/transformation_estimation_point_to_plane_lls/log/qemu/run_test_rvv.log
```

## 源码分工

| 源文件 | 主要分组 | 说明 |
| --- | --- | --- |
| `src/test_teptpl_public_semantics.cpp` | std reference。 | 公开入口与 test-only reference 的语义对拍。 |
| `src/test_teptpl_candidates.cpp` | full-cloud candidate、reduction candidate、fused formula。 | 保护候选公式、规约树、invalid lane、scale stress 和 near-cancellation。 |
| `src/test_teptpl_production_direct.cpp` | production direct、generic source/target、fallback。 | 真实 full-cloud public overload、production RVV helper 中间态和 gate 失败回退。 |
| `src/test_teptpl_row_sources.cpp` | source-indexed、dual-indices、correspondences、isolated helper。 | 历史 row-source diagnostic 和 isolated fallback/mask helper。 |

## 测试分组

| 分组 | TEST 名称 | 输入 / 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| std reference | `StdDiagnosticMatchesPublicEstimator` | full-cloud `PointNormal -> PointNormal`，test-only std reference 对公开 estimator。 | matrix 预算内一致。 | 标量 reference 复刻 full-cloud public overload 有效输入语义。 |
| std reference | `StdSourceIndicesDiagnosticMatchesPublicEstimator` | source-indexed 公开 overload 与 test-only indexed reference。 | matrix 预算内一致。 | source-indexed diagnostic reference 没偏离当前有效输入。 |
| std reference | `StdDualIndicesDiagnosticMatchesPublicEstimator` | dual-indices 公开 overload 与 test-only dual reference。 | matrix 预算内一致。 | dual-indices diagnostic reference 的有效索引语义。 |
| std reference | `StdCorrespondencesDiagnosticMatchesPublicEstimator` | correspondences 公开 overload 与 test-only correspondence reference。 | matrix 预算内一致。 | correspondence query/match 展开在有效输入下与公开入口一致。 |
| full-cloud candidate | `FullCloudCandidateMatchesStd` | early full-cloud staging candidate。 | matrix 和 accepted points 对齐。 | 历史 full-cloud candidate correctness；不是 production dispatch。 |
| reduction candidate | `FullCloudFusedReductionMatchesStdWithinBudget` | fused-reduction candidate。 | normal equation / matrix 预算。 | 证明早期 fused reduction 数值可行；不作为当前 production selector。 |
| reduction candidate | `FullCloudGroupedReductionMatchesStdWithinBudget` | grouped-reduction candidate。 | normal equation / matrix 预算。 | 证明 grouped candidate 数值可行；当前未采用。 |
| reduction candidate | `FullCloudGroupedReductionInvalidLanesWithinBudget` | grouped candidate + invalid lane。 | accepted points 和 matrix 预算。 | grouped finite mask 诊断。 |
| reduction candidate | `FullCloudGroupedReductionScaleStressWithinBudget` | grouped candidate + scale stress。 | matrix 预算。 | grouped reduction 高动态范围诊断。 |
| adopted block family | `FullCloudBlockReductionMatchesStdWithinBudget` | block-reduction candidate。 | normal equation / matrix 预算。 | 当前 production family 的 non-fused block baseline correctness。 |
| adopted block family | `FullCloudBlockReductionInvalidLanesWithinBudget` | block-reduction + invalid lane。 | accepted points、`ATA/ATb`、matrix。 | finite mask 和 vcpop 计数合同。 |
| adopted block family | `FullCloudBlockReductionScaleStressWithinBudget` | block-reduction + scale stress。 | normal equation / matrix 预算。 | block reduction 数值压力合同。 |
| fused formula | `FullCloudBlockFusedFormulaReductionMatchesStdWithinBudget` | block-fused-formula direct helper。 | normal equation / matrix 预算。 | 当前 fused formula hot path 的 direct helper correctness。 |
| fused formula | `FullCloudBlockFusedFormulaNearCancellationMatchesStdWithinBudget` | 大绝对坐标、小位移、invalid lane。 | accepted points、`ATA/ATb`、matrix。 | 约束 `d = n dot (target - source)` 的 near-cancellation 风险。 |
| public-entry-shaped | `FullCloudBlockReductionPublicEntryShapeMatchesPublicWithinBudget` | std public overload vs bench-only block shim。 | matrix 预算。 | 公开入口形态兼容；不等于 production dispatch。 |
| production direct | `ProductionFullCloudPublicOverloadMatrixMatchesStdWithinBudget` | 真实 full-cloud public overload。 | output matrix 对齐 std。 | production dispatch 或 fallback 后的可见输出。 |
| production direct | `ProductionFullCloudNormalEquationMatchesStdWithinBudget` | production RVV normal-equation 与 test-only std reference。 | `accepted_points`、`ATA/ATb`。 | 中间态 correctness，不只看最终 matrix。 |
| production direct | `ProductionFullCloudInvalidLanesMatchStdWithinBudget` | public overload + invalid lanes。 | invalid rows 不计入，matrix 对齐。 | production finite mask 与标量 skip 语义。 |
| production direct | `ProductionFullCloudScaleStressMatchesStdWithinBudget` | public overload + high dynamic range。 | matrix 预算。 | production path 数值压力样本。 |
| generic source | `ProductionFullCloudGenericPointXYZInvalidLanesMatchStdWithinBudget` | `PointXYZ -> PointNormal` public overload + invalid lane。 | matrix / accepted points。 | source 只需 xyz f32 AoS gate。 |
| generic source | `ProductionFullCloudGenericPointXYZScaleStressMatchesStdWithinBudget` | `PointXYZ -> PointNormal` + scale stress。 | matrix 预算。 | generic source 数值压力样本。 |
| generic target | `ProductionFullCloudGenericTargetXYZINormalMatchesStdWithinBudget` | `PointXYZ -> PointXYZINormal` public overload。 | matrix 预算。 | target 只需 xyz+normal f32 AoS gate，额外字段不参与公式。 |
| fused production | `ProductionFusedFullCloudPointNormalNearCancellationMatchesStdWithinBudget` | fused production path + near-cancellation。 | matrix / normal equation budget。 | 当前默认 fused path 的 near-cancellation production-facing 证据。 |
| fused production | `ProductionFusedFullCloudGenericPointXYZInvalidLanesMatchStdWithinBudget` | fused path + `PointXYZ -> PointNormal` invalid lane。 | finite mask / matrix。 | generic source fused production correctness。 |
| fused production | `ProductionFusedFullCloudPointXYZToPointXYZINormalScaleStressMatchesStdWithinBudget` | fused path + generic target scale stress。 | matrix 预算。 | generic target fused production correctness。 |
| fused fallback | `ProductionFusedFullCloudSmallInputFallsBackToScalar` | small input below RVV size gate。 | fallback stats / matrix。 | small input 不误命中 fused RVV。 |
| fused fallback | `ProductionFusedFullCloudLayoutGateFailureFallsBackToScalar` | layout gate miss 点型。 | fallback matrix。 | layout miss 回标量。 |
| fallback | `ProductionFullCloudSmallInputFallsBackToScalar` | production public overload below size gate。 | matrix 与标量一致。 | general small input fallback。 |
| fallback | `ProductionFullCloudScalarDoubleFallbackSmoke` | `Scalar=double` estimator。 | 编译/运行且输出合理。 | `Scalar=double` 保持标量；不证明 double RVV。 |
| rejected candidate | `FullCloudTrustedDenseCandidateMatchesStd` | trusted-dense diagnostic。 | matrix 预算。 | 只证明跳过部分 finite 成本的诊断可对齐；当前因 public semantics 风险 rejected。 |
| source-indexed diagnostic | `SourceIndicesCandidateMatchesStd` | source index stream + compact target。 | matrix / accepted points。 | source-indexed candidate correctness；production 仍标量。 |
| source-indexed diagnostic | `SourceIndicesTrustedDenseCandidateMatchesStd` | source-indexed trusted-dense diagnostic。 | matrix 预算。 | 历史诊断，不接 production。 |
| dual diagnostic | `DualIndicesCandidateMatchesStd` | source/target 两侧 index stream。 | matrix / accepted points。 | dual-indices candidate correctness。 |
| dual diagnostic | `DualIndicesSameStreamCandidateMatchesStd` | source/target 使用同一 index stream。 | matrix 预算。 | same-stream 分布诊断。 |
| correspondence diagnostic | `CorrespondenceCandidateMatchesStd` | same-index correspondences。 | matrix / accepted points。 | correspondence candidate correctness；production 仍标量。 |
| correspondence diagnostic | `CorrespondenceLocalOffsetCandidateMatchesStd` | local-offset correspondences。 | matrix 预算。 | match 偏移但局部性较强的诊断。 |
| correspondence diagnostic | `CorrespondenceIndependentStreamCandidateMatchesStd` | independent-stream correspondences。 | matrix 预算。 | query/match 双流展开诊断。 |
| correspondence diagnostic | `CorrespondenceIndependentStreamTrustedDenseCandidateMatchesStd` | independent-stream + trusted-dense。 | matrix 预算。 | 历史诊断，不接 production。 |
| isolated fallback | `SmallInputFallsBackForIsolatedSizeGate` | test-only isolated size gate。 | stats 和 matrix。 | 规模 gate 单独触发，不混同其它 fallback。 |
| mask helper | `InvalidLaneMaskMatchesStd` | finite mask helper。 | invalid rows 被剔除。 | RVV finite mask 基础合同。 |

## 不应从这些测试外推的结论

- `FullCloud*Candidate*` 和 row-source diagnostic tests 不证明真实 production dispatch。
- `ProductionFullCloud*` 只批准 full-cloud 边界；source-indexed、dual-indices 和 correspondences 仍需独立生产证据。
- `Scalar=double` fallback smoke 只证明回退，不证明 double RVV 可行。
- near-cancellation 和 scale-stress 覆盖当前样本预算，不承诺 bitwise 等价。
- gtest 不提供性能结论；即使 QEMU 跑得更快或更慢，也只作为功能和日志形状信号。
