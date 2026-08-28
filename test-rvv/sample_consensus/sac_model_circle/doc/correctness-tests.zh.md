# sac_model_circle correctness tests

## 本文职责

本文解释 `src/test_sac_model_circle.cpp` 中 gtest 的输入、断言和证明范围。它不承担性能结论，也不把 helper-level pass（辅助函数层通过）写成 production adoption（生产采纳）。

## 测试文件分工

| 对象 | 位置 | 作用 |
| --- | --- | --- |
| `SampleConsensusModelCircle2DAccess` | `src/test_sac_model_circle.cpp` | 测试派生类，暴露 Standard / RVV helper，并提供 test-only `getDistancesToModelCandidate`、`getDistancesToModelFullRVVCandidate` 和 `selectWithinDistanceFullRVVErrorTailCandidate`。 |
| `makeCircleDispatchCloud` | `src/test_sac_model_circle.cpp` | 构造固定 10 点圆 shell 输入，可生成 `PointXYZ` 或 `PointXYZI`。 |
| `expectSameCircleOutputs` | `src/test_sac_model_circle.cpp` | 对比 public select/count 与 Standard helper 的 inlier、count 和 `error_sqr_dists_`。 |

## 共同输入和断言

圆模型系数为 `(a=0.25, b=-0.20, r=1.00)`，主要 threshold 为 `0.08`。测试关心的是 `indices_` 顺序、shell 边界、public dispatch（公开分流）、fallback（回退路径）和 test-only candidate 数值一致性。误差对拍使用 `EXPECT_NEAR(..., 1e-6)`。

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不证明什么 |
| --- | --- | --- | --- | --- | --- |
| `PublicSelectWithinDistanceMatchesDirectRVVForSupportedLayout` | `PointXYZ`，乱序 indices | public select/count、Standard helper、RVV helper | inliers 顺序、count、误差数组一致；RVV helper count 与 select 命中数量一致。 | `PointXYZ` x/y float layout 下 public dispatch 和直接 RVV helper 对齐。 | 不证明其它点型 board 性能，不证明 `getDistancesToModel` production RVV。 |
| `SelectFullRVVErrorTailCandidateMatchesDirectRVV` | `PointXYZ`，乱序 indices | public/direct `selectWithinDistanceRVV` vs Phase 080 test-only full-RVV error-tail candidate | inliers 和 `error_sqr_dists_` 在 `1e-6` 内一致。 | 证明压缩后继续执行 `vfsqrt + vfwcvt + vse64` 的候选不会改变 select 输出语义；Phase 090 接入后同一 public/direct RVV helper 已覆盖当前 production 实现。 | 不单独证明性能收益；性能由 Phase 080 RVV-vs-RVV 和 Phase 090 production board 证据证明。 |
| `PointXYZILayoutMatchesStandardPath` | `PointXYZI`，乱序 indices | public select/count vs Standard | inliers、count、误差一致。 | 证明 traits gate（点类型字段准入）不限于 exact `PointXYZ` correctness。 | 不证明 `PointXYZI` dedicated board 性能。 |
| `CloudOnlyIdentityIndicesMatchStandardPath` | `PointXYZ`，默认 cloud-only identity indices | public select/count vs Standard | 输出一致。 | 证明没有显式 indices 时仍保持标量语义。 | 不证明 identity-index 专项 stride-load 性能。 |
| `ExplicitEmptyIndicesMatchStandardPath` | 显式空 indices | public select/count vs Standard | inliers 和误差清空，count 为 0。 | 证明空输入不会残留旧输出状态。 | 不覆盖超大 cloud offset fallback。 |
| `GetDistancesCandidateMatchesPublicPath` | `PointXYZ`，乱序 indices | public `getDistancesToModel` vs test-only candidate | `distances` 长度和值一致。 | 证明 Phase 020 candidate 数值上可和 public row 对拍。 | 不证明 candidate 有性能收益，也不证明 production dispatch。 |
| `GetDistancesFullRVVCandidateMatchesPublicPath` | `PointXYZ`，乱序 indices | public `getDistancesToModel` companion vs test-only full-RVV candidate | `distances` 长度和值在 `1e-6` 内一致。 | 证明 Phase 050 `vfsqrt + vfwcvt + vse64` 候选数值可接受。 | 仍不是 production direct，因为 helper 只在测试派生类中。 |
| `PublicGetDistancesMatchesStandardAndDirectRVV` | `PointXYZ`，乱序 indices | public `getDistancesToModel`、`getDistancesToModelStandard`、direct `getDistancesToModelRVV` | 三者输出长度一致，距离值在 `1e-6` 内一致。 | 证明 Phase 060 接入后公开入口命中 RVV 时仍与标量 helper 对齐。 | 不要求 bitwise checksum 一致，不证明其它点型或 `Scalar=double`。 |

## 验证命令

```bash
make -C test-rvv/sample_consensus/sac_model_circle run_test_compare
make -C test-rvv/sample_consensus/sac_model_circle run_circle_public_tests
make -C test-rvv/sample_consensus/sac_model_circle run_circle_select_error_tail_candidate_test
make -C test-rvv/sample_consensus/sac_model_circle run_circle_getdistances_candidate_test
make -C test-rvv/sample_consensus/sac_model_circle run_circle_getdistances_full_rvv_candidate_test
make -C test-rvv/sample_consensus/sac_model_circle run_circle_getdistances_production_test
```

板卡 correctness smoke 使用：

```bash
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle run_board_circle_public_tests
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle run_board_circle_select_error_tail_candidate_test
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle run_board_circle_getdistances_candidate_test
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle run_board_circle_getdistances_full_rvv_candidate_test
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle run_board_circle_getdistances_production_test
```
