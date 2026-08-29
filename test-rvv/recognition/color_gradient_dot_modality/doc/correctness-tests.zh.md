# color_gradient_dot_modality 正确性测试

本文件做什么：
这里逐个解释 gtest（Google Test 单元测试）和 production direct 对拍入口，回答每个测试证明什么、
不能证明什么。bench 统计和证据登记放到 `benchmark-and-evidence.zh.md`。

## 测试文件分工

| 文件 | 作用 | 证据角色 |
| --- | --- | --- |
| `src/test_cgdm.cpp` | correctness 主入口，既测 scalar reference，也测 RVV build 的 candidate path。 | correctness gate |
| `include/impl/cgdm_color_gradient.hpp` | scalar reference 与 candidate helper。 | test-only reference / production-shaped diagnostic |
| `include/cgdm.h` | 测试支撑聚合入口。 | navigation helper |

## 测试字典

| TEST | 输入 | 被测路径 | 证明范围 | 不能证明什么 |
| --- | --- | --- | --- | --- |
| `ScalarReferenceProducesDominantBits` | 19x13 synthetic `PointXYZRGB` cloud，`bin_size=4`，threshold=20.0f | `computeDominantMapScalar()` | scalar reference 能生成非空 dominant map，并保持尺寸关系。 | 不能证明 RVV 路径或 production dispatch。 |
| `RvvBuildHitsCandidatePathAndMatchesScalarReference` | 37x25 synthetic `PointXYZRGB` cloud，`bin_size=4`，threshold=20.0f | `computeDominantMapCandidate()` | RVV build 下能命中 candidate path，且 dominant map 与 scalar reference 一致。 | 不能证明板卡性能，也不能证明 production patch 已接入。 |
| `ProcessInputDataMatchesScalarReferenceMap` | 37x25 synthetic `PointXYZRGB` cloud，`bin_size=4`，threshold=20.0f | `ColorGradientDOTModality<PointXYZRGB>::processInputData()` | 公开入口输出与 scalar reference map 一致。 | 不能单独证明板卡收益或其它点型泛化。 |

## 边界

- 当前 correctness 只覆盖 organized `PointXYZRGB`。
- `computeInvariantQuantizedMap()` 不在这组测试里。
- 其它点型、其它 row source、`Scalar=double` 和非标准布局没有独立 correctness 矩阵。

## 验证命令

```bash
make -C test-rvv/recognition/color_gradient_dot_modality run_test_compare
```

QEMU 只用于 correctness 和日志形状，不用于性能结论。
