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
`{topic}`、`{function}` 这类运行时变量表示当前 artifact（产物）的上下文。解析时先合并 defaults、
local override（本机私有覆盖）和 prompt override（提示词覆盖）三层配置，递归展开 dotted config
keys，再用当前 topic / function / adapter 提供的运行时变量绑定剩余占位符。
如果 dotted config key 或运行时变量无法解析，worker 必须在 S0 报告或 Handoff Packet（交接数据包）
中写清缺失键和受影响产物；不要猜一个固定路径或文件名代替配置结果。

## 可配置内容

- `language`：中文主导、英文术语首次出现是否解释。
- `comments`：配置解析出的测试资产、diagnostic（诊断代码）、prototype（原型代码）和 production（生产源码）的注释策略。
- `documentation`：closeout（收尾文档）是否先写当前状态、是否要求数值算例、长期文档是否禁止保留对话流程话术，以及文档语言后缀、主题文档后缀、评估文档后缀和扩展名默认值。
- `evidence`：证据日志策略。默认 `summary-only`，不提交 raw logs（原始日志）。
- `agent_assets`：是否报告可沉淀到 skill（技能）或 knowledge map（知识索引）的经验。默认 `report-only`，不自动修改 agent asset。
- `test_support`：测试支撑代码的拆分阈值、聚合入口、内部目录、文件扩展名、兼容别名和职责拆分偏好。
- `paths`：work log（工作日志）、测试目录、文档目录、依赖库和交叉编译工具链的环境变量名。
- `artifact_layout`：topic（主题）测试目录、主题文档、evaluation（评估）文档、筛选目录、数学专项测试顶层目录和函数目录、QEMU / board 输出目录、Makefile 文件名、测试 / bench 源文件前缀和日志脱敏脚本的模板。
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

该文件可以包含真实本机路径、板卡 IP 或用户名，因为 `.agents/local/` 已被 `.gitignore` 忽略。
不要把本机覆盖内容复制到 `defaults.yaml`。
