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

## 数值语义诊断

手工展开 Eigen 小矩阵、小向量、投影、距离、阈值、float-to-int、floor、规约或其它浮点表达式时，必须同时检查源码公式、标量反汇编和 RVV intrinsic 求值顺序。

重点风险：

- FMA contraction 改变中间舍入。
- FRM/FCSR 污染后续 fallback。
- `static_cast<int>`、floor、像素/cell 边界或阈值 tie 附近的 1 ulp 差异。
- 输出顺序、predicate、indices 或 checksum 被窄边界 lane 改变。

细则见 [references/semantic-alignment.md](references/semantic-alignment.md)。

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
