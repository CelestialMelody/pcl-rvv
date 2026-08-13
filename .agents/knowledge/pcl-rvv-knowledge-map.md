# PCL RVV 知识索引

## Purpose（用途）

本文件是给 Codex 和未来 `rvv-agent`（RVV 优化代理）使用的轻量读取索引。它只说明
PCL RVV 工作中各类知识、历史案例、可执行证据和当前源码状态放在哪里，
以及在上下文有限时应该如何按需读取。

不要把配置解析出的主题文档或测试资产内容复制到 `.agents/knowledge/`。
大型文档、测试、日志和实现文件只在当前任务需要时读取。

## Source Classes（来源类别）

| Class（稳定标签）                               | Location（位置）                                                                  | Role（作用）                                                                     | Default loading rule（默认读取规则）                                             |
| ----------------------------------------------- | --------------------------------------------------------------------------------- | -------------------------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| `repo_rules`（仓库规则）                      | `AGENTS.md`、`.agents/skills/`                                                | 持久仓库约定和可复用 agent workflow（代理工作流）。                              | 先读适用的`AGENTS.md`，再读匹配的 skill（技能）。                              |
| `role_entry_defaults`（角色入口默认值）       | `.agents/config/defaults.yaml`、`.agents/local/user-preferences.yaml`、`.agents/skills/rvv-workflow/references/short-prompt-entry.zh.md`、`worker-quality-gates.zh.md` | 短 prompt（提示词）启动时的 worker（执行者）、reviewer（审查者）和 workflow improvement（工作流改进）默认读取链；S0 / phase / current handoff 路径模板；worker 写文件前的轻量质量门禁。 | 用户只给角色、工作目录和目标时读取；worker 选中 topic 后按配置解析产物位置，并按质量门禁决定是否继续读取 documentation / testing / implementation 细则。 |
| `agent_preferences`（代理偏好）               | `.agents/config/defaults.yaml`、`.agents/local/user-preferences.yaml`                  | 可提交默认偏好、本机私有覆盖和 env var 名。                                      | S0 读取后冻结注释、文档、证据和工作日志偏好。                                      |
| `normative_rvv_knowledge`（通用 RVV 知识）    | `artifact_layout.reusable_rvv_knowledge_dir_template`                           | 跨 topic（主题）的 RVV 知识、数学函数向量化说明和可复用 RVV 约定。               | 需要通用 RVV 规则时，优先从配置解析出的目录选择窄文件读取。                      |
| `historical_topic_reports`（历史 topic 报告） | `artifact_layout.module_doc_dir_template`                                       | topic 文档、closeout（收尾记录）记录、实现说明和历史决策。                       | 只读当前模块或 topic 相关文档，并用当前源码复核。                                |
| `screening_and_queue_docs`（筛选和队列文档）  | `artifact_layout.screening_root_template`                                       | 模块筛选、候选队列、状态表和下一主题建议。                                       | 选择、恢复或复筛 topic 时读取。                                                  |
| `executable_evidence`（可执行证据）           | `artifact_layout.topic_test_dir_template`                                       | test（测试）、bench（性能测试）、QEMU、反汇编检查、board（板卡）脚本、日志、manifest、Evidence Doctor 和 evidence registry 入口。 | 只读取当前结论需要的具体测试、脚本、summary、registry 或日志。                  |
| `current_source_state`（当前源码状态）        | 当前 PCL 源码、当前 git diff、`__RVV10__` 路径                                  | 用于复核当前实现行为。                                                           | 下结论前必须用当前源码和当前 diff 复核文档与测试。                               |
| `migration_only`（仅迁移材料）                | local migration sources（本地迁移来源，可能不存在；例如已移入`tmp` 的历史材料） | 历史 prompt（提示词）材料和旧 skill（技能）草稿。                                | 正常 topic runtime（主题运行过程）不读取；只有用户要求迁移、追溯或审计时才读取。 |

## Recommended Retrieval Order（推荐读取顺序）

