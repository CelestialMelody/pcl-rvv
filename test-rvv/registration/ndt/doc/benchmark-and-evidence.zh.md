# NDT Benchmark And Evidence

本文说明 bench（性能测试）输出、Evidence Doctor（证据体检）、反汇编和 evidence registry（证据登记表）边界。

## Bench case

| case-filter | case | 计时边界 | 证明范围 |
| --- | --- | --- | --- |
| `hessian` | `derivative-hessian-staged` | staged derivative sample 上的 score/gradient/6x6 hessian 累加 | Hessian 更新数学核是否值得 RVV 化 |
| `gradient` | `derivative-gradient-staged` | staged derivative sample 上的 score/gradient 累加 | line-search gradient-only 路径是否值得 RVV 化 |
| `all` | 两个 case | 以上两者 | 当前 repeated board 结论 |
| `production-public-align-pointxyz` | public-entry align probe | 真实 `NormalDistributionsTransform::align`；target voxel grid setup 在计时外，每次 align 的 derivative、line search 和 solver 在计时内 | 判断真实公开入口是否仍值得继续优化搜索 |

计时不包含 voxel neighbor search（体素邻域搜索）、`computePointDerivatives`、More-Thuente line search（线搜索）、Eigen SVD（奇异值分解）和 production dispatch（生产分流）。

## 当前 QEMU 证据

| 路径 | 角色 | 结果 |
| --- | --- | --- |
| `test-rvv/registration/ndt/log/qemu/run_test_std.log` | QEMU Std correctness | 4 TEST pass |
| `test-rvv/registration/ndt/log/qemu/run_test_rvv.log` | QEMU RVV correctness | 4 TEST pass |
| `test-rvv/registration/ndt/log/qemu/run_bench_all_rvv.log` | QEMU smoke log-shape（日志形状） | 可解析 |
| `test-rvv/registration/ndt/log/qemu/run_bench_public_rvv.log` | QEMU public-entry smoke | 可解析；不作性能结论 |
| `test-rvv/registration/ndt/log/qemu/evidence_manifest.json` | QEMU smoke manifest（证据清单） | 3 comparisons |
| `test-rvv/registration/ndt/log/qemu/evidence_doctor.md` | Evidence Doctor | Errors=0，Warnings=0 |
| `test-rvv/registration/ndt/build/asm/riscv/bench_ndt_rvv.asm` | filtered asm（过滤后的反汇编） | 223 行 RVV 指令候选；包含 `vle64.v`、`vfmul.vv`、`vfmacc.vv`、`vfredusum` 相关指令 |

QEMU timing 只保留作 smoke，不用于性能结论。

## 当前 board 证据

| 路径 | 角色 | 结果 |
| --- | --- | --- |
| `test-rvv/registration/ndt/log/board/derivative_accumulation_repeated/summary.md` | board repeated summary | hessian median 0.320x，gradient median 0.401x；两者 5/5 低于 1 |
| `test-rvv/registration/ndt/log/board/derivative_accumulation_repeated/evidence_manifest.json` | Board Evidence Doctor manifest | run_count=5 |
| `test-rvv/registration/ndt/log/board/derivative_accumulation_repeated/evidence_doctor.md` | Evidence Doctor | Errors=2，Warnings=2 |
| `test-rvv/registration/ndt/log/board/public_entry_profile_repeated/summary.md` | public-entry board repeated summary | median 1.003x；1/3 below 1；decision bucket=`neutral` |
| `test-rvv/registration/ndt/log/board/public_entry_profile_repeated/evidence_doctor.md` | public-entry Evidence Doctor | Errors=1，Warnings=2，Suggestions=1 |
| `test-rvv/registration/ndt/log/board/public_entry_gprof_probe/gprof.txt` | gprof 函数级剖析 | `updateDerivatives` / `updateHessian` 为主要自耗时热点 |
| `test-rvv/registration/ndt/log/board/double_exp_probe_repeated/summary.md` | double exp probe board summary | hessian median 0.321x，gradient median 0.405x；两者 5/5 低于 1 |
| `test-rvv/registration/ndt/log/board/double_exp_probe_repeated/evidence_doctor.md` | double exp probe Evidence Doctor | Errors=2，Warnings=3，Suggestions=1 |
| `test-rvv/registration/ndt/log/evidence_registry.json` | registry | QEMU 和 board summary/doctor 已登记 |

Evidence Doctor 的两个 Errors 是 `ba_degradation_frequency`：5/5 run 退化。两个 Warnings 是 `contract_mismatch`：标量 baseline 是 scalar order（标量顺序规约），candidate 是 RVV reduction（RVV 规约）。这是当前诊断的预期边界，因此只能作为 diagnostic negative evidence（诊断负向证据），不能作为 strict production A/B（严格生产 A/B）。

## 复现命令

```bash
make -C test-rvv/registration/ndt record_qemu_smoke_evidence_state
make -C test-rvv/registration/ndt record_qemu_public_smoke_state
make -C test-rvv/registration/ndt run_board_bench_repeated
make -C test-rvv/registration/ndt run_board_bench_ndt_public_repeated
make -C test-rvv/registration/ndt run_board_bench_ndt_double_exp_repeated
make -C test-rvv/registration/ndt collect_board_ndt_public_gprof_probe
make -C test-rvv/registration/ndt evidence_status
```

默认不提交 `run-*` raw log。若用户要求提交日志，应先按仓库日志脱敏策略处理。
