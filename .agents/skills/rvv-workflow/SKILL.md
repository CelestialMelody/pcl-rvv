---
name: rvv-workflow
description: 调度 C/C++ RVV 优化 agent 的 human-in-the-loop 工作流。适用于选择下一主题、从筛选队列进入函数级评估、协调 project-config/screening/diagnostics/implementation/benchmarking/documentation skills、中断恢复、closeout 同步和交接 prompt。
---

# RVV Agent 工作流

本 skill 是入口和调度层，不承载所有细则。执行具体任务时按需调用：

- 项目配置：`rvv-project-config`
- 候选筛选：`rvv-screening`
- 诊断和证据层级：`rvv-diagnostics`
- 生产实现：`rvv-implementation`
- test/bench/QEMU/反汇编/板卡：`rvv-benchmarking`
- 文档和 closeout：`rvv-documentation`

原始 workflow、diagnostics、documentation 规则到当前 `.skills` 的覆盖清单见 [references/rule-coverage.md](references/rule-coverage.md)。如果后续发现新规则，只补覆盖清单和对应 skill/reference，不把大段规则重新塞回本入口。

所有 RVV 工作的回复、文档、测试输出和 `test-rvv` / prototype 注释应遵循 [references/reviewability-and-language.zh.md](references/reviewability-and-language.zh.md)：英文术语首次出现时必须解释；中文主导时给中文解释，英文主导时也要给 plain-English explanation（白话解释），必要时再补中文解释。中文说明要自然，避免翻译腔、名词堆叠和模板填空；长测试/诊断文件提供“本文件做什么”这类阅读提示，非平凡函数用自然句说明作用、调用者和证据角色。

## 普通主题入口

从模块 second-pass 或 follow-up 状态表按推荐顺序选择第一条未完成主题。不要重新做模块级候选选择，除非文档与当前源码存在明确冲突。

主题入口短模板见 [references/topic-entry-template.md](references/topic-entry-template.md)。

## 状态机

1. 读取项目 adapter 和模块状态表。
2. 选择下一主题。
3. 建立或复查函数级评估。
4. 需要诊断时先做 production-shaped diagnostic 或 local fragment 证据。
5. 证据成立后进入 RVV 实现。
6. 补专项 test、bench、QEMU、反汇编和板卡验证。
7. closeout 同步评估文档、主题文档、模块工作日志和状态表。

函数级评估是生产接入门禁。建议队列表示优先评估，不表示跳过检验直接改生产路径。

## 中断恢复

如果任务被打断，先恢复状态，再继续实施。细则见 [references/recovery-and-handoff.md](references/recovery-and-handoff.md)。

## 背景

PCL 是当前验证项目，长期目标是形成可扩展到不同 C/C++ library 的 RVV 优化 agent。背景摘要见 [references/background-pcl.md](references/background-pcl.md)。

## 提交边界

本 skill 不自动提交。需要提交时按职责拆分：

- 生产实现。
- 专项 test/bench 与函数级评估。
- evidence logs。
- 主题文档与状态表。
- 通用经验文档。
- workflow / skill 规则。

不要默认加入忽略的 `.skills` 或 `chats`，除非用户明确要求。
