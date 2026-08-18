# RVV 筛选证据与执行边界

本文定义 `rvv-screening` 的证据记录、执行边界和跨 skill 交接口径。候选准入标准见 [screening-criteria.md](screening-criteria.md)；阶段和队列语义见 [stage-and-queue-policy.md](stage-and-queue-policy.md)。

## 证据记录要求

筛选文档中的判断依据必须是可复核事实：

- 源码路径、函数名、loop 位置或调用链。
- trip count 由什么输入决定，以及是否可能为空、小规模或常驻大规模。
- 运算、访存、分支、依赖、输出语义和测试入口。
- 主成本覆盖类型和被其它阶段稀释的可能性。
- 已完成主题、benchmark、profile、反汇编或上游 test 的具体证据。

不要写“感觉适合向量化”“应该有收益”“对话中认为”等不可复核依据。缺证据时写成待确认问题，并选择 `mid`、`保留实施`、`profile prerequisite` 或 `暂缓`。

## 筛选执行边界

执行 `rvv-screening` 时只做源码阅读、静态分析、轻量入口确认和筛选文档。

不要在筛选阶段执行：

- 修改 production 源码。
- 建立 topic test/bench 目录。
- 运行目标硬件 benchmark。
- 承诺 production 接入、最终收益或公开入口主成本覆盖。

可以做非破坏性源码阅读、搜索、静态分析和必要的轻量构建或测试入口确认。发现文件候选筛选与当前源码冲突时，在当前阶段文档中记录并修正去向；不要扩展成全库重新筛选，除非用户明确要求。

## 交接边界

筛选队列只授权进入函数级评估。是否进入 production 由 `rvv-workflow` 和 `rvv-test` 的 S2-S10、phase loop、EvidenceDecision、production integration loop、目标硬件证据和用户确认决定。

筛选输出可以提出：

- 第一 RVV 目标。
- 默认评估路径或首阶段证据问题。
- 需要建立的 correctness、QEMU、反汇编、board bench 或 production-shaped diagnostic 证据。
- 暂缓项的重新考虑条件。

筛选输出不要替代：

- 函数级 implementation 设计。
- 专项 test/bench 目录结构。
- 生产分流策略。
- 目标硬件收益结论。

## 与模板分工

本文件定义证据和边界语义。`references/templates/` 只定义不同阶段文档的输入、输出、表格字段、统计字段和 closeout 口径。

更新筛选标准时改 [screening-criteria.md](screening-criteria.md)；更新阶段/队列命名时改 [stage-and-queue-policy.md](stage-and-queue-policy.md)；更新文档形状时改模板。
