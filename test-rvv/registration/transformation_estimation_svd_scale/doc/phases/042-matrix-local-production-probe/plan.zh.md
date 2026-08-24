# Phase 042 计划：matrix-local production probe

## 阶段意图和边界

本阶段把 Phase 020 的 `matrix-local-scale-simplification` 从实现形态诊断推进到 production probe（生产探针）：production helper 直接改用 `trace(R * H)` 简化公式，去掉 `R4 * cloud_src_demean` 临时矩阵和逐列点积。目标是确认这个更小 patch 在当前 topic 的 fallback helper 上数值等价、QEMU smoke 正常、board repeated 仍保持正向。

本阶段不改变 ordered public overload 的 RVV fused 主线，不扩大 row-source，不改变 generic public 结论，只验证 post-demean helper 的较小生产补丁是否足够稳、足够清楚。

## S0 偏好冻结

| 字段 | 冻结值 |
| --- | --- |
| `preferences_loaded` | defaults loaded；local override absent；prompt override 延续当前 topic。 |
| `work_preferences` | test-rvv / diagnostic / prototype 详细中文注释；production 注释克制；文档 current-state-first；evidence policy summary-only。 |
| `commit_preferences` | 不自动 commit；若后续提交，topic、evidence logs 和 agent assets 分开。 |
| `artifact_publication_decision` | production probe 的 board / smoke summary 只有在被文档引用后才进入提交候选；历史 diagnostic summary 保留。 |
| `dirty_isolation` | 仅处理 `test-rvv/registration/transformation_estimation_svd_scale/**` 与必要的 topic-local phase 文档；不碰其它 topic 的 dirty work。 |

## 当前状态清单

| 对象 | 当前状态 | 证据 |
| --- | --- | --- |
| matrix-local helper | 已改为 `trace(R * H)` 简化。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| QEMU correctness | `run_test_compare` 当前 Std/RVV 8 tests passed。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU smoke | `matrix-local-scale` smoke 重新跑过，`Errors=0`、`Warnings=0`。 | `log/qemu/evidence_doctor.md` / `record_qemu_matrix_local_scale_smoke_state` |
| board repeated | 5-run matrix-local repeated 已重新跑过，仍为 weak-positive。 | `log/board/matrix_local_scale_repeated/summary.md`、`evidence_doctor.md` |
| registry | fresh。 | `log/evidence_registry.json` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `matrix-local-production-probe` | 旧后段公式改写为 `trace(R * H)` 后，production helper 数值等价、维护边界更清楚。 | 这不是 RVV intrinsic 主线；board 仍是 weak-positive，是否单独收口需要用户判断。 |

## 继续动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 验证 production helper | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare` | Std/RVV 8 tests passed。 |
| 验证 QEMU smoke | `ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter matrix-local-scale --iterations 3 --warmup-iterations 1"` | 日志形状、checksum、max_reference_error 可解析。 |
| 验证 board repeated | `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_matrix_local_scale_repeated` | 5 runs、summary、manifest、doctor 和 registry 刷新。 |

## 结束条件

若 QEMU correctness、QEMU smoke 和 board repeated 都保持正向，则可以把本阶段写成 production probe positive，但是否把它作为单独的提交点或和当前主线一起收口，需要你判断。
