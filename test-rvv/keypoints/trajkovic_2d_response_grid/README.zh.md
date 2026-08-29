# Trajkovic 2D Response Grid RVV 主题入口

本目录评估并验证 `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` 中
`TrajkovicKeypoint2D::detectKeypoints()` 的 FOUR_CORNERS / EIGHT_CORNERS response grid（响应图）
生产 RVV 路径。当前阶段只覆盖 organized `PointXYZI -> PointXYZI`、`window_size == 3`、
默认 `IntensityFieldAccessor`（强度字段访问器）和完整输入点云；NMS（非极大值抑制）排序、occupancy map
和输出 `push_back` 保持原标量路径。

## 阅读路径

| 文档 | 作用 |
| --- | --- |
| `doc/trajkovic_2d_response_grid-evaluation.zh.md` | 函数级评估、Traceability Map（可追踪性地图）和生产接入判断 |
| `../../../doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md` | adopted production behavior（已采用生产行为）的长期主题文档 |
| `doc/optimization-roadmap.zh.md` | topic-level optimization roadmap（主题级优化路线图） |
| `doc/phases/README.zh.md` | phase index（阶段索引）和默认恢复入口 |
| `doc/phases/000-current-state-and-response-grid-production-probe/plan.zh.md` | Phase 000 计划、范围、测试和证据合同 |
| `doc/phases/000-current-state-and-response-grid-production-probe/result.zh.md` | Phase 000 结果、板卡证据和停止条件 |
| `doc/phases/optimization-matrix.zh.md` | 跨阶段优化矩阵 |

## 常用命令

```bash
make -C test-rvv/keypoints/trajkovic_2d_response_grid run_test_compare
make -C test-rvv/keypoints/trajkovic_2d_response_grid check_production_rvv_asm
make -C test-rvv/keypoints/trajkovic_2d_response_grid run_board_trajkovic_2d_repeated
make -C test-rvv/keypoints/trajkovic_2d_response_grid run_board_evidence_doctor
make -C test-rvv/keypoints/trajkovic_2d_response_grid evidence_status
```

`run_bench_compare` 默认受 guard（保护门）限制，不用于 QEMU 性能结论；性能结论只使用板卡 repeated summary。

## Doc Suite Role Inventory

| role | status |
| --- | --- |
| topic_navigation | `standalone:README.zh.md` |
| testing_overview | `merged:doc/trajkovic_2d_response_grid-evaluation.zh.md#测试计划和 bench 计划` |
| correctness_tests | `merged:doc/trajkovic_2d_response_grid-evaluation.zh.md#测试计划和 bench 计划` |
| benchmark_and_evidence | `merged:doc/trajkovic_2d_response_grid-evaluation.zh.md#验证结果` |
| optimization_evidence | `merged:doc/optimization-roadmap.zh.md` 和 `doc/phases/optimization-matrix.zh.md` |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` |
| test_support_code_map | `merged:doc/trajkovic_2d_response_grid-evaluation.zh.md#Traceability Map` |
| phase_index / phase_result / optimization_matrix | `standalone:doc/phases/**` |
| evaluation_production | `standalone:doc/trajkovic_2d_response_grid-evaluation.zh.md` |
| production_topic_doc | `standalone:../../../doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md` |

## 当前结果

最终 EvidenceDecision（证据决策）为 `production-adopted-narrow-scope`。EIGHT_CORNERS
`PointXYZI` / 默认 accessor / 3x3 public `compute()` 已接入 production RVV；FOUR_CORNERS、
其它点型、其它 accessor、非 3x3 window 和 NMS 继续走标量 fallback。板卡 performance（性能）
结论只来自 `log/board/repeated-summary.md`，QEMU 只作为 correctness（正确性）和路径证据。
