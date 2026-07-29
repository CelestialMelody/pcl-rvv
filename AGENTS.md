# PCL RVV Agent（代理）指南

本仓库的可复用 RVV agent（代理）资产放在 `.agents/skills/`。开始 PCL RVV
筛选、诊断、实现、benchmark（性能测试）、文档或项目配置工作前，优先使用匹配的
skill（技能）。`.codex/` 只用于 Codex 专用配置，不承载通用 RVV 规则。

S0（恢复和偏好冻结）时，worker（执行者）和 reviewer（审查者）先读取
`.agents/config/defaults.yaml`，如果存在再读取 `.agents/local/user-preferences.yaml`。
S0 输出必须显式记录 `preferences_loaded`，并把注释、文档、证据、agent asset（代理资产）
反馈和 work log（工作日志）偏好冻结下来。默认偏好只说明规则；本机私有覆盖只放在 `.agents/local/`。

如果用户给出的是短 prompt（提示词），例如只给角色、工作目录和目标，先按
`.agents/skills/rvv-workflow/references/short-prompt-entry.zh.md` 的默认读取链启动。
用户不需要在每轮重复列出“先读哪些文件”；默认读取链会从仓库入口展开。
worker 或 reviewer 输出路径不是必填项；只有用户要求保存到固定工作日志或跨对话复用时才指定。
若需要保存，默认路径由 `PCL_RVV_WORK_LOG_ROOT` 控制；未设置时使用 `<repo>/tmp/rvv-work-logs/`。
短 prompt 只减少用户输入，不降低 worker 产物门槛。worker 选中 topic 后、开始写
`test-rvv/`、`doc-rvv/` 或 production 前，必须按
`.agents/skills/rvv-workflow/references/worker-quality-gates.zh.md` 自查；需要详细规则时再按该文件
渐进读取 `rvv-documentation`、`rvv-test` 和 `rvv-implementation` 的窄 reference。

## 目录约定

- `.agents/skills/<skill-name>/SKILL.md` 是通用 agent skill 说明。
- `.agents/skills/<skill-name>/agents/openai.yaml` 是 OpenAI/Codex 适配元数据；未来其它 agent 可以忽略。
- `.agents/skills/<skill-name>/references/` 放按需加载的详细规则。
- `.agents/config/defaults.yaml` 放可提交默认偏好。
- `.agents/local/user-preferences.yaml` 放本机私有覆盖，默认不提交。
- `.agents/skills/rvv-workflow/references/short-prompt-entry.zh.md` 定义 worker、
  reviewer 和 workflow improvement 的短启动入口、默认读取链和默认权限。
- `.agents/skills/rvv-workflow/references/worker-quality-gates.zh.md` 定义短 prompt worker
  写文件前的轻量质量门禁，避免为了 prompt 变短而丢失文档、注释、bench 和证据质量要求。
- `.agents/knowledge/pcl-rvv-knowledge-map.md` 是轻量知识索引入口，只说明读取策略，不复制 `doc-rvv/` 或 `test-rvv/` 内容。
- 未提交的本地迁移材料不作为正式 agent 资产；正常 RVV topic（主题）工作不要读取或依赖这些材料，除非用户明确要求做历史追溯或规则迁移。

## RVV 工作规则

- 优先使用匹配的 RVV skill：`rvv-workflow`、`rvv-project-config`、`rvv-screening`、`rvv-test`、`rvv-implementation`、`rvv-documentation`、`rvv-math-vectorization`。
- `rvv-test` 是统一测试与证据 skill。旧 diagnostics / benchmarking 职责已经迁移到 `rvv-test`，
  不再保留独立 skill 入口。
- 回复、代码注释、测试说明、文档、汇报必须遵循 `.agents/skills/rvv-workflow/references/reviewability-and-language.zh.md`：面向中文读者时不要堆英文术语，英文专有术语首次出现必须用括号解释中文含义。
- 除非用户明确要求，不修改 PCL 生产源码。短 prompt 中“处理 topic”视为授权修改该 topic 对应的
  `test-rvv/` 和 `doc-rvv/` 产物；不要把该授权扩展到其它 topic。
- RVV 结论必须有证据链，区分 correctness（正确性）、QEMU 证据、反汇编证据、板卡性能、fallback（回退路径）边界和生产接入判断。
- closeout（收尾）或 production-candidate（生产候选）topic 文档必须包含“正确性与高效性证据链”小节。未接 production（生产源码）的诊断结论使用“诊断证据链”，并写清 diagnostic evidence（诊断证据）不能替代 production evidence（生产证据）。
- QEMU 只用于正确性、日志格式和路径命中证据；性能结论必须来自目标硬件或板卡。
- 默认不提交生成日志、本地 build（构建）输出、个人绝对路径、私有板卡地址、本机 `config.mk` 或聊天记录。用户明确要求提交 evidence logs（证据日志）时，优先提交已脱敏日志，必须按 `rvv-workflow` 和 `rvv-test` 冻结日志策略、运行 `test-rvv/script/sanitize_evidence_logs.py` 或对应 Make target 检查、拆分 commit，并说明保留或排除哪些日志。

## 变更边界

整理 agent 资产时，diff（差异）应聚焦在 `.agents/skills/`、`.agents/knowledge/`、`.agents/config/`、`AGENTS.md` 和必要忽略规则。除非用户点名要求，不把 PCL 源码改动或 RVV 主题内容混入同一批变更。
workflow improvement（工作流改进）如果会批量改 skill、knowledge map（知识索引）或入口 prompt，先在 `.agents/backup/` 下创建不提交的备份目录，并在 Handoff Packet 写清 `backup_path`。
