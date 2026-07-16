# 原始规则覆盖矩阵

本矩阵用于防止仍有效规则只停留在旧 `chats` 材料中。覆盖质量只描述当前 `.skills` 草案状态；如果后续发现规则仍只是短句概括，应继续补对应 reference，而不是默认已经完成。

归属分类：

- `generic-rvv`：适用于不同 C/C++ 高性能库的 RVV 工作流。
- `pcl-adapter`：当前 PCL 验证项目的 adapter 规则，迁移到其它库时替换。
- `codex-entry`：当前 Codex 新对话入口、授权范围和恢复上下文规则。
- `historical-deprecated`：旧术语或旧路径表述，只作为迁移清理对象。
- `unclear`：需要在后续主题中继续验证是否仍有效。

覆盖质量当前以 `完整覆盖` 为主；后续审查如发现缺口，再在对应行写明不足类型和推荐处理。

## 覆盖矩阵

| 源文件 | 规则摘要 | 规则类型 | 归属 | 当前覆盖位置 | 覆盖质量 | 入口必读 | 推荐处理 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `chats/rvv-diagnostics/SKILL.md` | 类式算法优先使用 test-only 派生诊断类，复用公开 setter、准备流程、缓存、indices/mask/input 生命周期和输出顺序。 | diagnostics | generic-rvv / pcl-adapter | `rvv-diagnostics/references/entry-shapes.md` | 完整覆盖 | 是 | 保留入口面覆盖清单和继承形态差异清单。 |
| `chats/rvv-diagnostics/SKILL.md` | full correctness 和 full bench 默认调用最接近上游公开 API 的入口层；低层 helper 不能替代入口证据。 | diagnostics | generic-rvv | `rvv-diagnostics/SKILL.md`、`rvv-diagnostics/references/entry-shapes.md` | 完整覆盖 | 是 | 保留入口短提醒和 reference 细则。 |
| `chats/rvv-diagnostics/SKILL.md` | free function 或纯 helper 保持同名/同形参数；local fragment 结果不能直接写成生产入口收益。 | diagnostics | generic-rvv | `rvv-diagnostics/references/entry-shapes.md`、`rvv-documentation/references/diagnostic-docs.md` | 完整覆盖 | 否 | 保留。 |
| `chats/rvv-diagnostics/SKILL.md` | 证据层级为 local correctness -> local microbench -> entrance/full evidence -> production decision，不跳级。 | diagnostics / benchmarking | generic-rvv | `rvv-diagnostics/SKILL.md`、`rvv-documentation/references/evaluation-doc-structure.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-diagnostics/SKILL.md` | 多阶段 staging 必须说明字段来源、invalid lane、`vcompress` 保序、scalar tail 和 fallback。 | diagnostics / documentation | generic-rvv | `rvv-diagnostics/references/staging-and-evidence.md`、`rvv-documentation/references/topic-doc-structure.md` | 完整覆盖 | 是 | 保留双层结构。 |
| `chats/rvv-diagnostics/SKILL.md` | 固定缓冲区必须绑定容量、SEW、LMUL、`vlmax` gate；该 gate 限制单次 chunk，不限制总输入规模。 | diagnostics / implementation | generic-rvv | `rvv-diagnostics/references/staging-and-evidence.md` | 完整覆盖 | 是 | 保留 fixed buffer 检查和动态/分段替代方案。 |
| `chats/rvv-diagnostics/SKILL.md` | segment store 适用于字段类型、布局和 `LMUL * NFIELDS` 自然匹配；字段混合或多字段压缩时 SoA buffer 更可控。 | diagnostics / implementation | generic-rvv | `rvv-diagnostics/references/staging-and-evidence.md` | 完整覆盖 | 否 | 保留 segment store 适用/不适用条件。 |
| `chats/rvv-diagnostics/SKILL.md` | 手工展开 Eigen、小矩阵、投影、距离、阈值或 float-to-int 时必须做源码公式、标量反汇编、RVV intrinsic 顺序对齐。 | diagnostics | generic-rvv | `rvv-diagnostics/references/semantic-alignment.md` | 完整覆盖 | 是 | 保留入口短提醒。 |
| `chats/rvv-diagnostics/SKILL.md` | 语义差异按可修复、可带条件修复、暂缓 production、不能进入 production 四类记录。 | diagnostics / documentation | generic-rvv | `rvv-diagnostics/references/semantic-alignment.md` | 完整覆盖 | 是 | 保留 production 决策树。 |
| `chats/rvv-diagnostics/SKILL.md` | correspondences、indices、pixel/voxel id、阈值筛选、保序 staging 默认要求 bit/predicate/output 等价；容差只用于业务允许近似的路径。 | diagnostics | generic-rvv | `rvv-diagnostics/references/semantic-alignment.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-diagnostics/SKILL.md` | production-shaped diagnostic 至少考虑标量/RVV 对拍、小规模、NaN/Inf、默认范围、显式 subset、点类型 traits、边界 adversarial case。 | diagnostics / benchmarking | generic-rvv / pcl-adapter | `rvv-diagnostics/references/staging-and-evidence.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-diagnostics/SKILL.md` | 测试 inventory 需要记录删除、合并、改名理由和剩余覆盖关系；production 升级后不能简单删诊断测试。 | documentation / diagnostics | generic-rvv | `rvv-documentation/references/closeout-style.md`、`rvv-diagnostics/references/staging-and-evidence.md` | 完整覆盖 | 是 | 保留 production direct、RVV-only adversarial、production-shaped diagnostic 保留规则。 |
| `chats/rvv-diagnostics/SKILL.md` | bench case 名称表达入口层级：helper/staging、diagnostic/candidate、production、experimental。 | benchmarking | generic-rvv | `rvv-diagnostics/references/staging-and-evidence.md` | 完整覆盖 | 否 | 保留。 |
| `chats/rvv-diagnostics/SKILL.md` | 生产接入前必须有 full/production 收益、checksum、测试、反汇编、fallback、语义风险和维护成本证据。 | diagnostics / benchmarking | generic-rvv | `rvv-diagnostics/SKILL.md`、`rvv-benchmarking/SKILL.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-diagnostics/SKILL.md` | 诊断升级到生产后必须补真实 production direct test/bench/QEMU checksum/板卡结果，并区分接入前后证据。 | diagnostics / documentation | generic-rvv | `rvv-documentation/references/diagnostic-docs.md` | 完整覆盖 | 是 | 保留 production-shaped vs production direct 代码对照和 evidence 区分。 |
| `chats/rvv-documentation/SKILL.md` | 主题文档应覆盖入口作用、标量路径、生产观察、fallback、详细设计、核心片段、数值算例、bench、证据、生产评估、结论。 | documentation | generic-rvv / pcl-adapter | `rvv-documentation/references/topic-doc-structure.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-documentation/SKILL.md` | closeout 后文档采用 current-state-first，先写当前生产状态、RVV 化阶段、标量边界、性能结论和扩展建议。 | documentation | generic-rvv | `rvv-documentation/references/closeout-style.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-documentation/SKILL.md` | 评估文档面向决策审计，主题文档面向长期维护，两者同步但不重复完整历史。 | documentation | generic-rvv | `rvv-documentation/references/evaluation-doc-structure.md` | 完整覆盖 | 否 | 保留。 |
| `chats/rvv-documentation/SKILL.md` | full diagnostic 接近生产路径时，用实验代码段与上游对象状态路径对照，并列出生产面缺口。 | documentation / diagnostics | generic-rvv | `rvv-documentation/references/diagnostic-docs.md` | 完整覆盖 | 是 | 保留代码对照和生产面缺口清单。 |
| `chats/rvv-documentation/SKILL.md` | staging 对照表必须列真实代码符号、代码位置、字段来源、消费者/tail 和 gate。 | documentation | generic-rvv | `rvv-documentation/references/topic-doc-structure.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-documentation/SKILL.md` | RVV helper 与标量 tail 需要双片段展示，说明尾段保留原因。 | documentation | generic-rvv | `rvv-documentation/references/topic-doc-structure.md` | 完整覆盖 | 是 | 保留 RVV helper + scalar tail 双片段模板。 |
| `chats/rvv-documentation/SKILL.md` | bench case 必须说明入口、规模、参数、是否命中 RVV、speedup 计算方式和证明点。 | documentation / benchmarking | generic-rvv | `rvv-documentation/references/topic-doc-structure.md`、`rvv-benchmarking/references/bench-output-contract.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-documentation/SKILL.md` | 数值算例按约定、布局图、标量公式、RVV chunk、对齐结论组织。 | documentation | generic-rvv | `rvv-documentation/references/closeout-style.md`、`rvv-documentation/references/examples-quality-bar.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-documentation/SKILL.md` | QEMU 只作为正确性、日志格式和路径证据；板卡或目标硬件作为真实性能结论。 | documentation / benchmarking | generic-rvv | `rvv-benchmarking/references/qemu-board-disassembly.md`、`rvv-documentation/SKILL.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-documentation/SKILL.md` | 技术文档直接陈述事实，不把结论归因于对话参与者，不用反驳式句式组织核心结论。 | documentation | generic-rvv | `rvv-documentation/references/closeout-style.md` | 完整覆盖 | 否 | 保留。 |
| `chats/rvv-workflow-prompt-skill/background.md` | PCL 当前使用 `doc-rvv`、`test-rvv`、模块筛选、函数级评估、样例文档和 `__RVV10__` 搜索。 | project-config / workflow | pcl-adapter | `rvv-workflow/references/background-pcl.md`、`rvv-project-config/references/library-adapter.md` | 完整覆盖 | 是 | 保留 PCL adapter 路径约定和快速定位方式。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | 公开 API 不变，RVV 在 `__RVV10__` 下，未纳入 RVV 的路径保持语义。 | implementation | generic-rvv | `rvv-implementation/SKILL.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | dense/non-dense、indexed/non-indexed、float/double、小/大规模明确分流。 | implementation | generic-rvv / pcl-adapter | `rvv-implementation/references/fallback-and-dispatch.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | 公共模板头改动后按需补跑上游原始测试；遇到依赖错误先补 Makefile/依赖，不直接写环境阻塞。 | benchmarking / project-config | pcl-adapter | `rvv-benchmarking/references/upstream-and-x86.md` | 完整覆盖 | 是 | 保留常见依赖清单和参数复用规则。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | x86 SSE/AVX 可参考思路，不能照搬；x86 只做同平台 baseline vs SIMD 对照。 | benchmarking / implementation | generic-rvv / pcl-adapter | `rvv-benchmarking/references/upstream-and-x86.md`、`rvv-implementation/SKILL.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | PCL 旧代码格式如函数调用空格风格要保留，不做无关格式化。 | implementation | pcl-adapter | `rvv-implementation/SKILL.md` | 完整覆盖 | 否 | 保留在 adapter 注意点，不提升为 generic。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | 常驻 Std + `__RVV10__` RVV + 公开入口短路选择，未纳入 RVV 的类型自然落回 Std。 | implementation | generic-rvv | `rvv-implementation/references/implementation-patterns.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | 专项 Makefile 同时维护 QEMU 和板卡路径，提供 deploy/run/fetch 入口和可解析本地日志。 | project-config / benchmarking | pcl-adapter | `rvv-project-config/references/makefile-env.md` | 完整覆盖 | 是 | 保留 board/SSH adapter 规则。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | bench 输出必须包含 Dataset、Iterations、case avg、Total Time、checksum，并检查分析日志。 | benchmarking | generic-rvv | `rvv-benchmarking/references/bench-output-contract.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | 板卡 SSH/rsync sandbox 误判需通过 SSH_OPTS、SSH_CMD、RSYNC_SSH、check_board_ssh 处理，技术文档不写运维细节。 | project-config / benchmarking | pcl-adapter / codex-entry | `rvv-project-config/references/makefile-env.md`、`rvv-benchmarking/references/qemu-board-disassembly.md` | 完整覆盖 | 是 | 保留占位符示例和入口目标。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | 新 RVV 组织模式、前置纯函数 RVV staging + 标量状态机、算法等价改写必须在评估和主题文档中单独说明。 | implementation / documentation | generic-rvv | `rvv-implementation/references/implementation-patterns.md`、`rvv-documentation/references/topic-doc-structure.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | 发现 checksum、fallback、1 ulp 异常时按现象、最小复现、路径命中、反汇编、因果实验、回归记录。 | diagnostics / documentation | generic-rvv | `rvv-documentation/references/diagnostic-docs.md`、`rvv-diagnostics/references/semantic-alignment.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | evidence logs 单独归档和单独分组，不混入源码/文档/本机配置。 | benchmarking / commit | generic-rvv | `rvv-benchmarking/references/evidence-logs.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | 已改入生产但目标硬件性能不成立时回收默认生产分流，正确但不加速的实验留为诊断证据。 | diagnostics / implementation | generic-rvv | `rvv-diagnostics/SKILL.md`、`rvv-implementation/references/implementation-patterns.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/prompt.md` | 讨论型问题、实现策略取舍和 workflow 规则来源写入问题与讨论；工作日志只记推进事实和状态。 | handoff / documentation | pcl-adapter / codex-entry | `rvv-workflow/references/topic-entry-template.md`、`rvv-project-config/references/library-adapter.md` | 完整覆盖 | 否 | 保留 PCL adapter 的工作日志与问题讨论分工。 |
| `chats/rvv-workflow-prompt-skill/skill.md` | 四层筛选：已有全库/模块粗筛、模块 second-pass、函数级评估、实现证据筛选。 | workflow / screening | generic-rvv / pcl-adapter | `rvv-workflow/SKILL.md`、`rvv-screening/SKILL.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/skill.md` | 函数级筛选中的中优先级不默认放弃，需尝试或记录具体暂缓原因。 | workflow / screening | generic-rvv | `rvv-implementation/SKILL.md`、`rvv-documentation/references/evaluation-doc-structure.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/skill.md` | 中断恢复必须读取日志、状态表、评估、主题文档和最近证据，确认 closeout 后再换题。 | handoff / workflow | codex-entry | `rvv-workflow/references/recovery-and-handoff.md` | 完整覆盖 | 是 | 保留。 |
| `chats/rvv-workflow-prompt-skill/module_first_pass_prompt_template.md` | first-pass 生成 high/mid/low 文件级粗筛基线，覆盖目标源码全量文件，不进入实现。 | screening | generic-rvv / pcl-adapter | `rvv-screening/references/templates/first-pass-template.md` | 完整覆盖 | 否 | 保留 high/mid/low 定义、全量覆盖、短路径规则、closeout。 |
| `chats/rvv-workflow-prompt-skill/module_second_pass_prompt_template.md` | second-pass 以 high/mid 为必查基线，修正误筛/漏筛，形成三类队列。 | screening | generic-rvv | `rvv-screening/references/templates/second-pass-template.md` | 完整覆盖 | 是 | 保留完整统计字段、主成本覆盖类型、变化理由、输出限制。 |
| `chats/rvv-workflow-prompt-skill/module_followup_rescreen_prompt_template.md` | follow-up 先形成已完成主题证据包，再复筛保留候选，不直接实现。 | screening | generic-rvv | `rvv-screening/references/templates/followup-rescreen-template.md` | 完整覆盖 | 是 | 保留两个证据表、状态分布、bench 诊断状态、doc-rvv 内容边界。 |
| `chats/rvv-workflow-prompt-skill/module_topic_prompt_template.md` | 新对话从状态表选第一条未完成主题，不重新做模块级候选选择。 | workflow | codex-entry | `rvv-workflow/references/topic-entry-template.md` | 完整覆盖 | 是 | 保留短门禁。 |
| `chats/rvv-workflow-prompt-skill/module_topic_prompt_template.md` | 入口模板必须短提醒关键门禁并强制读取相关 skill/reference，避免细则迁移后漏执行。 | workflow | codex-entry | `rvv-workflow/references/topic-entry-template.md` | 完整覆盖 | 是 | 保留“本主题开始前必须读取”。 |
| sincos RVV 复盘 | 回复、文档、`test-rvv` prototype、测试输出和 reviewer 汇报中，英文术语首次出现必须解释；中文主导时给中文解释，英文主导时也要给 plain-English explanation。长测试/诊断文件需要自然的阅读提示，非平凡函数用自然句说明作用、调用者和证据角色，避免翻译腔、名词堆叠和“作用/类别”模板。 | workflow / documentation / diagnostics / implementation / benchmarking | generic-rvv | `rvv-workflow/references/reviewability-and-language.zh.md`，各 RVV skill 入口短提醒 | 完整覆盖 | 是 | 保留通用主规则；具体 skill 只补领域术语。 |
| 旧 chats 术语 | 旧英文标签、无空格旧中文写法、路径索引列等旧术语只作为清理对象。 | documentation | historical-deprecated | 本矩阵和 `rvv-documentation/SKILL.md` 术语段 | 完整覆盖 | 否 | 新文档使用 `local fragment`、`bench 诊断主题`、`production-shaped diagnostic`、`production direct`。 |

## 仍需复查的边界

- 当前矩阵记录的是本轮整理后的判断；后续若发现短句概括、遗漏或互相矛盾，应在对应行更新质量判断并补 reference。
- PCL adapter 规则不应污染 generic RVV 核心，但在当前 PCL 项目内仍是强约束。
- `doc-rvv`、`test-rvv`、模块工作日志、问题与讨论文档、公共 Makefile include、双库对拍和 PCL 旧格式都属于 adapter 层。
- 迁移到其它库时，保留 generic RVV 证据门禁、诊断层级、QEMU/目标硬件边界和文档质量标准；替换路径、测试命令、板卡部署和项目格式规则。
