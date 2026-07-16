# Evidence Logs

证据日志用于复核，不是普通开发日志。归档时应保持可读、可追溯、可重跑。

## 推荐分组

- QEMU correctness / checksum 日志。
- QEMU bench compare 和分析日志。
- 反汇编摘要或 dump。
- 板卡 test 和 bench compare 日志。

## 提交边界

如果需要提交，证据日志单独成组，不混入：

- 生产源码。
- 专项 test/bench 源码。
- 主题文档。
- 通用 workflow 或 skill。

## 清理要求

日志不得包含：

- 个人绝对路径。
- 私有板卡地址。
- 用户名或密钥路径。
- 临时本机配置。

如果原始工具输出不可避免包含敏感配置，应优先调整工具输出或只归档脱敏摘要。

## 日志完整性

归档日志前确认：

- std/RVV 构建来源清楚。
- 数据集、参数和 iterations 一致或差异已说明。
- QEMU 日志没有被写成性能结论。
- 目标硬件日志包含原始输出和分析表。
- 反汇编证据能定位关键 RVV 指令路径。
- 如果同一主题有 diagnostic 和 production 两类 case，日志名或文档说明能区分。
