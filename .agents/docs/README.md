# PCL RVV Agent Instructions 导览

这份文档帮助读者先理解 PCL 仓库中的 agent instructions（agent 指令体系），再逐个审查和修改相关入口、skill（技能）和 reference（参考文件）。它不是新的规则源，也不替代任何 skill 或 reference。它只解释指令如何分层、阅读时先看哪里、哪些文件负责长期规则，哪些文件只是索引或审查材料。

PCL RVV agent instructions 的主要问题不是缺少规则，而是规则分布在多个层级：仓库入口、配置、知识索引、skill 入口、详细 reference、local override 和历史备份。若不先建立这套分层模型，读者很容易把导航文件当成规则，把历史迁移材料当成当前事实，或在一个 skill 中加入本应属于另一个 skill 的内容。

## 总入口：`AGENTS.md`

`AGENTS.md` 是仓库级总入口。它定义这套 agent instructions 的最高边界：哪些路径承载可复用规则，worker 和 reviewer 默认先读什么，短 prompt 如何展开，哪些改动不能和 production、topic 文档、topic 测试或 evidence logs 混在同一批变更中。

因此，审查 agent instructions 时应先读 `AGENTS.md`。它不需要包含每条细则，但必须能回答三个问题：

1. 当前仓库的正式 agent instructions 放在哪里。
2. worker、reviewer 和 workflow improvement 的默认读取链从哪里开始。
3. 哪些文件是 source of truth，哪些只是导航、索引、本机覆盖或迁移材料。

如果 `AGENTS.md` 发生变化，后续应复核 `.agents/docs/README.md`、`.agents/knowledge/pcl-rvv-knowledge-map.md` 和相关 skill 是否仍与它一致。S0 恢复、偏好冻结和产物发布边界的具体合同见 `.agents/skills/rvv-workflow/references/s0-preferences-and-recovery.zh.md`。

## 指令分层模型

PCL 当前的 agent instructions 可以按职责分成六层。越靠上越接近入口和边界，越靠下越接近具体执行细则。

```text
AGENTS.md
  -> .agents/config/defaults.yaml
  -> .agents/local/user-preferences.yaml        (可选，本机私有)
  -> .agents/knowledge/pcl-rvv-knowledge-map.md
  -> .agents/skills/<skill>/SKILL.md
  -> .agents/skills/<skill>/references/*.md
```

这种分层的目的，是让短 prompt 也能稳定展开成完整工作流，同时避免每个文件都重复规则。`AGENTS.md` 负责入口和边界；`config` 负责可变默认值；`knowledge map` 负责告诉 agent 去哪里取证；`SKILL.md` 负责说明一个 skill 的适用范围；`references/` 承载细则和检查合同；`local/` 只保存本机私有覆盖。

## 各层职责

| 层级 | 主要路径 | 职责 | 地位 |
| --- | --- | --- | --- |
| 仓库入口 | `AGENTS.md` | 定义仓库级 agent 边界、默认读取链、提交隔离和短 prompt 入口 | source of truth |
| 配置默认值 | `.agents/config/defaults.yaml` | 保存可提交的默认偏好、目录模板、env var 名和 artifact layout | source of truth |
| 配置说明 | `.agents/config/README.zh.md` | 解释配置键如何读取和覆盖 | navigation only |
| 本机覆盖 | `.agents/local/user-preferences.yaml` | 保存个人路径、板卡地址、用户名或临时偏好 | local-only；不提交 |
| 知识索引 | `.agents/knowledge/pcl-rvv-knowledge-map.md` | 说明不同来源类别和按需读取策略 | index；不复制规则正文 |
| Skill 入口 | `.agents/skills/<skill>/SKILL.md` | 说明该 skill 适用范围、职责边界和应读取哪些 reference | skill source of truth |
| 细则文档 | `.agents/skills/<skill>/references/*.md` | 承载具体流程、质量门禁、模板、证据合同和 reviewer 检查点 | detailed source |
| Agent 适配 | `.agents/skills/<skill>/agents/openai.yaml` | 保存 OpenAI/Codex 适配元数据 | adapter metadata |
| 审查导览 | `.agents/docs/README.md` | 帮助用户理解和审查 agent instructions | navigation only |
| 历史备份 | `.agents/backup/` | 保存旧版本或迁移前快照 | archive only |

## `skill`、`reference`、`config`、`knowledge` 与 `local override` 的分工

`SKILL.md` 应保持短而清楚。它回答“这个 skill 何时触发、负责什么、不负责什么、需要继续读哪些 reference”。如果一个规则只在复杂 topic 中才需要，通常不应塞进 `SKILL.md` 主体，而应放进窄 reference。

`references/` 承载详细规则。它们适合保存质量门禁、reviewer protocol、handoff packet、文档结构、测试证据合同、实现模式和环境配置清单。reference 可以长一些，但每个 reference 应有明确主题，不应把多个 skill 的职责混在一起。

`config/defaults.yaml` 保存可变默认值，而不是自然语言规则。目录模板、文件后缀、env var 名、work log 根目录、板卡变量名、evidence policy 和 test support 拆分阈值都应放在配置中。规则正文只引用配置键，不应写死个人路径、私有地址或单台设备。

