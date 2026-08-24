# Integral Image Normal 正确性测试

## 本文职责

本文解释 `src/test_integral_image_normal.cpp` 中每个 gtest（Google Test 单元测试）的输入、被测路径、断言和证据边界。它不承担 bench 数值、board summary 或 production decision（生产决策）。

## 测试文件分工

| 文件 | 职责 |
| --- | --- |
| `src/test_integral_image_normal.cpp` | 构造合成 depth buffer 和 `XYZPadPoint` 点云，生成标量 reference（参考链路），对拍 RVV / Std helper；同时调用真实 public `compute()` 验证 production map-prep 接入后的 distance map。 |
| `include/integral_image_normal.h` | 仅测试使用的聚合入口，暴露 map-prep 和 diff-buffer helper API。 |
| `include/impl/integral_image_normal_map_prep.hpp` | 标量 fallback 和 RVV candidate（候选实现），不被 production 包含。 |

## 共同输入和断言

map-prep 测试使用连续 `float z` buffer 表示 organized image 的 depth（深度）字段。reference path（参考链路）复刻 `computeFeature()` 开头的 depth-change map 和 distance-map initialization 语义：相邻右侧 / 下侧 depth 超阈值或任一侧非有限值时，将当前点和邻点对应 map 写为 0；distance map 将 0 映射为 `0.0f`，其它值映射为 `width + height`。

diff-buffer 测试使用 `XYZPadPoint`，它是测试专用 4-float stride 点型。reference path 复刻 `initAverage3DGradientMethod()` 内圈写入：`diff_x` 来自右邻减左邻，`diff_y` 来自下邻减上邻，每个像素占 4 个 float，第四通道和边界保持 0。

断言使用容器完全相等。这里没有浮点误差预算问题，因为 map-prep 只在 `0.0f` 和同一个 `far_distance` 常量之间选择，diff-buffer 使用相同输入和相同减法顺序。

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明什么 |
| --- | --- | --- | --- | --- | --- |
| `MatchesScalarDepthChangeMapForEdgesAndNonFiniteDepth` | `19x11` 小图，包含深度突变、NaN 和 Inf | `buildDepthChangeMapRVV` | `actual == expected` | depth-change map 的阈值、非有限值和多点 0 写入语义 | distance map、production dispatch、真实 `PointInT` layout |
| `HandlesTailWidthAndInitializesDistanceMap` | `37x9` tail 宽度，含靠近行尾的突变和 NaN | `buildDepthChangeMapRVV`、`initializeDistanceMapRVV` | map 与 distance 都和 reference 完全相等 | VL tail（向量尾段）和 byte-to-float select 语义 | distance transform 两遍传播、normal output |
| `MatchesScalarDiffBuffersForInteriorAndBorders` | `23x13` 测试专用 `XYZPadPoint` organized image | `buildAverage3DGradientDiffBuffersRVV` | diff_x / diff_y 完整 buffer 和 reference 完全相等 | 内圈 xyz 差分、边界零初始化、第四通道零初始化 | `integral_image_DX_ / DY_.setInput()`、生产 `PointInT` layout |
| `LeavesTooSmallImagesZeroInitialized` | `2x7` 无有效内圈输入 | `buildAverage3DGradientDiffBuffersRVV` | diff_x / diff_y 全为 0 | 小尺寸 test helper robustness（稳健性） | production 是否对这类小尺寸调用有相同 guard |
| `PublicComputeDistanceMapMatchesReference` | `19x11` `PointXYZ` organized cloud，含深度突变、NaN 和 Inf | `IntegralImageNormalEstimation<PointXYZ, Normal>::compute()`，normal method 为 `AVERAGE_DEPTH_CHANGE` | `getDistanceMap()` 与独立 scalar reference 的完整 distance transform 结果一致 | production public compute 入口命中 map-prep patch 后仍保持 distance map 语义 | 其它点类型、indices output、其它 normal method 的性能 |

## 验证命令

```bash
make -C test-rvv/features/integral_image_normal run_test_compare
```

该命令会分别构建 `test_integral_image_normal_std` 和 `test_integral_image_normal_rvv`。Std 构建通过 `USE_PCL_RVV10=0` 走标量 fallback；RVV 构建通过 `USE_PCL_RVV10=1` 命中 RVV intrinsic（RVV 内建函数）。当前 Std/RVV 两侧各运行 5 个 gtest。

## 未覆盖边界

当前 correctness 已覆盖 map-prep 的 production direct distance map 语义，但 production 性能证据仍只覆盖
`PointXYZ -> Normal`、organized full image 和 `AVERAGE_DEPTH_CHANGE`。其它点类型、`computeFeaturePart()`、
其它 normal method 和 diff-buffer production 接入仍未覆盖。
