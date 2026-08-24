# Phase 047 计划：correspondence sorted-copy production probe

## 阶段意图和边界

本阶段把 Phase 046 中明确正向的子边界推进到 production integration loop（生产接入闭环）：只针对 correspondence（对应关系）公开入口，在 `Scalar=float`、dense、layout-gated xyz AoS、合法 correspondence、`size >= 64K` 且 query index order（查询索引顺序）呈非规则 shuffle-like disorder（类似洗牌的非规则乱序）时，尝试先复制并按 `index_query/index_match/distance` 排序，再复用当前 row-source RVV accumulation。

本阶段不证明：

- dual-indexed sorted-copy 可接入 production；
- 4K / 小规模输入可排序；
- contiguous / stride / reverse 等规则顺序应被排序；
- `Scalar=double`、非 dense、非法 correspondence 或全部自定义点型的性能结论。

## 当前状态清单

| 项目 | 当前事实 |
| --- | --- |
| adopted row-source path | Phase 043 已采纳 source-indexed、dual-indexed 和 correspondence public RVV path。 |
| Phase 045 locality profile | shuffle 仍 positive，但明显弱于 contiguous / stride / reverse。 |
| Phase 046 detail A/B | correspondence 64K / 256K sorted-copy median B/A 为 `2.344x` / `2.130x`；4K negative，dual-indexed mixed。 |
| correctness guard | `RowSourceSortedCopyMatchesShuffledPublicPath` 已证明 sorted-copy 不改变点对集合语义。 |
| 当前生产源码 | correspondence 入口只走 current shuffled gather 或父类 fallback，尚未接 sorted-copy probe。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `correspondence-sorted-copy-production-probe` | 对中大规模非规则 correspondence，排序副本改善 source gather locality（源点离散加载局部性），copy + sort 成本仍小于收益。 | 顺序启发式过宽会伤害 reverse / stride；排序改变 reduction order；生产代码引入 `std::sort` 成本和内存分配。 |

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `correspondence-sorted-copy-production-probe` | correspondence | `PointXYZ -> PointXYZ` representative；production gate 为 traits-gated xyz AoS / `float` / dense | public correspondence overload，size >= 64K，非规则 query order | `run_test_compare` 新增 production probe guard；小规模、规则顺序、double、非法 correspondence fallback / current path 保持 | 新增 production probe case-filter，至少覆盖 shuffle 64K/256K 的 public Std/RVV；可选 RVV-vs-RVV detail | board repeated production probe | production symbol 内仍命中 RVV gather / FMA / reduction；排序本身为 scalar pre-pass | Doctor 分开解释 4K not_applicable、regular order not_sorted、shuffle positive | PI5 后等待用户确认 |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| PI1 gate | 本计划 | 候选范围、fallback、启发式和暂停条件冻结。 |
| PI2 production patch | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | correspondence 入口在窄 gate 下复制排序并调用同一 RVV solver；不改 public API。 |
| PI3 correctness | `src/test_tesvd_scale.cpp` | 新增真实 production probe correctness / fallback guard；Std/RVV gtest 通过。 |
| PI4 evidence | Make / bench / summary 必要扩展 | QEMU smoke、ASM、board repeated、Evidence Doctor 和 registry 生成。 |
| PI5 decision | result 文档和矩阵 | 无论 positive / negative，都停在用户确认点；不自动采纳或回滚。 |

## Evidence Doctor 和 registry 规则

- QEMU 只证明 correctness、label、manifest shape；不使用 QEMU timing 做性能结论。
- board summary 必须按 size、order pattern 和 row-source 分开解释。
- 若 Doctor Error 来自预期 not_applicable / rejected slice，必须在 result 中解释，不得把它写成 clean pass。
- registry 必须登记新增 QEMU / board summary；`evidence_status` 必须 fresh。

## 阶段完成条件

| 状态 | 条件 |
| --- | --- |
| `pending_user_confirmation_adopt_production` | production probe 在 correspondence shuffle 64K/256K 的 public 或同边界 detail evidence 仍 positive，correctness / fallback / asm / doctor 可解释。 |
| `pending_user_confirmation_rollback` | production probe 退化、启发式误伤规则顺序、correctness / fallback / asm 失败，或 board evidence 不支持接入。 |
| `blocked` | 板卡不可用、生产启发式无法安全隔离、证据与 Phase 046 矛盾且需要人工判断。 |

## 板卡复跑预算和决策桶

默认 5 runs、warmup 5、iteration 20。`B/A > 1.2` 且 min 不退化为 positive，`1.05-1.2` 为 weak-positive，任一关键子边界 5/5 `< 1` 为 negative；预算耗尽仍正负摇摆则标为 unstable。

## 继续 / 停止条件

本阶段默认连续推进 PI2-PI5。PI5 后必须停在用户确认点：证据支持时等待用户明确确认采纳，证据不支持时等待用户明确授权回滚。若 production patch 需要扩大到 dual-indexed、4K、double、公共 API 或其它 topic，立即停止并回报。

## 文档更新清单

完成后同步本 result、phase README、optimization matrix、roadmap、evaluation、README、testing overview、benchmark/evidence、optimization evidence 和 test-support code map。`artifact_layout.topic_doc_template` 解析出的 production 长期主题文档只在 PI5 positive 且用户确认采纳后同步。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 046 是 `production_detail_rvv_vs_rvv`；Phase 047 进入 `production-public` / `production-detail` probe。 |
| A/B boundary | Phase 046 为 RVV-current vs sorted-copy wrapper；Phase 047 要变成真实 public correspondence overload。 |
| 当前决策问题 | sorted-copy 是否能作为有界 production patch 保留。 |
| diagnostic 是否可外推到 production | 部分可外推：copy + sort 成本已计入，但 Phase 046 仍是 bench wrapper；Phase 047 必须重跑真实入口证据。 |
| comparison-boundary / baseline mismatch 风险 | 有。生产启发式、内存分配、规则顺序不排序和异常语义都可能改变边界。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只允许 correspondence 64K/256K；4K 和 dual-indexed 不允许。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。若 public Std/RVV positive 但 RVV-vs-RVV 证据缺失，只能停在 pending user confirmation。 |
