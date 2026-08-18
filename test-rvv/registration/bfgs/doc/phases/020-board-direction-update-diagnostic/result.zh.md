# Phase 020: board direction-update diagnostic 结果

## 执行范围

本阶段仍只修改 `test-rvv/registration/bfgs/` 下的 diagnostic 资产和 topic-local 文档。`registration/include/pcl/registration/bfgs.h` 未修改，`doc-rvv/registration/bfgs-RVV.zh.md` 仍为 `not_applicable`。

## 计划动作回填

| action | 状态 | 证据路径 / 命令 | 结论 |
| --- | --- | --- | --- |
| checksum 口径先修 | done | `src/bench_bfgs.cpp`、`script/generate_bfgs_qemu_evidence_manifest.py` | bench checksum 只覆盖计算结果，不再混入 `used_rvv` 路径标志。 |
| board fragment | done | `board.mk` | 可部署 `bench_bfgs_std`、`bench_bfgs_rvv`、`test_bfgs_rvv` 到 `/root/pcl-test/registration/bfgs`。 |
| repeated run target | done | `collect_board_bfgs_direction_update_repeated` | Milkv-Jupiter 5-run direction-update board diagnostic 已完成。 |
| board summary / manifest | done | `script/generate_bfgs_board_repeated_summary.py`、`log/board/direction_update_repeated/summary.md`、`log/board/direction_update_repeated/evidence_manifest.json` | 生成 B/A summary 和 strict A/B manifest。 |
| Evidence Doctor / registry | done | `log/board/direction_update_repeated/evidence_doctor.md`、`log/evidence_registry.json` | doctor 明确报 2 个 degradation error；registry 已登记 board summary / manifest / doctor。 |

## Correctness 和 QEMU 回归

命令：

```bash
make -C test-rvv/registration/bfgs run_test_compare
make -C test-rvv/registration/bfgs run_qemu_smoke_evidence_doctor
make -C test-rvv/registration/bfgs record_qemu_correctness_state
make -C test-rvv/registration/bfgs record_qemu_smoke_evidence_state
```

结果：

| 证据 | 结果 |
| --- | --- |
| QEMU Std/RVV correctness | 各 6 tests passed。 |
| QEMU direction-update smoke | `direction-update-vector6` 和 `direction-update-vector128` 均可运行；checksum 新口径分别为 `1817980496570205313`、`12473622438914907152`。 |
| filtered asm | `test-rvv/registration/bfgs/build/asm/riscv/bench_bfgs_rvv.asm` 当前 935 行；仍只是 bench binary 级别，不是 production hot-symbol attribution。 |
| QEMU Evidence Doctor | `test-rvv/registration/bfgs/log/qemu/evidence_doctor.md`：Errors=0、Warnings=0、Suggestions=0。 |

QEMU timing 仍只作 log-shape（日志形状）参考，不进入性能结论。

## Board repeated 结果

命令：

```bash
make -C test-rvv/registration/bfgs run_board_bench_bfgs_direction_update_repeated
```

运行合同：

| 字段 | 值 |
| --- | --- |
| device | Milkv-Jupiter |
| runs | 5 |
| iterations | 20 |
| warmup_iterations | 5 |
| case_filter | `direction-update` |
| evidence_role | `pre_production_diagnostic` |

Board summary：

| case | runs | median B/A | min | max | B/A<1 | bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `direction-update-vector6` | 5 | 0.648x | 0.638x | 0.687x | 5 | `negative` |
| `direction-update-vector128` | 5 | 0.740x | 0.734x | 0.770x | 5 | `negative` |

证据路径：

- `test-rvv/registration/bfgs/log/board/direction_update_repeated/summary.md`
- `test-rvv/registration/bfgs/log/board/direction_update_repeated/evidence_manifest.json`
- `test-rvv/registration/bfgs/log/board/direction_update_repeated/evidence_doctor.md`
- `test-rvv/registration/bfgs/log/board/direction_update_repeated/run-01/run_bench_std.log`
- `test-rvv/registration/bfgs/log/board/direction_update_repeated/run-01/run_bench_rvv.log`
- `test-rvv/registration/bfgs/log/board/direction_update_repeated/run-05/run_bench_std.log`
- `test-rvv/registration/bfgs/log/board/direction_update_repeated/run-05/run_bench_rvv.log`

raw `run-*` log 只列代表路径，默认不作为提交候选。

## Evidence Doctor

`test-rvv/registration/bfgs/log/board/direction_update_repeated/evidence_doctor.md`：

| Errors | Warnings | Suggestions | 解释 |
| ---: | ---: | ---: | --- |
| 2 | 0 | 0 | 两个 case 都是 5/5 B/A 低于 1，doctor 把它们标为 `ba_degradation_frequency`。 |

这不是脚本失败，而是负向性能证据：当前 test-only RVV direction-update helper 在板卡上慢于 Std build，不能作为 production 接入依据。

## Checksum 边界

bench checksum 已从 `fnv1a_over_candidate_result_and_used_rvv_flag` 改为结果-only 口径。板卡日志里 Std/RVV exact bit checksum 仍不同：

| case | Std checksum | RVV checksum | 解释 |
| --- | --- | --- | --- |
| `direction-update-vector6` | `3255807657299441210` | `968818922218053737` | double reduction 顺序不同导致 bit-level 指纹不同；正确性主证据是 gtest tolerance 对拍。 |
| `direction-update-vector128` | `9009593913952823174` | `1688574400692744509` | 同上。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| 当前 A/B boundary | `bench_bfgs` test-support helper；不是 production `BFGS` 模板 public boundary。 |
| GICP representativeness | `direction-update-vector6` 只贴近 GICP 状态维度；它在板卡上为 negative。 |
| `direction-update-vector128` 作用 | 只证明 VL chunk 形态；它同样 negative，且不代表当前 caller。 |
| 是否需要 caller hotspot audit | 默认不需要；局部 helper 已无正向信号。 |
| 是否允许 production dispatch | no；没有用户授权，也没有 board positive / caller hotspot / production boundary 证据。 |

## Continue / stop decision

本阶段闭合后，EvidenceDecision 为 `diagnostic_stop_no_production`：

- 不进入 `030-caller-hotspot-audit` 默认队列。
- 不扩展 `move-to+slope` 默认队列。
- 不修改 `registration/include/pcl/registration/bfgs.h`。
- 不创建 `doc-rvv/registration/bfgs-RVV.zh.md`。

若用户后续明确要求继续，只建议做 bounded negative-analysis（负向原因分析），例如比较 RVV helper 的 intrinsic overhead、reduction 顺序和 Eigen 自动向量化边界；该分析仍不应默认进入 production integration loop。

## 后续 freshness

文档回填后应复跑：

```bash
make -C test-rvv/registration/bfgs evidence_status
```

期望状态为 fresh；若出现 `doc_ref_missing`，优先确认本文件是否仍引用上方 board summary / manifest / doctor 路径。
