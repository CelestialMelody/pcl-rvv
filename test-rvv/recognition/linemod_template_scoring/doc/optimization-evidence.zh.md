# LINEMOD Template Scoring Optimization Evidence

## Candidate 状态

| candidate family | 当前状态 | 证据 | 结论边界 |
| --- | --- | --- | --- |
| score accumulation RVV | attempted-negative | Phase 020 accumulation-only board median `0.862x`，5/5 退化，Doctor Error 表示退化频率 | 不接 production；Phase 000 positive 已被 supersede |
| threshold / max scan RVV | attempted-negative | Phase 010 score scan median `0.601x`，combined median `0.879x`，均 5/5 退化 | 不接 production；保序 index append 需要新形态才可恢复 |
| energy map generation RVV | attempted-negative | Phase 030 median `0.954x`，5/5 退化 | 不接 production；默认合并 energy map 的当前 RVV 形态不值得推进 |
| linearized map copy RVV | adopted production behavior | Phase 040 isolated copy median `2.160x`；Phase 050 full-chain total `1.565x`；Phase 070 production-public `matchTemplates 1.138x`、`detectTemplates 1.129x`；Phase 080 semi-scale production-public `1.136x`，Doctor 均 0/0/0 | 覆盖默认宏下三条公开入口的 `EnergyMaps -> LinearizedMaps` copy loop |
| semi-scale linearized copy | adopted production behavior | Phase 080 `detectTemplatesSemiScaleInvariant` median `1.136x`，0/5 退化，Doctor 0/0/0 | 只覆盖默认宏 semi-scale 单套 energy map 分支，不覆盖 separate-energy |
| separate-energy linearized copy | deferred | 默认宏未启用；四套 energy / linearized maps 会扩大 helper 范围 | 只有实际 build 需要该宏时恢复 |

## 当前采用方式

当前采用的是 stride load + contiguous store（跨步加载 + 连续存储）的 `u8` copy helper。它把原标量三层循环中的 `source_col = col_index * step_size + map_col` 访存改为 RVV `vlse8.v`，把每个 offset map 的输出行用 `vse8.v` 连续写回。score accumulation、threshold scan、NMS 和 detection 对象构造仍保持标量；semi-scale 入口只复用 copy helper，不改变 scale offset 的后续读取语义。

这个采用结论只回答当前 public RVV path 是否快于当前 public scalar path。若后续新增另一个 RVV family，例如多 bin fused copy 或 separate-energy 特化，必须在同一 production boundary 内做 RVV-vs-RVV A/B。
