# 证据输出与日志策略

本文定义 RVV topic（主题）的 evidence logs（证据日志）策略。

## 默认策略

默认使用 `summary-only`：

- 文档和 Handoff Packet（交接数据包）写摘要、命令和路径。
- QEMU 和 board（板卡）证据路径按 `.agents/config/defaults.yaml` 的 `artifact_layout.qemu_output_subdir` 和 `artifact_layout.board_output_subdir` 解析。当前默认值是 `log/qemu` 和 `log/board`。
- output summary（输出摘要）作为 bench / evidence 统计的主归属时，应列出生成脚本、输入日志、被测代码或 bench wrapper、相关文档章节和 Traceability Map 入口。
- raw run 目录、完整反汇编、build（构建）输出和本机日志不默认提交。
- 如果日志包含个人路径、板卡 IP、用户名或私有远端路径，只能留在本机工作区或先脱敏。
- summary artifact（摘要产物）可以临时记录本机 raw archive（原始归档）位置用于当轮溯源，但长期文档和可提交摘要优先使用
  `<local-raw-archive>/...`、`<board-output>/...` 或 env var（环境变量）名等占位符，不把绝对 `/tmp/...`、个人 home（主目录）路径或私有远端路径写成稳定证据入口。

## 脚本归属与目录边界

`paths.test_root/script`，只放跨 topic（主题）复用的通用脚本。典型例子包括
日志脱敏、通用 bench（性能测试）统计、通用反汇编比较、VLEN 探测或多个模块都能直接复用的工具。
`artifact_layout.sanitize_logs_script_template` 这类配置项指向的是通用脚本，不表示所有分析脚本都应放入全局
`script/` 目录。

与当前优化对象强绑定的脚本应放在配置解析出的 topic 测试目录下，例如
`{artifact_layout.topic_test_dir_template}/script/`，或该 topic 既有的等价本地脚本目录。满足任一条件时，
默认视为 topic-bound（主题绑定）脚本：

- 脚本名、参数、正则、case label（用例标签）或输出字段包含当前 topic 的缩写、函数名、helper 名、公式变体或 row source policy（行来源策略）。
- 脚本只解析某个 topic 的 board / QEMU output（板卡 / QEMU 输出）、summary、trace、反汇编符号或候选命名。
- 脚本假设某个 topic 的数据规模、字段布局、点类型、bench wrapper、output 目录结构或日志格式。
- 脚本虽然被同一 topic 的多个 Make target（Make 目标）调用，但离开该 topic 不能作为通用工具直接复用。

数学函数专项按同一原则处理：单个函数强绑定脚本放在 `artifact_layout.math_test_dir_template`
解析出的函数目录或其 `script/` 下；数学函数家族共享脚本可以放在 `artifact_layout.math_test_root_template`
解析出的数学专项根目录；只有跨模块、跨 topic 可复用的脚本才放到 `paths.test_root/script`。

如果发现 topic-bound 脚本误放到 `paths.test_root/script`，应迁回对应 topic 目录，并同步 Makefile、summary、
Traceability Map（可追踪性地图）、evaluation（函数级评估）或 Handoff Packet 中引用的路径。具体误放案例
属于当前 topic 的问题记录、review finding（审查问题）或 Handoff 恢复信息；agent asset 只记录通用归属规则，
不要把单个 topic 的误放案例追加到统一案例文件。

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
- `log/vec_logs/`、`log/latest_vec_missed.log`、`log/filtered_*.log` 和 `log/analyze_*.log`。
- `log/qemu/*.log`、`log/board/*.log`、`log/board/**/run*.log`、`log/board/**/board_env_*.log` 和 `log/board/**/collection_manifest.json`，除非用户明确要求提交脱敏日志或 manifest。
- 本机 `config.mk`。
- 临时 deploy（部署）脚本。
- 聊天记录。
- raw board fetch（原始板卡抓回）目录。
