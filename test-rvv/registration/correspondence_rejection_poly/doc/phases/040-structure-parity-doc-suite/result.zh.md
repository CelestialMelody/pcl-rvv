# Phase 040 文档套件结构对齐结果

## 当前结论

Phase 040 已完成 topic-local doc suite（主题本地文档套件）结构对齐。本阶段只修改当前 topic 的 README、`doc/` 文档、phase 文档、optimization roadmap（优化路线图）、optimization matrix（优化矩阵）和 current Handoff（当前交接数据包）。production（生产源码）保持只读，目标生产文件无本 topic diff。

EvidenceDecision（证据决策）保持 `rollback/no-production`。本阶段没有生成新的性能数据；生产接入判断继续使用 Phase 030 的 board production-direct repeated summary（真实生产入口板卡重复摘要）和 Evidence Doctor（证据体检）。

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| 040-A 更新 README | done | `README.zh.md` 新增“先读哪份文档”、目录分工、当前可提交证据和 `doc-rvv` 适用性 | reviewer 可从入口文档定位当前结论和证据边界 |
| 040-B 扩展测试文档 | done | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md` | deterministic corpus（确定性样本集）和 seeded random stress（固定种子随机压力样本）已分层说明 |
| 040-C 扩展 bench / evidence 文档 | done | `doc/benchmark-and-evidence.zh.md` | case-filter、计时边界、checksum、Evidence Doctor、registry 和提交边界已补齐 |
| 040-D 扩展优化证据和代码地图 | done | `doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` | candidate、helper、script、summary、doctor 和 production 边界可互相定位 |
| 040-E 扩展 evaluation 和 phase 索引 | done | `doc/correspondence_rejection_poly-evaluation.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` | no-production 诊断证据链、文档归属矩阵和 Phase 040 状态已同步 |
| 040-F 更新 roadmap 和 Handoff | done | `doc/optimization-roadmap.zh.md`、`tmp/rvv-work-logs/registration/correspondence_rejection_poly/current-handoff/` | 旧 `ready_for_review` 停止位已重新验证；Phase 040 完成后恢复到 review 边界 |

## Doc Suite 审计表

本表使用 `.agents/skills/rvv-documentation/references/doc-suite-quality-bar.zh.md` 作为 canonical quality bar（规范质量门）。成熟 sibling topic（同模块相邻主题）只作为 optional calibration（可选校准样例），本阶段没有复制任何 sibling 的算法、数值、phase 名或 production 结论。

| area | current shape scan | quality bar / optional calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已有当前结论和命令；本阶段补足阅读路径、目录分工和证据提交边界 | README 应包含当前结论、先读路径、目录分工、常用命令、可提交证据和默认排除产物 | adopted | `README.zh.md` | none |
| `testing-overview` | 已有覆盖矩阵；本阶段补测试类型定义、运行入口、输入数据总览和证据白名单 | 测试总览应说明 test / bench / board / QEMU / production direct 的入口分类和边界 | adopted | `doc/testing-overview.zh.md` | none |
| `correctness-tests` | 已列 8 个 gtest；本阶段补文件分工、共同断言、证明范围和随机样本策略 | 每个 TEST 或测试族应写输入、被测路径、断言、证明范围和验证命令 | adopted | `doc/correctness-tests.zh.md` | none |
| `benchmark-and-evidence` | 已列 case 和 board 结果；本阶段补 target 字典、计时边界、checksum、doctor 和提交边界 | bench 文档应说明 CLI、case-filter、summary / manifest / doctor、registry 和 QEMU / board 边界 | adopted | `doc/benchmark-and-evidence.zh.md` | none |
| `optimization-evidence` | 已列候选；本阶段补 current decision、标量 / RVV 差异和代码级证据索引 | 候选证据应映射到代码、target、board、asm、doctor 和 decision | adopted | `doc/optimization-evidence.zh.md` | none |
| `test-support-code-map` | 已有目录和 helper 表；本阶段补总调用图、script / output、production 边界和拆分触发条件 | 代码地图应能定位聚合头、internal helper、src、script、production helper 和 evidence output | adopted | `doc/test-support-code-map.zh.md` | none |
| evaluation | 已有标量流程和 Traceability Map；本阶段补 public entry 输出语义、文档归属矩阵和诊断证据链 | evaluation 是 no-production 诊断证据链和生产接入判断主归属 | adopted | `doc/correspondence_rejection_poly-evaluation.zh.md` | none |
| long-term `doc-rvv` | 当前不存在 | 只有 adopted production behavior（已采用生产行为）、production patch（生产补丁）或 PI5 生产证据闭环通过后适用 | not_applicable with evidence | production direct board negative；target production diff clean | none |
| phase index / result | 000-030 已存在；本阶段新增 040 | phase index 和 result 应记录 doc-suite parity 审计、停止条件和恢复动作 | adopted | `doc/phases/README.zh.md`、本文件 | none |
| artifact tracking | Phase 040 执行时 topic 目录整体为待提交产物 | 被文档引用的文件必须存在，并标为 tracked / to-be-staged / local-only / excluded | adopted | `git status --short --untracked-files=all -- test-rvv/registration/correspondence_rejection_poly tmp/rvv-work-logs/registration/correspondence_rejection_poly` | commit phase 按 topic-local / evidence / docs 分类提交 |

## Evidence Doctor 处理

本阶段没有重跑 bench。当前证据体检结果沿用已登记摘要：

| 输入 | 结果 | 处理 |
| --- | --- | --- |
| `log/qemu/production_direct/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=0 | 只支持 QEMU log-shape（日志形状）和 manifest 合同 |
| `log/board/production_direct_repeated/evidence_doctor.md` | Errors=2，Warnings=0，Suggestions=0 | production adoption 失败；生产补丁保持回滚 |
| `log/evidence_registry.json` | `evidence registry check: fresh` | 证据文件状态与登记一致 |

