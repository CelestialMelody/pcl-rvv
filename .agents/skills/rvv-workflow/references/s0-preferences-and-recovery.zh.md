# S0 偏好冻结与恢复合同

## 用途

本文定义 RVV 工作里 S0（恢复和偏好冻结）的最小合同。它告诉 worker / reviewer 在开始、恢复或交接时要记录什么、从哪里恢复、哪些产物默认提交、哪些必须保持本地或单独审查。

本文只定义语义和字段，不实现 checker，也不替代 `.agents/config/defaults.yaml`。路径模板和默认发布边界仍以配置为准；本文只说明如何把配置解析成可恢复的 S0 记录。

## 读取顺序

S0 恢复时，先读：

1. `AGENTS.md`
2. `.agents/config/defaults.yaml`
3. 如果存在，`.agents/local/user-preferences.yaml`
4. 本文
5. `rvv-workflow/references/short-prompt-entry.zh.md`
6. `rvv-workflow/references/handoff-packet.zh.md`
7. `rvv-workflow/references/topic-lifecycle.zh.md`

## S0 要回答的七个问题

S0 记录只需要回答七件事：

| 字段 | 作用 | 典型内容 |
| --- | --- | --- |
| `preferences_loaded` | 说明读了哪些偏好层 | `defaults`、`local_override`、`prompt_override` 的加载结果 |
| `frozen_policies` | 说明本轮实际采用了什么策略 | 注释、文档、证据、work log、commit、agent asset 反馈的有效策略 |
| `resolved_artifacts` | 说明哪些路径模板被解析成了实际产物 | `artifact_layout` 里的模板键、解析后的相对路径、用途和保存层级（包括 evidence registry） |
| `artifact_publication_decision` | 说明这些产物是否默认可提交 | `artifact_publication.classes` 里的类别、默认策略和提交边界 |
| `dirty_isolation` | 说明当前工作区如何与无关修改隔离 | 允许审查的路径集合、无关脏文件、必须忽略的产物 |
| `validation` | 说明本轮做了哪些检查 | 配置读取、模板展开、私有值扫描、`git diff --check`、Handoff 一致性检查 |
| `next_action` | 说明下一步应该做什么 | 一个可恢复、单一、具体的下一动作 |

## 推荐记录形状

S0 可以用 Markdown 或结构化文件表达，只要字段清楚、可恢复、可审查。推荐形状如下：

```yaml
preferences_loaded:
  defaults: loaded
  local_override: absent
  prompt_override: absent

frozen_policies:
  comment_policy: detailed_zh
  documentation_policy: closeout_current_state_first
  evidence_policy: summary-only
  work_log_policy: use_paths.work_log_root
  commit_policy: no_commit_without_user_request
  agent_asset_feedback_policy: report-only

resolved_artifacts:
  - artifact_key: s0_markdown
    template_key: artifact_layout.s0_markdown_template
    resolved_path: tmp/rvv-work-logs/<module>/<topic>/<run-id>/s0.zh.md
    publication_class: s0_run_record
  - artifact_key: current_handoff
    template_key: artifact_layout.current_handoff_template
    resolved_path: tmp/rvv-work-logs/<module>/<topic>/current-handoff/current-handoff.zh.md
    publication_class: current_handoff
  - artifact_key: evidence_registry
    template_key: artifact_layout.evidence_registry_template
    resolved_path: <resolved-path-from-artifact_layout.evidence_registry_template>
    publication_class: evidence_summary

artifact_publication_decision:
  s0_run_record: local_only
  phase_docs: review_required
  current_handoff: explicit_user_authorization_required
  final_topic_docs: review_required
  evidence_summary: summary_only_review_required
  sanitized_logs: explicit_user_request_required
  raw_logs: local_only
  agent_asset_patch: separate_review_required

dirty_isolation:
  allowed_paths:
    - .agents/config/defaults.yaml
    - .agents/config/README.zh.md
    - .agents/docs/README.md
    - .agents/skills/rvv-workflow/**
  unrelated_dirty_paths:
    - <other topic files>
    - <generated logs>
  isolation_note: ignore / do not stage / separate commit

validation:
  config_read: pass
  template_resolution: pass
  private_value_scan: pass
  git_diff_check: pass

next_action: "写 Handoff Packet，并按当前 topic 进入下一步或停在用户授权边界"
```

