# Phase 040 Result: production closeout 与 identity-index 负向边界

## 当前结论

本阶段完成两件事：

- Phase 000 的 `selectWithinDistance` / `countWithinDistance` production patch（生产补丁）已按用户确认采纳为 adopted production behavior（已采用生产行为）。长期 production 文档位于 `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md`，其中性能数据使用接入后的 Phase 000 board production direct（真实生产入口直连）5-run 结果。
- identity-index strided load（恒等索引跨步加载）作为后续优化方式已经尝试，并被同一 production boundary（生产边界）内的 RVV-vs-RVV（两个 RVV 实现族直接比较）板卡 A/B 证据拒绝。当前 production 源码保持 gather-only RVV，不保留 identity fast path。

## 阶段范围

| 维度 | 已验证范围 |
| --- | --- |
| adopted production | `PointXYZ` / registered single-float x-y AoS，direct indexed `indices_` 下的 `selectWithinDistance` 和 `countWithinDistance`。 |
| identity A/B | `PointXYZ` / identity indices / 65536 points / 200 iterations / 5 warmup / 5-run board。 |
| 不覆盖 | `getDistancesToModel` production RVV、更多 PointXYZ-like 点型 dedicated board、`Scalar=double`、非 float x-y layout、超大 offset fallback 和其它 row source。 |

## 动作回填

| action | 状态 | 证据 / 结果 |
| --- | --- | --- |
| S11 production closeout | done | 已创建 `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md`，并把 topic-local README、evaluation、roadmap、matrix 和队列表从 PI5 pending 刷新为 adopted。 |
| identity production probe | attempted and rejected | 曾短暂实现 identity chunk 检测与 strided x/y load，`check_identity_strided_asm` 证明候选二进制出现 `vlse32.v`。 |
| identity RVV-vs-RVV board A/B | done | `collect_identity_repeated_board_evidence` 采集 5-run，manifest / doctor / registry 已登记。 |
| identity source cleanup | done | 负向后已撤回 identity fast path，当前 `impl/sac_model_circle.hpp` 只保留 Phase 000 gather-style RVV production path。 |
| historical probe guard | done | `check_identity_strided_asm`、identity manifest regeneration 和 gather baseline replay 需要 `ALLOW_HISTORICAL_IDENTITY_PROBE=1`；常规验证只跑 `identity_evidence_status`。 |

## Production Closeout

| 文件 | 当前 production 行为 |
| --- | --- |
| `sample_consensus/include/pcl/sample_consensus/sac_model_circle.h` | 声明 `selectWithinDistanceStandard`、`countWithinDistanceStandard` 和 `__RVV10__` 下的 select/count RVV helper；公开 API 不变。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | `selectWithinDistance` 与 `countWithinDistance` 在 RVV 构建、x/y float layout、signed 32-bit `pcl::index_t` 和 u32 byte offset gate 成立时进入 RVV，否则回退 Standard / 既有 SIMD 路径。 |
| `getDistancesToModel` | production 保持标量。Phase 020 当前 helper 形态被诊断拒绝，未接入 production。 |

## Identity A/B 证据

本阶段 identity 候选的比较口径是 strict A/B（严格 A/B）：baseline 是 Phase 000 gather-only RVV，candidate 是加入 identity strided-load fast path 的 RVV；两侧都通过同一个 public overload（公开入口重载）运行。该证据回答 RVV-family-selection（RVV 实现族选择），不能用 Std/RVV positive 替代。

证据路径：

- manifest：`test-rvv/sample_consensus/sac_model_circle/doc/phases/040-production-closeout-and-identity-frontier/identity-repeated-evidence-manifest.json`
- Evidence Doctor：`test-rvv/sample_consensus/sac_model_circle/doc/phases/040-production-closeout-and-identity-frontier/identity-repeated-evidence-doctor.md`
- run label：`circle-phase040-identity-rvv-ab-repeated-board`

| row | baseline gather-only RVV ms | identity candidate ms | B/A values | median | min/max | `B/A < 1` | decision bucket |
| --- | ---: | ---: | --- | ---: | ---: | ---: | --- |
| `selectWithinDistance` | 1.681453 | 1.696425 | 0.9914x, 0.9910x, 0.9921x, 0.9943x, 0.9872x | 0.9914x | 0.9872x / 0.9943x | 5/5 | negative |
| `countWithinDistance` | 0.342237 | 0.352368 | 0.9793x, 0.9679x, 0.9715x, 0.9678x, 0.9698x | 0.9698x | 0.9678x / 0.9793x | 5/5 | negative |

Evidence Doctor（证据体检）结果为 Errors=2，Warnings=0，Suggestions=0。两个 Error 都是 `ba_degradation_frequency`：select/count 的 5-run B/A 全部低于 1。由于退化频率稳定、决策桶一致，本阶段不继续扩大 shuffled non-regression（乱序非退化）复跑；identity 场景本身已经不支持采用。

## EvidenceDecision

| 问题 | 决策 |
| --- | --- |
| Phase 000 select/count production patch | adopted production behavior。接入后 production direct 板卡数据为 select median 1.6702x、count median 1.4012x，Evidence Doctor 0/0/0。 |
| identity-index strided load | rejected with strict A/B evidence。当前 production 只保留 gather-only RVV family。 |
| `getDistancesToModel` 当前 helper | rejected with diagnostic evidence；production scalar retained。 |
| topic closeout | ready_for_review。当前授权范围内没有新的高优先级、未阻塞、证据支持继续的优化方向。 |

## Continue / Stop Decision

`continue_stop_decision`: `ready_for_review`。

停止理由：当前 topic 的可执行优化前沿已经闭合。Phase 000 select/count 已采纳；Phase 020 getDistances 当前实现族稳定退化；Phase 040 identity strided-load 在它最有利的 identity case 仍慢于 gather-only RVV。继续尝试 `getDistancesToModel` 需要新的 RVV sqrt（向量平方根）或 dense double store（密集 double 写回）实现族；更多点型 dedicated board、`Scalar=double` 或其它 SAC 模型属于扩展 scope，不是当前 topic 内未阻塞的默认续作。