1. 读取 `AGENTS.md`，确认仓库级边界。
2. 读取 `.agents/config/defaults.yaml`；如果存在，读取 `.agents/local/user-preferences.yaml`。
3. 如果用户使用短 prompt，读取 `rvv-workflow/references/short-prompt-entry.zh.md`；worker 选中 topic 后再读取 `rvv-workflow/references/worker-quality-gates.zh.md`。
4. 选择匹配的 `.agents/skills/<skill-name>/SKILL.md`。测试、诊断、benchmark、消融和证据日志优先读取 `rvv-test`；
   短 prompt 继续已有 topic、恢复 phase plan/result 或存在未阻塞下一步时，先从
   `artifact_layout.phase_root_template`、`artifact_layout.phase_plan_template`、
   `artifact_layout.phase_result_template`、`artifact_layout.optimization_roadmap_template`
   和 `artifact_layout.optimization_matrix_template`
   解析阶段文档位置，再读取
   `rvv-test/references/optimization-phase-loop.zh.md`；
   registration topic 涉及变换估计、对应关系估计、row source 或法方程时，读取
   `rvv-test/references/registration-topic-evidence.zh.md`。
5. 读取本知识索引，决定下一步需要打开哪类证据。
6. 如果任务是选题或恢复工作，读取 `artifact_layout.screening_root_template` 解析出的状态和队列文档。
7. 如果任务需要通用 RVV 规则，读取 `artifact_layout.reusable_rvv_knowledge_dir_template` 解析出的相关窄文件；涉及模板点类型、traits、字段 offset、POD / standard-layout、或从具体点类型诊断扩展到 production 泛型入口时，优先读取 `artifact_layout.generic_point_type_strategy_doc_template` 解析出的文档。
8. 如果任务针对已有 topic，读取 `artifact_layout.module_doc_dir_template` 或 `artifact_layout.topic_doc_template` 解析出的对应 topic 文档。
9. 如果当前 topic 暴露出多种公开入口、indices、correspondences、weights、staging、
   policy、row source 或相似数据流分发问题，按“Historical Analogy Retrieval Pattern”
   做同模块历史类比检索。
10. 用当前源码和当前 git diff 复核文档结论。
11. 只在需要证明具体结论时，读取 `artifact_layout.topic_test_dir_template` 解析出的测试、bench、脚本或日志。

## Source Priority（来源优先级）

- `artifact_layout` 解析出的路径是 S0 run record（S0 运行记录）、phase docs（阶段文档）、optimization roadmap（优化路线图）、optimization matrix（优化矩阵）、current handoff（当前交接摘要）、topic 测试资产和 topic 文档位置的默认来源；若 local override 或 prompt override 改变路径，Handoff 必须记录覆盖范围。
- `artifact_publication` 是默认提交边界来源；raw logs、S0 run record 和 current handoff 不应仅因存在于工作区而进入提交，agent asset patch 必须与 topic 产物分开审查。
- 下结论前，用当前源码和当前 git diff 复核实现行为。
- `artifact_layout.reusable_rvv_knowledge_dir_template` 解析出的目录可以作为可复用 RVV 约定入口；如果它描述的代码已经变化，以当前源码为准。
- `artifact_layout.module_doc_dir_template` 解析出的目录记录历史案例和 closeout 结论，可用于定位旧结论，但不自动代表当前源码。
- `artifact_layout.topic_test_dir_template` 解析出的目录是可执行证据入口；结论取决于具体测试、构建参数、日志、目标硬件和时间。
- QEMU evidence（QEMU 证据）只支持 correctness（正确性）、log shape（日志形状）和 path/instruction coverage（路径或指令覆盖）判断，不能支撑真实性能结论。
- Board/target-hardware benchmark（板卡或目标硬件 benchmark）日志才能支撑性能结论。
- 如果 topic 有 `log/evidence_registry.json` 或等价状态文件，恢复或提交前优先用它判断是否存在人工复跑、未登记覆盖或 stale 文档；没有 registry 时按 `rvv-test` 规则人工列出检查路径。
- evidence logs（证据日志）默认 `summary-only`；raw logs（原始日志）只在明确授权时进入提交边界。

## What Not To Load By Default（默认不要读取什么）

- 不要全量读取配置解析出的文档根目录。
- 不要全量读取配置解析出的测试资产根目录。
- 不要默认读取生成的 `build/`、`output/`、`log/` 或 `.log` 文件，除非任务明确要求查看这些证据。
- 正常 RVV topic 工作中不要读取未提交的本地迁移材料。
- 不要把历史文档、旧日志或迁移记录当作比当前源码更新的事实。
- 不要把大型文档、日志、测试或源码片段复制进 `.agents/knowledge/`。

## Topic Work Retrieval Pattern（主题工作读取模式）

处理某个具体 topic 时：

