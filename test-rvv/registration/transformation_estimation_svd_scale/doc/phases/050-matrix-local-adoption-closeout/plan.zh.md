# Phase 050 计划：matrix-local adoption closeout

## 阶段意图和边界

本阶段只做 adoption closeout（采纳收尾）：用户已确认当前有收益的实现可以接入，因此把 Phase 042 已落地到 production helper 的 `matrix-local-scale-simplification` 从 `positive_pending_user_judgment / smaller_patch_probe` 收口为 adopted production helper simplification（已采纳生产 helper 简化）。

本阶段不新增 RVV intrinsic（RVV 内建指令）实现，不改变 ordered / row-source / correspondence sorted-copy 的 dispatch gate，也不恢复 Phase 048 staged-selected-cloud 或 Phase 049 dual-indexed target-sorted 的负向路线。

## 当前状态清单

| area | 当前状态 | 路径 |
| --- | --- | --- |
| production helper | `getTransformationFromCorrelation` 已使用 `trace(R * H)` 计算 `sum_tt`，删除旧 `R4 * cloud_src_demean` 临时矩阵。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| Phase 042 evidence | correctness、QEMU smoke、board repeated 和 Evidence Doctor 已完成；board overall `weak_positive`。 | `doc/phases/042-matrix-local-production-probe/result.zh.md` |
| 当前 correctness | Phase 049 后 `run_test_compare_recorded` 已更新为 Std/RVV 各 15 tests passed；这是 Phase 050 启动时的状态。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| Phase 049 | dual-indexed target-sorted 为 negative / mixed，不进入 production。 | `doc/phases/049-dual-indexed-target-sorted-detail-ab/result.zh.md` |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `matrix-local-scale-simplification` | post-demean helper | Eigen demean matrices / `Scalar=float` and scalar fallback shape | `getTransformationFromCorrelation` production helper | `MatrixLocalScaleSimplificationMatchesLegacyPath`；Phase 050 启动时 QEMU Std/RVV 15 tests | `matrix-local-scale` implementation-shape A/B | board median B/A `1.683x` / `1.173x` / `1.173x`，overall `weak_positive` | not_applicable_scalar_formula_simplification | board Doctor `Errors=0`、`Warnings=2` | pending_adoption_closeout | 用户确认后更新为 adopted production helper simplification。 |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 source audit | 核对 production helper 当前 diff。 | 确认 `trace(R * H)` 已在源码中，未扩大 dispatch。 |
| A2 doc sync | 更新 README、evaluation、roadmap、optimization matrix、benchmark/evidence、optimization evidence、code map、correctness docs、phase README 和 `doc-rvv`。 | 不再把 matrix-local 写成待人工判断；同时把 Phase 050 启动时 correctness 统一为 15 tests。 |
| A3 validation | 运行 correctness / evidence status / diff check。 | `run_test_compare` 或等价 recorded target 通过；`evidence_status` fresh；`git diff --check` clean。 |

## 继续 / 停止条件

完成 adoption closeout 后，`matrix-local-scale-simplification` 不再是未阻塞候选。后续若继续优化，只能从新的 candidate family、更多泛型点型、`Scalar=double` 或更窄 row-source / locality 诊断另开 phase。