## 字段说明

### `preferences_loaded`

这个字段只回答“读了什么”，不回答“最后决定了什么”。它应区分：

- `defaults`：是否读取了 `.agents/config/defaults.yaml`。
- `local_override`：是否读取了 `.agents/local/user-preferences.yaml`。
- `prompt_override`：当前 prompt 是否覆盖了前两者。

如果 local override 存在，只报告覆盖范围或 env var 名，不写 IP、用户名、私有绝对路径或 token。

### `frozen_policies`

这个字段回答“本轮有效策略是什么”。它应把可执行偏好冻结为简短陈述，例如：

- 注释策略：测试资产 / diagnostic 详细中文，production 克制。
- 文档策略：closeout current-state-first，长期文档不保留对话流程话术。
- 证据策略：默认 summary-only，不提交 raw logs。
- work log 策略：是否记录、默认根目录、是否需要保存交接摘要。
- commit 策略：是否默认不提交，是否需要用户明确授权。
- agent asset 策略：是否只报告建议，不自动改 `.agents`。

### `resolved_artifacts`

这个字段回答“本轮把哪些模板真的解析成了哪些路径”。它应尽量使用仓库相对路径，并标明用途，例如：

- S0 run record：`artifact_layout.s0_markdown_template`、`artifact_layout.s0_structured_template`。
- current handoff：`artifact_layout.current_handoff_template`、`artifact_layout.current_handoff_structured_template`。
- phase docs：`artifact_layout.phase_plan_template`、`artifact_layout.phase_result_template`、`artifact_layout.optimization_matrix_template`。
- evidence registry：`artifact_layout.evidence_registry_template`。

如果某个模板无法解析，应该报告缺失键，而不是猜一个路径。

### `artifact_publication_decision`

这个字段回答“这些产物默认能不能提交”。它必须使用 `.agents/config/defaults.yaml` 里的 `artifact_publication.classes` 名称或等价说明，而不是自己发明一套新分类。

常见含义：

- `s0_run_record`：默认 local-only。
- `phase_docs`：默认 review-required，审查后才可进入 topic 产物边界。
- `current_handoff`：默认需要用户显式授权。
- `final_topic_docs`：默认 review-required，证据确认后可提交。
- `evidence_summary`：默认 summary-only + review-required。
- `sanitized_logs`：默认需要用户明确要求。
- `raw_logs`：默认 local-only。
- `agent_asset_patch`：必须与 topic 产物分开审查。

### `dirty_isolation`

这个字段回答“本轮怎么隔离无关脏工作”。它至少要说明：

- 当前允许审查或提交的路径集合。
- 现有无关 dirty paths 属于哪类：其它 topic、生成日志、本机缓存、旧迁移材料、用户自己的未完成改动。
- 哪些路径必须 `ignore / do not stage / separate commit`。

### `validation`

这个字段回答“本轮做了哪些检查”。S0 不需要 checker，但需要可复核的验证摘要，例如：

- YAML 语法可解析。
- 模板展开没有悬空占位符。
- changed config 不含私有绝对路径、IP、用户名、token-like 赋值。
- `git diff --check` 通过。

如果某项没运行，要写原因，而不是省略。

### `next_action`

这个字段只能写一个下一步动作。它应当可恢复、可执行、不会让下一轮 worker 重新猜上下文。例如：

- “继续当前 topic 的下一 phase。”
- “先更新 current handoff，再等待用户确认。”
- “只补齐 S0 记录，不进入实现。”

## 与 workflow 的关系

- `short-prompt-entry.zh.md` 负责决定短 prompt 如何触发 S0 恢复。
- `handoff-packet.zh.md` 负责把 S0 结果带进后续阶段。
- `topic-lifecycle.zh.md` 负责把 S0 作为主干状态机的起点。
- `worker-quality-gates.zh.md` 负责把 S0 变成可审查门禁。

S0 不是新的 topic 规则书，也不是 checker 说明书。它是“这一轮工作应该从哪里继续”的恢复合同。
