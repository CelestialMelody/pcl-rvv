# 证据输出与日志策略

本文定义 RVV topic（主题）的 evidence logs（证据日志）策略。

## 默认策略

默认使用 `summary-only`：

- 文档和 Handoff Packet（交接数据包）写摘要、命令和路径。
- raw run 目录、完整反汇编、build（构建）输出和本机日志不默认提交。
- 如果日志包含个人路径、板卡 IP、用户名或私有远端路径，只能留在本机工作区或先脱敏。
- summary artifact（摘要产物）可以临时记录本机 raw archive（原始归档）位置用于当轮溯源，但长期文档和可提交摘要优先使用
  `<local-raw-archive>/...`、`<board-output>/...` 或 env var（环境变量）名等占位符，不把绝对 `/tmp/...`、个人 home（主目录）路径或私有远端路径写成稳定证据入口。

## 可选策略

- `summary-only`：默认策略。不提交日志文件。
- `sanitized-logs`：提交脱敏日志。用户明确要求提交 logs（日志）时默认使用该策略。
- `raw-logs`：提交原始日志。只在用户明确要求保留原文、脱敏日志不足以复核、且 reviewer（审查者）确认无凭据或私有地址风险时使用。

## 提交前检查

提交 evidence logs 前必须：

- 运行 topic 目录提供的 `make sanitize_output_logs` 和 `make check_output_logs_sanitized`，或运行 `artifact_layout.sanitize_logs_script_template` 解析出的脚本并传入 `--check <logs>`。
- 列出将加入的文件和排除的文件。
- 说明是否仍包含本机路径、远端路径、用户名、私有地址或设备标签。
- 说明脱敏是否改变 benchmark（性能测试）数值、checksum（校验和）或命令参数。
- 将 topic 源码 / 文档、evidence logs 和 agent asset（代理资产）拆成独立 commit，除非用户明确要求合并。

## 不默认提交

不要默认提交：

- `build/` 二进制。
- 完整 asm dump（反汇编导出），除非摘要不足以复核。
- `log/vec_missed_log/`。
- 本机 `config.mk`。
- 临时 deploy（部署）脚本。
- 聊天记录。
- raw board fetch（原始板卡抓回）目录。
