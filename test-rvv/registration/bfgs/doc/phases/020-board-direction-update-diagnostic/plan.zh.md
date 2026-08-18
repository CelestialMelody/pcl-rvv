# Phase 020: board direction-update diagnostic 计划

## 阶段目标

本阶段只回答一个问题：Phase 010 建立的 test-only `direction-update` RVV candidate（候选实现）在真实 RVV board（板卡）上是否有继续追踪价值。阶段仍是 diagnostic（诊断）路径，不修改 `registration/include/pcl/registration/bfgs.h`，不生成 `doc-rvv/registration/bfgs-RVV.zh.md`。

## 输入状态

- Phase 010 已完成 correctness（正确性）对拍和 QEMU smoke（QEMU 烟测）。
- `make -C test-rvv/registration/bfgs evidence_status` 已为 fresh。
- 当前 production caller（生产调用方）只确认到 GICP `OptimizationFunctorWithIndices`，但本阶段不做 caller hotspot（调用方热点）或 production boundary（生产边界）结论。
- 当前 bench checksum 曾包含 `used_rvv` 标志；进入 strict A/B（严格基线/候选对比）前需要把 computation checksum（计算结果校验和）与 dispatch metadata（分发元数据）拆开。

## 工作项

| action | 范围 | 产物 | 说明 |
| --- | --- | --- | --- |
| checksum 口径先修 | `src/bench_bfgs.cpp`、QEMU manifest 文案 | 稳定的 Std/RVV computation checksum | checksum 只覆盖计算结果；是否走 RVV 仍由 build label、candidate side metadata 和 correctness test 说明。 |
| board fragment | `board.mk` | 可部署到 `/root/pcl-test/registration/bfgs` 的 board Makefile | 复用 `test-rvv/mk/rvv-board-run.mk`，远端 bench 为 `bench_bfgs_std` / `bench_bfgs_rvv`。 |
| repeated run target | `Makefile` | `collect_board_bfgs_direction_update_repeated` | 默认 5 次，每次 Std/RVV 都用 `--case-filter direction-update`。 |
| board summary / manifest | `script/generate_bfgs_board_repeated_summary.py` | `summary.md`、`evidence_manifest.json` | 解析每个 `run-*` 的 `run_bench_std.log` / `run_bench_rvv.log`，输出 B/A = Std ms / RVV ms。 |
| Evidence Doctor / registry | `Makefile` | `evidence_doctor.md`、`log/evidence_registry.json` 记录 | manifest role 为 `strict_ab` + `pre_production_diagnostic`；registry 引用 Phase 020 结果文档。 |
| 文档回填 | topic-local docs + second-pass 表 | Phase 020 result | 若 board 不可达，记录 blocker；若可达，记录 bucket 和后续决策。 |

## Board 运行合同

- `BFGS_BOARD_REPEATED_RUNS ?= 5`
- `BFGS_BOARD_BENCH_ITERATIONS ?= 20`
- `BFGS_BOARD_BENCH_WARMUP_ITERATIONS ?= 5`
- `BFGS_BOARD_RUN_LABEL ?= direction_update_repeated`
- case filter：`direction-update`
- case labels：`direction-update-vector6`、`direction-update-vector128`
- 输出目录：`test-rvv/registration/bfgs/log/board/direction_update_repeated/`

QEMU 仍只用于 correctness / log-shape（日志形状）和 smoke，不写性能结论。board timing（板卡计时）才可以进入 performance bucket（性能分桶）。

## Diagnostic-to-production mismatch audit

| question | 本阶段回答 |
| --- | --- |
| A/B boundary | `bench_bfgs` test-support helper，不是 production `BFGS` 模板 public boundary。 |
| caller representativeness | vector6 对 GICP 状态维度有形状意义；vector128 只验证 VL chunk，不代表 GICP workload。 |
| allowed mismatch | reduction / asm boundary / wrapper 是允许的 diagnostic mismatch；production boundary 不允许外推。 |
| checksum policy | `fnv1a_over_candidate_result_only`；`used_rvv` 不混入 checksum。 |
| clean adoption gate | 需要后续 caller hotspot、production public/same-boundary board evidence 和用户明确授权。 |

## 停止 / 继续条件

- `positive` 或 `weak_positive`：允许进入 caller hotspot audit（调用方热点审计）计划，但仍不能直接改 production。
- `neutral`：保留 diagnostic 结果；是否扩展 move-to+slope 需要另起阶段计划。
- `negative` 或 `unstable`：停止 production 推进，记录为 test-only direction-update 不支持继续。
- board 不可达：写 Phase 020 result 为 blocked / infrastructure，不伪造性能证据。

## 验证计划

```bash
make -C test-rvv/registration/bfgs run_test_compare
make -C test-rvv/registration/bfgs run_qemu_smoke_evidence_doctor
make -C test-rvv/registration/bfgs record_qemu_correctness_state
make -C test-rvv/registration/bfgs record_qemu_smoke_evidence_state
make -C test-rvv/registration/bfgs run_board_bench_bfgs_direction_update_repeated
make -C test-rvv/registration/bfgs evidence_status
```

若 board 命令失败，只记录失败点、远端配置和本地已验证命令，不把 QEMU 数字替代 board performance。
