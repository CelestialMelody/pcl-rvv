---
name: rvv-diagnostics
description: 设计、重构或审查 C/C++ 高性能库的 RVV 诊断代码和证据链。适用于 test-rvv 诊断入口、production-shaped diagnostic、低层 helper、staging 分层、语义对齐、local fragment 与生产入口关系、以及生产接入前的证据判断。
---

# RVV 诊断工作流

使用本 skill 时，目标是证明一个 RVV 候选是否具备生产接入价值，而不是只证明局部代码可以写成 intrinsic。

回复、诊断文档和 `test-rvv` 代码注释遵循 `rvv-workflow/references/reviewability-and-language.zh.md`：英文术语首次出现时必须解释；中文主导时给中文解释，英文主导时也要给 plain-English explanation（白话解释）。中文说明要自然，避免翻译腔；长诊断文件提供“本文件做什么”这类阅读提示；非平凡函数说明作用、调用者和证据角色。尤其要解释 `entry shape（入口形态）`、`local fragment（局部片段）`、`production-shaped diagnostic（生产形态诊断）`、`production direct（直接生产路径证据）` 和 `gate（可失败验收条件）`。

## 先选诊断入口

- 类式算法优先建立 test-only 派生诊断类，复用公开 setter、基类准备流程、indices/mask/input 生命周期和输出顺序。
- free function 或纯 helper 保持同名或同形参数；可以增加 test-only wrapper，但 full correctness 和 full bench 应能映射到真实入口。
- 低层 helper 可用于隔离 RVV 阶段，不能替代入口层证据。
- 如果 production（生产源码）通过 iterator（迭代器）、wrapper、dispatch（分流逻辑）或模板 helper 把多种入口统一成同一个循环，诊断设计必须先写清源码真实数据流，再解释为什么 RVV 需要显式拆成连续扫描、indices（索引）、correspondences（对应关系）、gather（离散加载）或 scatter（离散写回）路径。不要让读者误以为诊断里的两条数据流就是 production helper 中直接可见的两段代码。

诊断入口形态细则见 [references/entry-shapes.md](references/entry-shapes.md)。

## 证据层级

按下面顺序组织，不跳级：

```text
local correctness -> local microbench -> entrance/full evidence -> production decision
```

- `local correctness`：证明局部 RVV 片段与对应标量片段一致。
- `local microbench`：证明局部片段有潜在收益。
- `entrance/full evidence`：覆盖公开调用形状、对象状态、indices、mask、模板点类型或输出顺序。
- `production decision`：综合入口级证据、fallback、维护成本、上游测试和板卡收益。

staging、测试矩阵和证据层级细则见 [references/staging-and-evidence.md](references/staging-and-evidence.md)。

## Fallback gate 复核

fallback（回退路径）不是只要结果一致就算闭合。诊断代码同时存在规模阈值、identity / contiguous / finite / type / layout 等多个 gate（可失败验收条件）时，测试矩阵必须尽量隔离 gate 原因：

- 小规模 fallback 单独证明阈值 gate。
- 非连续、乱序、重复或 indexed/gather 输入单独证明语义 gate。
- 不支持的点类型、scalar 或布局单独证明类型 / 布局 gate。
- 如果某个 gate 只能在文档中说明而无法测试，必须写清为什么当前阶段不能测，以及生产接入前需要补什么证据。

不要把一个同时低于规模阈值又不满足语义条件的 case 写成完整 fallback 证据；它只能证明候选没有走 RVV，不能证明具体 gate 的维护边界正确。

## 数值语义诊断

手工展开 Eigen 小矩阵、小向量、投影、距离、阈值、float-to-int、floor、规约或其它浮点表达式时，必须同时检查源码公式、标量反汇编和 RVV intrinsic 求值顺序。

重点风险：

- FMA contraction 改变中间舍入。
- FRM/FCSR 污染后续 fallback。
- `static_cast<int>`、floor、像素/cell 边界或阈值 tie 附近的 1 ulp 差异。
- 输出顺序、predicate、indices 或 checksum 被窄边界 lane 改变。

