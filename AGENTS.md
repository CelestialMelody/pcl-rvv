# PCL RVV Agent 指南

本仓库的可复用 RVV agent instructions（agent 指令体系）以 `AGENTS.md` 为入口，主要由 `.agents/skills/` 下的 workflow skills（工作流技能）和 workflow references（工作流参考）承载；`.agents/config/defaults.yaml` 保存可提交 workflow config（工作流配置），`.agents/knowledge/` 保存轻量读取索引。开始 PCL RVV 优化工作的筛选、配置、测试、实现或文档前，优先使用匹配的 skill。

S0（恢复和偏好冻结）时，worker 和 reviewer 先读取
`.agents/config/defaults.yaml`，如果存在再读取 `.agents/local/user-preferences.yaml`。
S0 输出必须显式记录 `preferences_loaded`，并把注释、文档、证据、instruction_feedback（指令反馈）
反馈和 work log（工作日志）偏好冻结下来。默认偏好只说明规则；本机私有覆盖只放在 `.agents/local/`。

如果用户给出的是短 prompt（提示词），例如只给角色、工作目录和目标，先按
`.agents/skills/rvv-workflow/references/short-prompt-entry.zh.md` 的默认读取链启动。
用户不需要在每轮重复列出“先读哪些文件”；默认读取链会从仓库入口展开。
角色词只选择 worker、reviewer 或 workflow improvement 的启动模式；agent 先完成指令读取、目标解析、权限边界和 dirty isolation（脏工作区隔离）判断，再进入 topic 文件。
worker 或 reviewer 输出路径不是必填项；只有用户要求保存到固定工作日志或跨对话复用时才指定。
若需要保存，默认路径由 `PCL_RVV_WORK_LOG_ROOT` 控制；未设置时使用 `<repo>/tmp/rvv-work-logs/`。
短 prompt 只减少用户输入，不降低 worker 产物门槛。worker 选中 topic 后、开始写
配置解析出的 topic 测试资产、topic 文档或 production 前，必须按
`.agents/skills/rvv-workflow/references/worker-quality-gates.zh.md` 自查；需要详细规则时再按该文件
渐进读取 `rvv-documentation`、`rvv-test` 和 `rvv-implementation` 的窄 reference。
涉及 workflow improvement、术语迁移、输出字段或启动合同时，启动输出必须记录 `loaded_instruction_sources`，列出本轮实际读取并用于决策的 instruction sources（指令来源）。

## 目录约定

- `.agents/skills/<skill-name>/SKILL.md` 是通用 workflow skill 说明。
- `.agents/skills/<skill-name>/agents/openai.yaml` 是 OpenAI/Codex 适配元数据；未来其它 agent 可以忽略。
- `.agents/skills/<skill-name>/references/` 放按需加载的 workflow references。
- `.agents/config/defaults.yaml` 放可提交 workflow config 默认值。
- `.agents/local/user-preferences.yaml` 放本机私有覆盖，默认不提交。
- `.agents/skills/rvv-workflow/references/short-prompt-entry.zh.md` 定义 worker、
  reviewer 和 workflow improvement 的短启动入口、默认读取链和默认权限。
- `.agents/skills/rvv-workflow/references/worker-quality-gates.zh.md` 定义短 prompt worker
  写文件前的轻量质量门禁，避免为了 prompt 变短而丢失文档、注释、bench 和证据质量要求。
- `.agents/knowledge/pcl-rvv-knowledge-map.md` 是轻量知识索引入口，只说明按配置解析出的文档 / 测试资产读取策略，不复制具体产物内容。
- 短 prompt 继续已有 topic、恢复阶段状态或目标含有“继续完善 RVV 优化工作”时，必须读取 `.agents/skills/rvv-test/references/optimization-phase-loop.zh.md`；它定义 `artifact_layout.phase_root_template` 解析目录下的 plan/result、optimization matrix、Evidence Doctor 异常处理和继续 / 停止规则。
- phase scope（阶段范围）不等于 topic scope（主题范围）。每一轮只能关闭计划中明确冻结的入口、row source、点类型、
  `Scalar`、布局和规模组合；如果本轮只证明了代表性点类型、具体点型或单一 source/target 组合，必须同时记录已验证范围、
  未验证范围、下一扩展 phase 和恢复条件。`std::is_same_v<PointXYZ>` 之类的具体类型门禁只能作为有明确理由的阶段性范围，
  不能被解释成模板入口的最终泛型结论。