`knowledge map` 是读取索引。它告诉 worker 如何找到当前 topic 的源码、文档、测试资产、历史报告和可执行证据。它不应该复制 topic 文档、测试日志或大段规则正文。

`local override` 是本机私有覆盖。它可以保存真实路径、板卡地址、用户名或个人临时偏好，但不应提交，也不应成为团队规则。Handoff 中如果需要报告它，只报告覆盖范围和 env var 名，不复制私有值。

## 三类角色的默认阅读链

### Worker

普通 worker 从仓库入口开始，先冻结偏好，再进入具体 skill。默认顺序是：

1. `AGENTS.md`
2. `.agents/config/defaults.yaml`
3. `.agents/local/user-preferences.yaml`（若存在）
4. `.agents/knowledge/pcl-rvv-knowledge-map.md`
5. `.agents/skills/rvv-workflow/SKILL.md`
6. `.agents/skills/rvv-workflow/references/short-prompt-entry.zh.md`
7. `.agents/skills/rvv-workflow/references/reviewability-and-language.zh.md`
8. `.agents/skills/rvv-workflow/references/topic-lifecycle.zh.md`
9. `.agents/skills/rvv-workflow/references/handoff-packet.zh.md`
10. `.agents/skills/rvv-workflow/references/worker-quality-gates.zh.md`
11. 当前任务对应 skill 的 `SKILL.md` 和必要 `references/`

### Reviewer

Reviewer 默认只读。它先确认 worker 是否遵守仓库级边界，再按 Handoff、diff、证据和文档归属检查 blocking issue。默认顺序是：

1. `AGENTS.md`
2. `.agents/config/defaults.yaml`
3. `.agents/local/user-preferences.yaml`（若存在）
4. `.agents/knowledge/pcl-rvv-knowledge-map.md`
5. `.agents/skills/rvv-workflow/SKILL.md`
6. `.agents/skills/rvv-workflow/references/reviewability-and-language.zh.md`
7. `.agents/skills/rvv-workflow/references/topic-lifecycle.zh.md`
8. `.agents/skills/rvv-workflow/references/reviewer-protocol.zh.md`
9. `.agents/skills/rvv-workflow/references/handoff-packet.zh.md`
10. `.agents/skills/rvv-documentation/references/document-ownership-and-traceability.zh.md`
11. 当前证据类型对应的 skill 和窄 reference

### Workflow improvement

Workflow improvement 先按 reviewer 链确认当前缺口，再只读并修改与缺口直接相关的 instruction file、skill 或 reference。它不应借一次审查同时大改多个 skill。若要进入 Phase 6，默认一轮只处理一个 skill，并输出文件地图、问题清单、建议改动、forward test 和 Handoff Packet。

## 建议的 Phase 0.5 / Phase 6 审查顺序

当前目标是先熟悉 agent instructions，再逐个审查和修剪。建议顺序如下：

1. `AGENTS.md`：确认仓库级入口是否清楚，是否正确说明 source of truth、读取链和修改边界。
2. `.agents/docs/README.md`：确认导览是否帮助用户理解指令分层，而不是变成第二份规则书。
3. `rvv-workflow`：审查生命周期、短 prompt、worker/reviewer 权限、Handoff Packet 和质量门禁。
4. `rvv-documentation`：审查 topic 文档、evaluation、closeout、文档归属和 traceability map。
5. `rvv-test`：审查测试、diagnostic、bench、A/B、board stability、evidence policy 和输出摘要合同。
6. `rvv-implementation`：审查 production path、fallback、dispatch、load/store、泛型点类型和实现设计方法层。
7. `rvv-project-config`：审查工具链、QEMU、board、Makefile/env var 和本机配置边界。
8. `rvv-screening` 与 `rvv-math-vectorization`：最后审查相对独立或专项资产，判断是否过拟合、冗余或需要保留。

每轮审查只处理一个对象。若发现跨 skill 的结构问题，先记录在 Handoff Packet 或审查笔记中，不要在同一轮把多个 skill 一起重写。

## Source of truth 与非规则材料

正式规则来源包括：

- `AGENTS.md`
- `.agents/config/defaults.yaml`
- `.agents/skills/<skill>/SKILL.md`
- `.agents/skills/<skill>/references/*.md`

导航或索引材料包括：

- `.agents/docs/README.md`
- `.agents/config/README.zh.md`
- `.agents/knowledge/pcl-rvv-knowledge-map.md`

本机或历史材料包括：

- `.agents/local/*`
- `.agents/backup/*`
- `.agents/docs/README.bak.md`（当前仅作为 Phase 0.5/6 审查期间的临时阅读材料）

这些文件可以帮助理解上下文，但不应覆盖正式规则。若临时材料中有价值内容，应提炼到正式 README、skill 或 reference 中；若只用于追溯，应在审查结束后删除或改名为明确的 notes 文件。

## 审查时的读法

读一个 instruction file、skill 或 reference 时，先问四个问题：

1. 这个文件解决什么问题。
2. 它的上游 source of truth 是什么。
3. 它是否把别的 skill 的规则复制进来了。
4. 它是否能被 reviewer 用真实 topic 或 forward test 检查。

如果答案不清楚，优先补导航、职责边界或例子入口；不要先追加更细规则。Phase 0.5 的目标是建立可理解的地图，Phase 6 的目标才是逐个 skill 审查、修剪和验证。