细则见 [references/semantic-alignment.md](references/semantic-alignment.md)。

## 实现选择审计

RVV 诊断不能只证明“某段代码能写成 intrinsic”。当候选实现保留大量 scalar tail（标量尾段）、使用固定 buffer/staging、`vcompress`、gather/scatter、显式或非显式 fused multiply-add（融合乘加）、vector reduction（向量规约）或数学函数 helper 时，必须把这些选择写成可审查的设计决策：

- 标量路径原本做什么，哪些中间量被 RVV 生产，哪些仍交给标量 tail 消费。
- 为什么使用 buffer/staging，而不是继续在向量寄存器里累加、规约或写回。
- 为什么使用或暂缓 fused intrinsic；如果标量构建可能发生 FMA contraction（融合乘加收缩），应查看标量和 RVV 反汇编，不要凭源码形状下结论。
- 为什么暂缓 vector reduction；若只是因为累加顺序改变，要说明误差预算、对抗样本和板卡收益证据还缺什么。
- 数学函数、矩阵构造、solver 前后处理等只在外层或每次迭代末尾调用的代码，先估算调用频率和耗时占比，再决定是否值得向量化。

这些内容可以写在 evaluation（评估）文档或主题文档中；复杂 topic 建议同时在代码注释中给出短说明。不要把一次 topic 的具体数值上升为通用禁令，但要让 reviewer 能判断当前方案的弱收益是否来自实现方式、入口主成本还是证据不足。

## 生产接入判断

可以进入生产接入评审的信号：

- production-shaped candidate 在板卡 full case 有稳定收益。
- checksum 一致，测试矩阵通过。
- fallback 条件清晰，只替换已证明安全的阶段。
- 上游源码改动保持小范围，维护成本与收益匹配。

应暂缓或保留为 bench 诊断主题的信号：

- 只有 local fragment 快，full 入口被后续状态机、sort/search、target gather 或输出写回稀释。
- 当前 board test 未覆盖新增测试。
- 增量 RVV 阶段已有语义反例。
- 生产模板需要大量 helper 和复杂分流，但收益偏弱。

诊断升级为生产源码后，必须补真实生产入口的专项测试、bench、QEMU checksum、反汇编和板卡结果。

## bench 诊断升级生产

bench 诊断主题不是删除项，也不是低价值项。它用于保留“局部 RVV 可能成立但生产主路径覆盖不确定”的证据入口。

升级到生产路径前，必须同时满足：

- full diagnostic 或 production case 在目标硬件上稳定收益。
- RVV 覆盖入口主成本，或入口极常用且改动很小。
- checksum、专项测试和必要上游测试通过。
- 反汇编命中预期 RVV 指令路径。
- fallback 边界清晰。
- 语义风险和维护复杂度可控。

若 local fragment 正确且加速，但 full diagnostic 被 sort、search、map、Eigen、状态机、整点复制或输出写回稀释到不可稳定归因，默认不接生产，只保留诊断证据。

## 主成本稀释复核

当收益被后续主成本稀释时，评估和主题文档必须补源码链路复核：

- 公开入口调用点。
- wrapper / virtual / dispatch 层。
- 真实实现层。
- 主要耗时代码。
- 该主成本是否适合当前 RVV 模式。
- local fragment、full diagnostic、production case 的耗时拆分。
- 可 RVV 片段在 full 入口中的占比和理论收益上限。

不能只写“被稀释”。若热点边界可定位但内部不是适合手写 RVV 的批量算子，应说明重新评估需要的新算法、批量接口、数据布局或 profile 证据。

## 生产回收

如果 RVV 方案已经改入生产源码，但目标硬件验证显示真实性能不成立，应回收默认生产分流，避免普通 RVV 构建启用退化路径。正确但不加速的实验可以保留在专项 bench 诊断代码或显式诊断宏中。

评估文档、主题文档和工作日志必须记录：

- 回收范围。
- 保留的 correctness、bench 和反汇编证据。
- 目标硬件结果。
- 不接生产原因。
- 后续重新评估条件。
