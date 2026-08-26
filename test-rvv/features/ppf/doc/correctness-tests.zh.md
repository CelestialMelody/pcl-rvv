# PPF Correctness Tests

本文解释 `test-rvv/features/ppf/src/test_ppf.cpp` 中每个 gtest 的输入、被测路径和证明范围。
这些测试是 gate（失败会返回非 0 的验收条件），但不替代板卡性能证据。

| test | 被测路径 | 输入 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `PPFReference.ComputesProductionLikeAllPairsOutput` | test-only scalar reference vs public `PPFEstimation::compute` | 小型 synthetic `PointXYZ + Normal` grid 和显式 indices | 输出大小、dense 标志和 `f1..f4/alpha_m` 对齐 | reference 复刻当前 production 标量语义。 |
| `PPFReference.MarksIdentityPairsAsNaNAndNotDense` | identity pair 分支 | indices 包含自身点对 | 五个字段为 NaN，`output.is_dense=false` | identity pair 语义未被 candidate 改坏。 |
| `PPFCandidate.PairFeatureBatchRVVComputesProductionLikeOutput` | Phase 010 SoA-staged pair-feature RVV candidate | `PointXYZ + Normal` / float / AoS | candidate 输出与 reference 在误差预算内一致 | 只证明 test helper correctness；板卡已证明该 candidate 性能负向。 |
| `PPFAlphaM.ClosedFormMatchesEigenReference` | Phase 020 closed-form `alpha_m` helper | 常规 normal、接近 x 轴 normal 和一般点对 | closed-form 与 Eigen reference 对齐 | 证明可用闭式公式替代 Eigen transform 后段。 |
| `PPFCandidate.AlphaMBatchRVVComputesProductionLikeOutput` | Phase 030 alpha batch RVV candidate | `PointXYZ + Normal` / float / AoS | `alpha_m` RVV batch 输出与 reference 对齐 | 诊断层正向候选；不单独证明 production dispatch。 |
| `PPFProductionDirect.RVVAlphaMPathHitsPublicComputeForExactTypes` | Phase 040 production direct path | exact `PointXYZ + Normal -> PPFSignature` | public `compute()` 输出对齐，trace hit 计数增加 | 证明 exact gate 下真实 public entry 命中 RVV helper。 |
| `PPFProductionDirect.RVVAlphaMPathHitsPublicComputeForPointXYZILikeSource` | Phase 060 source point-type expansion | `PointXYZI + Normal -> PPFSignature` | public `compute()` 输出对齐，trace hit 增加 | 证明 source xyz AoS traits gate 可以命中 RVV helper。 |
| `PPFProductionDirect.RVVAlphaMPathHitsPublicComputeForPointNormalLikeNormals` | Phase 060 normal point-type expansion | `PointXYZ + PointNormal -> PPFSignature` | public `compute()` 输出对齐，trace hit 增加 | 证明 normal AoS gate 可以命中 RVV helper。 |
| `PPFProductionDirect.RVVAlphaMRejectsUnsupportedLayouts` | fallback correctness（回退正确性） | unsupported output / unsupported normal layout | trace hit 不增加，输出仍来自 Std fallback | 证明非覆盖模板实例不会误入 RVV path。 |

## 当前正确性证据

Phase 060 记录的 `make -C test-rvv/features/ppf run_test_compare` 结果是 Std 5/5、RVV 9/9 pass。
板卡 correctness 通过 `run_board_test fetch_board_logs` 验证；Phase 060 result 记录 RVV tests 为
9/9 pass。对应 raw `run_test.log` 默认 local-only（仅本机保留），不作为 topic-only commit 的必须文件。

## 不覆盖范围

这些 tests 不证明 `Scalar=double`、非 `PPFSignature` 输出的 RVV 加速、其它 row source
（行来源）或 PPFRGB / CPPF caller。它们也不证明所有用户自定义 traits-compatible 点型的板卡性能；
Phase 060 只用两个代表性点型证明 gate 语义和 public path 收益。
