# sac_model_plane phase 索引

本文是 `sample_consensus/sac_model_plane` topic 的阶段恢复入口。阶段文档保存计划、结果、
Evidence Doctor（证据体检）和 optimization matrix（优化矩阵）；长期 `doc-rvv` 主题文档只保存
已经采纳的 production（生产源码）行为。

| phase | 状态 | 入口 | 结果 | 默认恢复动作 |
| --- | --- | --- | --- | --- |
| `000-base-plane-select-distance-production` | `adopted_production_behavior` | `doc/phases/000-base-plane-select-distance-production/plan.zh.md` | `doc/phases/000-base-plane-select-distance-production/result.zh.md` | 已创建长期 `doc-rvv`，Phase 010 已继续验证 identity-index fast path。 |
| `010-identity-index-strided-load` | `adopted_narrow_select_count` | `doc/phases/010-identity-index-strided-load/plan.zh.md` | `doc/phases/010-identity-index-strided-load/result.zh.md` | select/count 保留 identity strided load；getDistances 保持 gather。后续补点型 correctness 后继续空 `indices_` 缺口。 |
| `020-point-type-expansion` | `adopted_for_representative_point_type_correctness` | `doc/phases/020-point-type-expansion/plan.zh.md` | `doc/phases/020-point-type-expansion/result.zh.md` | 代表点型 correctness 已闭合；Phase 025 继续补显式空 `indices_` 缺口。 |
| `025-empty-indices-correctness-gap` | `adopted_for_correctness` | `doc/phases/025-empty-indices-correctness-gap/plan.zh.md` | `doc/phases/025-empty-indices-correctness-gap/result.zh.md` | 显式空 `indices_` public entry 已闭合；无新的性能优化候选，后续仅 evidence metadata 硬化。 |
| `030-topic-closeout-submit-readiness` | `ready_for_topic_commit` | `doc/phases/030-topic-closeout-submit-readiness/plan.zh.md` | `doc/phases/030-topic-closeout-submit-readiness/result.zh.md` | 文档结构、production dispatch 和提交边界已审计；进入 topic-only commit。 |

## 当前证据边界

- QEMU correctness（QEMU 正确性验证）已覆盖 Std/RVV 两个构建的 7 个 gtest。
- 反汇编归属已在 RVV bench 二进制中定位：`selectWithinDistanceRVV` 和
  `countWithinDistanceRVV` 同时含 identity 检查、`vlsseg3e32` 和 shuffled fallback 的
  `vluxei32`；`getDistancesToModelRVV` 只保留 `vluxei32` gather。
- Phase 000 的基础 production direct（真实生产入口直连）5-run 板卡中位数为：
  `selectWithinDistance` 3.1965x、`countWithinDistance` 1.6678x、`getDistancesToModel` 2.3695x。
- Phase 010 的窄采纳板卡中位数为：identity 下 select 3.3896x、count 2.1969x；
  shuffled 下 select 3.1895x、count 1.6663x。Evidence Doctor 报告 select/count 无 Error
  或 Warning；剩余 Suggestions 是环境字段和二进制身份，不阻塞当前窄采纳。
- Phase 025 后的板卡 correctness smoke 运行 7 个 RVV gtest 全部通过；Makefile clock skew 是环境
  warning，不作为 correctness failure（正确性失败）。
