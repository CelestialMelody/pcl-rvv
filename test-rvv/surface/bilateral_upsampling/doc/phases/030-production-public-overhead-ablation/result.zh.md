# Phase 030 result: production public overhead ablation

## 当前结论

本阶段已新增 steady-state public shell（稳态公开入口外壳）bench cases，用于把 phase 020 的 full public probe（完整公开入口探针）和“对象构造 / setters 移出 timed window、stdout 打印临时静默”的 public `process` 形态分开看。生产源码没有新增改动；本阶段只触碰 topic-local bench、manifest 和文档。

本地可闭合证据：`dump_bench_rvv` 通过，QEMU correctness（QEMU 正确性）Std/RVV 各 9 个测试通过，QEMU bench smoke（QEMU 小型 bench 冒烟，只看日志和 checksum 形状）能看到新增 steady-state case，反汇编仍能归属到生产 helper 的 `vlse32.v`、`vmfeq.vv`、`vmerge.vvm`、`vfmul.vv`、`vfredusum.vs`。

板卡恢复后已完成 `board_smoke`。steady-state public shell 三项为 `0.98x`、`0.91x`、`0.98x`，Evidence Doctor（证据体检）为 `Errors=3`、`Warnings=9`、`Suggestions=0`。这说明把对象构造和 setters 移出 timed window、并静默 stdout 后，小 / 大 case 只从 phase 020 的 `0.96x/0.95x` 靠近到 `0.98x/0.98x`，中等 holes case 仍为 `0.91x`；public shell overhead ablation（公开入口外壳开销消融）没有把当前生产补丁救成可采纳。

当前 EvidenceDecision（证据决策）：`pending_user_confirmation_rollback`。phase 030 消融结果不支持把 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` 里的当前 RVV 生产补丁写成 adopted（已采纳）；但按照 PI5 规则，worker 仍不能自动回滚，需要用户确认。

## 计划回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| bench ablation | done | `src/bench_bilateral_upsampling.cpp` | 新增 `production steady public` 三个 case，复用对象并静默 stdout。 |
| asm refresh | done | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | RVV bench 编译通过，生产 helper 仍有目标 RVV 指令。 |
| QEMU correctness | done | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | Std/RVV 各 9 个测试通过。 |
| QEMU bench smoke | done | `make -C test-rvv/surface/bilateral_upsampling run_bench_rvv BENCH_ARGS='1 0'` | 新增 steady-state public case 的输出形状可见，checksum 与 correctness 仍对齐；QEMU timing 不作为性能结论。 |
| board refresh | done | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | steady-state public 三项为 `0.98x`、`0.91x`、`0.98x`，decision bucket 为 negative。 |
| doctor refresh | done-with-errors | `doc/phases/030-production-public-overhead-ablation/evidence_manifest.json` -> `evidence-doctor.md` | `Errors=3` 来自三项 `ba_degradation_frequency`；结论仍是不建议采纳当前生产补丁。 |

## 新增 bench case

| case | Std avg | RVV avg | speedup | 计时边界 | 证据角色 |
| --- | ---: | ---: | ---: | --- | --- |
| `bilateral upsampling production steady public PointXYZRGB 80x60 w3 dense` | 7.3080 ms | 7.4799 ms | 0.98x | `BilateralUpsampling` 对象和 setters 在 timed window 外，`process` 内 stdout 临时静默 | public shell overhead ablation |
| `bilateral upsampling production steady public PointXYZRGB 120x90 w4 holes` | 27.4150 ms | 30.2814 ms | 0.91x | 同上 | public shell overhead ablation |
| `bilateral upsampling production steady public PointXYZRGBA 180x120 w5 dense` | 81.1577 ms | 82.5887 ms | 0.98x | 同上 | public shell overhead ablation |

这些 case 不是 production adoption evidence（生产采纳证据）。它们只说明 public shell overhead 不是 phase 020 负向的充分解释；最终是否回滚或继续保留当前生产补丁，仍需要用户在 PI5 确认。

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | production-public-ablation |
| A/B boundary | public overload；phase 030 的计时边界和 phase 020 full public probe 不同。 |
| 当前决策问题 | 判断 public shell overhead 是否足以解释 phase 020 负向。 |
| diagnostic 是否可外推到 production | no。phase 030 只能解释开销边界，不能替代 phase 020 的 production public 证据；最新消融仍为 `0.98x/0.91x/0.98x`。 |
| comparison-boundary / baseline mismatch 风险 | yes，steady-state case 移出了 setup 并静默 stdout，是有意的消融边界。 |
| weak / negative / neutral / unstable 时是否允许 bounded probe | 本阶段已经完成 bounded probe；负向结果不支持继续扩大生产接入范围。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前不适用；phase 020 和 phase 030 都不支持 adopted。 |

## 板卡复跑预算和决策桶

本阶段预算只允许一次 board_smoke。最新结果三项都低于 1.0x，且中等 holes case 为 `0.91x` 明确负向；decision bucket 稳定为 negative，不追加复跑。

## 继续 / 停止决定

`continue_stop_decision`: stop_for_PI5_user_confirmation。

`stop_condition_hit`: phase 030 已完成板卡性能和 Evidence Doctor，但结论不支持采纳；PI5 规则要求保留当前生产补丁并等待用户确认，不能自动回滚。

`next_phase_default`: 默认建议用户确认回滚 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` 的当前生产补丁，保留 topic-local 测试、bench、manifest、Evidence Doctor 和文档。若用户要求继续调查，只建议另起更深的 production-detail component ablation（生产细节组件消融）阶段；不建议把当前补丁接入源码。

`qemu_bench_smoke_note`: `run_bench_rvv BENCH_ARGS='1 0'` 已验证新增 steady-state case 的日志和 checksum 形状，但它只证明 correctness / output shape，不能替代板卡性能。
