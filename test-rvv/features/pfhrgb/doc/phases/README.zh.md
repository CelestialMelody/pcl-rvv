# PFHRGB Phase Index

当前默认恢复入口：无同边界未阻塞优化动作。Phase 020 production integration loop（生产接入闭环）已完成，
`features/include/pcl/features/impl/pfhrgb.hpp` 的 exact-gated（精确门控）RVV 路径已采纳；
`doc-rvv/features/pfhrgb-RVV.zh.md` 已适用并记录接入后板卡数据。

| phase | status | plan | result | default recovery |
| --- | --- | --- | --- | --- |
| 000-current-state-and-scaffold | completed / historical evidence | `000-current-state-and-scaffold/plan.zh.md` | `000-current-state-and-scaffold/result.zh.md` | 原始 scaffold 关闭；当前 board summary 已由 Phase 030/040 rerun 覆盖。 |
| 010-public-with-candidate-diagnostic | completed / historical baseline | `010-public-with-candidate-diagnostic/plan.zh.md` | `010-public-with-candidate-diagnostic/result.zh.md` | public-with-candidate 仍稳定正向，但当前数值以 Phase 030/040 rerun 为准。 |
| 020-pi1-production-integration-plan | completed / adopted production behavior | `020-pi1-production-integration-plan/plan.zh.md` | `020-pi1-production-integration-plan/result.zh.md` | 当前 exact `PointXYZRGBNormal` production-public 5-run median `1.27x`。 |
| 030-staging-reuse-ablation | completed / positive production-shaped diagnostic | `030-staging-reuse-ablation/plan.zh.md` | `030-staging-reuse-ablation/result.zh.md` | reuse public-shaped diagnostic 5-run median `1.25x`；Doctor Errors 只阻塞 component/helper 收益外推。 |
| 040-structure-parity-doc-suite | completed / historical pre-PI1 | `040-structure-parity-doc-suite/plan.zh.md` | `040-structure-parity-doc-suite/result.zh.md` | topic-local doc suite 已拆出；production truth 以后续 Phase 020 result 为准。 |

## 文档归属

Phase plan/result 保存阶段探索和停止条件；`optimization-matrix.zh.md` 保存 candidate 到证据状态；
`../optimization-roadmap.zh.md` 保存后续候选搜索空间；`../pfhrgb-evaluation.zh.md` 保存当前 EvidenceDecision
和 Traceability Map；README 只做导航与证据白名单。
