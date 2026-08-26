# moment_invariants phase index

## 当前恢复入口

当前默认恢复入口是 review / commit decision（审查或提交判断）。Phase 030/040 已把 `features/include/pcl/features/impl/moment_invariants.hpp` 接入 adopted production behavior（已采纳生产行为）：真实 `MomentInvariantsEstimation::computeFeature` 的 indexed neighbor moment accumulation（索引邻域矩累加）在 RVV 构建、dense xyz 单 float AoS（结构数组）点型、`PointOutT=pcl::MomentInvariants` 和邻域规模达到 16 时走 RVV；其它路径回退标量。

## 阶段表

| phase | 状态 | 主要证据 | 计划 | 结果 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| 000-current-state-and-diagnostic-plan | completed | helper-only board median `1.141x`；Evidence Doctor 0E/0W/2S | `000-current-state-and-diagnostic-plan/plan.zh.md` | `000-current-state-and-diagnostic-plan/result.zh.md` | 已进入 Phase 010 |
| 010-public-search-dilution-check | completed | public-search-shaped board median `1.031x`；Evidence Doctor 0E/0W/3S | `010-public-search-dilution-check/plan.zh.md` | `010-public-search-dilution-check/result.zh.md` | 已进入 Phase 020 |
| 020-doc-suite-closeout | completed | topic-local doc suite、registry fresh、verification pass | `020-doc-suite-closeout/plan.zh.md` | `020-doc-suite-closeout/result.zh.md` | 后续被用户授权生产探针覆盖 |
| 030-production-integration-probe | completed | `PointXYZ` production-public median `1.067x`，min `1.023x`，0/5 退化；Doctor 0E/0W/2S | `030-production-integration-probe/plan.zh.md` | `030-production-integration-probe/result.zh.md` | 已进入 Phase 040 |
| 040-pointxyz-like-production-expansion | completed | `PointXYZI` median `1.071x`、`PointXYZRGB` median `1.078x`、`PointXYZRGBA` median `1.078x`；三组 Doctor 均 0E/0W/2S | `040-pointxyz-like-production-expansion/plan.zh.md` | `040-pointxyz-like-production-expansion/result.zh.md` | review / commit decision |

## 文档归属

| 文档 | 职责 |
| --- | --- |
| `../moment_invariants-evaluation.zh.md` | EvidenceDecision（证据决策）、生产接入判断、Traceability Map（可追踪性地图）和 doc-suite parity audit（文档套件对齐审计）。 |
| `../optimization-roadmap.zh.md` | 候选搜索空间、默认恢复队列和后续扩展条件。 |
| `optimization-matrix.zh.md` | candidate × row source × point type × evidence 状态。 |
| 各 phase `plan.zh.md` / `result.zh.md` | 阶段计划、实际执行、Evidence Doctor 解释和继续 / 停止判断。 |
| `doc-rvv/features/moment_invariants-RVV.zh.md` | adopted production behavior 的长期维护文档。 |

## 早停检查

`ready_for_review_validity_check=pass`：当前 topic-local doc suite、phase result、optimization matrix、roadmap、production `doc-rvv`、summary/manifest/doctor registry 和 production asm target 已同步到 Phase 040 状态。仍未覆盖的 `PointXYZRGBNormal`、`PointXYZINormal`、自定义点型、full-cloud overload、其它输出类型、`Scalar=double` 和 search/KdTree 优化不属于本阶段必须继续的未阻塞动作；继续它们需要新 phase 冻结范围并重新补 correctness、asm、board 和 Evidence Doctor。
