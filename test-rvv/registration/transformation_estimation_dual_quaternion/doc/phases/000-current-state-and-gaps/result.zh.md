# Phase 000 Result：current-state-and-gaps

## 当前状态

本阶段建立了 topic-local RVV diagnostic scaffold。production 文件 `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` 未修改；`doc-rvv` 长期主题文档不适用。

## 完成矩阵

| action | 状态 | 证据 | 缺口 |
| --- | --- | --- | --- |
| A1 S0 偏好冻结 | done | `doc/transformation_estimation_dual_quaternion-evaluation.zh.md` 的 S0 表 | none |
| A2 源码 shape scan | done | evaluation 的“标量路径重建”和 Traceability Map | none |
| A3 test support scaffold | done | `include/tedq.h`、`include/impl/tedq_candidates.hpp`、`src/test_tedq.cpp`、`src/bench_tedq.cpp` | 后续 row-source 扩展前再拆 adapters |
| A4 correctness | done | `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/run_test_std.log`、`test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/run_test_rvv.log` | board correctness not_run |
| A5 QEMU smoke / doctor | done | `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/run_bench_std.log`、`test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/run_bench_rvv.log`、`test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/evidence_manifest.json`、`test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/evidence_doctor.md` | QEMU timing not performance |
| A6 roadmap / matrix | done | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | board phase completed in `001-board-diagnostic` |
| A7 stop decision | done | 本 result 的 EvidenceDecision | production direct not_started |

## 验证摘要

执行过的命令：

```bash
make run_test_compare
make dump_bench_rvv
make run_qemu_smoke_evidence_doctor BENCH_ARGS="--iterations 2 --warmup-iterations 1 --case-filter ordered-cloud-pair"
make record_qemu_correctness_state
make record_qemu_smoke_evidence_state BENCH_ARGS="--iterations 2 --warmup-iterations 1 --case-filter ordered-cloud-pair"
make evidence_status
```

`make evidence_status` 在文档写入前报告 `doc_ref_missing`；当前文档已补齐这些 doc refs，应在 closeout 前复查。

## Evidence Doctor

`test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/evidence_doctor.md` 报告：Errors=0，Warnings=0，Suggestions=0。输入是 `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/evidence_manifest.json`。该结果只覆盖 QEMU smoke diagnostic，不证明 target hardware performance（目标硬件性能）。

## 诊断证据链

Correctness：Std / RVV QEMU gtest 各 5 tests passed；candidate 与 scalar reference 的最大误差落在测试预算内；small input fallback 和 `PointXYZI` layout 已覆盖。

QEMU path / log shape：Std / RVV bench smoke 生成相同 checksum，case-filter 为 `ordered-cloud-pair`，iterations=2，warmup=1。

Disassembly：`make dump_bench_rvv` 生成 RVV bench asm dump，当前只作为 test support binary 的指令存在证据。

Board performance：本 phase 结束时未运行；后续 `001-board-diagnostic` 已补齐 board repeated diagnostic，见 `doc/phases/001-board-diagnostic/result.zh.md`。

Production decision：不修改 production；EvidenceDecision 为 `diagnostic`。

## Doc-suite 审计

| area | current shape scan | quality bar / optional calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README | present | 入口导航、命令、证据白名单、doc-rvv 适用性 | `adopted` | `README.zh.md` | keep |
| testing-overview | present | 测试类型、target、矩阵 | `adopted` | `doc/testing-overview.zh.md` | keep |
| correctness-tests | present | gtest 语义和失败含义 | `adopted` | `doc/correctness-tests.zh.md` | keep |
| benchmark-and-evidence | present | bench label、doctor、registry、QEMU/board 边界 | `adopted` | `doc/benchmark-and-evidence.zh.md` | refreshed after board |
| optimization-evidence | present | candidate 到 target / evidence 映射 | `adopted` | `doc/optimization-evidence.zh.md` | keep |
| test-support-code-map | present | 聚合入口、internal helper、拆分审计 | `adopted` | `doc/test-support-code-map.zh.md` | split if row-source adapters grow |
| evaluation | present | 函数级评估和诊断证据链 | `adopted` | `doc/transformation_estimation_dual_quaternion-evaluation.zh.md` | keep |
| doc-rvv | not_present | 只在 adopted production behavior 后适用 | `not_applicable with evidence` | production 未修改 | none |
| phase index / matrix | present | phase 恢复和 matrix | `adopted` | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` | board phase added |

## Artifact Tracking

| artifact | git visibility | boundary |
| --- | --- | --- |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/evidence_registry.json` | to-be-staged | summary evidence |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/evidence_manifest.json` | to-be-staged | summary evidence |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/evidence_doctor.md` | to-be-staged | summary evidence |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/*.log` | ignored-local | raw QEMU run logs，不默认提交 |
| `test-rvv/registration/transformation_estimation_dual_quaternion/build/**` | ignored-local | build / asm local evidence，不默认提交 |

## Continue / Stop Decision

`continue_stop_decision`：本阶段合法停止在 diagnostic，因为下一步需要 board / target hardware repeated evidence；继续生产会扩大到 production integration loop，需要先由 PI1 冻结 scope。

`unblocked_next_actions`：`001-board-diagnostic` 已完成；后续若继续，进入 `002-production-integration-plan`。

`next_phase_default`：PI1 production integration plan，先冻结 ordered-cloud-pair `float` scope、fallback matrix 和 production direct gate。
