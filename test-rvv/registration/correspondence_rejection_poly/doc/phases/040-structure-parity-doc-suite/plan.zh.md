# Phase 040 文档套件结构对齐计划

## 阶段意图和边界

本阶段只完善 `registration/correspondence_rejection_poly` 的 topic-local doc suite（主题本地文档套件）和当前 Handoff（交接数据包）。目标是让 reviewer 不依赖对话上下文，也能从 README、evaluation（函数级评估）、测试说明、bench（性能测试）说明、优化证据、代码地图和 phase index（阶段索引）恢复当前 `rollback/no-production` 结论。

本阶段不修改 production（生产源码），不重新应用 Phase 030 的生产探针补丁，不运行新的 production-direct board bench（真实生产入口板卡性能测试）。目标生产文件只读复核：

- `registration/include/pcl/registration/correspondence_rejection_poly.h`
- `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`

## 当前状态清单

| 对象 | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| production diff | clean | `git diff -- registration/include/pcl/registration/correspondence_rejection_poly.h registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp` 无输出 |
| EvidenceDecision（证据决策） | `rollback/no-production` | `doc/phases/030-pi1-production-integration-plan/result.zh.md`、`doc/correspondence_rejection_poly-evaluation.zh.md` |
| production-direct board summary | negative | `log/board/production_direct_repeated/summary.md` |
| Evidence Doctor（证据体检） | board production-direct Errors=2 | `log/board/production_direct_repeated/evidence_doctor.md` |
| doc-rvv 适用性 | not_applicable | `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md` 当前不存在 |
| topic-local doc suite | present but thin | README、testing overview、correctness、benchmark/evidence、optimization evidence、code map、roadmap、phase docs 已存在 |

## Doc Suite 审计表

本阶段使用 `.agents/skills/rvv-documentation/references/doc-suite-quality-bar.zh.md` 作为规范质量门。成熟 sibling topic（同模块相邻主题）只作为 optional calibration（可选校准样例），不作为规范来源。

| area | current shape scan | quality bar / optional calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 有当前结论、命令和文档导航，但缺少清晰的“先读哪份文档”和提交边界分组 | README 应给当前结论、先读路径、目录分工、命令、可提交证据和默认排除产物 | adopted by this phase | 无 blocker | 补“先读哪份文档”“目录分工”“提交边界” |
| `testing-overview` | 覆盖矩阵较短 | 需要测试类型定义、运行入口分类、细粒度 target、输入数据总览和证据白名单 | adopted by this phase | 无 blocker | 扩展测试体系说明 |
| `correctness-tests` | 已列 8 个 gtest，但缺少测试文件分工和边界 / 随机样本策略细节 | 每个 TEST 或测试族应写输入、被测路径、断言、证明范围和验证命令 | adopted by this phase | 无 blocker | 补测试族字典和输入策略 |
| `benchmark-and-evidence` | 已列 case 和 evidence，但 registry / artifact tracking / production-direct target 边界说明可更清晰 | 需要 CLI、case-filter、计时边界、checksum、QEMU / board、doctor、asm、复现命令和提交边界 | adopted by this phase | 无 blocker | 补 target 字典、提交白名单和 doctor 处理 |
| `optimization-evidence` | 候选表存在，但 current decision 与 production 边界需要更直接 | 需要 adopted / attempted / rejected / deferred 候选到代码、target、board、asm 和 doctor 的映射 | adopted by this phase | 无 blocker | 扩展候选证据索引和 no-production 边界 |
| `test-support-code-map` | 有目录和 helper 表，但调用图、source layout 和拆分判据偏薄 | 需要聚合入口、internal helper、src、script、production 对照、调用图和拆分审计 | adopted by this phase | 无 blocker | 补总调用图、职责边界和拆分阈值 |
| evaluation | 有标量流程、Traceability Map 和结论，但文档归属矩阵较薄 | evaluation 是 no-production 诊断证据链、Traceability Map 和生产接入判断主归属 | adopted by this phase | 无 blocker | 补文档归属矩阵和诊断证据链 |
| long-term `doc-rvv` | absent | 只有 adopted production behavior、production patch 或 PI5 生产证据闭环通过后适用 | not_applicable with evidence | Phase 040 当时 EvidenceDecision 是 `rollback/no-production`；Phase 050 后续 replay 仍为 negative，用户已确认回滚 | README / evaluation / Handoff 明确不创建 |
| phase index / result | phase index 有 000-030；无 040 | phase index 和 result 应记录本次 doc-suite parity 审计、继续 / 停止条件和恢复动作 | adopted by this phase | 无 blocker | 新增 040 result，并更新 phase README |
| artifact tracking | topic 目录未跟踪文件较多 | 被 README / evaluation / roadmap / Handoff 引用的 doc-suite 文件必须存在，并标为 topic artifact / review-required | adopted by this phase | 无 blocker | 路径限定扫描后回填 Handoff |

## 实现和文档动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 040-A 更新 README | `README.zh.md` | 读者可先读当前结论，再按文档分工定位测试、bench、证据和 Handoff |
| 040-B 扩展测试文档 | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md` | deterministic corpus（确定性样本集）和 seeded random stress（固定种子随机压力样本）都被解释 |
| 040-C 扩展 bench / evidence 文档 | `doc/benchmark-and-evidence.zh.md` | production-direct target 边界、Evidence Doctor、registry 和提交白名单清晰 |
| 040-D 扩展优化证据和代码地图 | `doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` | 候选、helper、script、summary 和 production 边界可互相定位 |
| 040-E 扩展 evaluation 和 phase 索引 | `doc/correspondence_rejection_poly-evaluation.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` | no-production 诊断证据链、文档归属矩阵、phase 040 状态可恢复 |
| 040-F 更新 roadmap 和 Handoff | `doc/optimization-roadmap.zh.md`、`current-handoff/*` | 旧 `ready_for_review` 停止位重新验证；本阶段完成后给出新的 continue / stop decision |

## Evidence Doctor 和 Registry 规则

本阶段不生成新的性能数据。Evidence Doctor 结果沿用已登记摘要：

- `log/qemu/production_direct/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=0，只支持 QEMU log-shape（日志形状）证据。
- `log/board/production_direct_repeated/evidence_doctor.md`：Errors=2，Warnings=0，Suggestions=0，支撑 `rollback/no-production`。

阶段结束前运行或复核：

- `git diff --check -- test-rvv/registration/correspondence_rejection_poly tmp/rvv-work-logs/registration/correspondence_rejection_poly`
- `git status --short --untracked-files=all -- test-rvv/registration/correspondence_rejection_poly tmp/rvv-work-logs/registration/correspondence_rejection_poly`
- 如 registry 工具可用，运行 topic 等价 evidence freshness check；否则在 Handoff 中写人工检查边界。

## 继续 / 停止条件

本阶段完成后，如果 doc-suite 审计表全部为 `adopted` 或 `not_applicable with evidence`，且没有 topic-local 文档、artifact tracking 或 registry 缺口，默认恢复动作可以回到 `ready_for_review`。

如果文档仍存在未阻塞缺口，本阶段 result 必须列出 `phase_deferred + unblocked` 项，并把 `next_phase_default` 指向具体补齐 phase。继续 profile / component ablation（组件消融）会生成新的性能假设，应由用户明确选择后另开阶段；它不能在本阶段直接修改 production。
