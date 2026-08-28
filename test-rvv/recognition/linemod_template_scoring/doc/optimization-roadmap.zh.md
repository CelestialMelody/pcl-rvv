# Optimization Roadmap

## 当前边界

当前 topic scope 是 `recognition/src/linemod.cpp` 的 LINEMOD 模板打分链路。当前同边界证据显示 score accumulation、保守 threshold scan 和 energy map generation 均为 production-shaped diagnostic 负向；Phase 040 linearized map copy 为 production-shaped diagnostic 正向。Phase 050 进一步证明 linearized-copy-only full-chain split 在板卡上仍为 positive：full-chain total median `1.565x`，linearized copy median `1.790x`，均 0/5 退化且 Evidence Doctor clean。Phase 070 已完成 production integration loop：默认宏下 `matchTemplates` / `detectTemplates` 的 `EnergyMaps -> LinearizedMaps` copy loop 接入 RVV helper，接入后的 production-public 板卡 median 分别为 `1.138x` 和 `1.129x`，均 0/5 退化，Evidence Doctor clean。Phase 080 继续接入默认宏下 `detectTemplatesSemiScaleInvariant` 的同类 copy loop，semi-scale production-public median `1.136x`，0/5 退化，Evidence Doctor clean。Phase 000 的 accumulation positive 只保留为 historical evidence，不再作为 current truth。默认宏 adopted 范围内没有剩余 high-priority unblocked candidate；通过 closeout verification 后进入 topic-only commit flow。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| u8 map -> u16 score accumulation RVV | 当前源码中 `score_sums[mem_index] += data[mem_index]` | already-linearized maps, all candidate positions | 用 widening add（拓宽加法）替代逐元素标量累加 | Phase 020 同边界复跑 5/5 退化；旧 positive 只作 historical evidence | correctness、asm、board bench、Evidence Doctor | attempted-negative diagnostic | completed in 020-accumulation-bench-boundary-ablation |
| threshold / max scan RVV | 队列建议和源码 threshold scan | `detectTemplates` raw_score scan | 减少 threshold 比较和最大值扫描开销 | 当前双 pass 形态 5/5 退化；保序 index append 成本高 | correctness、ordered output tests、asm、board bench | attempted-negative diagnostic | completed in 010-threshold-scan-and-detection-order |
| energy map generation RVV | 源码 bit-mask energy loop | `QuantizedMap` -> `EnergyMaps` | byte mask compare 可批量处理，每个 bin 扫 `width*height` | Phase 030 board repeated median `0.954x`，5/5 退化；负向 diagnostic 不能外推完整 production no-go | same-chain correctness、bench ablation、asm、board repeated、Evidence Doctor | attempted-negative diagnostic | completed in 030-energy-map-generation |
| linearized map copy RVV / layout ablation | 源码 8x8 map copy | `EnergyMaps` -> `LinearizedMaps` | stride load + contiguous store 可能降低 offset copy 成本 | 隔离 helper 不覆盖对象分配和后续 `getOffsetMap`；需回到完整链路判断收益占比 | component bench、asm、board repeated、Evidence Doctor | attempted-positive / partial-production-candidate | completed in 040-linearized-map-copy-ablation |
| production-shaped profile / full-chain timing split | Phase 010/020/030 负向与 Phase 040 正向后升级 | energy map、linearized copy、accumulation、scan 的相对成本，以及 linearized copy RVV 单独接入是否可能抵消其它负向 | 避免把正向子核接入到非热点路径；确认对象分配和类布局成本 | per-stage timing、board repeated、doctor、production eligibility audit | attempted-positive / completed | 050-full-chain-timing-and-production-eligibility |
| PI1 default-entry production integration plan | Phase 050 positive 后升级 | 默认宏下 `matchTemplates` / `detectTemplates` 的 `EnergyMaps -> LinearizedMaps` copy loop | production direct 仍未证明；semi-scale 和 separate-energy 不在第一轮范围 | production scope audit、fallback / dispatch plan、PI2-PI5 evidence plan | completed / superseded by Phase 070 | 060-pi1-production-integration-plan |
| production-public linearized map copy RVV | 用户本轮确认“接入后板卡有收益即可采纳” | 当前 `recognition/src/linemod.cpp` 默认宏下 `matchTemplates` / `detectTemplates` | 只覆盖默认 energy-map -> linearized-map 拷贝；不覆盖 semi-scale、separate-energy、NMS 或新的 RVV family | production-direct correctness、asm attribution、5-run board repeated、Evidence Doctor、freshness | adopted production behavior | 070-production-integration-loop |
| semi-scale production-public linearized map copy RVV | Phase 070 后的同类默认宏公开入口 | 当前 `recognition/src/linemod.cpp` 默认宏下 `detectTemplatesSemiScaleInvariant` | 只覆盖 semi-scale 中默认单套 energy-map -> linearized-map 拷贝；不覆盖 separate-energy、NMS 或新的 RVV family | semi-scale production-direct correctness、asm attribution、5-run board repeated、Evidence Doctor、freshness | adopted production behavior | 080-semi-scale-production-direct-probe |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | threshold / max scan should be separated before production integration | score accumulation 正向，但 production 真实收益还依赖 threshold / detection scan 成本和输出顺序 | strict `>` tie-break、raw threshold、ordered candidate index correctness、board repeated、Evidence Doctor | high |
| 010 | accumulation direction must be rechecked under isolated binary shape | 三项 bench 中 accumulation 也从 historical positive 反转为 negative | accumulation-only binary、board repeated、Evidence Doctor | high |
| 020 | energy map generation becomes the best next candidate | score accumulation 与保守 scan 已 5/5 退化，继续同一形态微调价值低；energy map 是 `width*height*8` 的直接字节循环 | same-chain correctness、RVV bit-mask helper、board repeated | high |
| 030 | linearized map copy becomes the next isolated candidate | energy map generation RVV 5/5 退化；源码中 `EnergyMaps -> LinearizedMaps` 仍是 `8 * 8 * lin_width * lin_height * bins` 的规则拷贝循环 | offset layout correctness、stride-load RVV helper、asm、board repeated、Evidence Doctor | high |
| 040 | full-chain timing / production eligibility is now the right next gate | linearized map copy RVV 在 test helper 边界 median `2.160x` 且 doctor clean，但其它三个子核均负向；需要判断正向子核在真实链路中的成本占比和 production 接入风险 | full-chain per-stage timing、object-allocation boundary audit、board repeated、Evidence Doctor、PI1 前置范围冻结 | high |
| 050 | PI1 production integration plan is warranted, but PI2 needs explicit authorization | full-chain total median `1.565x` 且 doctor clean，说明 linearized-copy-only 候选值得生产探针；但证据仍是 test helper 边界 | production direct tests、fallback tests、production-linked asm、board repeated、Evidence Doctor、PI5 用户确认 | high |
| 060 | production patch is paused at authorization boundary | PI1 已把默认宏下 `matchTemplates` / `detectTemplates` 范围冻结；继续会修改 `recognition/src/linemod.cpp` | 用户授权 PI2-PI5；TDD RED before production patch；PI5 后等待采纳或回滚确认 | high |
| 070 | production-public evidence confirms adoption for the default path | 用户本轮规则把“接入后板卡有收益即可采纳”作为采纳条件；Phase 070 生产直连板卡 `matchTemplates 1.138x`、`detectTemplates 1.129x`，Doctor 0/0/0 | final verification、freshness、reviewer 检查；若继续扩展，semi-scale 和 separate-energy 必须另起 phase | high for closeout, medium for follow-up expansion |
| 080 | semi-scale default path also benefits from the same helper | Phase 080 生产直连板卡 `detectTemplatesSemiScaleInvariant 1.136x`，Doctor 0/0/0；用户规则允许有收益即采纳 | final verification、freshness、reviewer 检查；separate-energy 仍需独立宏语义和四套 map 证据 | high for closeout, low for follow-up expansion |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| score accumulation production patch | Phase 020 isolated accumulation-only repeated board 为 negative，当前不支持该 helper 进入 production | 只有后续 profile 证明真实入口中该子核仍是热点，且新实现族同边界 positive，才恢复 |
| conservative threshold scan production patch | Phase 010 score scan 和 combined 均 5/5 退化；双 pass 形态不值得推进 | 只有新 compress / block index staging 方案通过 order correctness 和 board repeated，才恢复 |
| energy map generation production patch | Phase 030 isolated energy-only repeated board 为 negative，当前不支持该 helper 进入 production | 只有 full-chain profile 证明 energy generation 是主热点，且新的 multi-bin/table-lookup 实现族同边界 positive，才恢复 |
| `LINEMOD_USE_SEPARATE_ENERGY_MAPS` production patch | 默认宏未启用且四套 energy / linearized maps 会改变 helper 范围；当前真实使用概率和收益未证明 | 只有用户或 build 配置需要该宏时，创建 separate-energy helper phase 并补四套 map 对拍、asm、board 和 doctor |

## 默认恢复队列

| next action | status | reason |
| --- | --- | --- |
| final verification for Phase 070 / 080 | completed | closeout rerun 覆盖 correctness、asm、freshness、diff check 和 topic path status；提交前若 staging 变化，只需重跑 diff check 和状态扫描。 |
| separate-energy helper phase | turn_stop_deferred with stop_condition_hit | 依赖非默认编译宏和四套 map 语义，当前生产证据不能外推；没有实际 build 需求时不建议继续扩大范围。 |