- 短 prompt 恢复时，必须把 phase result、optimization matrix、roadmap 和 Handoff 中的
  `point_type_expansion_queue`（点类型扩展队列，或等价字段）与 row source / `Scalar` / layout 扩展队列一起检查；
  只要仍有当前授权范围内、未阻塞的扩展动作，就不能把当前窄范围阶段写成 topic closeout。
- 未提交的本地迁移材料不作为正式 agent instructions；正常 RVV topic（主题）工作不要读取或依赖这些材料，除非用户明确要求做历史追溯或规则迁移。

## RVV 工作规则

- 优先使用匹配的 RVV skill：`rvv-workflow`、`rvv-project-config`、`rvv-screening`、`rvv-test`、`rvv-implementation`、`rvv-documentation`、`rvv-math-vectorization`。
- `rvv-test` 是统一测试与证据 skill。旧 diagnostics / benchmarking 职责已经迁移到 `rvv-test`，
  不再保留独立 skill 入口。
- `rvv-test/references/optimization-phase-loop.zh.md` 是多阶段优化循环的细则源。短 prompt 继续已有 topic 时，worker 必须恢复或创建 phase plan，按阶段完成实现、测试、证据解释、矩阵更新和 continue / stop decision；仍有 unblocked next action 时不得因微任务完成而早停。若历史 phase 或 Handoff 写着 `ready_for_review`，仍要重新执行 `ready_for_review_validity_check`，确认 roadmap、matrix、structure parity、doc suite、legacy 清理和 shape scan 没有未阻塞缺口。
- 回复、代码注释、测试说明、文档、汇报必须遵循 `.agents/skills/rvv-workflow/references/reviewability-and-language.zh.md`：面向中文读者时不要堆英文术语，英文专有术语首次出现必须用括号解释中文含义。
- 除非用户明确要求，不修改 PCL 生产源码。短 prompt 中“处理 topic”视为授权修改该 topic 对应的、
  由 `artifact_layout` 解析出的测试资产、topic-local evaluation / phase 文档和适用的文档产物；不要把该授权扩展到其它 topic。
  `artifact_layout.topic_doc_template` 解析出的 `doc-rvv` 长期主题文档只适用于已有 adopted production behavior（已采用生产行为）、
  用户确认保留的 production patch（生产补丁）或 PI5 生产证据闭环通过且用户确认采纳后的主题。`diagnostic`、`bench-only`、`rollback/no-production`
  或未进入生产接入闭环的 `partial-production-candidate` 不默认创建 `doc-rvv`；其诊断证据链写入 topic-local
  evaluation、phase result、roadmap / matrix 和 Handoff。
- RVV 结论必须有证据链，区分 correctness（正确性）、QEMU 证据、反汇编证据、板卡性能、fallback（回退路径）边界和生产接入判断。
- diagnostic evidence（诊断证据）和 production evidence（生产证据）必须分层。diagnostic 或
  production-shaped diagnostic（生产形态诊断）的 repeated board 结果为 `weak`、`negative`、`neutral`
  或 `unstable` 时，只能说明该 diagnostic boundary（诊断边界）当前不支持对应候选，不能直接推出
  `no-production` / `rejected`，也不能直接拒绝 bounded production probe（有界生产探针）。凡 diagnostic
  结果参与 production 取舍，必须先完成 `diagnostic-to-production mismatch audit`，写清 evidence role
  （证据角色）、A/B boundary（A/B 边界）、当前决策问题、是否可外推到 production、comparison-boundary /
  baseline mismatch（比较边界 / 基线不一致）风险、弱 / 负 / 中性 / 不稳定时允许 bounded production probe 的条件，
  以及 clean adoption（干净采纳）是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B。
- production public Std/RVV（真实公开入口标量 / RVV）positive 只证明“当前 public RVV path 是否快于
  当前 public scalar path”。它不能证明新 RVV family（实现族）优于已有 adopted RVV family。若当前决策
  是 RVV-family-selection，必须补同一 production boundary 内的 RVV-vs-RVV detail A/B；否则只能写成
  bounded production candidate（有界生产候选）、explicit probe（显式探针）或 experiment path（实验路径），
  不能写成 clean adopted。
