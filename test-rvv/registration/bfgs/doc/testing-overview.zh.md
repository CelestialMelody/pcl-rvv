# bfgs 测试体系总览

## 本文职责

本文记录 `bfgs` topic 的测试体系和 target 粒度审计。Phase 020 已建立 Makefile、test、bench、QEMU smoke、filtered asm、board repeated 和 Evidence Doctor 入口；当前证据仍是 test-only diagnostic（测试专用诊断），board 结果为 negative，不能作为 production performance（生产性能）正向结论。

## 文档阅读路径

- 函数级判断见 `doc/bfgs-evaluation.zh.md`。
- 当前阶段结果见 `doc/phases/020-board-direction-update-diagnostic/result.zh.md`；QEMU scaffold 结果见 `doc/phases/010-diagnostic-scaffold-and-asm-probe/result.zh.md`。
- 候选搜索空间见 `doc/optimization-roadmap.zh.md`。
- 证据状态见 `doc/phases/optimization-matrix.zh.md`。

## 测试类型定义

| 类型 | 本 topic 中的含义 | 当前状态 |
| --- | --- | --- |
| unit test（单元测试） | 固定 functor 和固定输入下检查 BFGS 状态推进 | done：`BFGSPublicApiSmoke.*` |
| numerical consistency（数值一致性） | 标量 reference 与 test-only RVV candidate 保持同构结果 | done：direction update、move-to+slope、边界样本 |
| caller-shaped smoke（调用方形态小型验证） | 使用 GICP `Vector6d` 状态形状证明 BFGS public API 可运行 | partial：不是完整 GICP caller smoke |
| QEMU correctness（QEMU 正确性） | 只证明构建、路径和测试通过 | done：Std/RVV 各 6 tests passed |
| QEMU bench smoke（QEMU bench 小型验证） | 只证明 bench 可运行、输出可解析 | done：`direction-update` 2 case |
| asm attribution（反汇编归属） | 判断 RVV bench binary 是否出现 RVV 指令 | partial：filtered asm 有指令，hot symbol 归属未闭合 |
| board benchmark（板卡性能测试） | 在目标硬件测 direction-update diagnostic | done / negative：Milkv-Jupiter 5-run repeated |

## 运行入口分类

| target 类别 | target | 作用 | 当前决策 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | Std/RVV 两种构建运行全部 BFGS correctness | adopted |
| correctness aliases | `run_test_candidate_direction`、`run_test_public_api_smoke` | 分别覆盖方向更新和 public API smoke | adopted |
| QEMU smoke aliases | `run_qemu_smoke_evidence_doctor` | 生成小规模 bench 日志形状和 doctor 输入 | adopted |
| bench diagnostic aliases | `run_bench_direction_update_smoke`、`run_bench_move_to_slope_smoke`、`run_bench_gicp_shaped_vector6_smoke`、`run_bench_all_smoke` | 按 case-filter 跑 test-only diagnostic bench | adopted；只作 smoke |
| board repeated aliases | `collect_board_bfgs_direction_update_repeated`、`run_board_bench_bfgs_direction_update_repeated` | 5-run repeated board 证据、summary、doctor、registry | adopted / negative |
| doctor / registry aliases | `generate_qemu_smoke_evidence_manifest`、`run_qemu_smoke_evidence_doctor`、`record_qemu_correctness_state`、`record_qemu_smoke_evidence_state`、`evidence_status` | 检查 manifest 和 registry freshness | adopted |
| historical probe guarded aliases | 不适用 | 当前没有历史 production probe | `not_applicable with evidence` |

## 覆盖矩阵

| 覆盖项 | 当前测试 | 证明范围 | 不能证明 |
| --- | --- | --- | --- |
| BFGS public API | `BFGSPublicApiSmoke.QuadraticFunctorMinimizeOneStepRuns` | `minimizeInit()` / `minimizeOneStep()` 可运行，状态有限 | GICP cost 或 production dispatch |
| Direction update | `BFGSDiagnostic.DirectionUpdate*` | 标量 reference 与 candidate 数值一致 | production 可接入 |
| Move-to + slope | `BFGSDiagnostic.MoveToAndSlopeMatchesScalarReference` | `x_alpha` 和 `gradient.dot(p)` 同构 | 完整 line search |
| 边界输入 | `ZeroDxdg`、`EmptyInput` | 除零边界和空输入 fallback shape | 真实性能 |
| RVV 指令 | `dump_bench_rvv` + filtered asm | 当前 RVV bench binary 是否含 RVV 指令 | 目标硬件加速和热点归属 |

## 当前 Board 结果

| case | median B/A | bucket | doctor |
| --- | ---: | --- | --- |
| `direction-update-vector6` | 0.648x | `negative` | `ba_degradation_frequency` |
| `direction-update-vector128` | 0.740x | `negative` | `ba_degradation_frequency` |

证据路径：`test-rvv/registration/bfgs/log/board/direction_update_repeated/summary.md`、`test-rvv/registration/bfgs/log/board/direction_update_repeated/evidence_doctor.md`。

## 当前可提交证据

可提交候选仍以 topic-local 文档和 summary-only 证据为主。`log/qemu/evidence_doctor.md`、`log/board/direction_update_repeated/summary.md` 和 `log/board/direction_update_repeated/evidence_doctor.md` 可作为摘要证据候选；raw QEMU / board `.log`、`build/` 反汇编和二进制默认不提交。

## 默认不提交的生成产物

`build/`、raw QEMU log、raw board log 和私有 board 路径默认不提交。`log/qemu/evidence_manifest.json`、`log/qemu/evidence_doctor.md` 和 `log/evidence_registry.json` 当前用于本地复核；若后续需要提交，需单独检查 allowlist（允许提交清单）和脱敏边界。

## 当前结论边界

当前合法结论是 `diagnostic_stop_no_production`。Phase 020 证明 test-only direction-update RVV helper 在板卡上为 negative；不能写 `production-ready`，也不能继续默认 caller hotspot / production direct 队列。
