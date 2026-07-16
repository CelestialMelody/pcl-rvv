# PCL RVV Agent 指南

本仓库的可复用 RVV agent 资产放在 `.agents/skills/`。开始 PCL RVV
筛选、诊断、实现、benchmark、文档或项目配置工作前，优先使用匹配的
skill。`.codex/` 只用于 Codex 专用配置，不承载通用 RVV 规则。

## 目录约定

- `.agents/skills/<skill-name>/SKILL.md` 是通用 agent skill 说明。
- `.agents/skills/<skill-name>/agents/openai.yaml` 是 OpenAI/Codex 适配元数据；未来其它 agent 可以忽略。
- `.agents/skills/<skill-name>/references/` 放按需加载的详细规则。
- `.agents/knowledge/pcl-rvv-knowledge-map.md` 是轻量知识索引入口，只说明读取策略，不复制 `doc-rvv/` 或 `test-rvv/` 内容。
- `.skills/` 是旧草稿来源；迁移确认前不要删除或编辑，除非用户明确要求清理。
- `chats/` 是历史对话和迁移原料，不作为正式 agent 资产提交。

## RVV 工作规则

- 优先使用匹配的 RVV skill：`rvv-workflow`、`rvv-project-config`、`rvv-screening`、`rvv-diagnostics`、`rvv-implementation`、`rvv-benchmarking`、`rvv-documentation`、`rvv-math-vectorization`。
- 除非用户明确要求，不修改 PCL 生产源码，不修改 `test-rvv/` 或 `doc-rvv/` 的主题内容。
- RVV 结论必须有证据链，区分 correctness、QEMU 证据、反汇编证据、板卡性能、fallback 边界和生产接入判断。
- QEMU 只用于正确性、日志格式和路径命中证据；性能结论必须来自目标硬件或板卡。
- 不提交生成日志、本地 build 输出、个人绝对路径、私有板卡地址、本机 `config.mk` 或聊天记录。

## 变更边界

整理 agent 资产时，diff 应聚焦在 `.agents/skills/`、`AGENTS.md` 和忽略规则。除非用户点名要求，不把 PCL 源码改动或 RVV 主题内容混入同一批变更。
