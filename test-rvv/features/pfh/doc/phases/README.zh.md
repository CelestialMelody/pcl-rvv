# PFH Phase Index

| phase | status | plan | result | default recovery |
| --- | --- | --- | --- | --- |
| 000-current-state-and-pfh-scaffold | done | `000-current-state-and-pfh-scaffold/plan.zh.md` | `000-current-state-and-pfh-scaffold/result.zh.md` | Baseline neutral；继续 pair feature candidate。 |
| 010-pair-feature-batch-diagnostic | done | `010-pair-feature-batch-diagnostic/plan.zh.md` | `010-pair-feature-batch-diagnostic/result.zh.md` | Candidate diagnostic positive；进入 PI1 计划。 |
| 020-production-integration-plan | historical_plan | `020-production-integration-plan/plan.zh.md` | not_started | Phase 030 后 direct AoS 取代 staged 作为首选生产探针。 |
| 030-direct-aos-pair-feature-diagnostic | done | `030-direct-aos-pair-feature-diagnostic/plan.zh.md` | `030-direct-aos-pair-feature-diagnostic/result.zh.md` | Direct AoS 5-run `2.86x-2.90x`，强于 staged；继续 Phase 040。 |
| 040-direct-aos-production-probe | done_adopted | `040-direct-aos-production-probe/plan.zh.md` | `040-direct-aos-production-probe/result.zh.md` | 接入 exact `PointNormal -> PointNormal` direct AoS production probe；production-public 5-run `1.92x-1.94x`，用户已确认采纳。 |
| 050-production-closeout-doc-rvv | done | `050-production-closeout-doc-rvv/plan.zh.md` | `050-production-closeout-doc-rvv/result.zh.md` | 已创建 `doc-rvv/features/pfh-RVV.zh.md`，并继续 Phase 060。 |
| 060-pointxyz-normal-production-expansion | done_adopted | `060-pointxyz-normal-production-expansion/plan.zh.md` | `060-pointxyz-normal-production-expansion/result.zh.md` | exact `PointXYZ -> Normal` 接入后 production-public 5-run `1.84x-1.87x`，按用户策略采纳；当前无建议继续同轮扩大的高价值方向。 |

当前默认恢复入口：Phase 060 closeout。若后续继续，优先新建独立 phase 评估 PointXYZ-like /
Normal-like 泛型 traits 或 cache / OMP path；不能把两个 exact 组合的证据外推为完整模板泛型结论。
