# Phase 030 Centroid Normal Reduction RVV Diagnostic Plan

## 阶段意图和边界

本阶段继续 VFH（Viewpoint Feature Histogram，视点特征直方图）的 test-only diagnostic
（测试专用诊断）。Phase 020 已证明 centroid-to-point SPFH-like pair math（从质心到点的简化点特征直方图点对数学）
加 viewpoint normal-dot preparation（视点法线点积分箱准备）的 combined helper 在板卡上稳定正向。
Phase 030 只评估 xyz centroid（坐标质心）和 normal centroid（法线质心）两个 O(N) reduction
（规约）是否也值得放入 RVV（RISC-V Vector，可变长度向量）。

不修改 `features/include/pcl/features/impl/vfh.hpp`，不创建 `doc-rvv/features/vfh-RVV.zh.md`。
覆盖范围仍是 dense finite `PointNormal -> VFHSignature308`、`float`、默认 45/128 bin、full-cloud
sequential indices。非 dense normals、给定 centroid / normal、泛型点类型、`Scalar=double` 和 production
dispatch（生产分流）均不在本阶段关闭。

## 当前状态清单

| item | 当前事实 |
| --- | --- |
| Phase 020 | combined candidate 5-run board `2.86x-2.91x`，mean `2.884x`，Doctor `0E/0W/10S`。 |
| public baseline | Phase 020 仍为 `1.00x-1.02x`，mean `1.004x`，只说明当前未接 production 的公开入口近阈值。 |
| PI1 | 生产补丁需要用户明确确认；未确认前只继续 topic-local diagnostic。 |
| 当前缺口 | centroid / normal centroid 仍走标量；需要判断这些规约是否还能降低 combined helper 耗时。 |

## 优化矩阵

本阶段新增 `vfh-centroids-spfh-viewpoint-rvv` 条目。它只能关闭 test-only helper 边界，不能关闭
production dispatch，也不能替代未来 production direct evidence（真实生产路径直连证据）。

## 实现和测试动作

| action | 产物 | 验收 |
| --- | --- | --- |
| RED-1 | `src/test_vfh.cpp` 新增 `CentroidsSPFHAndViewpointRVVMatchesReference`。 | helper 缺失时 `run_test_rvv` 编译失败，证明测试先行。 |
| GREEN-1 | `include/impl/vfh_reference.hpp` 新增 RVV centroid / normal centroid reduction helper 和 combined candidate。 | RVV 构建通过，Std 构建 fallback 返回 false；直方图与 reference 在误差预算内。 |
| BENCH-1 | `src/bench_vfh.cpp` 新增 `candidate_vfh_centroids_spfh_viewpoint_rvv` case。 | board repeated 能同时比较 Phase 020 和 Phase 030 case。 |
| ASM-1 | `dump_bench_rvv`。 | 反汇编中可见 centroid / normal centroid reduction 所需 RVV load / arithmetic / reduction。 |
| BOARD-1 | `board_repeated` + `evidence_doctor_repeated`。 | 5-run repeated 判断 reduction candidate 是否强于 Phase 020 combined candidate。 |

## Evidence Doctor 和决策桶

沿用 5-run、warmup 2、iterations 8。若 Phase 030 candidate 相对 Std 的 speedup 稳定大于 `1.15`，
且相对 Phase 020 RVV 平均耗时缩短超过 `3%`，标为 `positive diagnostic`；若相对 Phase 020 收益小于
`3%` 或波动接近阈值，标为 `attempted / neutral`；若退化，标为 `attempted / rejected for now`。

若 Evidence Doctor 出现 Error，先修复或降级，不关闭阶段。Suggestion 只作为后续 metadata / binary identity
补齐动作，不阻塞当前 diagnostic decision。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic`。 |
| A/B boundary | test helper；Std / RVV build 对同一 bench wrapper 的 case。 |
| 当前决策问题 | `RVV-vs-scalar`，并辅助判断 production probe 是否值得包含 centroid / normal centroid reduction。 |
| diagnostic 是否可外推到 production | 不能直接外推；production 仍需 PI2/PI5 同边界直连证据。 |
| comparison-boundary / baseline mismatch 风险 | 存在；public baseline 没有新 production RVV helper。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 可以，但必须把 reduction 从 production probe 首轮范围中移除或设为独立 A/B。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要，尤其要比较 Phase 020 combined 与 Phase 030 reduction-combined family。 |

## 继续 / 停止条件

如果 Phase 030 positive，更新 PI1 计划，把 centroid / normal centroid reduction 纳入可选 production probe。
如果中性或退化，保留 Phase 020 combined candidate 作为优先 production probe。任何 production patch 仍需用户确认。
