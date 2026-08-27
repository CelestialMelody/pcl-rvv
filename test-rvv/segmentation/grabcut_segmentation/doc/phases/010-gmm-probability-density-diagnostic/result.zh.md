# Phase 010: GMM probability density diagnostic result

## 阶段结论

Phase 010 完成了 `segmentation/src/grabcut_segmentation.cpp` 中 `GMM::probabilityDensity(i, c)` 的 test-only same-chain（测试专用同构链路）诊断。RVV candidate（RVV 候选实现）在 QEMU correctness（QEMU 正确性）和板卡 component ablation（组件消融）里均可运行；当前 GMM 专用 manifest 记录的板卡输入 `640x480`、`iterations=8`、`warmup=2` 下，`gmm_probability` 从 Std `22.013766 ms` 到 RVV `13.026312 ms`，B/A（候选相对基线收益）为 `1.69x`。

这个结果支持继续做 public-shaped diagnostic（公开入口形态诊断）或 terminal-weight（端点权重）组件拆分。它不支持直接修改 production（生产源码），也不支持把 GMM 组件收益外推到完整 GrabCut `extract/refineOnce`。

## 执行范围回填

| 计划动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED GMM correctness | done | `make -C test-rvv/segmentation/grabcut_segmentation run_test_compare` 曾因 `grabcut_diag::Gaussian` 和 GMM helper 缺失失败。 | 测试能捕获缺失 helper。 |
| GREEN same-chain helper | done | `test-rvv/segmentation/grabcut_segmentation/include/impl/grabcut_diagnostic_reference.hpp` | Std build 走标量 reference；RVV build 走 `pcl::expf_RVV_f32m2` 和 RVV FMA（融合乘加）链路。 |
| correctness 对拍 | done | `make -C test-rvv/segmentation/grabcut_segmentation run_test_compare` | Std 2 tests passed；RVV 3 tests passed。GMM 最大误差由测试阈值 `abs=2e-5`、`rel=2e-4` 限制。 |
| Bench case | done | `src/bench_grabcut.cpp` 支持 `--case gmm_probability`。 | bench 输出 `max_abs_error`、`max_rel_error` 和 `checksum_policy=input_fingerprint_error_budget`。 |
| QEMU smoke | done | `make -C test-rvv/segmentation/grabcut_segmentation run_bench_rvv BENCH_ARGS='--case all --iterations 2 --warmup 1'`、`make -C ... run_bench_std BENCH_ARGS='--case all --iterations 2 --warmup 1'` | 只证明 build、路径和日志形状，不写性能结论。 |
| 反汇编归属 | done | `test-rvv/segmentation/grabcut_segmentation/build/asm/riscv/bench_grabcut_rvv.full.asm` | `computeGMMProbabilityCandidate` 符号范围内出现 `vle32.v`、`vse32.v`、`vfmacc.vf`、`vfmacc.vv`。 |
| 板卡 GMM bench | done | `make board_smoke BENCH_ARGS="--width 640 --height 480 --iterations 8 --warmup 2 --case gmm_probability" REMOTE_TEST_ARGS=""` 后执行 `make run_gmm_evidence_doctor`。 | Milkv-Jupiter 上 `640x480` GMM component 为 `1.69x`，当前只按单次 repeated group 解释。 |
| Evidence Doctor manifest | done | `make -C test-rvv/segmentation/grabcut_segmentation run_gmm_evidence_doctor` | `Errors=0, Warnings=1, Suggestions=0`；warning 是 `low_run_count`。 |

## 诊断证据链

