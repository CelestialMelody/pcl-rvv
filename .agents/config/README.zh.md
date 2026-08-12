# PCL RVV Agent 配置

本目录保存可提交的默认偏好。agent（代理）在 S0 恢复和偏好冻结阶段读取
`.agents/config/defaults.yaml`，然后在存在时读取 `.agents/local/user-preferences.yaml`。
local override（本机私有覆盖）只用于个人路径、板卡地址、用户名和临时偏好，默认不提交。

可变的命名、目录、文件扩展名、工作路径、板卡地址、用户名、远端路径和设备标签不要写进
skill（技能）或 reference（参考文档）正文作为固定规则。通用 agent 资产只引用配置键、
env var（环境变量）名或当前 topic（主题）的既有结构；默认值放在 `defaults.yaml`，本机或
团队临时偏好放在 local override。

## 读取顺序

1. 读取 `.agents/config/defaults.yaml`。
2. 如果存在，读取 `.agents/local/user-preferences.yaml`。
3. 用户当前 prompt（提示词）中的明确要求覆盖前两者。
4. 在 S0 输出 `preferences_loaded`，并冻结注释、文档、证据、日志、agent asset（代理资产）反馈和环境偏好。

## 模板解析规则

配置模板允许用 `{section.key}` 形式引用 dotted config keys（点分配置键），也允许用 `{module}`、
`{topic}`、`{function}`、`{run_id}`、`{phase_id}`、`{phase_slug}` 这类运行时变量表示当前 artifact（产物）的上下文。解析时先合并 defaults、
local override（本机私有覆盖）和 prompt override（提示词覆盖）三层配置，递归展开 dotted config
keys，再用当前 topic / function / adapter 提供的运行时变量绑定剩余占位符。
列表型路径配置也可以使用同样的 `{section.key}` 引用；worker 应逐项展开，而不是把当前默认目录名写进规则正文。
如果 dotted config key 或运行时变量无法解析，worker 必须在 S0 报告或 Handoff Packet（交接数据包）
中写清缺失键和受影响产物；不要猜一个固定路径或文件名代替配置结果。

## 可配置内容

- `language`：中文主导、英文术语首次出现是否解释。
- `comments`：配置解析出的测试资产、diagnostic（诊断代码）、prototype（原型代码）和 production（生产源码）的注释策略。
- `documentation`：closeout（收尾文档）是否先写当前状态、是否要求数值算例、长期文档是否禁止保留对话流程话术，以及文档语言后缀、主题文档后缀、评估文档后缀和扩展名默认值。
- `evidence`：证据日志策略。默认 `summary-only`，不提交 raw logs（原始日志）。
- `agent_assets`：是否报告可沉淀到 skill（技能）或 knowledge map（知识索引）的经验。默认 `report-only`，不自动修改 agent asset。
- `test_support`：测试支撑代码的拆分阈值、聚合入口目录、聚合入口命名、内部目录、文件扩展名、兼容别名和职责拆分偏好。
- `paths`：work log（工作日志）、测试目录、文档目录、依赖库和交叉编译工具链的环境变量名。
- `artifact_layout`：topic（主题）测试目录、主题文档、evaluation（评估）文档、S0 run（S0 运行记录）目录、phase plan/result（阶段计划 / 结果）、optimization matrix（优化矩阵）、current handoff（当前交接摘要）、测试 / bench 源码位置、筛选目录、数学专项测试顶层目录和函数目录、QEMU / board 输出目录、evidence registry（证据登记表）、Makefile 文件名和日志脱敏脚本的模板。
- `artifact_publication`：产物发布策略。它只表达默认提交边界和审查要求；S0 run record 默认 local-only（仅本地），phase docs 默认 review-required（需要审查），current handoff 默认需要用户显式授权，raw logs 默认不提交，agent asset patch 必须与 topic 产物拆分审查 / 提交。
- `board`：板卡配置的环境变量名。不要在可提交配置里写 IP、用户名或私有路径。

## 本机覆盖示例

`.agents/local/user-preferences.yaml` 可以写：

```yaml
paths:
  work_log_root: <local-work-log-root>
  dependency_root_env_var: <dependency-root-env-var>
  cross_toolchain_env_var: <cross-toolchain-env-var>

board:
  target_env_var: <board-target-env-var>
  user_env_var: <board-user-env-var>
  host_env_var: <board-host-env-var>
  remote_root_env_var: <board-remote-root-env-var>

comments:
  test_rvv: detailed_zh
  production: concise_boundary_only

agent_assets:
  feedback_mode: report-only
  allow_auto_update: false
```

## Artifact layout 与 publication policy

`artifact_layout` 是路径 source of truth（事实来源）。S0 运行记录、phase docs（阶段文档）、optimization matrix（优化矩阵）和 current handoff（当前交接摘要）都应通过模板解析，不要在 skill 或 reference 中重新写死默认路径。新增或修改模板时应遵守两条规则：

1. 模板值只使用仓库相对路径、配置键和运行时变量；不要写私有绝对路径、板卡地址、用户名或 topic-specific（特定主题）硬编码。
2. 运行时变量缺失时必须 fail closed（保守失败）：在 S0 / Handoff 中报告缺失键和受影响产物，而不是猜一个目录。

`artifact_publication` 是产物发布策略 source of truth。它只记录默认策略和提交边界，不替代 reviewer 的证据判断。常用分类如下：

| class | 默认策略 | 默认提交边界 |
| --- | --- | --- |
| `s0_run_record` | local-only（仅本地） | 默认不提交 |
| `phase_docs` | review-required（需要审查） | 审查后可作为 topic test asset 提交 |
| `current_handoff` | 需要用户显式授权 | 默认不提交 |
| `final_topic_docs` | review-required | 证据审查后作为 topic 文档提交 |
| `evidence_summary` | summary-only + review-required | 被文档引用且脱敏后可提交 |
| `sanitized_logs` | 需要用户明确要求 | 脱敏后单独提交 |
| `raw_logs` | local-only | 默认永不提交 |
| `agent_asset_patch` | separate-review-required（单独审查） | 必须与 topic 产物拆分 |

Handoff Packet（交接数据包）中的 `dirty_isolation`、`artifacts_created_or_updated` 和 `commit_preferences` 应引用这些 class（类别）或说明 prompt override（提示词覆盖），不要把本轮 topic 输出和 agent asset patch 混成一个提交边界。

该文件可以包含真实本机路径、板卡 IP 或用户名，因为 `.agents/local/` 已被 `.gitignore` 忽略。
不要把本机覆盖内容复制到 `defaults.yaml`。
