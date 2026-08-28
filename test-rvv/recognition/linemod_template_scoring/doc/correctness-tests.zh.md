# LINEMOD Template Scoring Correctness Tests

## 测试专用候选链路

`src/test_linemod_template_scoring.cpp` 中的 `LINEMODTemplateScoring.*` 测试只验证 test support（测试支撑）helper 与标量参考一致。它们用于筛选 candidate family，不直接证明 production dispatch（生产分流）。

| TEST | 输入 | 断言 | 证明范围 |
| --- | --- | --- | --- |
| `ScoreAccumulationMatchesScalarReferenceWithTail` | 多张 linearized `u8` score map，长度包含 tail | RVV / 标量累加到 `u16` 的结果一致 | score accumulation helper 语义 |
| `ScoreSummaryPreservesMaxIndexTieBreak` | 人工构造并列最大值 | 最大值索引保持标量最早位置语义 | max tie-break（并列处理） |
| `ScoreScanKeepsThresholdStrictAndCandidateOrder` | `u16` score sums 和 raw threshold | strict `>`、候选 index 顺序和 checksum 一致 | threshold scan helper 语义 |
| `EnergyMapGenerationMatchesDefaultDetectTemplatesSemantics` | synthetic `QuantizedMap` 字节流 | 默认合并 energy map 与标量参考一致 | 默认 energy map generation |
| `LinearizedMapCopyMatchesDefaultDetectTemplatesLayout` | 单个 energy map | 64 个 offset map 与标量参考一致 | step=8 linearized copy |
| `LinearizedMapsCopyMatchesAllBinsLayout` | 8 个 bin 的 energy maps | 多 bin copy 布局和 checksum 一致 | Phase 040/050 copy-only 候选 |

## Production Direct Tests

`src/test_linemod_template_scoring_production_direct.cpp` 链接当前 `recognition/src/linemod.cpp`，不是 test-only helper。它通过 `FixedQuantizableModality` 构造稳定 `QuantizedMap`，再调用真实 `LINEMOD` 公开入口。

| TEST | 输入 | 断言 | 证明范围 |
| --- | --- | --- | --- |
| `MatchTemplatesProducesStableDetection` | `mem_size=257`、`nr_features=17`，默认 step=8 | `matchTemplates` 输出 1 个 detection，坐标、template id 和 checksum 合法 | 真实 `matchTemplates` 入口可运行，tail 输入不破坏输出 |
| `DetectTemplatesKeepsDefaultEntrySemantics` | 同上 | `detectTemplates` 输出非空，所有 detection 字段合法，checksum 非 0 | 真实 `detectTemplates` 默认入口语义稳定 |
| `SemiScaleDetectTemplatesKeepsScaleSemantics` | 同上，`min_scale=1.0f`、`max_scale=2.0f`、`scale_multiplier=2.0f` | `detectTemplatesSemiScaleInvariant` 输出非空，scale 字段只来自预期集合，checksum 非 0 | 真实 semi-scale 默认入口语义稳定 |

这些测试证明 correctness 和公开入口可运行；性能结论仍必须来自 Phase 070 / 080 board repeated summary。
