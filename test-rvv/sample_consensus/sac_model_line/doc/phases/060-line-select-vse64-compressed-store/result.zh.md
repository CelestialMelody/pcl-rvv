# Phase 060 Result: line select RVV compressed double store

## 当前结论

本阶段把 `SampleConsensusModelLine<PointT>::selectWithinDistanceRVV` 中
`error_sqr_dists_` 的压缩写回从 `float scratch + 标量 lane 转 double` 改成
RVV `vfwcvt + vse64` 直接写回。变更后 correctness（正确性）、production asm
attribution（生产反汇编归属）、5-run board repeated（板卡重复性能测试）和
Evidence Doctor（证据体检）均闭合。

Phase 060 public `selectWithinDistance` 的 5-run 结果为 `Std avg 2.464332 ms`、
`RVV avg 0.772475 ms`，B/A values 为
`3.2460x, 3.1463x, 2.9755x, 3.2513x, 3.3620x`，median/min/max 为
`3.2460x / 2.9755x / 3.3620x`。相对 Phase 050 的 public select RVV avg
`0.791395 ms`，本阶段约快 `1.024x`；收益小但方向正，且去掉了 scratch buffer
和标量 lane loop。因此当前决策为 `adopted production behavior for current
boundary`。

## 实际执行范围

| 计划项 | 实际状态 | 证据 |
| --- | --- | --- |
| 修改 select 压缩距离写回 | done | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` 中删除 `compressed_sqr_distances` 和标量 lane loop，改用 `vfwcvt + vse64`。 |
| asm gate update | done | `check_line_production_asm.py` 对 `selectWithinDistanceRVV` 增加 `vfwcvt` / `vse64` 检查，并保留 `vcompress` / `vse32` 检查。 |
| correctness | done | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare`：Std/RVV 各 7 个 gtest 全部通过。 |
| asm attribution | done | `make -C test-rvv/sample_consensus/sac_model_line check_production_asm`：三条 production RVV helper 均通过。 |
| board repeated | done | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_select_vse64_evidence`：5/5 run 完成，板卡 RVV gtest 每轮 7/7 通过。 |
| Evidence Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_select_vse64_evidence_state`：Errors=0 / Warnings=0 / Suggestions=0，并登记本阶段三份 summary evidence。 |

## Production Direct Board Summary

| public entry | Std avg ms | RVV avg ms | B/A values | median / min / max | decision bucket |
| --- | ---: | ---: | --- | --- | --- |
| `countWithinDistance` | 1.785273 | 0.373631 | `4.8068x, 4.8607x, 4.6009x, 4.7936x, 4.8351x` | `4.8068x / 4.6009x / 4.8607x` | positive-stable |
| `selectWithinDistance` | 2.464332 | 0.772475 | `3.2460x, 3.1463x, 2.9755x, 3.2513x, 3.3620x` | `3.2460x / 2.9755x / 3.3620x` | positive-stable |
| `getDistancesToModel` | 2.268423 | 0.555793 | `4.0460x, 4.0334x, 4.0348x, 4.2406x, 4.0557x` | `4.0460x / 4.0334x / 4.2406x` | positive-stable |

`countWithinDistance` 和 `getDistancesToModel` 的实现未在本阶段改变；它们的 public rows
用于确认同一 RVV binary 下仍保持 positive-stable。与 Phase 050 比较时，count/getDistances
的细小差异属于不同 run batch（运行批次）的波动，不作为本阶段的优化结论。

## Evidence Doctor 和 Registry

Evidence paths:

- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.json`

Evidence Doctor 输出：Errors=0 / Warnings=0 / Suggestions=0。`log/evidence_registry.json`
已经登记 manifest、doctor Markdown 和 doctor JSON；registry 是 local-only metadata（本地元数据），
不默认提交。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-detail（生产细节实现族比较）和 production-public（公开入口 Std/RVV 对比）。 |
| A/B boundary | A 是 Phase 050 已采纳 `select-vcompress-scratch-scalar-store`；B 是 Phase 060 `select-vcompress-vse64-error-store`。两者都在 `selectWithinDistance` production helper 边界内。 |
| 当前决策问题 | 新压缩距离写回形态是否比当前已采纳形态更值得保留。 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；本阶段直接修改 production helper 并重跑 public entry。 |
| comparison-boundary / baseline mismatch 风险 | Phase 050 和 Phase 060 来自不同 binary / run batch，因此 RVV-vs-RVV 数值只能作为同 production boundary 的 detail A/B 参考。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段 probe 已有界；实际结果没有退化到需要回滚的桶。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 本阶段使用接入后 public entry 的 Phase 050 / Phase 060 RVV avg 和 median 对比；因为不是逐 run 同步 A/B，结论写成小幅 positive，而不是夸大收益。 |

## 矩阵更新

| candidate family | decision | 理由 |
| --- | --- | --- |
| `select-vcompress-scratch-scalar-store` | historical adopted baseline, replaced by Phase 060 | Phase 050 public select 已正向，但源码仍有 scratch + 标量 lane double store。 |
| `select-vcompress-vse64-error-store` | adopted production behavior | correctness、asm、5-run board 和 doctor 均通过；public select RVV avg 相对 Phase 050 小幅改善。 |
| `count-indexed-gather-f32m2` | adopted production behavior | 本阶段未改 count；Phase 060 同一 binary public count 仍 positive-stable。 |
| `getDistances-vfsqrt-vse64-store` | adopted production behavior | 本阶段未改 getDistances；Phase 060 同一 binary public getDistances 仍 positive-stable。 |

## 继续 / 停止判断

本阶段完成。当前同一 `PointXYZ + direct indexed indices_ + float xyz AoS` production
边界下，`getDistances` dense store 和 `select` compressed error store 两个明确标量写回热点都已完成
RVV direct double store 形态替换。

Phase 060 结束时仍可继续的方向是 `identity-index-strided-load`，它会改变 load family（加载实现族），需要新 phase
计划、identity / shuffled 输入、同一 production boundary 内 RVV-vs-RVV detail A/B、asm、board repeated
和 Evidence Doctor。后续状态：Phase 070 已尝试并拒绝该实现族，production 已回到 Phase 060 gather-only load family。`point-type-expansion` 仍是更宽点型 / layout 范围，不由本阶段关闭。
