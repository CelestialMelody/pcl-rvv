# 单主题入口模板

用于从模块状态表进入一个具体 RVV 主题。

## 先读

1. 项目 adapter 或配置说明。
2. `<module_work_log>`。
3. `<module_second_pass_doc>`。
4. 如存在，`<module_followup_rescreen_doc>`。
5. 如存在，当前模块的问题与讨论文档。
6. 目标源码、同类已完成主题文档和函数级评估。

## 必读 Skill / Reference

开始普通主题前，先读下面的入口和门禁材料；后续按任务需要继续读取更细 reference。

若用户使用短 prompt 启动，先按 `short-prompt-entry.zh.md` 的默认读取链加载，再进入本模板列出的模块材料。

- `rvv-workflow/SKILL.md`：状态机、恢复、closeout 和提交边界。
- `rvv-workflow/references/topic-lifecycle.zh.md`：S0-S12 主干状态、S10 后分支和生产接入闭环。
- `rvv-workflow/references/handoff-packet.zh.md`：worker 阶段边界和 blocked 时的结构化交接字段。
- `rvv-workflow/references/worker-quality-gates.zh.md`：短 prompt worker 写文件前的标量路径、数据流映射、文档、注释、证据和归因门禁。
- `rvv-workflow/references/reviewability-and-language.zh.md`：术语解释、注释密度、文档和 reviewer 汇报规则。
- `rvv-screening/SKILL.md`：确认当前主题来自 second-pass 还是 follow-up，不重新筛选。
- `rvv-documentation/references/evaluation-doc-structure.md`：函数级评估是 production gate。
- `rvv-documentation/references/function-evaluation-and-closeout.zh.md`：S2 evaluation 与 S11 closeout 的文档职责分工。
- `rvv-test/SKILL.md`：测试、诊断、benchmark、消融和 evidence logs 的统一规则。
- `rvv-test/references/test-taxonomy.zh.md`：unit、边界 / 对抗、回归、production-shaped、production direct、fallback 和 smoke 测试分类。
- `rvv-test/references/entry-shapes-and-test-support.zh.md`：local fragment、row source policy、production-shaped diagnostic 和 production direct 分层。
- `rvv-test/references/numerical-consistency.zh.md`：浮点、FRM/FCSR、FMA、reduction、finite mask 和反汇编归属。
- `rvv-test/references/performance-and-ablation.zh.md`：QEMU、板卡、bench 输出、component ablation 和负向归因。
- `rvv-test/references/evidence-doctor.zh.md`：Evidence Doctor（证据体检 / 证据校验器），用于在 benchmark、board summary、checksum、asm attribution 和 EvidenceDecision 前暴露 Errors / Warnings / Suggestions。
- `rvv-test/references/evidence-output-policy.zh.md`：summary-only、sanitized-logs、raw-logs 和提交边界。
- `rvv-test/references/registration-topic-evidence.zh.md`：registration 主题的变换估计、对应关系估计、row source 和法方程证据清单。
- `rvv-implementation/SKILL.md`：Std/RVV 分发、fallback、注释粒度和 PCL 代码风格。
- `rvv-project-config/references/makefile-env.md`：专项 Makefile、board 入口和本机配置隔离。
- `rvv-documentation/SKILL.md`：主题文档、评估文档、筛选状态、诊断文档和 closeout 同步。

如果本主题属于 `bench 诊断主题`、涉及 staging、手工浮点表达式、目标硬件验证、上游测试或生产回退，必须读取对应 reference 后再写实现或结论。

如果用户用短 prompt 启动，仍必须在选中 topic 后执行 `worker-quality-gates.zh.md` 的
“开始写文件前的自查”。短 prompt 只减少用户需要输入的文字，不减少 worker 必须满足的
文档、注释、bench 和证据门槛。

如果用户用短 prompt 继续已有 topic，且最近 Handoff Packet 的结论是
`partial-production-candidate`（局部生产候选），默认进入 PI1 production integration plan
（生产接入计划），不是直接改 production。PI1 涉及模板点类型、traits、字段 offset、fallback
或从具体点类型诊断扩展到 production 模板入口时，必须读取：

