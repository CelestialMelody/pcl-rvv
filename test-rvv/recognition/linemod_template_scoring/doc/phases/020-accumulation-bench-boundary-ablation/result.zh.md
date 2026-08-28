# Phase 020 Result: accumulation-bench-boundary-ablation

Status: completed

## EvidenceDecision

Phase 020 的结论是 `attempted-negative / continue-next-phase`。编译期隔离的 accumulation-only bench（只编译累加计时路径的性能测试）仍显示 RVV 分数累加 helper 在 Milkv-Jupiter 上 5/5 退化：median speedup `0.862x`，min `0.825x`，max `0.889x`。这说明 Phase 000 的 positive 主要应降级为 historical evidence（历史证据）；当前同边界复跑不支持把 score accumulation RVV 作为 production candidate（生产候选）推进。

## 实现和测试结果

| action | result | evidence |
| --- | --- | --- |
| RED：accumulation-only 宏未生效 | 已观察到宏打开后仍输出 `score scan` / `accumulate+scan`，断言失败 | `make run_bench_rvv EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_ACCUMULATION_ONLY" BENCH_ARGS="4096 96 1 0"` |
| GREEN：编译期开关 | 宏打开时只输出 accumulation timing 和旧 checksum 字段；默认 bench 仍保留三项输出 | `src/bench_linemod_template_scoring.cpp` |
| Make target | 已新增 Phase 020 collect / manifest / doctor / registry / freshness target，并让 dump target 强制重建正确 binary shape | `Makefile` |
| QEMU log-shape smoke | 宏打开后只输出 `LINEMOD score accumulation`，不输出 scan / combined；该 timing 不参与性能结论 | `log/qemu/phase020-green-accumulation-only-rvv.log` |
| board repeated | 5-run 完成，checksum 一致，5/5 退化 | `doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-summary.md` |
| Evidence Doctor | Errors=1，Warnings=0，Suggestions=0；Error 为 accumulation 5/5 退化频率 | `doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-evidence-doctor.md` |
| Evidence registry | 已登记 summary / manifest / doctor | `log/evidence_registry.json` |

## Board Summary

当前 summary 主归属为 `test-rvv/recognition/linemod_template_scoring/doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-summary.md`，manifest 为 `test-rvv/recognition/linemod_template_scoring/doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-evidence-manifest.json`。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum |
| --- | ---: | ---: | ---: | --- |
| run-01 | 189.350000 | 229.422000 | 0.825x | `9840709576022462159` |
| run-02 | 197.333000 | 223.635000 | 0.882x | `9840709576022462159` |
| run-03 | 202.322000 | 227.662000 | 0.889x | `9840709576022462159` |
| run-04 | 194.960000 | 227.175000 | 0.858x | `9840709576022462159` |
| run-05 | 194.162000 | 225.359000 | 0.862x | `9840709576022462159` |

统计口径：Std median `194.960 us/iter`，RVV median `227.175 us/iter`，speedup median `0.862x`，5/5 退化。与 Phase 010 的 accumulation median `0.842x` 方向一致；与 Phase 000 old accumulation-only run 的 `1.404x` 方向相反。

## Evidence Doctor 处理

Evidence Doctor 报告 `Errors=1，Warnings=0，Suggestions=0`。该 Error 是负向性能证据，而不是 manifest 合同或 checksum 错误。处理方式：

- Phase 000 old summary 降级为 historical baseline，不再作为 current truth。
- 当前 accumulation RVV helper 判为 `attempted-negative`，不进入 production integration loop。
- 因为 Phase 020 已用编译期隔离消融复核，短期不建议继续在同一 `u8->u16` 单累加形态上微调；下一阶段更应转向真实入口成本拆分，例如 energy map generation、linearized map copy 或 production-shaped profile。
- `script/rvv-board-run.mk` clock skew warning 持续出现，但命令成功、测试通过、日志完整；记录为环境 warning，不作为板卡 blocker。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper / bench binary shape ablation` |
| 当前决策问题 | `implementation-shape` 与 `RVV-vs-scalar` 边界复核 |
| diagnostic 是否可外推到 production | `unknown`；只说明当前 isolated accumulation helper 不支持生产接入，不能证明完整 `detectTemplates` 无 RVV 空间 |
| comparison-boundary / baseline mismatch 风险 | `yes`；Phase 000、010、020 是不同 binary / output shape，已在本阶段分层解释 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `yes`，但不针对当前单累加 helper；需要 profile 或 production-shaped direct 证明其它 LINEMOD 子核是热点 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若后续进入 production，需要 production direct Std/RVV repeated；若存在多个 RVV family，需要同边界 RVV-vs-RVV A/B |

## 根因判断

当前证据支持的最小结论是：Phase 000 的正向主要来自旧 bench / binary / Std 侧状态，不适合作为当前同边界性能 truth。Phase 010 和 Phase 020 的 Std 侧都稳定在约 `194 us/iter`，RVV 侧稳定在约 `225-231 us/iter`；因此 accumulation RVV helper 本身在当前工具链和板卡状态下并不优于标量。尚未证明的事项包括：旧 Phase 000 Std 侧为什么偏慢、真实 production 中 score accumulation 占比多少、energy map 或 linearized map 是否有更好的 RVV 机会。

## Continue / Stop Decision

未命中停止条件。当前不建议继续推进 score accumulation / conservative score scan 的 production patch，但 topic 仍有未阻塞的 `energy map generation`、`linearized map copy / layout ablation` 和 production-shaped profile 候选。默认下一 phase 应转向真实入口成本拆分或 energy map generation，而不是在已 5/5 退化的累加 helper 上继续微调。
