---
name: rvv-screening
description: 组织 C/C++ 高性能库的 RVV 候选筛选流程。适用于全库或模块 first-pass、模块 second-pass、follow-up rescreen、候选队列维护、函数级评估入口选择，以及需要把源码事实整理成可执行 RVV 优化队列但尚不进入实现的任务。
---

# RVV 筛选工作流

使用本 skill 时，只做源码阅读、静态分析、轻量入口确认和筛选文档；不修改生产源码，不建立专项 test/bench 目录，不运行板卡性能验证。

## 入口选择

- 全库或模块粗筛：使用 [references/templates/first-pass-template.md](references/templates/first-pass-template.md)。
- 已有 first-pass 后形成实施队列：使用 [references/templates/second-pass-template.md](references/templates/second-pass-template.md)。
- 建议队列完成或已完成主题反哺排序：使用 [references/templates/followup-rescreen-template.md](references/templates/followup-rescreen-template.md)。

## 核心规则

1. first-pass 回答“文件中是否存在值得继续下钻的可 SIMD/RVV 片段”，只形成 `high/mid/low` 粗筛基线。
2. second-pass 必须把 first-pass 的 `high/mid` 候选逐项交代去向；如果纳入 `low` 或遗漏文件，必须说明源码证据和补入边界。
3. second-pass 固定使用三类结果：`建议进行 RVV 优化的文件`、`保留实施的候选文件`、`暂缓或不推荐考虑 RVV 优化的文件`。
4. follow-up rescreen 必须先总结已完成主题的真实性能、回退原因、诊断价值和可复用模式，再复筛保留候选。
5. 筛选队列只授权进入函数级评估；是否接入生产由 full diagnostic、生产入口证据、测试、反汇编和板卡结果决定。

## 文档口径

- 分析表和执行状态表分离。
- 候选较多时先给统计，再按推荐动作拆成分组表。
- 执行清单保留 `顺序`、`主题`、`主文件`、`推荐入口 / 第一 RVV 目标`、`状态`、`当前结论 / 下一步条件`。
- 不把可按命名规则推导的长文档位置放进执行清单常规列。
- 判断依据必须来自源码、测试入口、已有主题证据、bench 结果或当前文档，不写成对话来源。

## 输出边界

closeout 时报告：

- 筛选文档位置；
- high/mid/low 或三类队列数量；
- 第一条未完成主题；
- 是否发现需要补充到 workflow、diagnostics、implementation、benchmarking 或 documentation skill 的通用规则。
