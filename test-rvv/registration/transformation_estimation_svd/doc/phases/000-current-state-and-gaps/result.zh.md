# Phase 000 结果：dense ordered-cloud-pair 诊断 scaffold

## 当前阶段结论

Phase 000 已完成 `transformation_estimation_svd` 的 topic-local scaffold、S2 evaluation（函数级评估）、dense ordered-cloud-pair（稠密顺序点云对）fused accumulation（融合累加）test-only candidate、QEMU correctness（QEMU 正确性验证）、窄 QEMU bench smoke（只验证日志形状）和 bench RVV 反汇编输入。

当前 EvidenceDecision（证据决策）仍是 `diagnostic`。本阶段不修改 production（生产源码），不创建 `doc-rvv/registration/transformation_estimation_svd-RVV.zh.md`。QEMU timing（QEMU 计时）不能作为性能结论；板卡或目标硬件 repeated benchmark（重复性能测试）仍缺失。

## 完成矩阵

| action | status | 证据路径 | 缺口 |
| --- | --- | --- | --- |
| A1 test support scaffold | done | `include/tesvd.h`、`include/impl/tesvd_candidates.hpp` | none |
| A2 correctness tests | done | `src/test_tesvd.cpp`、`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | board correctness 未跑 |
| A3 bench smoke wrapper | done | `src/bench_tesvd.cpp`、`log/qemu/analyze_bench_compare.log` | QEMU timing 只作日志形状；board repeated 缺失 |
| A4 Makefile / board.mk | done | `Makefile`、`board.mk` | board repeated target / summary wrapper 待 Phase 010 |
| A5 topic-local docs | done | README、evaluation、testing overview、correctness、benchmark、optimization evidence、code map、roadmap、matrix | current handoff 仅在最终回复输出 |

## Correctness 结果

命令：

```bash
make -C test-rvv/registration/transformation_estimation_svd run_test_compare
```

结果：

| 构建 | 状态 | 说明 |
| --- | --- | --- |
| Std / `USE_PCL_RVV10=0` | pass | 5 tests passed。 |
| RVV / `USE_PCL_RVV10=1` | pass | 5 tests passed；RVV 构建下 ordered-cloud-pair candidate 命中 `used_rvv`，小规模 case 命中 fallback。 |

覆盖范围：

- public Umeyama baseline（当前公开入口默认路径）与 fused scalar reference 在 `3e-4` 内一致。
- RVV candidate 与同构 fused scalar reference 在 `7e-4` / `8e-4` 预算内一致。
- 小规模输入保持 fallback（回退路径）。
- `PointXYZI` 只证明 x/y/z AoS（结构数组）布局可读，不证明完整泛型 production gate。
- reflection stress（反射压力样本）覆盖 determinant sign fix（行列式符号修正）附近路径。

## QEMU bench smoke

命令：

```bash
ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd run_bench_compare BENCH_ARGS="--case-filter fused-full-cloud --iterations 3 --warmup-iterations 1"
```

结果只用于 log shape（日志形状）：

| case | Std avg | RVV avg | QEMU smoke ratio | 说明 |
| --- | ---: | ---: | ---: | --- |
| fused full-cloud 4K | 0.3033 ms | 0.3826 ms | 0.79x | 历史 label；语义为 ordered-cloud-pair。QEMU timing 不代表目标硬件性能。 |
| fused full-cloud 64K | 3.3905 ms | 4.6053 ms | 0.74x | 历史 label；作为风险信号，不能作 no-production 结论。 |
| fused full-cloud 256K | 13.9964 ms | 18.1893 ms | 0.77x | 历史 label；需要 board repeated 判断真实方向。 |

checksum 已修正为非零混合值，避免偶数次 XOR 折回成 0。

## 反汇编归因

命令：

```bash
make -C test-rvv/registration/transformation_estimation_svd dump_bench_rvv
```

证据：

- `build/asm/riscv/bench_transformation_estimation_svd_rvv.full.asm`
- `build/asm/riscv/bench_transformation_estimation_svd_rvv.asm`

人工抽查看到与 candidate 相关的 `vlsseg3e32.v`、`vfadd.vv`、`vfmacc.vv` 和 `vfredosum.vs` 指令簇。当前归因只覆盖 bench binary 的 test-only helper；production 符号归因不适用。

## Evidence Doctor 和 registry

当前已有 topic-local QEMU smoke manifest wrapper，并用全局 Evidence Doctor（证据体检）生成报告：

| 项 | 结果 | 处理 |
| --- | --- | --- |
| Errors | 0 | correctness 通过；bench smoke manifest 可由 doctor 解析。 |
| Warnings | 0 | QEMU timing 已在 manifest 中降级为 `qemu_smoke_only`，不作为性能分布输入。 |
| Suggestions | 0 | QEMU smoke 证据合同当前干净；Phase 010 仍需补 board repeated summary / manifest / doctor wrapper。 |

证据路径：

- `log/qemu/evidence_manifest.json`
- `log/qemu/evidence_doctor.md`
- `log/evidence_registry.json`

`make -C test-rvv/registration/transformation_estimation_svd evidence_status` 报告 registry fresh。QEMU correctness log、bench smoke raw log、summary、manifest 和 doctor 均已登记；raw logs 默认仍按 local-only / review-required 边界处理。

## Optimization matrix 更新

| candidate family | 状态 | 当前证据 | 下一步 |
| --- | --- | --- | --- |
| `fused_full_cloud_accum` | correctness pass / bench smoke pass as log-shape | QEMU Std/RVV 各 5 tests passed；bench RVV asm 有 RVV 指令；QEMU smoke 只作风险信号 | Phase 010 上板 repeated + Evidence Doctor |
| `public_umeyama_baseline` | baseline ready | public semantic anchor 已纳入 gtest；bench wrapper 有 `public-umeyama` case-filter | board A/B 时作为 baseline |
| `source_indexed_fused_accum` | deferred | 未实现 | ordered-cloud-pair board 方向明确后做 row-source audit |
| `dual_indices_or_correspondences` | deferred | 未实现 | 独立 phase，不能继承 ordered-cloud-pair 结论 |
| `production_direct_dispatch` | blocked | 无 production patch | 需要 board positive 和 PI1 |

## 继续 / 停止决定

`continue_stop_decision`：Phase 000 本地动作已闭合，下一步必须是目标硬件证据或其前置脚本，不是 production patch。`make -C test-rvv/registration/transformation_estimation_svd check_board_ssh` 失败，板卡 SSH 路由不可达，因此 Phase 010 命中真实 blocker。QEMU log shape 只作为本地证据，不升级为性能结论。

`next_phase_default`：恢复板卡网络后进入 `010-board-and-asm-diagnostic`，先运行 `make -C test-rvv/registration/transformation_estimation_svd check_board_ssh`，再补 board repeated target / summary / manifest / Evidence Doctor；板卡恢复前 EvidenceDecision 保持 `diagnostic`。
