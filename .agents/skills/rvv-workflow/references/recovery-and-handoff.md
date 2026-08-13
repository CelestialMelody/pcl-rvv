# 中断恢复与交接

结构化交接使用 [handoff-packet.zh.md](handoff-packet.zh.md)。reviewer（审查者）的只读审查、输出格式和 worker prompt patch（给 worker 的提示词补丁）规则见 [reviewer-protocol.zh.md](reviewer-protocol.zh.md)。

## 恢复检查

中断后继续同一模块或主题时，先读取：

1. 模块工作日志。
2. 模块 second-pass 或 follow-up 状态表。
3. 当前主题函数级评估文档。
4. 当前主题 RVV 文档。
5. 最近 QEMU、反汇编和板卡证据摘要。

确认：

- 当前主题是否已经完整 closeout。
- 哪些实现、测试、bench、文档和状态表仍未完成。
- 旧结论是否被新证据推翻。
- 下一步应该继续当前主题，还是进入下一个主题。
- 最近 Handoff Packet 中是否存在 `next_worker_action_if_review_passes`。如果存在且不被用户新指令覆盖，
  该字段是默认续作入口；worker 应按它恢复下一阶段，而不是自行重选 topic 或重跑已完成阶段。

## 交接 prompt 内容

新对话交接 prompt 应包含：

- 仓库占位路径和目标源码。
- 配置解析出的目标测试资产、topic-local 文档和适用的 production 长期主题文档位置；no-production 时 `doc-rvv` 应恢复为 `not_applicable`。
- 应继续参考的项目内 skill。
- 当前 RVV 覆盖范围、fallback 边界和标量 tail。
- 已解决的数值 / 反汇编问题。
- 当前测试数量、已删除 / 合并 / 改名测试及理由。
- 最近 QEMU correctness、checksum、反汇编和板卡结果。
- 下一目标边界、建议先做的判断、必须新增的测试和验证命令。

交接 prompt 以技术状态和下一步执行为主，不写成对话摘要。

如果后续对话使用短 prompt，只保留角色、工作目录和目标。当前 topic 或工作日志路径是可选信息。
默认读取链由 `short-prompt-entry.zh.md` 提供，不需要在 prompt 中重复列出。

推荐短 prompt：

```text
在 <repo> 中，以 RVV worker 身份继续当前 topic。
请按当前 Handoff Packet 的 next_worker_action_if_review_passes 恢复，并保存本轮 work log。
```

如果用户没有显式写出 `next_worker_action_if_review_passes`，但说了“继续当前 topic”或“进入下一阶段”，
worker 仍按同一规则读取最近 Handoff Packet 并使用该字段。