## Artifact Tracking 和 Dirty Isolation

Phase 040 执行时，路径限定扫描显示 `test-rvv/registration/correspondence_rejection_poly/**` 是待提交 topic 资产。它们属于当前 topic 的 commit phase 候选。`tmp/rvv-work-logs/registration/correspondence_rejection_poly/**` 是 local-only Handoff，默认不提交。

本阶段未触碰无关 dirty paths，也未修改 production。无关 `.agents/**`、其它 topic 和 `registration/include/pcl/registration/impl/icp.hpp` 继续隔离。

## 验证

| 命令 | 结果 |
| --- | --- |
| `git diff --check -- test-rvv/registration/correspondence_rejection_poly tmp/rvv-work-logs/registration/correspondence_rejection_poly` | pass |
| `git status --short --untracked-files=all -- test-rvv/registration/correspondence_rejection_poly tmp/rvv-work-logs/registration/correspondence_rejection_poly` | pass as artifact scan；显示 topic-local files 为待提交产物 |
| `python3 test-rvv/script/evidence_registry.py check --registry test-rvv/registration/correspondence_rejection_poly/log/evidence_registry.json` | pass；`evidence registry check: fresh` |
| `rg` writing-style trigger check | initial hit in historical Handoff / Phase 030 text，Phase 030 text 已改写；Handoff 在本阶段重写 |

## 继续 / 停止决定

Phase 040 的 doc-suite 审计已闭合，没有 `phase_deferred + unblocked` 的 topic-local 文档缺口。当前 production decision（生产接入判断）仍是 no-production，且 production 文件无本 topic diff。

next_phase_default：`ready_for_review`。

可选后续性能探索应另开 Phase 050 profile / ablation plan，先解释完整 public entry 中 random sampling（随机采样）、edge staging（边暂存）、histogram / Otsu 和输出 append 的成本占比，再提出新候选。该方向需要用户明确选择，不应默认恢复 Phase 030 生产补丁。
