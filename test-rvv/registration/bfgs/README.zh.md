# bfgs RVV 主题入口

## 当前结论

本 topic 已完成 Phase 020 board direction-update diagnostic（板卡方向更新诊断）。当前 EvidenceDecision（证据决策）是 `diagnostic_stop_no_production`：test-only direction update（测试专用方向更新）候选通过 QEMU correctness（QEMU 正确性）和 QEMU smoke（小型日志形状验证），但 Milkv-Jupiter 5-run board repeated 结果为 negative，`direction-update-vector6` median B/A=`0.648x`、`direction-update-vector128` median B/A=`0.740x`，Evidence Doctor（证据体检）报告 2 个 degradation error。`registration/include/pcl/registration/bfgs.h` 未修改，`doc-rvv/registration/bfgs-RVV.zh.md` 仍不适用。

当前只确认 GICP（Generalized Iterative Closest Point，广义迭代最近点）在 `useBFGS()` 路径下使用 `BFGS<OptimizationFunctorWithIndices>`，状态维度是 `Vector6d`。因此本 topic 的诊断只回答“BFGS 优化器局部向量状态更新是否值得继续评估”，不能外推到 NDT 或其它 registration 入口。

## 先读哪份文档

1. `doc/bfgs-evaluation.zh.md`：函数级评估、标量路径、Traceability Map（可追踪性地图）和生产接入判断。
2. `doc/phases/020-board-direction-update-diagnostic/result.zh.md`：board repeated negative 结果、doctor error 和停止判断。
3. `doc/phases/010-diagnostic-scaffold-and-asm-probe/result.zh.md`：correctness、QEMU smoke、asm 和 Evidence Doctor 结果。
4. `doc/optimization-roadmap.zh.md`：候选搜索空间和默认恢复队列。
5. `doc/phases/optimization-matrix.zh.md`：candidate / evidence 状态矩阵。
6. `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`：当前 test / bench / evidence 入口和边界。
7. `doc/test-support-code-map.zh.md`：测试支撑代码结构和调用图。

## 目录分工

| 路径 | 职责 |
| --- | --- |
| `doc/bfgs-evaluation.zh.md` | S2 函数级评估和诊断证据链主归属。 |
| `doc/testing-overview.zh.md` | 当前测试体系和 target 粒度审计。 |
| `doc/correctness-tests.zh.md` | 已实现 correctness（正确性）测试族说明。 |
| `doc/benchmark-and-evidence.zh.md` | QEMU、asm、board 和 Evidence Doctor 证据边界。 |
| `doc/optimization-evidence.zh.md` | 候选实现族和证据状态索引。 |
| `doc/test-support-code-map.zh.md` | `include/`、`include/impl`、`src/` 和 `script/` 布局地图。 |
| `doc/phases/` | 阶段计划、阶段结果、优化矩阵和恢复入口。 |

## 常用命令

```bash
make -C test-rvv/registration/bfgs run_test_compare
make -C test-rvv/registration/bfgs run_test_candidate_direction
make -C test-rvv/registration/bfgs run_test_public_api_smoke
make -C test-rvv/registration/bfgs run_bench_direction_update_smoke
make -C test-rvv/registration/bfgs dump_bench_rvv
make -C test-rvv/registration/bfgs run_qemu_smoke_evidence_doctor
make -C test-rvv/registration/bfgs record_qemu_correctness_state
make -C test-rvv/registration/bfgs record_qemu_smoke_evidence_state
make -C test-rvv/registration/bfgs run_board_bench_bfgs_direction_update_repeated
make -C test-rvv/registration/bfgs evidence_status
```

不应运行 QEMU bench compare（QEMU 性能对比）并把 timing（计时）写成性能结论。QEMU 这里只用于 build、correctness 和 log-shape（日志形状）证据。

## 当前可提交证据

| 证据路径 | 角色 | 提交边界 |
| --- | --- | --- |
| `doc/bfgs-evaluation.zh.md` | 函数级评估和诊断计划 | topic-local evaluation，review 后可提交 |
| `doc/phases/010-diagnostic-scaffold-and-asm-probe/result.zh.md` | Phase 010 当前证据和继续判断 | phase docs，review 后可提交 |
| `doc/phases/020-board-direction-update-diagnostic/result.zh.md` | Phase 020 board negative 和停止判断 | phase docs，review 后可提交 |
| `doc/optimization-roadmap.zh.md` | 候选搜索空间 | phase docs，review 后可提交 |
| `doc/phases/optimization-matrix.zh.md` | 优化矩阵 | phase docs，review 后可提交 |
| `log/qemu/evidence_doctor.md` | QEMU smoke Evidence Doctor 摘要 | summary-only 证据，review 后可提交候选 |
| `log/board/direction_update_repeated/summary.md`、`log/board/direction_update_repeated/evidence_doctor.md` | board repeated negative 摘要和 doctor | summary-only 证据，review 后可提交候选 |

## 默认不提交的生成产物

`build/`、raw `.log`、本机 board run 目录和个人环境配置默认不提交。`log/qemu/evidence_manifest.json`、`log/board/direction_update_repeated/evidence_manifest.json` 和 `log/evidence_registry.json` 当前用于本地证据体检和新鲜度检查；若后续需要提交，需单独审查 allowlist（允许提交清单）和脱敏边界。

## doc-rvv 适用性

`doc-rvv/registration/bfgs-RVV.zh.md` 当前 `not_applicable`。本 topic 没有 adopted production behavior（已采用生产行为）、没有 production patch（生产补丁），也没有 PI5 production evidence（生产证据闭环）和用户确认；诊断结论主归属保留在 topic-local evaluation 和 phase 文档。
