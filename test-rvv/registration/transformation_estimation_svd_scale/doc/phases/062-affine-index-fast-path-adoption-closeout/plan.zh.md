# Phase 062 计划：affine index fast-path adoption closeout

## 阶段意图和边界

本阶段在用户明确确认“当前有收益的实现可以接入”之后，把 Phase 061 的 `affine-index-fast-path-production-probe` 从 PI5 pending 状态收口为 adopted production behavior（已采纳生产行为）。

范围只覆盖 Phase 061 已验证的 production patch：source-indexed、dual-indexed 和 correspondence public overload 在 `__RVV10__`、`Scalar=float`、dense、traits-gated xyz AoS、`nr_points >= 16` 且 step=1 contiguous indices / correspondences 命中时走 contiguous offset RVV accumulation。未命中时保持既有 gather、correspondence sorted-copy 或父类 fallback。

本阶段不新增算法、不扩大到 stride / reverse / shuffle、非法 index / correspondence、`Scalar=double`、全部 custom layout 或全部点型全集。

## 当前证据基线

| evidence | current state |
| --- | --- |
| correctness | Phase 061 已运行 `run_test_compare_recorded`，Std/RVV 各 23 tests passed。 |
| QEMU smoke | `record_qemu_affine_index_fast_path_production_probe_state`：6 comparisons，Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能证据。 |
| board repeated | `run_board_bench_affine_index_fast_path_production_probe_repeated`：6/6 case positive，median B/A `10.115x` 到 `14.021x`，Doctor `0/0/0`。 |
| production patch | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` 已包含 contiguous range 检测与 contiguous offset RVV accumulation。 |
| user confirmation | 用户已确认当前有收益实现可以接入。 |

## 执行动作

| action | output | completion criteria |
| --- | --- | --- |
| A1 Phase 061 result closeout | `061-.../result.zh.md` | pending wording 改为 adopted-by-user，并保留 PI5 曾经的人工确认边界。 |
| A2 topic-local docs refresh | README、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map、evaluation | 当前 aggregate correctness 统一为 23 tests；新增 TEST、target 和证据路径可定位。 |
| A3 matrix / roadmap refresh | `doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` | `affine-index-fast-path-production-probe` 状态改为 adopted-by-user；剩余方向写成需要独立 phase 或用户预算。 |
| A4 production long-term doc refresh | `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md` | 长期文档新增已采纳 contiguous affine fast path，并说明 fallback / evidence / 不覆盖范围。 |
| A5 verification | py_compile、`evidence_status`、`git diff --check`、路径限定 status | 验证命令通过或记录阻塞。 |

## Continue / Stop 条件

本阶段完成后，若 matrix / roadmap 仍只有 `Scalar=double` 数值预算、更广 custom layout / alignment 取样或新的 row-source mitigation family，这些都需要独立 phase 目标、取样空间或数值预算；本阶段不把它们写成已完成，也不在没有新计划和证据预算时强行推进。
