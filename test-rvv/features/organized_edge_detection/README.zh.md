# Organized Edge Detection RVV 主题入口

本目录评估并验证 `features/include/pcl/features/impl/organized_edge_detection.hpp`
中 `OrganizedEdgeBase::extractEdges()` 的 depth-label path（深度标签路径）。当前 Phase 010 已将
depth label RVV path 接入 production（生产源码）并采纳，Phase 020 又补齐常见点型扩展证据：RVV helper 批量处理 8 邻域全有限的内部像素，
遇到 NaN / Inf 邻域时逐 lane（向量通道）回退到同构标量 helper，以保持 label bits（标签位）和
`assignLabelIndices()` 输出顺序。

## 阅读路径

| 文档 | 作用 |
| --- | --- |
| `doc/organized_edge_detection-evaluation.zh.md` | 函数级评估、Traceability Map（可追踪性地图）和 production 接入判断 |
| `doc/optimization-roadmap.zh.md` | topic-level optimization roadmap（主题级优化路线图） |
| `doc/phases/README.zh.md` | phase index（阶段索引）和默认恢复入口 |
| `doc/phases/000-current-state-and-label-equivalence/plan.zh.md` | Phase 000 计划、范围、测试和证据合同 |
| `doc/phases/000-current-state-and-label-equivalence/result.zh.md` | Phase 000 结果、板卡数值、Evidence Doctor（证据体检）和下一步 |
| `doc/phases/010-production-depth-label-probe/plan.zh.md` | Phase 010 生产接入计划、fallback matrix（回退矩阵）和采纳条件 |
| `doc/phases/010-production-depth-label-probe/result.zh.md` | Phase 010 production direct 结果、接入后板卡数值和 EvidenceDecision |
| `doc/phases/020-point-type-expansion/plan.zh.md` | Phase 020 点型扩展计划、证据边界和停止条件 |
| `doc/phases/020-point-type-expansion/result.zh.md` | Phase 020 `PointXYZI` / `PointXYZRGB` / `PointXYZRGBNormal` production-public 结果 |
| `doc/phases/optimization-matrix.zh.md` | 跨阶段优化矩阵 |
| `../../../doc-rvv/features/organized_edge_detection-RVV.zh.md` | 已采纳 production RVV 行为的长期维护文档 |

## 当前结论

Phase 010 的 production direct evidence（真实生产入口证据）是 strong positive（强正向）：Milkv-Jupiter
板卡 5-run 显示 `prod_depth_finite_320x240` mean `6.194x`、`prod_depth_finite_641x481_tail`
mean `5.521x`、`prod_depth_nan_boundary_320x240` mean `3.016x`，checksum 全部一致。Evidence Doctor
结果为 `0E/0W/3S`，Suggestions 仅提示缺少 taskset / governor / freq / temperature 环境字段。

当前 adopted / covered scope（已采纳 / 已覆盖范围）是真实 `OrganizedEdgeBase<PointT, Label>::compute()`
depth path，已测点型为 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal`。生产 gate 使用
`PointT` 的 AoS 单 float xyz traits，并将 `PointLT` 收窄到 `pcl::Label`。RGB / normal Canny 前处理、
泛型 `PointLT` 和 `assignLabelIndices()` RVV 化仍未覆盖。

Phase 020 point-type board 5-run summary 为：`PointXYZI` finite mean `5.327x`、`PointXYZRGB` finite mean
`5.437x`、`PointXYZRGBNormal` finite mean `4.158x`、`PointXYZRGB` NaN boundary mean `2.822x`，checksum
全部一致。Evidence Doctor 为 `0E/1W/4S`；Warning 是 `PointXYZRGBNormal` 组内收益较低，因此该点型按单独
case 报告，不继承其它点型的更高收益。

## 常用命令

```bash
make -C test-rvv/features/organized_edge_detection run_test_compare
make -C test-rvv/features/organized_edge_detection dump_bench_rvv
make -C test-rvv/features/organized_edge_detection check_production_rvv_asm
make -C test-rvv/features/organized_edge_detection run_board_organized_edge_detection_production_repeated
make -C test-rvv/features/organized_edge_detection run_board_organized_edge_detection_point_type_repeated
make -C test-rvv/features/organized_edge_detection run_board_organized_edge_detection_repeated
make -C test-rvv/features/organized_edge_detection run_board_evidence_doctor
make -C test-rvv/features/organized_edge_detection evidence_status
```

`run_bench_compare` 默认受 guard（保护门）限制，不用于 QEMU 性能结论。

## Doc Suite Role Inventory

| role | status |
| --- | --- |
| topic_navigation | `standalone:README.zh.md` |
| testing_overview | `merged:doc/organized_edge_detection-evaluation.zh.md#测试计划和 bench 计划` |
| correctness_tests | `merged:doc/organized_edge_detection-evaluation.zh.md#测试计划和 bench 计划` |
| benchmark_and_evidence | `merged:doc/organized_edge_detection-evaluation.zh.md#验证结果` |
| optimization_evidence | `merged:doc/optimization-roadmap.zh.md` 和 `doc/phases/optimization-matrix.zh.md` |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` |
| test_support_code_map | `merged:doc/organized_edge_detection-evaluation.zh.md#Traceability Map` |
| phase_index / phase_result / optimization_matrix | `standalone:doc/phases/**` |
| evaluation_diagnostic | `standalone:doc/organized_edge_detection-evaluation.zh.md` |
| production_topic_doc | `standalone:../../../doc-rvv/features/organized_edge_detection-RVV.zh.md` |
