# Phase 030 结果：row-source family carry-over audit

## 执行范围

本阶段先闭合 `source-indexed-cloud-pair`（源索引点云对）的同边界 candidate。`dual-indices-cloud-pair`（双索引点云对）和 `correspondence-pair`（对应关系点对）没有被本阶段合并进 production；它们仍需要独立 phase、双 gather / query-match 语义审计、正确性、反汇编和板卡证据。

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| source-indexed public 语义对拍 | done | `run_test_compare` 中 `SourceIndexedPublicMatchesFusedReference` | public iterator 标量链路与 fused source-indexed reference 在误差预算内一致。 |
| source-indexed RVV candidate 对拍 | done | `run_test_compare` 中 `SourceIndexedCandidateMatchesScalar` | RVV gather + target strided load 的同构 candidate 与标量 reference 一致。 |
| source-indexed board repeated diagnostic | done | `log/board/source_indexed_cloud_pair_repeated/summary.md` | same-boundary fused Std/RVV median 为 4K `1.917x`、64K `1.843x`、256K `1.785x`，decision bucket 为 `positive`。 |
| public baseline vs fused RVV 交叉检查 | done | 同一 summary 的 mixed-boundary 表 | median 为 4K `7.012x`、64K `9.031x`、256K `8.827x`；只支持进入 production integration loop，不替代 production direct。 |
| Evidence Doctor | done | `log/board/source_indexed_cloud_pair_repeated/evidence_doctor.md` | Errors=0、Warnings=4、Suggestions=0。3 个命名 warning 已按 diagnostic role 降级，4K group outlier 按 size 分开报告。 |
| dual-indices / correspondences | phase_deferred + unblocked | 本阶段未实现 | 不继承 source-indexed 或 ordered-cloud-pair 结论，默认进入后续独立 phase。 |

## EvidenceDecision

`source-indexed-cloud-pair` 的 Phase 030 结论为 `partial-production-candidate`。诊断证据说明同一 math family 在 source-indexed row source 上值得进入 Phase 040 production integration；它不能直接写成 production-ready，因为当时真实 public overload 还没有 RVV dispatch、fallback gtest、production symbol asm 和 production direct board repeated。

## 阶段反思和路线图更新

source-indexed 的 gather 成本没有抵消 fused accumulation 的收益；同边界 board B/A 稳定正向。因此 roadmap 将 `source_indexed_fused_accum` 从 `planned` 更新为 `adopted for PI1/040`。dual-indices 和 correspondences 的风险更高：前者需要两侧 gather，后者需要 query/match 语义和 correspondence 边界审计；它们保持 `phase_deferred + unblocked`，不能被本阶段关闭。

## 继续 / 停止决定

本阶段没有命中合法停止条件。默认继续到 `040-source-indexed-production-integration/plan.zh.md`，范围只允许接入 `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)`，并保持 dual-indices、correspondences 和 `Scalar=double` 标量 fallback。
