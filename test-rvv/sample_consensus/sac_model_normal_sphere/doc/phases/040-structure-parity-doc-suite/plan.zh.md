# Phase 040: topic-local doc suite 结构对齐计划

## 阶段意图和边界

Phase 000-030 已经形成多阶段 diagnostic（诊断）、board summary（板卡摘要）、Evidence Doctor（证据体检）和 evidence registry（证据登记表）。当前 README、evaluation、roadmap 和 phase 文档能恢复工作，但 testing overview、correctness tests、benchmark/evidence、optimization evidence 和 test-support code map 仍合并在少数文档与源码注释里，reviewer 需要跨文件跳转才能判断 target、case、helper 和证据边界。

本阶段只补齐 topic-local doc suite（主题本地文档套件）和恢复入口：

- 新增 `doc/testing-overview.zh.md`
- 新增 `doc/correctness-tests.zh.md`
- 新增 `doc/benchmark-and-evidence.zh.md`
- 新增 `doc/optimization-evidence.zh.md`
- 新增 `doc/test-support-code-map.zh.md`
- 更新 README、phase index、optimization matrix、roadmap、evaluation 和筛选复筛状态表

不修改 production 源码，不新增 RVV candidate，不重跑 board benchmark。

## 当前状态清单

| area | current shape scan | quality bar / supplemental calibration | planned decision |
| --- | --- | --- | --- |
| README navigation | 已有 `README.zh.md`，但缺 Phase 030/040 和 doc-suite 阅读路径。 | `topic_navigation` 应列当前结论、阅读路径、常用命令和提交边界。 | adopted by update |
| testing overview | 当前缺独立 role 文档，target 粒度信息散在 Makefile、README 和 phase result。 | 复杂 topic 有多 Make target、board summary 和 Evidence Doctor，应拆出 overview。 | adopted by new doc |
| correctness tests | gtest 语义在源码注释中可读，但没有稳定 TEST 字典。 | 每个 TEST 要说明输入、被测路径、断言和不能证明的范围。 | adopted by new doc |
| benchmark/evidence | summary / manifest / doctor / registry 路径存在，README 只列部分命令。 | bench role 需要 CLI、case label、board smoke、doctor、registry 和提交边界。 | adopted by new doc |
| optimization evidence | 候选状态在 phase result 和 matrix 中存在，但缺按 candidate family 聚合的证据索引。 | 多候选 topic 应拆出 adopted / attempted / deferred / not_applicable 映射。 | adopted by new doc |
| test-support code map | `src/` + `include/` + `include/impl/` 已符合当前配置，但调用图和职责边界只在源码文件头里。 | 复杂 topic 应有 helper、bench、script、output 的 Traceability Map。 | adopted by new doc |
| production topic doc | 不存在。 | 没有 adopted production behavior 或 PI5 用户确认时，`doc-rvv` 为 not_applicable。 | not_applicable with evidence |
| artifact tracking | 当前 doc suite 新文件尚不存在。 | README / evaluation / phase result 引用的新增文件必须存在并进入当前 topic artifact boundary。 | adopted by final scan |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| structure-parity-doc-suite | not_applicable | all current diagnostic scopes | topic-local docs only | no new correctness target | no new bench target | reuse Phase 000-030 summaries | no new asm | reuse Phase 000-030 doctors | planned | create role docs and refresh references |

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| 补 topic-local role docs | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` | 每份文档说明职责、路径、证据边界和不能证明的范围。 |
| 更新入口引用 | README、evaluation、phase README、matrix、roadmap、筛选复筛表 | Phase 030/040 和 doc-suite 路径可从入口恢复。 |
| 记录 result | `doc/phases/040-structure-parity-doc-suite/result.zh.md` | 写入 doc_suite_role_inventory、target granularity audit、artifact tracking 和停止条件。 |
| 验证 | `make -C ... evidence_status`、`git diff --check`、路径限定 `git status --untracked-files=all` | registry fresh；文档引用的新增文件出现在当前 topic artifact boundary。 |

## Evidence Doctor 和 registry 规则

本阶段不生成新的性能证据，不需要新增 board summary。Evidence Doctor 输入继续引用 Phase 000-030：

- `doc/phases/000-normal-sphere-count-select-diagnostic/board-evidence-doctor.md`
- `doc/phases/010-vcompress-select-ablation/board-evidence-doctor.md`
- `doc/phases/020-getdistances-dense-store-audit/board-evidence-doctor.md`
- `doc/phases/030-rgb-rgba-point-type-expansion/board-evidence-doctor.md`

完成时仍运行 `make -C test-rvv/sample_consensus/sac_model_normal_sphere evidence_status`，确认新增引用没有把已登记证据变成 stale。

## 继续 / 停止条件

若 doc-suite role 文档、Phase 030 result、matrix、roadmap、evaluation、README 和筛选复筛表同步完成，且 verification 通过，则当前 topic 在未授权 production 修改前没有剩余的 topic-local high-priority unblocked action。此时停止条件是：继续需要用户明确授权 production integration loop，或另行指定自定义点型 / `Scalar=double` / normal 点型扩展范围。
