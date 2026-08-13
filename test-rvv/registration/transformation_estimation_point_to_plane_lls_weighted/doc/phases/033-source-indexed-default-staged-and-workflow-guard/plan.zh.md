# Phase 033 Plan：source-indexed default staged and workflow guard

## 阶段意图和边界

本阶段把 Phase 032 的结论落成当前生产默认路径和可复用 agent 规则。目标是：

- source-indexed production 默认优先使用 staged-gather / compressed-tail RVV helper。
- `block-fused-abcd-ilp` 保留为显式 probe / detail A/B helper，不再作为默认优先 dispatch。
- agent 资产补上“diagnostic negative 与 production public positive 分叉时必须补同边界 production detail A/B”的规则。
- topic-local 文档、长期 `doc-rvv` 文档和 Handoff 同步到当前真实状态。

本阶段不新增板卡采集，不扩大到 dual-indices、correspondences、`Scalar=double` 或其它 topic。

## 当前状态清单

| 证据 / 文件 | 当前事实 | 本阶段动作 |
| --- | --- | --- |
| Phase 030 `source_indexed_family_repeated` | test-rvv diagnostic 中 `block-fused-abcd-ilp` full estimate median `0.90x`，Doctor `3E / 6W / 13S`。 | 保留为 historical diagnostic / family-risk signal。 |
| Phase 031 production public probe | public Std/RVV 6 个代表 case median `1.54x` 到 `1.71x`，Doctor `0E / 9W / 12S`。 | 保留为 public RVV-vs-scalar positive evidence。 |
| Phase 032 production detail A/B | 同一 RVV binary 内 block-fused vs staged mixed / negative；`full pointxyz-to-pointxyzinormal 262144` median `0.721x`。 | 作为默认路径退回 staged 的主要证据。 |
| production selector | `buildPointToPlaneLLSWeightedSourceIndicesDefault` 和 `estimatePointToPlaneLLSWeightedSourceIndicesRVV` 当前先试 block-fused，再试 staged。 | 改为 staged 默认；block-fused 只由显式 helper / bench probe 调用。 |
| topic docs | 多处仍写 block-fused bounded production candidate / staged rollback。 | 更新为 staged adopted/default，block-fused probe rejected for default。 |
| agent assets | 已有 family comparison 和同边界规则，但没有明确写“diagnostic negative 不能直接拒绝 production probe，也不能用 public Std/RVV positive 证明 RVV family 最优”。 | 补规则。 |

## 优化矩阵

| candidate family | row source | scope and entry | correctness / fallback target | board evidence | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- |
| full-cloud block-reduction + fused-abcd-ilp | full-cloud | production public overload | `run_test_production_direct_compare` | `production_dispatch_fused_abcd_ilp/summary.md` | adopted | 无本阶段动作。 |
| staged-gather / compressed-tail | source-indexed | production public overload default | `run_test_source_indices_compare`、`run_test_production_direct_compare` | `production_source_indices_staged_gather/summary.md` | restore adopted/default | 改 production selector 和文档。 |
| block-fused-abcd-ilp | source-indexed | explicit probe / detail helper only | `run_test_source_indices_compare`、detail helper tests | `production_source_indices_block_fused_abcd_ilp_probe/summary.md`、`production_source_indices_detail_ba/analyze_rvv_ba.md` | rejected for default / retained for probe | 保留 helper，不删 bench target。 |
| block-fused-abcd-ilp | dual-indices / correspondences | test-rvv diagnostic | existing family tests | dual/correspondence diagnostic summaries | no-production diagnostic | 无本阶段动作。 |

## 实现和测试动作

| item | 动作 | 完成判据 |
| --- | --- | --- |
| P1 phase plan | 建立本计划，冻结本阶段边界。 | plan 在代码和文档修改前存在。 |
| P2 production default | 将 source-indexed default selector 和 public RVV wrapper 改为 staged-gather 优先；保留 block-fused helper。 | 源码显示默认路径不再调用 block-fused；显式 helper 仍可被 detail A/B bench 使用。 |
| P3 correctness guard | 增加或更新 gtest，证明默认 normal-equation 与 staged helper 对齐。 | RVV 构建下测试命中 staged default；std 构建仍保持 fallback 语义。 |
| P4 agent assets | 更新 `rvv-test` 相关 reference。 | 规则覆盖 diagnostic negative / production public positive 分叉和同边界 production detail A/B。 |
| P5 docs refresh | 更新 README、evaluation、optimization evidence、testing / benchmark / correctness / code map、`doc-rvv` 和 phase index。 | 文档不再把 block-fused 写成默认优先 dispatch。 |
| P6 validation | 运行 py_compile、QEMU gtest、dry-run board target 和 diff check。 | 通过；如失败，result 记录失败和处理。 |
| P7 handoff | 刷新 current handoff。 | 下一轮能从 Phase 033 恢复。 |

## Evidence Doctor 和 registry 规则

本阶段不新增 board summary，不重新运行 Evidence Doctor。使用的 Evidence Doctor 结果仍来自：

- `production_source_indices_staged_gather/evidence_doctor.md`：0 Errors / 7 Warnings / 6 Suggestions。
- `production_source_indices_block_fused_abcd_ilp_probe/evidence_doctor.md`：0 Errors / 9 Warnings / 12 Suggestions。
- `source_indexed_family_repeated/evidence_doctor.md`：3 Errors / 6 Warnings / 13 Suggestions。

这些结果的角色要分层：staged summary 是当前默认 public Std/RVV 性能依据；block-fused probe summary 是历史 probe positive；detail A/B 是 family selection negative。

## 板卡复跑预算和决策桶

本阶段不复跑板卡。Phase 032 已完成 5-run production detail A/B，decision bucket 为 mixed / negative。若后续要重新挑战 block-fused 默认路径，必须新建阶段并至少补 extended production detail A/B、source-indexed-specific asm attribution 和 binary identity。

## 继续 / 停止条件

本阶段完成后，如果 production selector、gtest、agent 资产、topic 文档、长期文档和 Handoff 均同步，默认进入 `ready_for_review`。若 QEMU gtest 失败，停止为 blocked 并保留 staged rollback patch 或修复测试。

## 文档更新清单

- `doc/phases/README.zh.md`
- 本阶段 `result.zh.md`
- `README.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- `doc/transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md`
- `doc-rvv/registration/transformation_estimation_point_to_plane_lls_weighted-RVV.zh.md`
- current handoff Markdown / YAML

## roadmap 同步动作

Phase 033 不新增候选 family。它把 Phase 032 的 rejected-for-default 结论写回当前默认路径。roadmap / 后续恢复条件为：只有当新的同边界 production detail A/B、asm attribution 和 extended run 同时转正时，才重新考虑 block-fused source-indexed 默认路径。
