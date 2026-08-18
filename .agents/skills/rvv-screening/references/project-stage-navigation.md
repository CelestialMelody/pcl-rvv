# RVV 项目阶段导航

本文定义 `rvv-screening` 如何使用项目筛选状态文档判断下一个模块、下一阶段和交接边界。阶段规则和候选口径见 [stage-and-queue-policy.md](stage-and-queue-policy.md)；候选准入标准见 [screening-criteria.md](screening-criteria.md)。

## 读取入口

当任务涉及“下一个模块”、“下一阶段”、“模块状态”、“重新筛选某模块”或“筛选后继续推进”时，先读取：

1. `artifact_layout.screening_index_doc_template`
2. `artifact_layout.screening_module_status_doc_template`
3. `artifact_layout.screening_workflow_status_doc_template`

这些文档是项目状态和推进顺序来源。当前 PCL 默认配置会把这些模板解析到长期文档根下的 `library-screening` 目录；如果 `.agents/config/defaults.yaml` 或本地覆盖改动了路径 / 文件名，按解析结果读取，不凭固定字符串猜路径。`rvv-screening` 的 reference 是规则来源；两者冲突时，规则口径以 skill reference 为准，模块顺序和完成状态以项目筛选状态文档当前内容为准，并在输出中说明冲突。

## 阶段判断

- 没有模块筛查文档：执行阶段 0，全库模块筛查。
- 有模块筛查文档，但目标模块没有文件候选筛选文档：执行第一轮文件候选筛选。
- 有文件候选筛选文档，但没有函数评估队列：执行第二轮函数评估队列。
- 有函数评估队列，且建议队列仍有未完成主题：报告第一条未完成主题并交给 `rvv-workflow`；不要继续做模块级候选选择。
- 有函数评估队列，但建议队列已完成、没有明确下一主题，或已完成主题证据改变排序口径：执行保留候选复筛。
- 有保留候选复筛，且其中仍有 `建议启动函数级评估` 的未完成主题：报告第一条未完成主题并交给 `rvv-workflow`。

## 重做第一轮

用户明确要求重做第一轮时，旧文件候选筛选文档只作为 previous baseline 和覆盖检查参考。以当前源码、筛选标准和文件筛选模板重写同一路径，并在 closeout 中报告：

- high / mid / low 数量变化。
- 主要口径变化。
- 是否发现旧文档漏判、误判或源码冲突。
- 后续函数评估队列应读取的文档路径。

重做第一轮仍不得修改生产源码、建立 topic 测试资产或承诺生产收益。

## 状态文档分工

`artifact_layout.screening_workflow_status_doc_template` 解析出的文档可以保留项目级阶段说明、模块推进顺序和状态索引。不要把它整篇复制到 skill；skill 只保留可复用规则和读取算法。

`artifact_layout.screening_index_doc_template` 和 `artifact_layout.screening_module_status_doc_template` 解析出的文档记录当前模块池、排除模块、暂缓模块和模块顺序。判断“下一个模块”时必须以这些文件的当前内容为准，而不是凭记忆或旧对话。

## 交接边界

筛选阶段只输出候选队列、首阶段证据问题、重新考虑条件和下一份筛选 / workflow 输入。进入单 topic S1-S11、实现、专项 test/bench、QEMU、反汇编、板卡验证、production integration loop 或 closeout 时，切换到 `rvv-workflow`、`rvv-test`、`rvv-implementation` 和 `rvv-documentation` 的对应规则。
