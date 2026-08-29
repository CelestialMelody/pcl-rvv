# Trajkovic 3D Correctness Tests

默认入口：

```bash
make -C test-rvv/keypoints/trajkovic_3d run_test_compare
```

该命令分别运行 Std/RVV gtest binary。QEMU（仿真器）结果只作为 correctness（正确性）和日志形状证据。

| TEST | 层级 | 输入 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `Trajkovic3DFourCornersResponse.MatchesScalarForInteriorFiniteAndInvalidNormals` | response diagnostic | 37x29 synthetic points/normals，含 NaN/Inf | 每个 response 近似相等 | response helper 公式、finite gate 和普通尾部。 |
| `Trajkovic3DFourCornersResponse.TreatsInvalidNeighborNormalsAsNullNormals` | response diagnostic | 指定中心点上下邻域 normal 非法 | 中心 response 与标量一致且 finite | `getNormalOrNull()` 语义。 |
| `Trajkovic3DFourCornersResponse.LeavesBorderAndRejectedResponsesZero` | response diagnostic | 高 threshold 和边界点 | 边界和 rejected response 为 0 | 清零和 threshold mask。 |
| `Trajkovic3DProductionRVV.FourCornersGateCoversPointXYZAndNormal` | traits gate | `PointXYZ` + `Normal` | production gate 为 true | 当前 adopted 点型命中 RVV。 |
| `Trajkovic3DProductionRVV.PublicComputeFourCornersMatchesResponseReference` | production direct | 41x31 public `compute()` | output intensity 对齐 response reference | 真实入口命中后的输出语义。 |
| `Trajkovic3DProductionRVV.PublicComputeInvalidAndTailMatchesResponseReference` | production direct / fallback | 641x481，含 invalid 数据 | output indices 和 intensity 对齐 reference NMS | non-dense fallback 和尾部 public 行为。 |
| `Trajkovic3DProductionRVV.PublicComputeTinyCloudFallsBackWithoutResponses` | fallback | 2x2 tiny cloud | output 和 indices 为空 | 小输入不误写。 |

当前测试不覆盖 `EIGHT_CORNERS`、normal estimation、其它 window size、其它点型或 `Scalar=double`。这些范围需要新 phase 独立补 test、asm、board 和 Evidence Doctor。
