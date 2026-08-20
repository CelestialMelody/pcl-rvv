# bilateral_upsampling 正确性测试说明

本文承担 `correctness_tests` role：解释 `src/test_bilateral_upsampling.cpp` 中每个 TEST 的输入、被测路径、断言和证明边界。性能、asm 和 Evidence Doctor 不在本文下结论。

## 测试文件分工

| 文件 / 符号 | 职责 | 边界 |
| --- | --- | --- |
| `src/test_bilateral_upsampling.cpp` | gtest 入口，包含 diagnostic helper 对拍和 production public tests | QEMU / board correctness |
| `include/impl/bilateral_upsampling_core.hpp` | `makeCloud`、`computeTables`、`processScalar`、`processCandidate`、`processDirectDepthCandidate`、`compareClouds`、`checksumCloud` | test support，不是 production API |
| `runProductionPublicEntry<PointInT, PointOutT>` | 构造真实 `pcl::BilateralUpsampling<PointInT, PointOutT>` 并调用 `process` | production direct correctness；覆盖 RGB/RGBA same-type 与交叉输出样本 |

## 共同输入和断言

测试使用合成 RGBD grid、固定投影矩阵和固定 `sigma_depth=0.5f`、`sigma_color=15.0f`。比较项主要是 xyz 的 `max_abs_xyz` 与 `rmse_xyz`，RGB 字段要求保持中心点复制语义。NaN holes 用来验证有限深度跳过和 `norm_sum == 0` 时的 NaN fallback。

## TEST 字典

| TEST | 输入 | 被测路径 | 主要断言 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `BilateralUpsamplingDiagnostic.DepthAndColorTablesMatchFormula` | `window=3`、固定 sigma | `computeTables` / `depthWeight` | 中心权重为 1，远处权重更小，RGB 查表随色差减小 | 查表公式与测试 reference 一致 | production public dispatch |
| `BilateralUpsamplingDiagnostic.ScalarProducesNanWhenWindowHasNoFiniteDepth` | `5x5` 全 NaN depth | `processScalar` | 中心输出 z 为 NaN，RGB 保持输入中心点 | 标量 fallback 的空有效窗口语义 | RVV helper 性能或 public path |
| `BilateralUpsamplingDiagnostic.CandidateMatchesScalarOnDenseCloud` | `32x24` dense、`window=3` | staged-window diagnostic candidate vs scalar | xyz 误差阈值内 | 旧 staged candidate 的 dense 数值等价 | production adoption |
| `BilateralUpsamplingDiagnostic.CandidateMatchesScalarWithNanHoles` | `40x28` holes、`window=4` | staged-window diagnostic candidate vs scalar | xyz 误差阈值内并保留 NaN 统计 | 旧 staged candidate 的 NaN holes 语义 | 板卡收益 |
| `BilateralUpsamplingDiagnostic.DirectDepthCandidateMatchesScalarWithNanHoles` | `40x28` holes、`window=4` | direct-depth diagnostic candidate vs scalar | xyz 误差阈值内 | direct-depth 诊断的有限深度跳过和规约等价 | production public dispatch |
| `BilateralUpsamplingDiagnostic.DirectDepthCandidateSkipsInfiniteDepth` | `40x28` dense 中注入 `+/-infinity` | direct-depth diagnostic candidate vs scalar | xyz 误差阈值内 | RVV mask 与 `std::isfinite` 一样跳过 infinity | 板卡性能 |
| `BilateralUpsamplingDiagnostic.WindowBoundaryKeepsProductionLoopShape` | `8x7` dense、`window=3` | staged-window candidate vs scalar | 四个角点 xyz 接近 | 窗口边界裁剪和 production loop shape | 大规模性能 |
| `BilateralUpsamplingDiagnostic.DirectDepthWindowBoundaryKeepsProductionLoopShape` | `8x7` dense、`window=3` | direct-depth candidate vs scalar | 四个角点 xyz 接近 | direct-depth 边界裁剪语义 | color-gather production path |
| `BilateralUpsamplingProductionPublic.PointXYZRGBMatchesReferenceWithNanHoles` | `PointXYZRGB`、`40x28` holes、`window=4` | 真实 `BilateralUpsampling::process` | public 输出与 test scalar reference 阈值内 | RGB 点型 production public correctness 和 NaN holes | RGBA、大图性能、其它点型 |
| `BilateralUpsamplingProductionPublic.PointXYZRGBAMatchesReferenceDense` | `PointXYZRGBA`、`32x24` dense、`window=3` | 真实 `BilateralUpsampling::process` | public 输出与 test scalar reference 阈值内 | RGBA 点型 production public correctness | holes、大图性能、其它 layout |
| `BilateralUpsamplingProductionPublic.PointXYZRGBSkipsInfiniteDepth` | `PointXYZRGB`、`40x28` dense 中注入 `+/-infinity`、`window=4` | 真实 `BilateralUpsampling::process` | public 输出与 test scalar reference 阈值内 | production color-gather finite mask 等价标量 `std::isfinite` | RGBA infinity、板卡性能、其它点型 |
| `BilateralUpsamplingProductionPublic.PointXYZRGBToPointXYZRGBAMatchesReferenceWithNanHoles` | `PointXYZRGB -> PointXYZRGBA`、`40x28` holes、`window=4` | 真实 `BilateralUpsampling::process` | public 输出 `x/y/z/r/g/b` 与 test scalar reference 阈值内 | 交叉输出 holes correctness | alpha 输出语义、其它点型 |
| `BilateralUpsamplingProductionPublic.PointXYZRGBAToPointXYZRGBMatchesReferenceDense` | `PointXYZRGBA -> PointXYZRGB`、`32x24` dense、`window=3` | 真实 `BilateralUpsampling::process` | public 输出 `x/y/z/r/g/b` 与 test scalar reference 阈值内 | 交叉输出 dense correctness | alpha 输入之外的额外字段语义、其它点型 |

## 验证命令

```bash
make -C test-rvv/surface/bilateral_upsampling run_test_compare
```

当前 `run_test_compare` 中 Std / RVV 均为 13/13 passed。该结果证明当前 adopted production helper 没破坏已覆盖的 RGB/RGBA exact-family public-entry correctness，并补齐了 infinity skip；它不证明板卡性能，也不覆盖 `Scalar=double`、非 RGB/RGBA 点型、其它内存布局或完整 alpha 输出语义。