- row source、point type、`Scalar` 和 layout 必须继续独立批准。ordered-cloud-pair 的结论不能外推到
  source-indexed、dual-indexed 或 correspondence；代表点型或具体点型的结论不能外推成完整泛型结论。
- production integration loop（生产接入闭环）中的 production patch（生产补丁）是用户可见的生产源码变更。
  用户对“进入 / 推进 production integration loop”或“连续推进 PI2-PI5”的授权，只覆盖已说明范围内的接入、
  测试和证据采集，不自动包含 PI5 后的生产决策。PI5 是对称的用户检查点：无论生产证据支持采纳还是不支持采纳，
  worker 都必须先暂停，保留当前 patch，报告生产 diff、实际公开入口、可复现测试命令、板卡 / Evidence Doctor
  结果和拟议下一步，等待用户检查确认。若证据支持采纳，只有用户明确确认保留 / 采纳后，才能把 patch 视为
  adopted production behavior、进入 S11 production closeout、更新适用的 `doc-rvv` 或创建相关提交；若证据不支持
  接入生产，只有用户明确授权回滚后，才能回滚、删除或覆盖 patch。用户此前说“进入 / 推进闭环”或后续只说“继续”，
  都不能视为对 PI5 最终采纳或回滚的授权；未确认时不能自行收口，也不能以“证据优先级更高”“清理”或
  “恢复 no-production”为理由撤回生产改动。
- bench（性能测试）默认在板卡或目标硬件上跑；QEMU 默认只编译 bench binary 或跑窄范围 smoke，不运行完整 bench matrix，不把 QEMU bench compare 的计时写成性能结论。
- 板卡复跑必须先有 bounded rerun budget（有界复跑预算）和 decision bucket（决策桶）。数字小幅波动但决策桶不变时不要无限复跑；预算耗尽仍摇摆时标成 `unstable`、降级证据或交给人工判断。
- 如果一次复跑改变了已写文档中的方向、decision bucket、数值结论、Evidence Doctor 数量或证据角色，旧 summary / phase result 立即降级为历史 run；必须刷新对应 evaluation、phase 文档、Handoff Packet 和适用的 production 长期主题文档，不允许继续把旧数值当当前 truth。若当前结论为 no-production 且没有 adopted production behavior，不得为了刷新而新建 `doc-rvv`。
- 官方 Make / script target 覆盖证据文件时应更新 topic-local `log/evidence_registry.json` 或等价登记表；S0 恢复、phase loop 恢复和提交前检查必须发现 `unregistered_change`、`unregistered_file` 或 `stale_doc_pending_refresh`，不能把未登记覆盖当当前 truth。
- benchmark、board summary、checksum、反汇编归属或 EvidenceDecision 前，必须按 `rvv-test` 的 Evidence Doctor（证据体检）规则暴露 Errors / Warnings / Suggestions；异常信号不是自动判错，但不能无解释地跳过。
- closeout（收尾）或 production-candidate（生产候选）文档必须包含证据链。production 长期主题文档使用“正确性与高效性证据链”；未接 production（生产源码）的诊断结论在 topic-local evaluation / phase closeout 中使用“诊断证据链”，并写清 diagnostic evidence（诊断证据）不能替代 production evidence（生产证据）。
- QEMU 只用于正确性、日志格式和路径命中证据；性能结论必须来自目标硬件或板卡。
- 默认不提交生成日志、本地 build（构建）输出、个人绝对路径、私有板卡地址、本机 `config.mk` 或聊天记录。用户明确要求提交 evidence logs（证据日志）时，优先提交已脱敏日志，必须按 `rvv-workflow` 和 `rvv-test` 冻结日志策略、运行 `artifact_layout.sanitize_logs_script_template` 解析出的脚本或对应 Make target 检查、拆分 commit，并说明保留或排除哪些日志。

## 变更边界

整理 agent instructions 时，diff（差异）应聚焦在 `.agents/skills/`、`.agents/knowledge/`、`.agents/config/`、`AGENTS.md` 和必要忽略规则。除非用户点名要求，不把 PCL 源码改动或 RVV 主题内容混入同一批变更。
workflow improvement（工作流改进）如果会批量改 skill、knowledge map（知识索引）或入口 prompt，先在 `.agents/backup/` 下创建不提交的备份目录，并在 Handoff Packet 写清 `backup_path`。
