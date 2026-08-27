# Phase 010 Result: four-score-image pipeline diagnostic

## 当前结论

EvidenceDecision：`partial-production-candidate` 继续成立。

Phase 010 把 Phase 000 的单张 score image 扩展为 left/right/top/bottom 四张连续 float score image 同批更新。板卡 clean repeated benchmark 仍为稳定正向：`score_pipeline_641x481_four_images` median speedup `2.210x`，0/5 低于 1，checksum 一致。该证据仍是 synthetic score-image diagnostic，不覆盖真实 `RangeImage` score 生成。

## 计划回填

| action | result | evidence |
| --- | --- | --- |
| source audit | done | production 的 `updateScoresAccordingToNeighborValues()` 会依次更新 `border_scores_left_`、`right_`、`top_`、`bottom_`；真实 score 生成仍依赖 `extractLocalSurfaceStructure()` 和 `getNeighborDistanceChangeScore()` |
| fixture / oracle | done for synthetic four-score images | 新增 `ScoreImageSet`、`updateScoreImageSetStd`、`updateScoreImageSetRVV`；QEMU `make run_test_compare` 为 2/2 passed |
| public-shaped bench | partial | 新增 `score_pipeline_641x481_four_images`，覆盖四方向 score-update，但未接真实 RangeImage fixture |
| QEMU + asm | done | QEMU smoke checksum 一致；`make dump_bench_rvv` 后 manifest 记录 RVV 指令相关行数 93 |
| board repeated + Doctor | done | clean 5-run summary + manifest + Doctor 已生成 |

## Board repeated summary

当前 clean evidence 路径：

- `log/board/repeated_phase010_four_score_pipeline_clean/summary.md`
- `log/board/repeated_phase010_four_score_pipeline_clean/evidence_manifest.json`
- `log/board/repeated_phase010_four_score_pipeline_clean/evidence_doctor.md`
- `log/evidence_registry.json`

| case | runs | Std mean ms | RVV mean ms | median speedup | values | checksum |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `score_pipeline_641x481_four_images` | 5 | 571.9038 | 259.3330 | `2.210x` | `2.200x, 2.160x, 2.220x, 2.230x, 2.210x` | `8.70961e+08` both sides |

Evidence Doctor：`Errors=0, Warnings=0, Suggestions=1`。剩余 suggestion 是缺少 taskset、governor、freq、temperature。该缺口不阻塞当前 diagnostic positive，但不能升级为 strict production performance。

## Historical run note

曾先运行一批 `REPEATED_BOARD_TAG=phase010_four_score_pipeline`，但没有同步覆盖 `REPEATED_BOARD_REMOTE_TAG`，导致 compare log 中远端路径仍显示 `repeated_phase000_score_update`。这批数据方向同样正向，但因路径标签错配，已降级为 historical / discarded run，不进入当前 EvidenceDecision。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic，但只到 four-score-image update boundary |
| A/B boundary | test helper；Std build 走四图标量 update，RVV build 走四图 RVV update |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape |
| diagnostic 是否可外推到 production | 可外推到四张连续 score 图像的 update 成本；不能外推到 `extractBorderScoreImages()` 和完整 `computeFeature()` |
| comparison-boundary / baseline mismatch 风险 | medium；synthetic score image 与真实 RangeImage / LocalSurface 分布可能不同 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive；若后续 RangeImage fixture 转弱或负，必须先解释 score 生成稀释和 public boundary 差异 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；production patch 需要用户确认后进入 PI1-PI5 |

## 下一阶段

默认继续到 Phase 020：`range-image-fixture-and-neighbor-score`。目标是构造真实 `RangeImage` fixture 或 production-adjacent oracle，覆盖 `extractBorderScoreImages()` / `getNeighborDistanceChangeScore()` 的输入分布与成本，判断 score-update RVV 是否仍值得接入 production。
