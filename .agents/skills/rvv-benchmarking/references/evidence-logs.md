# Evidence Logs

证据日志用于复核，不是普通开发日志。归档时应保持可读、可追溯、可重跑。

## 推荐分组

- QEMU correctness / checksum 日志。
- QEMU bench compare 和分析日志。
- 反汇编摘要或 dump。
- 板卡 test 和 bench compare 日志。

## 提交边界

默认只在文档和 handoff 中写日志摘要与路径，不提交日志文件。用户明确要求提交时，先冻结 evidence log policy（证据日志策略）：

- `summary-only`：只保留摘要和路径，不提交日志文件。默认策略。
- `sanitized-logs`：提交脱敏日志。用户说要提交 log 时默认采用这个策略。
- `raw-logs`：提交原始日志；只在用户明确要求保留原文、脱敏日志不足以复核、且 reviewer 已确认没有凭据或私有地址风险时使用。

如果需要提交，证据日志单独成组，不混入：

- 生产源码。
- 专项 test/bench 源码。
- 主题文档。
- 通用 workflow 或 skill。

推荐拆分为独立 commit。topic commit、evidence logs commit、agent asset commit 应分开，除非用户明确要求合并。

## 脱敏工具

优先使用仓库内置脚本处理 `test-rvv` 日志：

- topic 已接入公共 Makefile 时，在 topic 目录运行 `make sanitize_output_logs` 改写 `output/qemu/*.log` 和 `output/board/*.log`，再运行 `make check_output_logs_sanitized` 验证。
- 需要跨 topic 或手工列文件时，运行 `python3 test-rvv/script/sanitize_evidence_logs.py --check <logs>`；确认要改写时再加 `--in-place`。
- 脱敏脚本会替换本机路径、板卡路径、运行库路径、SSH target（SSH 目标）和 IP 地址。若日志出现新形态敏感信息，先扩展脚本规则并单独提交，再提交日志。

不要用手工搜索替代脚本检查；手工检查只用于补充说明脚本未覆盖的特殊风险。

## 清理要求

提交前必须检查日志是否包含：

- 个人绝对路径。
- 私有板卡地址。
- 用户名或密钥路径。
- 临时本机配置。

如果原始工具输出不可避免包含敏感配置，应优先调整工具输出、扩展脱敏脚本或只归档脱敏摘要。选择 `raw-logs` 时，不代表跳过检查；它只代表检查后确认可以保留原文。

提交前列出：

- 将加入的日志文件。
- 明确排除的文件，例如 `build/` 二进制、完整反汇编 dump、`log/vec_missed_log/`、本机 `config.mk`、临时 deploy 脚本和聊天记录。
- 脱敏命令和 check 命令。
- 是否仍发现本机路径、远端路径、用户名、私有地址或环境变量。
- 脱敏是否改变 benchmark（性能测试）数值、checksum（校验和）或命令参数。

## 日志完整性

归档日志前确认：

- std/RVV 构建来源清楚。
- 数据集、参数和 iterations 一致或差异已说明。
- QEMU 日志没有被写成性能结论。
- 目标硬件日志包含原始输出和分析表。
- 反汇编证据能定位关键 RVV 指令路径。
- 如果同一主题有 diagnostic 和 production 两类 case，日志名或文档说明能区分。
