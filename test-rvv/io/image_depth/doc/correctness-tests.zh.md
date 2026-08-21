# image_depth correctness tests

## 本文职责

本文解释 `src/test_image_depth.cpp` 中每个 gtest（Google Test 单元测试）的输入、断言和证明边界。性能、反汇编和 Evidence Doctor 归属到 `benchmark-and-evidence.zh.md`。

## 测试文件分工

| 文件 / 符号 | 作用 |
| --- | --- |
| `src/test_image_depth.cpp` | correctness aggregate，运行 4 个 `ImageDepthDiagnostic` 和 5 个 `ImageDepthProductionPublic` 测试。 |
| `include/image_depth.h` | 测试专用 scalar reference（标量参考链路）和 RVV candidate（候选链路）。 |
| `VectorFrameWrapper` | 测试专用 `FrameWrapper`，让 gtest 调用真实 `DepthImage` public entry。 |
| `makeDepthPixels()` | 构造包含 valid / invalid pixel 的合成输入。 |
| `expectSameFloatImage()` | 对比 NaN 和普通 float 输出；NaN 只检查两侧都是 NaN。 |

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `DepthFloatMatchesScalarWithInvalidPixelsAndPadding` | `19x7 -> 19x7`，padded output row | depth meters scalar vs candidate | 全输出逐元素一致，padding sentinel 保留 | contiguous depth、invalid mask、NaN、padding 语义 | production dispatch、板卡性能 |
| `DisparityMatchesScalarWithDownsampleStride` | `24x12 -> 8x6` | disparity scalar vs candidate | 全输出逐元素一致 | downsample disparity、`constant / pixel` 与 invalid 写 0 | depth downsample path selection、production dispatch |
| `ZeroLineStepUsesTightOutputRows` | `32x5 -> 32x5`，`line_step=0` | depth meters scalar vs candidate | tight row 输出一致 | `line_step == 0` 归一语义 | padded row、downsample |
| `RvvBuildSelectsDownsamplePathForIntegerStride` | selector only，`24x12 -> 8x6` | `selectDepthMetersCandidatePath()` / `selectDisparityCandidatePath()` | RVV build 选择 `DownsampleRvv`；Std build 选择 `Scalar` | path-selection gate（路径选择门禁）区分 Std/RVV | 不证明数值输出或性能 |
| `DepthDownsampleFallsBackAfterProductionEvidenceDidNotSupportRvv` | `24x12 -> 8x6`，padded output row | `DepthImage::fillDepthImage()` public entry | 输出与 scalar reference 一致，padding sentinel 保留；RVV build 命中 depth scalar fallback hook | production-public depth downsample fallback correctness | RVV depth downsample 收益或 asm 归属 |
| `DepthFloatHitsContiguousRvvPathWithZeroLineStep` | `32x5 -> 32x5`，`line_step=0` | `DepthImage::fillDepthImage()` public entry | 输出与 scalar reference 一致；RVV build 命中 contiguous hook | production-public depth contiguous correctness 和 zero line step | padded row |
| `DisparityMatchesScalarAndHitsContiguousRvvPath` | `32x5 -> 32x5` | `DepthImage::fillDisparityImage()` public entry | 输出与 scalar reference 一致；RVV build 命中 contiguous hook | production-public disparity contiguous correctness | downsample |
| `DisparityHitsDownsampleRvvPath` | `24x12 -> 8x6` | `DepthImage::fillDisparityImage()` public entry | 输出与 scalar reference 一致；RVV build 命中 downsample hook | production-public disparity downsample correctness | padded row |
| `KeepsExceptionBehaviorForNonIntegerDownsample` | `23x11 -> 6x5` | `DepthImage::fillDepthImage()` / `fillDisparityImage()` public entry | 两个入口都抛 `pcl::io::IOException` | 非整数 downsample 异常语义保持 | upsample 异常消息文本 |

## 共同输入和断言策略

输入像素包含周期性 valid 值、`0`、`2047` 和 `65535`。depth meters 输出把 invalid 写为 NaN；disparity 输出把 invalid 写为 0。普通 float 使用 `EXPECT_FLOAT_EQ`，避免把性能 checksum 当 correctness oracle（正确性判据）。

## 当前缺口

production-public correctness 已实现并通过。当前未覆盖 raw path、OpenNI legacy、更多尺寸组合和 upsample 异常文本；这些不属于 phase 050 接入范围。
