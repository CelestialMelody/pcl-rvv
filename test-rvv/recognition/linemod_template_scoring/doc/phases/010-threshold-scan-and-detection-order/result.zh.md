# Phase 010 Result: threshold-scan-and-detection-order

Status: completed

## EvidenceDecision

Phase 010 的结论是 `attempted-negative / continue-next-phase`。`scanScoresRVV` 正确保持了 raw threshold strict `>`（严格大于）、最大值并列保留最早位置和候选 index 保序，但 Milkv-Jupiter 5-run board repeated 结果显示当前 RVV 形态稳定退化：score scan median speedup `0.601x`，5/5 退化；accumulate+scan median `0.879x`，5/5 退化。该结果是 production-shaped diagnostic（生产形态诊断）负向证据，不是 production no-go。

## 实现和测试结果

| action | result | evidence |
| --- | --- | --- |
| RED 测试 | 已观察到 `scanScoresStd/RVV` 缺失导致编译失败 | `make run_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*"` 历史 RED |
| 标量 / RVV scan helper | 已实现；RVV 只向量化 threshold count，max 和候选 index append 保留标量 | `include/impl/linemod_template_scoring_candidates.hpp` |
| correctness | Std/RVV 3 个 gtest 通过，覆盖累加、tail、max tie-break、threshold strict 和候选顺序 | `make run_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*"` |
| bench harness | 输出 accumulation、score scan、accumulate+scan 三项，checksum 额外包含 `count_above_threshold` 和 `index_checksum` | `src/bench_linemod_template_scoring.cpp` |
| manifest wrapper | 已支持三项 benchmark 和 scan checksum 字段 | `script/generate_linemod_template_scoring_evidence_manifest.py` |
| board repeated | 5-run 完成；三项均 checksum 一致且 5/5 退化 | `doc/phases/010-threshold-scan-and-detection-order/score-scan-repeated-summary.md` |
| Evidence Doctor | Errors=3，Warnings=0，Suggestions=0；3 个 Error 均为 5/5 退化频率 | `doc/phases/010-threshold-scan-and-detection-order/score-scan-repeated-evidence-doctor.md` |
| Evidence registry | 已登记 summary / manifest / doctor | `log/evidence_registry.json` |

## Board Summary

当前 summary 主归属为 `test-rvv/recognition/linemod_template_scoring/doc/phases/010-threshold-scan-and-detection-order/score-scan-repeated-summary.md`，manifest 为 `test-rvv/recognition/linemod_template_scoring/doc/phases/010-threshold-scan-and-detection-order/score-scan-repeated-evidence-manifest.json`。

| benchmark | Std median us/iter | RVV median us/iter | speedup median | min | max | degrade frequency | bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| score accumulation in Phase 010 binary | 194.544000 | 230.582000 | 0.842x | 0.839x | 0.859x | 5/5 | negative |
| score scan | 17.037300 | 28.362300 | 0.601x | 0.595x | 0.605x | 5/5 | negative |
| accumulate+scan | 210.102000 | 239.401000 | 0.879x | 0.872x | 0.963x | 5/5 | negative |

checksum 全部一致：score checksum `9840709576022462159`，`count_above_threshold=1638`，`index_checksum=3938728128246843911`。QEMU timing 不参与性能结论。

## Evidence Doctor 处理

Evidence Doctor 报告 `Errors=3，Warnings=0，Suggestions=0`。这些 Error 不是 correctness 错误，而是退化频率达到 5/5 后，不能把任何一项写成正向或 production evidence。处理方式：

- 保留负向结果，不修改 doctor 阈值来消除 Error。
- score scan RVV 候选判为 `attempted-negative`，因为当前实现先向量化 threshold count 又二次标量扫描保序，额外 pass 和 `vcpop` 开销超过收益。
- accumulation 在 Phase 010 新 bench binary 中也变为负向，触发 Phase 020 编译期隔离消融，避免把旧 Phase 000 positive 继续写成 current truth。
- 不用本阶段负向证据直接拒绝 bounded production probe；进入 production 前仍需要真实入口或更接近 production 的 profile / bench。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` |
| 当前决策问题 | `RVV-vs-scalar` for threshold / max scan helper |
| diagnostic 是否可外推到 production | `unknown`；真实入口还包含 NMS、averaged detection、对象构造和 semi-scale 输出换算 |
| comparison-boundary / baseline mismatch 风险 | `yes`；candidate index sink 是 vector<size_t>，不等同于真实 `detections.push_back` |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `yes`，但必须先有 production-shaped direct 或 profile 证明 scan 仍是热点，并且能保持输出顺序 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若进入 production，需要 production direct Std/RVV repeated；多 RVV family 时需要同边界 RVV-vs-RVV A/B |

## Phase Reflection

本阶段生成两个新事实。第一，当前保守 scan RVV 形态不值得继续向 production 推进；若以后重新尝试，应避免“先 vector count 再完整标量 append”的双 pass 形态，改用更有界的 compress / block index staging（索引暂存）并单独验证输出顺序。第二，Phase 010 多项 bench 二进制把 accumulation 方向也翻成负向，说明 Phase 000 的旧 positive 不能继续作为当前 truth，必须通过 Phase 020 的 accumulation-only ablation 复核。

## Continue / Stop Decision

未命中停止条件。板卡可用，correctness 通过，负向结果已经被 doctor 暴露并解释，且 roadmap 仍有 Phase 020 accumulation bench boundary ablation、energy map generation 和 linearized map copy / profile 等未阻塞动作。默认继续 Phase 020。
