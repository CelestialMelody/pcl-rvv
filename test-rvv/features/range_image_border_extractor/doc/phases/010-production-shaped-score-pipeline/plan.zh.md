# Phase 010: production-shaped score pipeline diagnostic

## 阶段意图和边界

本阶段继续在 `test-rvv/features/range_image_border_extractor` 内推进，不修改 production。目标是构造更接近 `RangeImageBorderExtractor::computeFeature()` 前半段的 production-shaped diagnostic（生产形态诊断）：先把 production 中 left/right/top/bottom 四张分数图连续更新的形态放入同一 test helper / bench case，接入 Phase 000 的 score-update RVV helper，观察 helper-level positive 是否仍能穿透到四方向 score pipeline boundary。

已验证范围计划：synthetic organized 四方向 float score image、score-update helper、`Scalar=float`、row-major layout。未验证范围：真实 RangeImage fixture、`extractBorderScoreImages()` 中的 neighbor-distance score 生成、真实 production dispatch、indices 输入、完整 shadow / veil 状态机、`BorderDescription` 输出稳定性、点类型泛型扩展。

## 当前状态清单

| item | state |
| --- | --- |
| Phase 000 score-update helper | `partial-production-candidate`，board repeated median `2.560x` |
| production source | unchanged |
| upstream dedicated test | not found |
| Evidence Doctor | Phase 000 `0E/0W/1S`，环境 metadata suggestion 未阻塞 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| source audit | 复读 `extractBorderScoreImages`、`updatedScoresAccordingToNeighborValues`、`classifyBorders` | 写清 score pipeline 与 public entry 的差异 |
| fixture / oracle | 新增或扩展 test helper | 标量 oracle 可生成四方向 synthetic score image 并对拍 |
| public-shaped bench | 新增 case-filter | Std/RVV 两侧同边界 checksum 一致 |
| QEMU + asm | `make run_test_compare`、`make dump_bench_rvv` | correctness 通过，关键 RVV 指令仍归因到 score-update |
| board repeated + Doctor | `make board_repeated` 覆盖 `REPEATED_BOARD_TAG` / `REPEATED_BOARD_REMOTE_TAG` / case-filter | 5-run decision bucket 和 Doctor 结果回填 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper / public-shaped helper |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape |
| diagnostic 是否可外推到 production | 只能外推 score pipeline 局部；仍不能替代 production dispatch |
| comparison-boundary / baseline mismatch 风险 | medium；fixture 和真实 RangeImage 状态可能不同 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许条件是同边界 checksum 正确且 profile 表明 score-update 仍是 public path 主成本之一 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes |

## 完成条件和下一步

若 Phase 010 仍为 positive 且 Evidence Doctor 无 Error，下一步先进入 Phase 020 `range-image-fixture-and-neighbor-score`，补真实 RangeImage fixture 和 neighbor-distance score 生成审计；只有 Phase 020 仍支持 score-update 对 public-shaped boundary 有意义时，才请求进入 PI1 production integration plan。若 Phase 010 转为 weak / neutral / negative，只能说明 four-score-image diagnostic 不支持直接接 production；需要先做 mismatch audit 或转向 neighbor-distance / classification 候选，不能直接把 Phase 000 helper positive 写成 production decision。