- `rvv-implementation/SKILL.md`
- `rvv-implementation/references/point-load-store.md`
- `rvv-implementation/references/fallback-and-dispatch.md`
- 配置或 adapter 指定的 generic point type strategy（泛型点类型策略）文档

## 目标

- 从状态表选择第一条未完成主题。
- 按 `artifact_layout.evaluation_doc_template` 复查或建立函数级评估文档。
- 先回答生产价值：RVV 是否覆盖入口主成本，fallback 和维护边界是否可控。
- 证据不足时收敛为 bench 诊断主题、暂缓或不接生产。
- 证据成立时再进入 RVV 实现、专项 test/bench、QEMU、反汇编、板卡验证和文档 closeout。

## 短提醒

- 从 second-pass 或 follow-up 状态表选择第一条未完成主题；建议队列只授权进入函数级评估，不授权跳过检验直接改生产路径。
- 函数级评估是 production gate：只有 full diagnostic 或 production case 能证明入口主成本、fallback、维护边界和板卡收益成立，才进入生产实现。
- `1.05x ~ 1.2x` 弱收益不能机械接入；只适合入口常用、实现小、fallback 简单、语义风险低且证据完整的路径。
- bench 诊断主题默认授权范围限于专项 test/bench、诊断文档和状态表；升级生产路径必须先有 full diagnostic 或 production case 的稳定目标硬件收益。
- 对 registration（配准）类 topic，如果已有 adopted math family 只在某个 row source policy 上闭合，而其它 policy 仍未尝试同 family，默认先做 family carry-over audit：在配置解析出的 RVV test 资产中补对应 policy 的 candidate、bench 和 board 证据，再决定是否进入 production integration loop。不要把单一 policy 的 positive summary 直接外推成其它 policy 的 production 结论。
- QEMU 不写成性能结论。
- 板卡或目标硬件结果才是性能结论。
- bench 输出必须能解析 `Dataset:`、`Iterations:`、case avg、`Total Time` 和 checksum；格式异常先修 bench 或脚本。
- benchmark、board summary、checksum、asm attribution 或 EvidenceDecision 前必须执行 Evidence Doctor（证据体检）检查；Error 阻塞严格结论，Warning 必须进入 summary / evaluation / Handoff 的风险说明。
- full-cloud、source-indexed、dual-indices 和 correspondences 是不同 row source policy；production 必须逐 policy 独立批准。
- production-shaped diagnostic 和 production direct 分层记录；diagnostic evidence 不能替代 production evidence。
- 若手工展开浮点表达式，检查源码公式、标量反汇编和 RVV intrinsic 求值顺序。
- FMA / reduction 测试需要反汇编归属、误差预算和必要板卡 A/B；不能仅凭源码表达式判断是否允许 fused 指令。
- 若使用显式舍入或修改 FRM/FCSR，保存并恢复调用者浮点环境。
- 若使用 `no-tree-vectorize` 或同类局部优化限制，评估、主题文档和源码注释都要说明限制范围、保护的语义和反汇编证据。
- 如果 RVV 只覆盖前置片段或 staging，文档和 bench 必须区分 local fragment、full diagnostic 和 production case，不能引用局部 speedup 作为生产结论。
- 新增 RVV 注释解释边界和语义，不逐行复述代码。
- 上游原始测试不是每个主题强制项；有对应测试时优先复用仓库测试数据和参数，链接/运行失败先补依赖与 Makefile，不直接写成环境阻塞。
- x86 SIMD 只做同平台 baseline vs SIMD 对照；可参考思路，但不能照搬 x86 单点寄存器粒度。
- 板卡 SSH/rsync 属于 Makefile、workflow 或工作日志层；技术文档只记录目标硬件、日志、数据集、iterations 和真实性能结论。
- evidence logs 默认 `summary-only`，单独分组，不能混入源码、文档或本机配置。
- 已经改入上游但板卡性能不成立时，回收生产分流；正确但不加速的实验保留为诊断证据。
- closeout 同步评估文档、主题文档、模块工作日志和状态表。
- 讨论型问题、实现策略取舍和 workflow 规则来源写入模块“问题与讨论”；模块工作日志只记录推进事实、证据链和状态同步。
