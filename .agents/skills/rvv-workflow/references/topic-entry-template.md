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

- `rvv-workflow/SKILL.md`：状态机、恢复、closeout 和提交边界。
- `rvv-workflow/references/rule-coverage.md`：确认高风险规则是否已有入口短提醒和详细落点。
- `rvv-screening/SKILL.md`：确认当前主题来自 second-pass 还是 follow-up，不重新筛选。
- `rvv-documentation/references/evaluation-doc-structure.md`：函数级评估是 production gate。
- `rvv-diagnostics/SKILL.md`：local fragment、full diagnostic、production decision 的证据层级。
- `rvv-diagnostics/references/semantic-alignment.md`：浮点、FRM/FCSR、FMA contraction 和反汇编语义对齐。
- `rvv-diagnostics/references/staging-and-evidence.md`：staging、标量 tail、测试矩阵和 bench 命名。
- `rvv-implementation/SKILL.md`：Std/RVV 分发、fallback、注释粒度和 PCL 代码风格。
- `rvv-benchmarking/SKILL.md`：QEMU、反汇编、目标硬件、bench 输出合同和 evidence logs。
- `rvv-project-config/references/makefile-env.md`：专项 Makefile、board 入口和本机配置隔离。
- `rvv-documentation/SKILL.md`：主题文档、评估文档、筛选状态、诊断文档和 closeout 同步。

如果本主题属于 `bench 诊断主题`、涉及 staging、手工浮点表达式、目标硬件验证、上游测试或生产回退，必须读取对应 reference 后再写实现或结论。

## 目标

- 从状态表选择第一条未完成主题。
- 复查或建立 `test-rvv/<module>/<topic>/<topic>-evaluation.zh.md`。
- 先回答生产价值：RVV 是否覆盖入口主成本，fallback 和维护边界是否可控。
- 证据不足时收敛为 bench 诊断主题、暂缓或不接生产。
- 证据成立时再进入 RVV 实现、专项 test/bench、QEMU、反汇编、板卡验证和文档 closeout。

## 短提醒

- 从 second-pass 或 follow-up 状态表选择第一条未完成主题；建议队列只授权进入函数级评估，不授权跳过检验直接改生产路径。
- 函数级评估是 production gate：只有 full diagnostic 或 production case 能证明入口主成本、fallback、维护边界和板卡收益成立，才进入生产实现。
- `1.05x ~ 1.2x` 弱收益不能机械接入；只适合入口常用、实现小、fallback 简单、语义风险低且证据完整的路径。
- bench 诊断主题默认授权范围限于专项 test/bench、诊断文档和状态表；升级生产路径必须先有 full diagnostic 或 production case 的稳定目标硬件收益。
- QEMU 不写成性能结论。
- 板卡或目标硬件结果才是性能结论。
- bench 输出必须能解析 `Dataset:`、`Iterations:`、case avg、`Total Time` 和 checksum；格式异常先修 bench 或脚本。
- 若手工展开浮点表达式，检查源码公式、标量反汇编和 RVV intrinsic 求值顺序。
- 若使用显式舍入或修改 FRM/FCSR，保存并恢复调用者浮点环境。
- 若使用 `no-tree-vectorize` 或同类局部优化限制，评估、主题文档和源码注释都要说明限制范围、保护的语义和反汇编证据。
- 如果 RVV 只覆盖前置片段或 staging，文档和 bench 必须区分 local fragment、full diagnostic 和 production case，不能引用局部 speedup 作为生产结论。
- 新增 RVV 注释解释边界和语义，不逐行复述代码。
- 上游原始测试不是每个主题强制项；有对应测试时优先复用仓库测试数据和参数，链接/运行失败先补依赖与 Makefile，不直接写成环境阻塞。
- x86 SIMD 只做同平台 baseline vs SIMD 对照；可参考思路，但不能照搬 x86 单点寄存器粒度。
- 板卡 SSH/rsync 属于 Makefile、workflow 或工作日志层；技术文档只记录目标硬件、日志、数据集、iterations 和真实性能结论。
- evidence logs 单独分组，不能混入源码、文档或本机配置。
- 已经改入上游但板卡性能不成立时，回收生产分流；正确但不加速的实验保留为诊断证据。
- closeout 同步评估文档、主题文档、模块工作日志和状态表。
- 讨论型问题、实现策略取舍和 workflow 规则来源写入模块“问题与讨论”；模块工作日志只记录推进事实、证据链和状态同步。
