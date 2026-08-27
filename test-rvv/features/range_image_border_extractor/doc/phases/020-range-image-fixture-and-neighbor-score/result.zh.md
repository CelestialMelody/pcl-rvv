# Phase 020 Result: RangeImage fixture and score-generation dilution audit

## 当前结论

EvidenceDecision：`attempted-neutral / no production for score-update-only`。

Phase 020 构造了真实 `RangeImage` fixture，并把 `RangeImageBorderExtractor::extractBorderScoreImages()` 计入 production-shaped diagnostic（生产形态诊断）bench。结论是：单独 score-update 在 synthetic 四图上有 `2.210x`，但一旦把真实 score generation 计入边界，`range_image_generation_plus_update_160x120` 的 5-run median 只有 `1.000x`，decision bucket 为 `neutral`。因此当前证据不建议把“仅 score-update RVV”接入 production。

## 计划回填

| action | result | evidence |
| --- | --- | --- |
| RangeImage fixture audit | done | 新增 `include/impl/range_image_border_extractor_range_fixture.hpp`；fixture 直接构造全有限 `RangeImage`，避免依赖外部 PCD |
| public-shaped oracle | done | 新增 gtest `RangeImageFixtureProducesStableFourDirectionScores`；QEMU Std/RVV 均 3/3 通过；板卡 gtest 3/3 通过 |
| component timing | partial | 新增 `range_image_score_generation_160x120` 和 `range_image_generation_plus_update_160x120`；本阶段确认 update 收益被 score generation 稀释，但尚未拆出 local surface 与 border-score 子成本 |
| board repeated + Doctor | done | `log/board/repeated_phase020_range_image_generation_score_update/{summary.md,evidence_manifest.json,evidence_doctor.md}` |

## Board repeated summary

| case | runs | median speedup | values | checksum | decision |
| --- | ---: | ---: | --- | --- | --- |
| `range_image_score_generation_160x120` | 5 | `1.000x` | `1.000x, 0.990x, 1.000x, 1.000x, 1.000x` | `24039.1` both sides | neutral |
| `range_image_generation_plus_update_160x120` | 5 | `1.000x` | `1.010x, 0.990x, 1.000x, 1.000x, 1.000x` | `26940.6` both sides | neutral |

Evidence Doctor：`Errors=0, Warnings=2, Suggestions=4`。Warnings 均为 `ba_degradation_frequency`：两个 case 都有 1/5 低于 1。Suggestions 包含缺少 taskset / governor / freq / temperature，以及 near-threshold B/A。处理方式：当前 production-shaped diagnostic 降级为 neutral，不作为 production adoption 证据。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper；真实 `extractBorderScoreImages()` 被计入，RVV update 仍是 test-only helper |
| 当前决策问题 | RVV-vs-scalar 和 production probe 是否值得继续 |
| diagnostic 是否可外推到 production | 可说明 score-update-only 收益在真实 score generation 边界下被稀释；不能证明完整 `computeFeature()`，也不能证明 score generation 本身没有可优化空间 |
| comparison-boundary / baseline mismatch 风险 | medium；fixture 是全有限合成 RangeImage，未覆盖 inf / max range / unobserved 分支和 shadow/veil 输出构造 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不建议对 score-update-only 做 production probe；允许继续做 score-generation component split / neighbor-score candidate |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；当前未进入 production integration loop，不创建 `doc-rvv` |

## 下一阶段

默认继续到 Phase 030：`score-generation-component-split`。先拆分 `extractLocalSurfaceStructure()` 与 `extractBorderScoreImages()` after-surface 的板卡成本；如果 after-surface score generation 占比足够，再尝试 `getNeighborDistanceChangeScore` 的有限 RangeImage RVV candidate。若 local surface extraction 明显支配成本且 neighbor-score 占比很小，则暂停，不建议继续当前 topic 的 production RVV 接入。
