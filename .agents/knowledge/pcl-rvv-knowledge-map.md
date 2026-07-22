# PCL RVV 知识索引

## Purpose（用途）

本文件是给 Codex 和未来 `rvv-agent`（RVV 优化代理）使用的轻量读取索引。它只说明
PCL RVV 工作中各类知识、历史案例、可执行证据和当前源码真相放在哪里，
以及在上下文有限时应该如何按需读取。

不要把 `doc-rvv/` 或 `test-rvv/` 的内容复制到 `.agents/knowledge/`。
大型文档、测试、日志和实现文件只在当前任务需要时读取。

## Source Classes（来源类别）

| Class（稳定标签）                               | Location（位置）                                                                  | Role（作用）                                                                     | Default loading rule（默认读取规则）                                             |
| ----------------------------------------------- | --------------------------------------------------------------------------------- | -------------------------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| `repo_rules`（仓库规则）                      | `AGENTS.md`、`.agents/skills/`                                                | 持久仓库约定和可复用 agent workflow（代理工作流）。                              | 先读适用的`AGENTS.md`，再读匹配的 skill（技能）。                              |
| `role_entry_defaults`（角色入口默认值）       | `.agents/skills/rvv-workflow/references/short-prompt-entry.zh.md`、`worker-quality-gates.zh.md` | 短 prompt（提示词）启动时的 worker（执行者）、reviewer（审查者）和 workflow improvement（工作流改进）默认读取链；worker 写文件前的轻量质量门禁。 | 用户只给角色、工作目录和目标时读取；worker 选中 topic 后按质量门禁决定是否继续读取 documentation / diagnostics / benchmarking 细则。 |
| `normative_rvv_knowledge`（通用 RVV 知识）    | `doc-rvv/rvv/`                                                                  | 跨 topic（主题）的 RVV 知识、数学函数向量化说明和可复用 RVV 约定。               | 需要通用 RVV 规则时，优先从这里选择窄文件读取。                                  |
| `historical_topic_reports`（历史 topic 报告） | `doc-rvv/<module>/`                                                             | topic 文档、closeout（收尾记录）记录、实现说明和历史决策。                       | 只读当前模块或 topic 相关文档，并用当前源码复核。                                |
| `screening_and_queue_docs`（筛选和队列文档）  | `doc-rvv/library-screening/`                                                    | 模块筛选、候选队列、状态表和下一主题建议。                                       | 选择、恢复或复筛 topic 时读取。                                                  |
| `executable_evidence`（可执行证据）           | `test-rvv/<module>/<topic>/`                                                    | test（测试）、bench（性能测试）、QEMU、反汇编检查、board（板卡）脚本和日志入口。 | 只读取当前结论需要的具体测试、脚本或日志。                                       |
| `source_truth`（当前源码真相）                | 当前 PCL 源码、当前 git diff、`__RVV10__` 路径                                  | 当前实现行为的权威来源。                                                         | 下结论前必须用当前源码和当前 diff 复核文档与测试。                               |
| `migration_only`（仅迁移材料）                | local migration sources（本地迁移来源，可能不存在；例如已移入`tmp` 的历史材料） | 历史 prompt（提示词）材料和旧 skill（技能）草稿。                                | 正常 topic runtime（主题运行过程）不读取；只有用户要求迁移、追溯或审计时才读取。 |

## Recommended Retrieval Order（推荐读取顺序）

1. 读取 `AGENTS.md`，确认仓库级边界。
2. 如果用户使用短 prompt，读取 `rvv-workflow/references/short-prompt-entry.zh.md`；worker 选中 topic 后再读取 `rvv-workflow/references/worker-quality-gates.zh.md`。
3. 选择匹配的 `.agents/skills/<skill-name>/SKILL.md`。
4. 读取本知识索引，决定下一步需要打开哪类证据。
5. 如果任务是选题或恢复工作，读取 `doc-rvv/library-screening/` 中的状态和队列文档。
6. 如果任务需要通用 RVV 规则，读取 `doc-rvv/rvv/` 下的相关窄文件；涉及模板点类型、traits、字段 offset、POD / standard-layout、或从具体点类型诊断扩展到 production 泛型入口时，优先读取 `doc-rvv/rvv/RVV Generic Point Type Strategy.zh.md`。
7. 如果任务针对已有 topic，读取 `doc-rvv/<module>/` 下的对应 topic 文档。
8. 用当前源码和当前 git diff 复核文档结论。
9. 只在需要证明具体结论时，读取 `test-rvv/<module>/<topic>/` 下的测试、bench、脚本或日志。

## Authority and Freshness（权威性与新鲜度）

- 当前源码和当前 git diff 是实现行为的最新真相。
- `doc-rvv/rvv/` 可以作为可复用 RVV 约定入口；如果它描述的代码已经变化，以当前源码为准。
- `doc-rvv/<module>/` 记录历史案例和 closeout 结论，是强线索，但不自动代表当前源码。
- `test-rvv/<module>/<topic>/` 是可执行证据入口；结论取决于具体测试、构建参数、日志、目标硬件和时间。
- QEMU evidence（QEMU 证据）只支持 correctness（正确性）、log shape（日志形状）和 path/instruction coverage（路径或指令覆盖）判断，不能支撑真实性能结论。
- Board/target-hardware benchmark（板卡或目标硬件 benchmark）日志才能支撑性能结论。

## What Not To Load By Default（默认不要读取什么）

- 不要全量读取 `doc-rvv/`。
- 不要全量读取 `test-rvv/`。
- 不要默认读取生成的 `build/`、`output/`、`log/` 或 `.log` 文件，除非任务明确要求查看这些证据。
- 正常 RVV topic 工作中不要读取未提交的本地迁移材料。
- 不要把历史文档、旧日志或迁移记录当作比当前源码更新的事实。
- 不要把大型文档、日志、测试或源码片段复制进 `.agents/knowledge/`。

## Topic Work Retrieval Pattern（主题工作读取模式）

处理某个具体 topic 时：

1. 从用户请求或筛选队列中确认 module（模块）和 topic（主题）名称。
2. 读取匹配的 RVV skill，并且只读取当前任务需要的 skill references（参考文件）。
3. 如果任务涉及选题、恢复或复筛，打开 `doc-rvv/library-screening/`。
4. 如果存在对应 topic 文档，打开 `doc-rvv/<module>/` 下的窄文档。
5. 检查当前源码里的公开入口、helper、fallback 和 `__RVV10__` 分支。
6. 只为验证当前问题，读取 `test-rvv/<module>/<topic>/` 下必要的测试、bench、脚本或日志。
7. 结论中分开说明 correctness（正确性）、QEMU path evidence（QEMU 路径证据）、disassembly evidence（反汇编证据）、board performance（板卡性能）和 production decision（生产接入判断）。

## Maintenance Rules（维护规则）

- 保持本文件是索引和读取策略，不要写成知识 dump（知识堆积文档）。
- 只有当新的来源类别会改变 agent 取证方式时，才新增 source class（来源类别）。
- 优先写路径和读取策略，不复制原文内容。
- `.agents/skills/` 放可复用 workflow（工作流）说明；`.agents/knowledge/` 放轻量 map/index（映射或索引）。
- `.codex/` 只放 Codex 专用配置。
- 未提交的本地迁移材料保持 migration-only（仅迁移材料），除非用户明确要求清理、追溯或规则迁移；这些材料可能不存在，缺失时直接跳过。