| 证据层 | 当前事实 | 不能证明的范围 |
| --- | --- | --- |
| correctness | `GrabCutDiagnosticReference.GMMProbabilityBatchMatchesScalarReference` 对固定 Gaussian（高斯分量）和 BGR 样本验证 same-chain 输出。 | 不覆盖 `probabilityDensity(c)` 的 `pi` 加权求和，也不覆盖 GMM fitting。 |
| 数值预算 | RVV 使用 `pcl::expf_RVV_f32m2`，board log 记录 `max_abs_error=2.384186e-07`、`max_rel_error=2.700003e-07`，低于 `2e-5 / 2e-4` 预算。 | 该 helper 是 finite-domain fast approximation（有限输入域快速近似），不是 strict libm replacement（严格 libm 替换）。 |
| QEMU | QEMU 跑通 test 和小型 bench log shape。 | QEMU timing 不进入性能排序。 |
| 反汇编 | GMM candidate 符号内有 RVV load/store 和 FMA 指令。 | 反汇编只证明 test-support helper 命中 RVV，不证明 production dispatch。 |
| 板卡 | Milkv-Jupiter `640x480`、`iterations=8`、`warmup=2` 下 GMM component `1.69x`。 | run_count 为 1，不能证明长尾、稳定性或完整 GrabCut public path speedup。 |
| Evidence Doctor | GMM 专用 manifest 有 1 个 comparison，`Errors=0`。 | `low_run_count` warning 要求后续 repeated board summary 或降级为初筛诊断。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic / component_ablation`。 |
| A/B boundary | `test_support_helper`，baseline 是 topic-local scalar reference，candidate 是 topic-local RVV helper。 |
| 当前决策问题 | `RVV-vs-scalar feasibility` 和是否进入 public-shaped diagnostic。 |
| diagnostic 是否可外推到 production | 只能部分外推。公式来自 production `GMM::probabilityDensity(i, c)`，但当前 bench 不包含 `probabilityDensity(c)` 的 K=5 求和、`learnGMMs`、terminal `log`、graph mutation 和 max-flow。 |
| comparison-boundary / baseline mismatch 风险 | 存在。当前 Std/RVV 是不同 build 的 helper-level 对比，不是同一 production boundary 内的 RVV-vs-RVV family selection（实现族选择）。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 public-shaped diagnostic 证明 GMM / terminal weight 在真实入口中仍占可见比例，可以进入 PI1；若完整路径被 graph / max-flow 稀释，则保留为 bench-only 诊断。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。生产接入前还需 public entry、fallback、asm 和 repeated board 证据。 |

## Evidence Doctor 和证据新鲜度

`doc/phases/010-gmm-probability-density-diagnostic/evidence-manifest.json` 是本阶段 GMM 专用 manifest。它由 topic-local wrapper `script/generate_grabcut_board_evidence_manifest.py` 从 `log/board/run_bench_std.log` 和 `log/board/run_bench_rvv.log` 生成。

`doc/phases/010-gmm-probability-density-diagnostic/evidence-doctor.md` 报告：

- Errors：0。
- Warnings：1，`low_run_count`。当前 manifest 只含一个 GMM comparison，因此性能证据是 positive diagnostic signal（正向诊断信号），不是 production performance evidence（生产性能证据）。
- Suggestions：0。

`log/board/evidence_manifest.json` 和 `log/board/evidence_doctor.md` 保存当前本地 board log 的整体 manifest / doctor，但会被后续 `board_smoke` 覆盖。需要复核 Phase 010 时以 `doc/phases/010-gmm-probability-density-diagnostic/evidence-manifest.json` 和 `evidence-doctor.md` 为准。organized n-link 的上一批 `case=all` 结果只作为 historical observation（历史观察）写入矩阵：`320x240` 下 organized n-link 曾出现 `0.98x` 退化信号，不能继续写成待采纳候选。

## 阶段反思和矩阵更新

| candidate family | 更新 | 下一步 |
| --- | --- | --- |
| GMM probability density | `attempted / positive diagnostic`。当前证据支持继续拆 public-shaped terminal weight，不支持直接 production。 | Phase 020：把 GMM probability 连接到 `probabilityDensity(c)` / terminal-weight 形态，补 K=5 求和、`pi` 权重和 `-log` 口径。 |
| organized n-link RVV exp candidate | `attempted / weak-negative historical observation`。上一批 all-case board log 在 `320x240` 下为 `0.98x`，且全量 doctor 曾提示 `ba_degradation_frequency`。 | 若继续该路线，需要独立 repeated board 和更窄 asm / memory traffic（内存流量）分析；默认不把它放在下一 phase 首位。 |
| evidence manifest wrapper | `adopted` 到当前 topic。 | 后续 phase 复用 `run_gmm_evidence_doctor` 或扩展 CASE_LABELS。 |

## Continue / stop decision

`continue_stop_decision=continue`。没有命中停止条件：板卡可用，当前修改没有越过 production，dirty isolation 可用，GMM component 证据给出正向诊断信号。默认下一阶段是 Phase 020 public-shaped GMM / terminal-weight diagnostic，范围仍限制在 `test-rvv/segmentation/grabcut_segmentation`，先不改 production。

Phase 020 应先写 plan，再实现或调整测试资产。建议覆盖：

- `GMM::probabilityDensity(c)` 的 K=5 `pi` 加权求和。
- `initGraph` terminal weights 中的 `-log(fromSource)` / `-log(toSink)` 口径。
- 真实 graph mutation 和 max-flow 继续作为标量边界，bench 只测 terminal-weight 组件或 public-shaped wrapper。
- 至少一次 board repeated summary 或明确 run budget；若继续要进入 production integration loop，必须先完成 PI1。
