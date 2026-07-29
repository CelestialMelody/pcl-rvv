# PCL RVV Agent 配置

本目录保存可提交的默认偏好。agent（代理）在 S0 恢复和偏好冻结阶段读取
`.agents/config/defaults.yaml`，然后在存在时读取 `.agents/local/user-preferences.yaml`。
local override（本机私有覆盖）只用于个人路径、板卡地址、用户名和临时偏好，默认不提交。

## 读取顺序

1. 读取 `.agents/config/defaults.yaml`。
2. 如果存在，读取 `.agents/local/user-preferences.yaml`。
3. 用户当前 prompt（提示词）中的明确要求覆盖前两者。
4. 在 S0 输出 `preferences_loaded`，并冻结注释、文档、证据、日志、agent asset（代理资产）反馈和环境偏好。

## 可配置内容

- `language`：中文主导、英文术语首次出现是否解释。
- `comments`：`test-rvv`、diagnostic（诊断代码）、prototype（原型代码）和 production（生产源码）的注释策略。
- `documentation`：closeout（收尾文档）是否先写当前状态、是否要求数值算例、长期文档是否禁止保留对话流程话术。
- `evidence`：证据日志策略。默认 `summary-only`，不提交 raw logs（原始日志）。
- `agent_assets`：是否报告可沉淀到 skill（技能）或 knowledge map（知识索引）的经验。默认 `report-only`，不自动修改 agent asset。
- `paths`：work log（工作日志）、测试目录、文档目录、依赖库和交叉编译工具链的环境变量名。
- `board`：板卡配置的环境变量名。不要在可提交配置里写 IP、用户名或私有路径。

## 本机覆盖示例

`.agents/local/user-preferences.yaml` 可以写：

```yaml
paths:
  work_log_root: /tmp/pcl-rvv-work-logs
  dependency_root_env_var: PCL_RVV_LOCAL_DEP_ROOT
  cross_toolchain_env_var: PCL_RVV_LOCAL_TOOLCHAIN_ROOT

board:
  target_env_var: PCL_RVV_MY_BOARD
  user_env_var: PCL_RVV_MY_BOARD_USER
  host_env_var: PCL_RVV_MY_BOARD_HOST
  remote_root_env_var: PCL_RVV_MY_BOARD_REMOTE_ROOT

comments:
  test_rvv: detailed_zh
  production: concise_boundary_only

agent_assets:
  feedback_mode: report-only
  allow_auto_update: false
```

该文件可以包含真实本机路径、板卡 IP 或用户名，因为 `.agents/local/` 已被 `.gitignore` 忽略。
不要把本机覆盖内容复制到 `defaults.yaml`。
