---
name: rvv-screening
description: 组织 C/C++ 高性能库的 RVV 候选筛选流程。适用于全库或模块文件候选筛选、模块函数评估队列、第三轮 / 保留候选复筛、候选队列维护、函数级评估入口选择，以及需要把源码事实整理成可执行 RVV 优化队列但尚不进入实现的任务。
---

# RVV 筛选工作流

使用本 skill 时，只做源码阅读、静态分析、轻量入口确认和筛选文档；不修改生产源码，不建立专项 test/bench 目录，不运行板卡性能验证。

## 入口选择

先读最小必要 reference，再按任务读取对应模板：

- 所有筛选任务：先读 [references/screening-criteria.md](references/screening-criteria.md) 和 [references/stage-and-queue-policy.md](references/stage-and-queue-policy.md)。
- 需要写证据、边界或 closeout：再读 [references/evidence-boundaries.md](references/evidence-boundaries.md)。

- 全库或模块文件候选筛选：使用 [references/templates/file-candidate-screening-template.md](references/templates/file-candidate-screening-template.md)。
- 文件候选筛选后形成函数评估队列：使用 [references/templates/function-evaluation-queue-template.md](references/templates/function-evaluation-queue-template.md)。
- 函数评估队列完成或已完成主题反哺排序：使用 [references/templates/second-pass-retained-candidate-rescreen-template.md](references/templates/second-pass-retained-candidate-rescreen-template.md)。语义是复筛第二轮保留候选。

## 规则

1. 文件候选筛选回答“文件中是否存在值得继续下钻的可 SIMD/RVV 片段”，只形成 `high/mid/low` 粗筛基线。
2. 函数评估队列必须把文件候选筛选的 `high/mid` 候选逐项交代去向；如果纳入 `low` 或遗漏文件，必须说明源码证据和补入边界。
3. 函数评估队列固定使用三类结果：`建议进行 RVV 优化的文件`、`保留实施的候选文件`、`暂缓或不推荐考虑 RVV 优化的文件`。
4. 第三轮 / 保留候选复筛必须先总结已完成主题的真实性能、回退原因、诊断价值和可复用模式，再复筛函数评估队列 `保留实施的候选文件`。
5. 第三轮 / 保留候选复筛使用 `建议启动函数级评估` 和 `暂缓 / 不单独实施` 两类主分组；diagnostic / bench 是后续 topic 内的证据路径，不是模块复筛固定队列。
6. 筛选队列只授权进入函数级评估；是否接入生产由 full diagnostic、生产入口证据、测试、反汇编和板卡结果决定。

## 文档口径

- 分析表和执行状态表分离。
- 候选较多时先给统计，再按推荐动作拆成分组表。
- 执行清单保留 `顺序`、`主题`、`主文件`、`推荐入口 / 第一 RVV 目标`、`状态`、`当前结论 / 下一步条件`。
- 不把可按命名规则推导的长文档位置放进执行清单常规列。
- 判断依据必须来自源码、测试入口、已有主题证据、bench 结果或当前文档，不写成对话来源。

## 输出边界

closeout 时报告：

- 筛选文档位置；
- 文件候选筛选的 high/mid/low、函数评估队列三类队列，或第三轮 / 保留候选复筛两类队列数量；
- 第一条未完成主题；
- 是否发现需要补充到 workflow、rvv-test、implementation 或 documentation skill 的通用规则。
