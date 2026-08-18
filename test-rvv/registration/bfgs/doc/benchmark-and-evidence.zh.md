# bfgs Benchmark 与证据说明

## 本文职责

本文记录 `bfgs` topic 的 benchmark（性能测试）和 evidence（证据）口径。Phase 010 已经生成 QEMU smoke、filtered asm（过滤反汇编）、manifest（证据清单）和 Evidence Doctor（证据体检）输出；Phase 020 已完成 Milkv-Jupiter 5-run board repeated diagnostic，结果为 negative，不支持 production 接入。

## Bench 输出格式

`src/bench_bfgs.cpp` 输出：

- `Dataset:`。
- `Iterations:` 和 `Warmup Iterations:`。
- 每个 case 的 `ms/iter`。
- `Total Time`。
- checksum（校验和）。

## CLI 参数

| 参数 | 作用 |
| --- | --- |
| `--case-filter` | 选择 `all`、`direction-update`、`move-to-slope` 或 `gicp-shaped-vector6`。 |
| `--iterations` | 控制测量迭代次数。 |
| `--warmup-iterations` | 控制热身次数。 |

## Bench Label / case-filter 字典

| case-filter | 计时边界 | 证明点 | 不能证明 |
| --- | --- | --- | --- |
| `direction-update` | 只测 `minimizeOneStep()` 中 direction update 等价片段 | 局部向量状态更新是否有板卡收益 | GICP end-to-end 加速 |
| `move-to-slope` | `moveTo()` + `slope()` 组合 | Eigen baseline 是否已自动向量化，手写候选是否还有空间 | line search 或 functor 成本 |
| `gicp-shaped-vector6` | 固定 6 维，模拟 GICP BFGS 状态更新 | 真实维度下局部收益是否被小规模稀释 | 完整 GICP workload |

## 推荐 Target

| target | 证据角色 | 当前状态 |
| --- | --- | --- |
| `run_bench_direction_update_smoke` | QEMU log-shape smoke（日志形状小型验证） | done |
| `run_bench_move_to_slope_smoke` | QEMU smoke | available |
| `run_bench_gicp_shaped_vector6_smoke` | caller-shaped smoke（调用方形态小型验证） | available |
| `run_bench_all_smoke` | 多 case QEMU smoke | available |
| `dump_bench_rvv` | asm attribution 输入 | done |
| `generate_qemu_smoke_evidence_manifest` | QEMU manifest | done |
| `run_qemu_smoke_evidence_doctor` | QEMU doctor | done |
| `record_qemu_correctness_state`、`record_qemu_smoke_evidence_state` | evidence registry | done |
| `collect_board_bfgs_direction_update_repeated` | board repeated performance（板卡重复性能） | done / negative，Phase 020 |
| `run_board_bench_bfgs_direction_update_repeated` | collect + summary + doctor + registry | done / negative，Phase 020 |

## 计时边界

当前 bench 只测 test-only diagnostic helper，不包含 GICP nearest-neighbor search（最近邻搜索）、Mahalanobis 成本、`functor.fdf()` 或完整 `align()`。因此即使局部结果正向，也只能证明“BFGS 向量状态更新可能值得继续”，不能证明 production 接入。

## Checksum 来源

checksum 来自最终 `p`、`x_alpha`、`fp0` 和中间标量的 FNV-1a 风格摘要，不依赖 raw pointer（原始指针地址）、运行时间或未初始化填充。Phase 020 起 checksum 不再混入 `used_rvv` 路径标志；是否为 RVV build 由 build label、manifest side metadata 和 correctness tests 表达。

## 当前 QEMU 证据

| 证据 | 路径 | 当前状态 |
| --- | --- | --- |
| QEMU correctness Std/RVV | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | 各 6 tests passed；raw log 默认不提交 |
| QEMU direction-update smoke | `log/qemu/run_bench_direction_update_rvv.log` | 2 case 可解析；QEMU timing 不作性能结论 |
| QEMU manifest | `log/qemu/evidence_manifest.json` | `evidence_role=qemu_smoke_only` |
| Evidence Doctor | `log/qemu/evidence_doctor.md` | Errors=0、Warnings=0、Suggestions=0 |

## 当前 Board 证据

Phase 020 已在 Milkv-Jupiter 上完成 5-run repeated diagnostic：

| case | median B/A | min | max | B/A<1 | bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `direction-update-vector6` | 0.648x | 0.638x | 0.687x | 5/5 | `negative` |
| `direction-update-vector128` | 0.740x | 0.734x | 0.770x | 5/5 | `negative` |

证据路径：

- `test-rvv/registration/bfgs/log/board/direction_update_repeated/summary.md`
- `test-rvv/registration/bfgs/log/board/direction_update_repeated/evidence_manifest.json`
- `test-rvv/registration/bfgs/log/board/direction_update_repeated/evidence_doctor.md`

Evidence Doctor 对两个 case 报告 `ba_degradation_frequency`，Errors=2、Warnings=0、Suggestions=0。该结果支持 `diagnostic_stop_no_production`，不支持 caller hotspot audit 或 production patch。

## Evidence Doctor / Manifest 边界

Phase 010 已用 `script/generate_bfgs_qemu_evidence_manifest.py` 生成 topic-local manifest，再运行 `test-rvv/script/evidence_doctor.py`。manifest 不提供 `ba_values/std_ms/rvv_ms`，目的是让 doctor 明确这不是 Std/RVV performance A/B。

Phase 020 使用 `script/generate_bfgs_board_repeated_summary.py` 生成 board `strict_ab` manifest。该 manifest 仍标注为 pre-production diagnostic（接入生产前诊断），因为 A/B boundary 是 `bench_bfgs` test-support helper，不是 production `BFGS` public boundary。

## ASM Attribution 口径

当前 `dump_bench_rvv` 生成 `build/asm/riscv/bench_bfgs_rvv.asm`。filtered asm 行数为 935，只说明 RVV bench binary 中存在 RVV 指令；由于 helper 可能被内联，仍未区分：

- Eigen baseline 中的自动 RVV 指令。
- test-only candidate helper 中的手写 RVV 指令。
- 编译器自动向量化生成但无法归属到目标 helper 的指令。

## 复现命令

```bash
make -C test-rvv/registration/bfgs run_test_compare
make -C test-rvv/registration/bfgs run_qemu_smoke_evidence_doctor
make -C test-rvv/registration/bfgs record_qemu_smoke_evidence_state
make -C test-rvv/registration/bfgs run_board_bench_bfgs_direction_update_repeated
make -C test-rvv/registration/bfgs evidence_status
```

## 提交边界

raw log 默认不提交。`log/qemu/evidence_doctor.md`、`log/board/direction_update_repeated/summary.md` 和 `log/board/direction_update_repeated/evidence_doctor.md` 可作为 summary-only 证据候选；manifest 和 registry 当前用于本地复核，若要提交需单独检查 allowlist 和脱敏边界。