1. 从用户请求或筛选队列中确认 module（模块）和 topic（主题）名称。
2. 读取匹配的 RVV skill，并且只读取当前任务需要的 skill references（参考文件）。
3. 如果任务涉及选题、恢复或复筛，打开 `artifact_layout.screening_root_template` 解析出的目录。
4. 如果存在对应 topic 文档，打开 `artifact_layout.module_doc_dir_template` 或 `artifact_layout.topic_doc_template` 解析出的窄文档。
5. 检查当前源码里的公开入口、helper、fallback 和 `__RVV10__` 分支。
6. 只为验证当前问题，读取 `artifact_layout.topic_test_dir_template` 解析出的必要测试、bench、脚本或日志。
7. closeout 或 production-candidate topic 要读取主题文档的“正确性与高效性证据链”；未接 production 的诊断结论要读取“诊断证据链”。
8. 结论中分开说明 correctness（正确性）、QEMU path evidence（QEMU 路径证据）、disassembly evidence（反汇编证据）、board performance（板卡性能）和 production decision（生产接入判断）。

## Historical Analogy Retrieval Pattern（历史类比检索模式）

当当前 topic 出现下列信号时，worker 应自动检索同模块历史经验，不需要用户在短 prompt
里额外说明：

- 公开入口不止一种，例如 full-cloud、indices、dual indices、correspondences、weights。
- 标量源码通过 iterator、helper 或对象状态把不同入口统一起来，但 RVV 需要重新区分
  stride load（跨步加载）、gather（离散加载）、weight load（权重加载）或 append tail（追加尾段）。
- 设计中出现 staging（分阶段暂存）、row source / weight source、policy（策略类型）、
  common pipeline（共同流水线）、scalar tail（标量尾段）或 production integration loop
  （生产接入闭环）。
- 当前 topic 的 bench 结果需要解释“数学相同但数据流不同”“诊断正向但 production 未接”
  或“某条 indexed / correspondence 路径退化原因不能单因归因”。

检索方式：

1. 先读当前 topic 文档和当前源码，确认当前问题，而不是先套历史模板。
2. 在 `artifact_layout.module_doc_dir_template` 解析出的同模块文档目录中优先查找 dataflows（数据流）总览、同系列函数文档和
   closeout 摘要；必要时用 `rg` 搜索 `indices`、`correspondences`、`weight`、
   `staging`、`policy`、`row source`、`production-ready`、`bench-only` 等窄关键词。
3. 只打开命中的窄文档和必要段落；不要全量加载配置解析出的文档根目录或无关模块。
4. 把历史经验写成候选设计或风险清单，例如“可以尝试 row-source policy”、
   “weight 来源应拆成独立 policy”、“correspondence append 保持标量尾段”等。
5. 如果 worker 在 prompt、设计文档、Handoff Packet（交接数据包）或最终回复中声明采用
   sibling topic（同模块相邻主题）经验，应输出 experience-migration audit（经验迁移审计）
   对照表。该表不强制当前 topic 采用 sibling 的具体 helper 或算法；它只要求把相邻成功或
   负向方案逐维审计清楚。至少覆盖 row source、source / weight policy、shared math pipeline、
   staging / reduction、formula / FMA、evidence model 和 production boundary，并用
   `adopted`、`attempted`、`deferred` 或 `rejected` 说明采用、尝试、暂缓或拒绝的理由。
6. 用当前源码、当前 diff、QEMU correctness、反汇编和板卡证据重新闭合结论；历史 topic
   只能启发检索和设计，不能替代当前 topic 的证据。

这个模式沉淀的是“如何找相似经验”，不是某个 topic 的固定做法。不要在 agent 资产中写死某个
函数必须采用某个 helper 名称，也不要把 historical topic reports 当作 normative rules（规范规则）。

## Maintenance Rules（维护规则）

- 保持本文件是索引和读取策略，不要写成知识 dump（知识堆积文档）。
- 只有当新的来源类别会改变 agent 取证方式时，才新增 source class（来源类别）。
- 优先写路径和读取策略，不复制原文内容。
- `.agents/skills/` 放可复用 workflow（工作流）说明；`.agents/knowledge/` 放轻量 map/index（映射或索引）。
- `.codex/` 只放 Codex 专用配置。
- 未提交的本地迁移材料保持 migration-only（仅迁移材料），除非用户明确要求清理、追溯或规则迁移；这些材料可能不存在，缺失时直接跳过。
