# Phase 050 结果：dual-indices / correspondences audit

## 执行范围

本阶段只审计剩余两条 row source policy（行来源策略）是否值得从 test-only diagnostic（测试专用诊断）进入 production integration loop（生产接入闭环）：

- `dual-indices-cloud-pair`
- `correspondence-pair`

本阶段不扩大到 `Scalar=double`，也不把 ordered-cloud-pair 或 source-indexed-cloud-pair 的 production 结论外推到其它 row source。

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| dual-indices board repeated diagnostic | done | `log/board/dual_indices_cloud_pair_repeated/summary.md` | same-boundary fused Std/RVV median 为 4K `1.801x`、64K `1.683x`、256K `1.425x`，overall decision bucket 为 `positive`。 |
| correspondence board repeated diagnostic | done | `log/board/correspondence_pair_repeated/summary.md` | same-boundary fused Std/RVV median 为 4K `2.174x`、64K `1.905x`、256K `1.772x`，overall decision bucket 为 `positive`。 |
| mixed-boundary cross-check | done | 两份 summary 的 public baseline vs fused RVV 表 | dual-indices median 为 4K `4.729x`、64K `4.321x`、256K `3.934x`；correspondence median 为 4K `8.659x`、64K `8.180x`、256K `7.824x`。两者都只说明值得进入下一阶段，不替代 production direct。 |
| Evidence Doctor | done | `log/board/dual_indices_cloud_pair_repeated/evidence_doctor.md`、`log/board/correspondence_pair_repeated/evidence_doctor.md` | Errors=0；dual-indices Warnings=3，correspondence Warnings=7。warning 主要是 public build sanity 命名角色和长尾方差，已按诊断边界解释。 |
| candidate frontier update | done | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | dual-indices 与 correspondences 从纯 `planned` 升级为已验证的生产接入候选。 |

## EvidenceDecision

`dual-indices-cloud-pair` 和 `correspondence-pair` 都达到 `partial-production-candidate`。这说明 test-support RVV candidate、row semantics（行语义）、board repeated 和 Evidence Doctor 已经支持进入生产接入闭环，但本阶段本身仍只是 audit，不应直接把 production direct 写成已闭合。

## 阶段反思和路线图更新

双索引和对应关系两条线都表现为正向：双 gather 与 query/match gather 的融合累加都有收益，且没有出现 Evidence Doctor Error。下一步不再是继续扩 audit，而是新建生产接入阶段，把 public overload 接上 RVV fast path，再刷新 correctness、asm、board repeated 和长期文档。

## 继续 / 停止决定

本阶段 audit 已完成。默认下一 phase 是 `060-dual-indices-correspondences-production-integration`，先补 production patch，再跑对应的 correctness、board direct 和证据刷新。
