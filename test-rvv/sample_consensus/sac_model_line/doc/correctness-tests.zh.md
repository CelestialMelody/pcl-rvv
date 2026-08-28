# sac_model_line correctness tests

## 测试文件

`src/test_sac_model_line.cpp` 构造围绕同一条直线的 `PointXYZ` 样本，并用乱序 `indices` 调用公开入口与测试专用 candidate。当前覆盖 count、select 和 getDistances：select 测试除了 inlier 顺序，还检查 `error_sqr_dists_`（平方误差缓存）；getDistances 测试检查 dense distance output（连续距离输出）的长度、顺序和数值容忍度。

## 测试证明什么

| 测试 | 输入 | 断言 | 证明范围 |
| --- | --- | --- | --- |
| `CountCandidateMatchesPublicEntryOnShuffledIndices` | 非单位方向系数、阈值附近 offset、乱序 indices。 | public count 等于 candidate count。 | RVV candidate 的 cross3/squaredNorm 公式、严格 `< threshold^2` 判断和 direct indexed gather 与公开入口一致。 |
| `SelectCandidateMatchesPublicEntryOnShuffledIndices` | 非单位方向系数、阈值附近 offset、乱序 indices，调用前预填输出容器。 | public / candidate 的 inlier 顺序和 `error_sqr_dists_` 在 `1e-6` 内一致。 | select candidate 的清空输出、保序写回、平方误差对应关系和阈值判断与公开入口一致。 |
| `SelectCandidateMatchesPublicEntryOnBenchScaleInput` | 65536 点 synthetic line-distance cloud 和 adjacent-pair shuffled indices。 | public / candidate 的 inlier 顺序和 `error_sqr_dists_` 在 `1e-6` 内一致。 | Phase 010 board checksum policy 的数值 gate；证明 bench 规模下的 RVV float 公式仍在允许误差内。 |
| `GetDistancesCandidateMatchesPublicEntryOnShuffledIndices` | 非单位方向系数和乱序 indices。 | public / scalar-sqrt candidate 的距离向量在 `1e-6` 内一致。 | Phase 020 标量 sqrt 形状的基本输出顺序和距离语义。 |
| `GetDistancesCandidateMatchesPublicEntryOnBenchScaleInput` | 65536 点 synthetic line-distance cloud 和 adjacent-pair shuffled indices。 | public / scalar-sqrt candidate 的距离向量在 `2e-6` 内一致。 | 多个 VL chunk 和 tail 下的 dense store 边界。 |
| `GetDistancesCandidatePreservesOutputForInvalidModel` | 方向系数为 0 的 invalid model。 | public / candidate 都保持原 `distances` 内容。 | invalid model fallback（回退路径）副作用边界。 |
| `GetDistancesVFSqrtCandidateMatchesPublicEntryOnBenchScaleInput` | 65536 点 synthetic line-distance cloud 和 adjacent-pair shuffled indices。 | public / vfsqrt candidate 的距离向量在 `2e-6` 内一致。 | Phase 030 `vfsqrt` 候选在 bench 规模下的数值一致性。 |

## 不覆盖什么

本测试在 Phase 040 后覆盖当前 `PointXYZ + direct indexed indices_` 公开入口 production dispatch（生产分流）。它不覆盖空 indices、identity full-cloud、非 `PointXYZ` 点型、`Scalar=double` 或上游 RANSAC 完整流程。

## 命令

```bash
make -C test-rvv/sample_consensus/sac_model_line run_test_compare
```

当前验证结果：Std/RVV 两个 QEMU 构建各 7 个 gtest 通过。QEMU correctness（QEMU 正确性验证）只证明功能和路径可运行，不用于性能结论。
