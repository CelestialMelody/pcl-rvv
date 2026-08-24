# Phase 066: Scalar Double Adoption Closeout Plan

## 阶段意图和边界

本阶段响应用户对 Phase 065 positive production probe（正向生产探针）的采纳确认：把 `scalar-double-production-probe` 从 `pending_user_confirmation_adopt_production` 收口为 `adopted-by-user`，并同步 topic-local 文档、长期 `doc-rvv`、optimization matrix（优化矩阵）和 optimization roadmap（优化路线图）。

本阶段不扩大 production 源码范围。已采纳范围仍只覆盖 ordered-cloud-pair（顺序点云对）公开入口、`PointXYZ -> PointXYZ`、`Scalar=double`、dense 输入和 `nr_points >= 16`。row-source double、泛型 xyz AoS double、自定义 layout double、非 dense、小规模和非法输入继续保持 fallback 或未覆盖。

## 当前状态清单

| area | current state | evidence / path |
| --- | --- | --- |
| production probe | Phase 065 已接入真实 public ordered overload 的 bounded double RVV probe。 | `doc/phases/065-scalar-double-production-probe/result.zh.md` |
| correctness | Std/RVV 各 27/27 passed；新增 `ScalarDoubleProductionProbeMatchesReference` 和 `ScalarDoubleProductionProbeFallbackBoundaries`。 | `src/test_tesvd_scale.cpp`、`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU smoke / ASM | `scalar-double-production-probe` QEMU smoke Doctor `0/0/0`；asm 输入显示 f64 widened 指令特征。 | `log/qemu/scalar_double_production_probe/evidence_doctor.md` |
| board repeated | 5-run B/A `33.955, 33.664, 33.781, 34.077, 33.792`，median `33.792x`，Doctor `0/0/0`。 | `log/board/scalar_double_production_probe_repeated/summary.md`、`evidence_doctor.md` |
| user decision | 用户确认“当前有收益的实现可以接入”，允许继续推进。 | 本阶段 plan/result 记录为 PI5 采纳确认。 |

## Adoption Closeout 动作

| id | action | done criteria |
| --- | --- | --- |
| AC1 | 把 matrix 中 `scalar-double-production-probe` decision 更新为 `adopted-by-user`。 | matrix 行包含 correctness、QEMU、board、ASM、Doctor 和边界说明。 |
| AC2 | 刷新 roadmap 默认恢复队列。 | Phase 065/066 状态为 adopted / closeout complete；下一未阻塞方向不再是 double production probe，而是独立的 row-source double、generic double 或更广 custom layout 取样。 |
| AC3 | 刷新 topic-local README、phase index、evaluation、benchmark/evidence、optimization evidence。 | 文档不再把 ordered `Scalar=double` 写成只到 Phase 064 diagnostic；同时保留 row-source / generic double 未覆盖。 |
| AC4 | 刷新长期 `doc-rvv`。 | `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md` 记录 adopted double ordered branch、fallback matrix 和正确性与高效性证据链。 |
| AC5 | 重新运行 registry / Evidence Doctor 状态。 | QEMU / board scalar-double production probe 证据 fresh；`evidence_status` 通过。 |

## Continue / Stop Conditions

完成 AC1-AC5 后，本阶段可收口。若 registry stale、Evidence Doctor 出 Error、或文档与 production diff 冲突，则暂停并修复；不能把 adopted 状态写入长期文档后留下证据登记缺口。

下一阶段默认不自动扩大 production。若继续优化，优先从 roadmap 中选择独立边界，例如 row-source `Scalar=double` diagnostic / production probe、generic xyz AoS double traits gate，或用户定义的更广 custom layout / alignment 取样。
