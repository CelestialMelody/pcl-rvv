---
name: rvv-documentation
description: 编写、重排或审查 C/C++ RVV 优化文档。适用于主题 RVV 文档、函数级评估、模块筛选报告、closeout 整理、bench case 解释、staging 表、测试保留策略、生产接入判断和跨文档同步。
---

# RVV 文档工作流

使用本 skill 时，目标是产出离开当前对话后仍可维护的技术文档。读者应能仅凭源码、测试、bench 日志和文档恢复函数职责、标量路径、RVV 边界、证据链、生产接入判断和遗留风险。

术语解释和可审查性规则见 `rvv-workflow/references/reviewability-and-language.zh.md`。文档中英文术语首次出现时必须解释；中文主导文档给中文解释，英文主导文档也要给 plain-English explanation（白话解释），必要时补中文解释。中文文档应使用自然工程说明，避免翻译腔、名词堆叠和模板填空。如果文档引用 `test-rvv` prototype、诊断入口或 bench case，应解释其证据角色和不能覆盖的边界。

## 文档类型

- 主题 RVV 文档：见 [references/topic-doc-structure.md](references/topic-doc-structure.md)。
- 函数级评估文档：见 [references/evaluation-doc-structure.md](references/evaluation-doc-structure.md)。
- 筛选文档：见 [references/screening-docs.md](references/screening-docs.md)。
- 诊断和回退文档：见 [references/diagnostic-docs.md](references/diagnostic-docs.md)。
- closeout 重排和写作风格：见 [references/closeout-style.md](references/closeout-style.md)。
- 通用写作风格：见 [references/writing-style.md](references/writing-style.md)。
- 当前 PCL 项目满意样例抽出的质量门槛：见 [references/examples-quality-bar.md](references/examples-quality-bar.md)。

## 通用规则

- 文档使用仓库相对路径和占位符，不写个人路径、私有地址或设备内部绝对路径。
- QEMU 只写成正确性、日志格式和路径证据；性能结论来自板卡或目标硬件。
- 生产接入判断必须连接 local fragment、full diagnostic、production case、fallback 和维护成本。
- 评估文档负责决策审计；主题文档负责长期维护。
- 筛选文档负责队列和状态，不承担实现事实的长期解释。
- 诊断文档必须区分授权边界：局部实验、production-shaped diagnostic、production direct 或生产回退。
- 技术结论以源码、测试、日志和反汇编为依据，不写成对话来源。
- 如果规则来自一次讨论或复盘，技术文档写技术事实；规则来源写入 workflow 或问题讨论记录。
- 未闭合项、剩余风险和 production gate 不能只列术语；必须用陈述句说明每项是什么、为什么未闭合、完成后能证明什么或降低什么风险、当前阶段是否必须完成。具体写法见 `rvv-workflow/references/reviewability-and-language.zh.md`。

## 术语

推荐使用：

- `local fragment`
- `production-shaped diagnostic`
- `production direct`
- `bench 诊断主题`
- `上游生产分流`
- `回退标量路径`

旧表述只作为迁移清理对象，不作为新文档术语。

## 同步边界

主题文档修改后，如果影响生产接入结论、bench case 含义、新增诊断失败、workflow 规则或筛选状态，应同步函数级评估、模块工作日志和相关筛选状态表。

如果一次主题工作暴露出可复用文档规则，应沉淀到本 skill 或 reference，避免规则只停留在一次主题文档中。

## 质量门槛

新文档至少应回答：

- 入口是什么，调用链如何进入目标函数。
- 标量路径哪一段被 RVV 接管，哪一段仍是标量。
- gate 和 fallback 如何保持公开语义。
- 关键 RVV 片段是否覆盖完整阶段，是否只贴公式。
- 数值算例或图示是否能让读者手工对齐一个 VL chunk。
- 每个 bench case 证明什么，性能结论来自哪里。
- 生产接入、bench 诊断或回退判断是否由 full evidence 支撑。
- 未闭合项是否让读者知道“要做什么”和“做了有什么用”，而不是只看到名词清单。
