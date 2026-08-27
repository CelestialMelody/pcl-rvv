# Correctness Tests

## `RangeImageBorderExtractorScoreUpdate.RvvMatchesScalarForInteriorBoundaryAndSignGates`

路径：`src/test_range_image_border_extractor.cpp`

该测试构造 17x9 row-major float score image，混入正负分数、低阈值分数和边界像素。断言 `updateScoresRVV` 与 `updateScoresStd` 的输出逐元素近似一致，误差阈值为 `1e-6f`。

它覆盖三类风险：

- 内部像素固定 8 邻域求和或平均值错误。
- 图像边界走标量参考时的邻居数量和索引错误。
- `minimum_border_probability` gate 和 opposite-sign gate（符号相反保留原值）错误。

RED 记录：曾临时让 RVV candidate 输出全零，`make run_test_rvv` 出现 expected mismatch。GREEN 记录：`make run_test_compare` 在 QEMU 上 Std/RVV 两侧均 1/1 passed；`make run_board_test fetch_board_logs` 在板卡上 1/1 passed。

不能证明的范围：完整 `computeFeature()`、四方向 score 图像生成、`RangeImage::get1dPointAverage`、`LocalSurface` 指针数组、shadow / veil 状态机、输出 `BorderDescription` 构造和 production dispatch。

## `RangeImageBorderExtractorScoreGeneration.RangeImageFixtureProducesStableFourDirectionScores`

路径：`src/test_range_image_border_extractor.cpp`

该测试构造 32x24 全有限 `RangeImage` fixture，配置 `RangeImageBorderExtractor` 后读取 left/right/top/bottom 四张 score image。断言至少存在非零 meaningful score（有意义分数），并对四方向 score image 做稳定 checksum。

它覆盖真实 `extractLocalSurfaceStructure()` 与 `extractBorderScoreImages()` 可运行性、fixture 的 determinism（确定性）和 production `range_image_border_extractor.cpp` 链接边界。它不覆盖 score-update RVV 生产接入、inf / max range / unobserved 分支、shadow / veil 状态机或完整 `BorderDescription` 输出分类。
