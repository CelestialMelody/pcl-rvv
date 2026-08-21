# Phase 000 result: current-state-and-bilinear-interior

## 执行范围

本阶段实际覆盖 `DeBayer::debayerBilinear` 的 full-size contiguous Bayer（连续 Bayer 输入）内区 2x2 stencil（邻域模板）。边界行列、Bayer 输入 padding、`debayerEdgeAware`、`debayerEdgeAwareWeighted`、OpenNI Bayer wrapper、PCLZF Bayer reader 和真实 production dispatch（生产分流）未关闭。

## 计划动作回填

| 动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED | done | `make -C test-rvv/io/debayer run_test_rvv` 先因 RVV path gate（路径命中验收）失败 | 测试能捕捉“RVV build 只走标量 fallback”的错误。 |
| GREEN correctness | done | `make -C test-rvv/io/debayer run_test_compare` | Std / RVV QEMU correctness（QEMU 正确性）均通过；RVV build 命中 candidate path。 |
| bench smoke | done | `make -C test-rvv/io/debayer run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` | QEMU 只证明日志形状和 checksum；不作为性能结论。 |
| asm | done | `make -C test-rvv/io/debayer dump_bench_rvv`；`build/asm/riscv/bench_debayer_rvv.asm` | 反汇编出现 `vlse8.v`、`vzext.vf2`、`vadd.vv`、`vsseg6e8.v` 等 RVV 指令，归属到 bench binary 内联 candidate 热区。 |
| board strided-store | attempted | `make -C test-rvv/io/debayer run_board_debayer_bilinear_inner` 两次 | checksum 一致；0.94x / 0.95x，decision bucket 为 negative。 |
| board segmented-store | attempted | 同 target，改为 `vsseg6e8` 后复跑 | checksum 一致；0.93x，decision bucket 仍为 negative。 |
| Evidence Doctor | done | `log/board/evidence_doctor.md` | 无 Error；有 negative speedup 和 metadata_incomplete Warning。 |

## Board evidence

| candidate shape | Std avg | RVV avg | speedup | checksum | decision bucket |
| --- | ---: | ---: | ---: | --- | --- |
| strided-store RVV, run 1 | 7.6153 ms | 8.1154 ms | 0.94x | match | negative |
| strided-store RVV, run 2 | 7.6372 ms | 8.0157 ms | 0.95x | match | negative |
| segmented-store RVV | 7.6639 ms | 8.2245 ms | 0.93x | match | negative |

性能结论只来自板卡。QEMU bench smoke 中的耗时只保留为日志形状证据。

## Diagnostic 到 production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断），不是 production direct。 |
| A/B boundary | test helper Std/RVV；不含 production 边界行列和调用方包装成本。 |
| 当前决策问题 | RVV-vs-scalar 候选筛选。 |
| diagnostic 是否可外推到 production | no for current candidate family；当前内区同边界已稳定慢于标量，不支持 bounded production probe（有界生产探针）。 |
| comparison-boundary / baseline mismatch 风险 | 存在：只测内区 helper，不测真实 production entry；但当前是负向同边界 A/B，因此足以拒绝这两个 test-only code shape 的生产接入。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no；两种写回形态均 negative，且没有 profile 证明生产边界会反转结论。 |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | not_applicable；本阶段不进入 production。 |

## Optimization Matrix 更新

| candidate family | entry / layout | correctness | bench | board | asm | doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| bilinear-inner-u8-stencil-strided-store | full-size 内区，12 次 `vsse8` 写回 | pass | qemu_smoke_only | negative 0.94x / 0.95x | pass | warning: negative + metadata incomplete | rejected | none inside current family |
| bilinear-inner-u8-stencil-segmented-store | full-size 内区，2 次 `vsseg6e8` 写回 | pass | qemu_smoke_only | negative 0.93x | pass | warning: negative + metadata incomplete | rejected | none inside current family |
| scalar-boundary-split production probe | production 边界标量、内区 RVV | not_run | not_run | not_run | not_run | not_run | rejected for now | 只有出现新的 positive diagnostic family 才恢复。 |
| edge-aware-mask | edge-aware 内区 | not_run | not_run | not_run | not_run | not_run | deferred with resume condition | 需要 profile 或新候选显示 `abs` / mask 成本可摊薄，不能从 bilinear negative 直接继续。 |
| weighted-inner | weighted 内区 | not_run | not_run | not_run | not_run | not_run | deferred with resume condition | 需要先有 edge-aware 或 profile 正向证据。 |

## 阶段反思

本阶段新增的 segmented-store code shape 没有改善板卡结果，反而从约 0.95x 退到 0.93x。当前负向信号更像是邻域 load 数量、u8->u16 扩展、平均计算中间向量数量和内区诊断计时边界共同造成，而不是单纯 stride store 成本。继续在同一 bilinear inner family 微调 LMUL（向量寄存器组倍率）或 store 形态，预计不容易扭转到 production-worthy（值得生产接入）的 positive bucket。

## Evidence freshness / registry

当前 topic 尚未接入 `log/evidence_registry.json`。本阶段 board target 输出在远端日志路径，未自动 fetch 成本地 raw log；可提交边界只保留人工 summary 和 Evidence Doctor 摘要。若后续恢复本 topic，优先补 topic-local board fetch / manifest wrapper，再重跑需要引用的证据。

## Continue / Stop decision

`continue_stop_decision`: stop for current bilinear RVV candidate family。停止条件命中：同边界 correctness / asm 已闭合，板卡预算内 decision bucket 稳定 negative；继续到 production 会扩大到不受当前证据支持的 production patch；edge-aware / weighted 扩展需要新的 profile 或候选设计，不再是本阶段未阻塞动作。

`next_phase_default`: `turn_stop_deferred_no_production_for_bilinear_inner`。若用户明确要求继续当前 topic，推荐先写 `010-edge-aware-profile-or-branch-distribution` 或等价 phase，做 profile / load-count ablation（加载数量消融）或 edge-aware 分支分布诊断，而不是直接接 production。
