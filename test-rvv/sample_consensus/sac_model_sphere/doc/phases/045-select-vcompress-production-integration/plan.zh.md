# Phase 045: selectWithinDistance vcompress 生产接入计划

## 阶段意图和边界

本阶段把 Phase 040 的 `vcompress` 候选接入真实 production `selectWithinDistanceRVV`，并重新执行生产接入闭环
PI2-PI5。接入范围只限 `SampleConsensusModelSphere<PointT>::selectWithinDistance` 的 RVV helper 内部输出组织：
RVV 仍负责 indexed xyz gather（按 `indices_` 离散加载点坐标）、平方距离和球壳 mask（掩码），新变化是用
`vcompress` 压缩命中的 index 和平方距离，再用标量尾段只对命中元素执行精确 `sqrt` 误差写回。

本阶段不修改 public API（公开接口），不接入 `getDistancesToModel`，不扩大到其它 row source、其它 `Scalar`
或新的点型性能结论。若接入后真实 public entry 板卡结果仍正向，按用户偏好采纳并刷新正式 `doc-rvv`；若不正向，
保留证据并等待用户确认是否回滚生产补丁。

## 当前状态

| 对象 | 状态 |
| --- | --- |
| Phase 020 production baseline | `selectWithinDistanceRVV` 已采纳，public entry 5-run median `1.5020x`。 |
| Phase 040 candidate | test-only `vcompress` vs current production select 的 RVV-vs-RVV median `1.3020x`，但存在 mixed-boundary Warning。 |
| correctness | Phase 040 `run_test_compare` 已通过 Std/RVV 各 5 个测试。 |
| asm | Phase 040 候选独立符号内 RVV instruction count 为 28，包含 2 条 `vcompress.vm`。 |
| evidence gap | 缺真实 production public entry 接入后 repeated board 和 Evidence Doctor。 |

## 生产接入边界

| question | answer |
| --- | --- |
| evidence role | production direct（真实生产入口直连证据） |
| A/B boundary | Std build public `selectWithinDistance` vs RVV build public `selectWithinDistance` |
| 当前决策问题 | RVV-vs-scalar 与 implementation-family adoption after integration |
| diagnostic 是否可外推到 production | Phase 040 只作为升级理由；本阶段必须用接入后的 production direct 证据决策。 |
| comparison-boundary / baseline mismatch 风险 | Phase 045 的主证据不应存在 helper/public mismatch；若 doctor 仍报边界 mismatch，降级并暂停。 |
| 弱 / 负 / 中性 / 不稳定时是否允许继续采纳 | 不允许自动采纳；保留 patch，报告证据，等待用户确认回滚或继续诊断。 |
| clean adoption 是否需要同一 production boundary 内证据 | 需要，本阶段通过真实 public entry 的接入后 repeated board 闭合。 |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| PI2 production patch | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` | `selectWithinDistanceRVV` 使用 `vcompress` 压缩命中 index 和平方距离；fallback gate 不扩大。 |
| PI3 correctness | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare` | Std/RVV 构建通过，public helper、Standard helper 和 vcompress candidate 语义一致。 |
| PI4 asm | `make -C test-rvv/sample_consensus/sac_model_sphere clean_bench_rvv dump_bench_rvv` | production `selectWithinDistanceRVV` 符号内出现 `vcompress.vm`，且 RVV 指令归属闭合。 |
| PI4 board repeated | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_production_vcompress_repeated_board_evidence` | 5-run 真实 public entry 板卡数据生成。 |
| PI4 doctor / registry | `make -C test-rvv/sample_consensus/sac_model_sphere record_production_vcompress_board_evidence_state` | manifest、doctor JSON/Markdown 和 registry entry 生成。 |
| PI5 EvidenceDecision | phase result、matrix、roadmap、evaluation、正式 `doc-rvv` | 若 public `selectWithinDistance` 稳定正向且 doctor select 行无阻塞 Error，按用户偏好采纳并刷新文档。 |

## 板卡预算和决策桶

使用 5-run repeated board，`BENCH_ARGS=65536 200`，warmup 为 5。public `selectWithinDistance`
median 明显大于 1 且所有 run 方向正向时判为 positive；若接近 1、跨 1 或 doctor 对 select 行报阻塞 Error，
标为 unstable / blocked，不自动采纳。预算耗尽后不无限复跑。

## 文档更新清单

阶段结束后更新：

- `doc/phases/045-select-vcompress-production-integration/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/sac_model_sphere-evaluation.zh.md`
- `doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md`，仅在接入后证据正向并采纳时刷新
