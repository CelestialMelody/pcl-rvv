# Integral Image Normal RVV 主题入口

本目录评估 `features/include/pcl/features/impl/integral_image_normal.hpp`
中 `IntegralImageNormalEstimation` 的 organized image（有组织图像点云）
主路径。当前已完成 Phase 050 map-prep production probe（生产探针）：生产源码中接入了
`computeFeature()` 的 depth-change map（深度突变图）和 distance-map initialization（距离图初始化）
前缀 RVV 路径，并完成 public `compute()` 入口板卡验证。Phase 020/030/040 评估
`initAverage3DGradientMethod()` 的 diff_x / diff_y buffer、AVERAGE_3D_GRADIENT production-shaped
profile（生产形态剖析）和真实 PCL `IntegralImage2D<float,3>` boundary profile（积分图边界剖析）；
这些证据不支持 diff-buffer 本轮接 production。

## 阅读路径

| 文档 | 作用 |
| --- | --- |
| `doc/integral_image_normal-evaluation.zh.md` | 函数级评估、Traceability Map（可追踪性地图）和生产接入前置条件 |
| `doc/testing-overview.zh.md` | test / bench / board / Evidence Doctor（证据体检）入口总览 |
| `doc/correctness-tests.zh.md` | 每个 correctness test（正确性测试）的输入、断言和边界 |
| `doc/benchmark-and-evidence.zh.md` | bench case、板卡 repeated summary、manifest、doctor 和提交边界 |
| `doc/optimization-evidence.zh.md` | candidate family（候选族）的证据索引和取舍 |
| `doc/test-support-code-map.zh.md` | 测试支撑代码、helper、script 和 output 的定位图 |
| `doc/phases/README.zh.md` | 阶段索引和默认恢复入口 |
| `doc/phases/000-current-state-and-map-prep-diagnostic/plan.zh.md` | 本阶段计划、边界、测试和证据合同 |
| `doc/phases/000-current-state-and-map-prep-diagnostic/result.zh.md` | Phase 000 结果、板卡数值和 EvidenceDecision（证据决策） |
| `doc/phases/010-pi1-production-integration-plan/plan.zh.md` | 生产接入计划；修改 production 前的授权门槛 |
| `doc/phases/020-average-3d-gradient-diff-buffer-diagnostic/plan.zh.md` | diff-buffer 诊断计划 |
| `doc/phases/020-average-3d-gradient-diff-buffer-diagnostic/result.zh.md` | Phase 020 结果、弱收益解释和 no-production-now 决策 |
| `doc/phases/030-average-3d-gradient-production-shaped-profile/plan.zh.md` | AVERAGE_3D_GRADIENT profile 计划 |
| `doc/phases/030-average-3d-gradient-production-shaped-profile/result.zh.md` | Phase 030 profile 结果、Evidence Doctor Error 解释和 no-production-now 决策 |
| `doc/phases/040-pcl-integral-image2d-boundary-profile/plan.zh.md` | exact PCL `IntegralImage2D` boundary profile 计划 |
| `doc/phases/040-pcl-integral-image2d-boundary-profile/result.zh.md` | Phase 040 exact PCL profile 结果、Evidence Doctor Error 解释和 no-production-now 决策 |
| `doc/phases/050-map-prep-production-probe/plan.zh.md` | map-prep 生产探针计划和 PI2-PI5 范围 |
| `doc/phases/050-map-prep-production-probe/result.zh.md` | Phase 050 生产入口板卡数据、Evidence Doctor 解释和 PI5 建议 |
| `doc/phases/optimization-matrix.zh.md` | 跨阶段优化矩阵 |
| `doc/optimization-roadmap.zh.md` | topic-level optimization roadmap（主题级优化路线图） |

## 当前结论

当前结论分两层。Phase 050 的 map-prep production probe 已经接入 production source（生产源码），
并在真实 public `IntegralImageNormalEstimation<PointXYZ, Normal>::compute()` 入口上得到 weak-positive
（弱正向）板卡结果：`prod_compute_avg_depth_320x240` median / mean speedup 都为 1.06x，
`prod_compute_avg_depth_641x481_tail` median / mean speedup 都为 1.06x；checksum 一致。两项各有
1/5 run 低于 1x；用户已确认采纳并保留当前 production patch。

同一批板卡证据中，map-prep diagnostic helper 仍稳定正向：`map_prep_320x240` mean speedup 5.25x，
`map_prep_641x481_tail` mean speedup 5.11x。

Phase 020 的 diff-buffer 候选是 `weak-size-dependent-diagnostic / no-production-now`。当前 Phase 040 复跑后，
`avg3d_diff_641x481_tail` mean speedup 1.35x，但 `avg3d_diff_320x240` mean speedup 1.02x 且 1/5 run 退化。

Phase 030 的 production-shaped profile 进一步把 diff-buffer、积分图构建和 normal query 拆开。完整
`avg3d_profile_320x240` mean speedup 1.09x、1/5 run 退化；`avg3d_profile_641x481_tail`
mean speedup 1.02x、2/5 run 退化。

Phase 040 使用真实 PCL `IntegralImage2D<float,3>` 替换测试专用积分图边界后，仍没有得到 clean positive（干净正向）：
当前 Phase 050 复跑中 `pcl_avg3d_profile_320x240` mean speedup 1.01x，`pcl_avg3d_profile_641x481_tail`
mean speedup 1.03x，且 PCL query / profile component 仍有退化频率 finding。Evidence Doctor（证据体检）
当前为 Errors=3 / Warnings=21 / Suggestions=8；Error 来自非 production-direct 的 profile component，
不是 checksum 错误。

长期 production 文档已创建为 `doc-rvv/features/integral_image_normal-RVV.zh.md`，并使用接入后的板卡数据。
当前默认恢复动作不是继续扩大 patch，而是进入 review / commit 前验证；若未来继续优化，应另开专项 phase。

## 常用命令

```bash
make -C test-rvv/features/integral_image_normal run_test_compare
make -C test-rvv/features/integral_image_normal dump_test_rvv
make -C test-rvv/features/integral_image_normal board_smoke
make -C test-rvv/features/integral_image_normal run_board_integral_image_normal_repeated
make -C test-rvv/features/integral_image_normal run_board_evidence_doctor
make -C test-rvv/features/integral_image_normal record_board_evidence_state
make -C test-rvv/features/integral_image_normal evidence_status
```

`run_bench_compare` 默认受 guard（保护门）限制，不用于 QEMU 性能结论。

## Doc Suite Role Inventory

| role | status |
| --- | --- |
| topic_navigation | `standalone:README.zh.md` |
| testing_overview | `standalone:doc/testing-overview.zh.md` |
| correctness_tests | `standalone:doc/correctness-tests.zh.md` |
| benchmark_and_evidence | `standalone:doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | `standalone:doc/optimization-evidence.zh.md` |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` |
| test_support_code_map | `standalone:doc/test-support-code-map.zh.md` |
| phase_index / phase_result / optimization_matrix | `standalone:doc/phases/**` |
| evaluation_diagnostic | `standalone:doc/integral_image_normal-evaluation.zh.md` |
| production_topic_doc | `standalone:doc-rvv/features/integral_image_normal-RVV.zh.md`：用户已确认采纳当前 map-prep production patch |
