# Phase 000 Plan：current-state-and-gaps

## 目标

建立 `transformation_estimation_dual_quaternion` 的 S2 函数级评估、test-only RVV accumulation candidate、QEMU correctness、QEMU smoke manifest、Evidence Doctor 和 evidence registry。production 源码保持未修改。

## 计划动作

| action | 范围 | 完成判据 | 状态 |
| --- | --- | --- | --- |
| A1 S0 偏好冻结 | 读取 defaults、local override、短 prompt 入口和 worker 门禁 | evaluation / Handoff 写明 preferences_loaded | done |
| A2 源码 shape scan | 读取 production `.h` / `.hpp` 和上游测试 | evaluation 写清公开入口、iterator 和 C1/C2 累加路径 | done |
| A3 test support scaffold | `test-rvv/registration/transformation_estimation_dual_quaternion` | `src/`、`include/`、`include/impl`、Makefile、board.mk 和 manifest wrapper 存在 | done |
| A4 correctness | QEMU Std/RVV gtest | `make run_test_compare` 两个构建通过 | done |
| A5 QEMU smoke / doctor | 窄范围 bench smoke | manifest 和 doctor 生成；不做性能结论 | done |
| A6 phase docs / roadmap / matrix | topic-local doc suite | README、evaluation、phase result、roadmap、matrix 引用证据路径 | done |
| A7 stop decision | 本阶段 closeout | EvidenceDecision 为 diagnostic；next phase 指向 board diagnostic | done |

## 证据计划

QEMU 只用于 correctness、日志形状和路径命中。性能结论需要后续 board repeated benchmark。Evidence Doctor 必须在 EvidenceDecision 前运行或人工记录；本阶段使用 `log/qemu/evidence_manifest.json` 和 `log/qemu/evidence_doctor.md`。

## 停止条件

本阶段可以停止在 diagnostic，因为生产接入需要 board / target hardware 性能证据和 PI1 授权边界。原定下一阶段是 `001-board-diagnostic`；该阶段已完成，新的恢复入口见 `doc/phases/README.zh.md`。
