# Trajkovic 3D Optimization Evidence

| candidate family | 状态 | production / test support 代码 | 测试和证据 | 结论边界 |
| --- | --- | --- | --- | --- |
| `four-corners-response-rvv` | adopted | production: `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp`; diagnostic: `include/impl/trajkovic_3d_response.hpp` | gtest 10/10、asm gate、Phase 000 diagnostic board、Phase 020 production board | 只覆盖 `FOUR_CORNERS` / `EIGHT_CORNERS` 3x3 dense public path。 |
| `production-dispatch` | adopted narrow | `detectKeypoints()` 中 `response_done` gate | `public_four_corners_320x240` median 1.790x、`public_eight_corners_320x240` median 1.677x，Evidence Doctor 无 Error/Warning | public API 不变；non-dense 和未覆盖方法 fallback。 |
| `eight-corners-response-rvv` | adopted narrow | production: `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp`; diagnostic: `include/impl/trajkovic_3d_response.hpp` | gtest 10/10、Phase 020 production board、Evidence Doctor 无 Error/Warning | 与 FOUR_CORNERS 一起构成当前 adopted scope。 |
| `point-type-expansion` | deferred | 当前 traits gate 支持一部分 xyz / normal float layout | 当前 production evidence 只覆盖 `PointXYZ + Normal` | 需要代表点型 gtest、public bench、asm 和 Evidence Doctor。 |
| `normal-estimation-rvv` | not_applicable | 不在本文件 | 无 | 若要优化，转入 normal estimation topic。 |
| `nms-rvv` | deferred | 未实现 | public bench 包含 NMS 但没有单独 profile | 需要证明 NMS 成本主导并保护输出顺序。 |

## 当前采用理由

`four-corners-response-rvv` 与 `eight-corners-response-rvv` 的 response-only diagnostic 都有稳定局部收益，production public bench 在包含 NMS 和输出构造后仍保持正向。实现只增加内部 helper 和窄 gate，fallback 明确，维护成本低，因此当前范围采纳。

## 暂缓理由

`point-type-expansion` 仍有潜在价值，但需要新的 correctness、asm、board 和 Evidence Doctor。把它混入当前 patch 会扩大证据边界，并降低 reviewer 对当前 adopted scope 的可审查性。
