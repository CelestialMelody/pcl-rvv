# 第三轮 / 保留候选复筛模板

用于建议队列完成、没有明确下一主题，或已完成主题证据显示需要重新比较函数评估队列中的保留候选时。本文正式语义是第三轮 / 保留候选复筛。目标是用真实实现和板卡结果反哺排序。

模板文件名中的 `second-pass-retained-candidate-rescreen` 表示“复筛第二轮保留候选”，不是重新执行第二轮函数评估队列。

## 输入

- `<module_work_log>`
- `<module_function_evaluation_queue_doc>`
- 函数评估队列 `3.2 保留实施的候选文件`。
- 已完成主题文档。
- 已完成主题函数级评估。
- QEMU、反汇编、板卡日志摘要。

## 输出

```text
artifact_layout.module_second_pass_retained_candidate_rescreen_doc_template
```

## 已完成主题证据包

每个已完成主题至少抽取：

- 主题名和主文件。
- 函数评估队列原始定位。
- 实际覆盖范围：生产主路径、production-shaped diagnostic、full diagnostic、local fragment、bench-only / no-production 或已回退。
- 板卡结论：强收益、中高收益、弱收益、无收益 / 回退、暂缺。
- 正确性证据：专项测试、上游测试、checksum、fallback case。
- 反汇编证据：是否确认 RVV 指令路径。
- 回退 / 暂缓原因。
- 可复用模式和失败模式。
- 对后续候选的影响。

已完成主题经验总结至少包含两个表：

```text
| 主题 | 主文件 | 函数评估队列原始定位 | 实际覆盖范围 | 目标硬件结论 | 正确性证据 | 反汇编证据 | 生产接入状态 | 回退 / 暂缓原因 |
```

```text
| 模式标签 | 来自哪些已完成主题 | 成立条件 | 失败 / 回退边界 | 对后续候选的影响 |
```

表中应明确哪些主题是强收益生产主路径、弱收益但可维护的生产主路径、正确但目标硬件性能不成立的诊断路径，以及已尝试后回退或暂缓的函数。

## 固定分组

按 [../stage-and-queue-policy.md](../stage-and-queue-policy.md) 的 retained-candidate rescreen 规则和 [../screening-criteria.md](../screening-criteria.md) 的候选判定标准分类。本模板只规定复筛文档的证据槽位、统计字段和执行清单形状。

- `建议启动函数级评估`
- `暂缓 / 不单独实施`

`建议启动函数级评估` 表示模块级后续主题入口，不表示可以跳过检验直接修改上游生产路径。每个主题必须写清 `默认评估路径 / 首阶段证据问题`，由单 topic 的 `rvv-workflow` / `rvv-test` 决定后续走 production-value evaluation、production-shaped diagnostic、component ablation、profile prerequisite 或 no-production confirmation。

`diagnostic`、`bench-only` 和 `production-shaped diagnostic` 是后续 topic 内的证据路径或 EvidenceDecision 结果，不是本复筛文档的固定候选队列。若局部 RVV 点还不能代表真实入口，通常放入 `暂缓 / 不单独实施`，并写清重新考虑条件。

## 文档结构

1. 输入依据与复筛原因。
2. 筛选统计。
3. 已完成主题经验总结。
4. 筛选口径修正 / 复筛变化理由。
5. 保留实施候选逐项复筛。
6. 新的执行清单 / 状态表。

## 筛选统计字段

统计至少包含：

- 保留实施候选输入总数。
- 建议启动函数级评估的主题数量。
- 暂缓 / 不单独实施的主题数量。
- `建议启动函数级评估` 中默认评估路径的分布，例如 production-value evaluation、production-shaped diagnostic、component ablation、profile prerequisite、no-production confirmation。
- 重新纳入、合并、删除或源码冲突的主题数量和原因。

## 逐项复筛字段

候选较多时，不使用一个超宽总表承载全部信息。第 5 节按推荐动作拆成小表：

- `建议启动函数级评估`：主题、关键入口、主成本覆盖类型、默认评估路径 / 首阶段证据问题、匹配的已验证模式、主要风险、推荐理由、证据来源。
- `暂缓 / 不单独实施`：主题、主成本覆盖类型、暂缓原因、重新考虑条件、证据来源。

第 5 节解释分类理由，第 6 节只负责执行顺序和状态跟踪。执行清单字段优先使用 `顺序`、`主题`、`主文件`、`推荐入口 / 第一 RVV 目标`、`依据模式 / 证据来源`、`状态`、`当前结论 / 下一步条件`。

提交型复筛文档不要写入内部操作入口、prompt 路径、对话路径或 adapter 标记为工作日志 / 讨论记录的路径。此类信息只保留在工作日志或入口模板中，不能成为提交型技术文档内容。

本轮不进入 RVV 实现，不建立专项目录，不运行板卡 bench。
