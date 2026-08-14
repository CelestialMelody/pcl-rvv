# 证据输出与日志策略

本文定义 RVV topic（主题）的 evidence logs（证据日志）策略。

## 默认策略

默认使用 `summary-only`：

- 文档和 Handoff Packet（交接数据包）写摘要、命令和路径。
- QEMU 和 board（板卡）证据路径按 `.agents/config/defaults.yaml` 的 `artifact_layout.qemu_output_subdir` 和 `artifact_layout.board_output_subdir` 解析。当前默认值是 `log/qemu` 和 `log/board`。
- output summary（输出摘要）作为 bench / evidence 统计的主归属时，应列出生成脚本、输入日志、被测代码或 bench wrapper、相关文档章节和 Traceability Map 入口。
- `evidence_doctor.md`、`evidence_doctor.json` 或 summary 内的 Evidence Doctor（证据体检）小节属于 summary artifact（摘要证据产物），不是 raw log。它可以进入提交候选，但必须被 `evidence.committable_log_reference_roots` 解析出的文档根明确引用，并且不能包含个人路径、私有板卡地址或未脱敏 raw log 片段。
- raw run 目录、完整反汇编、build（构建）输出和本机日志不默认提交。
- `artifact_layout.qemu_output_subdir` 或 `artifact_layout.board_output_subdir` 解析目录下的生成证据只有在 `evidence.committable_log_reference_roots` 解析出的文档根中被明确引用时才进入提交候选。这里的“生成证据”包括 correctness / unit test run log（正确性 / 单元测试运行日志）、bench analyze log（性能分析日志）、summary artifact、checksum、asm attribution、Evidence Doctor report（证据体检报告）等。没有被文档引用的日志、摘要、manifest（清单）或环境探测文件，即使已经生成，也默认留在本机工作区。
- 如果日志包含个人路径、板卡 IP、用户名或私有远端路径，只能留在本机工作区或先脱敏。
- summary artifact（摘要产物）可以临时记录本机 raw archive（原始归档）位置用于当轮溯源，但长期文档和可提交摘要优先使用
  `<local-raw-archive>/...`、`<board-output>/...` 或 env var（环境变量）名等占位符，不把绝对 `/tmp/...`、个人 home（主目录）路径或私有远端路径写成稳定证据入口。

## Evidence Freshness（证据新鲜度）

bench、board summary、Evidence Doctor 或 checksum summary 一旦被重新运行，worker 必须判断新 run 是否改变了已经写入 phase result、evaluation、topic 文档、README 或 Handoff Packet 的数值结论。

- 如果新 run 与旧文档数值一致，可以在 Handoff 中写 `fresh`，并列出 run label / summary path。
- 复跑不追求每个耗时数字完全一致。phase plan 应先写明 run budget（复跑预算）、统计口径和 decision bucket（决策桶，例如 positive / weak-positive / neutral / negative / unstable）。如果精确数值波动但 decision bucket、方向和 EvidenceDecision 不变，长期文档可以只引用新的 summary / run label，不需要复制每个易漂移数字。
- 如果新 run 改变了方向、decision bucket、speedup 结论、Warning / Error 数量、run count、输入规模或 evidence role（证据角色），旧 summary 和旧 phase result 只能保留为 historical run（历史运行）。当前结论必须引用新 run label / summary artifact，并同步更新对应 phase result、optimization matrix、evaluation、topic 文档和 Handoff Packet。
- 如果本轮只生成了新日志但还没有同步文档，Handoff 必须写成 `stale_doc_pending_refresh`，列出需要刷新的路径；不能继续把旧数值写成当前 truth。
- 不要把裸 `analyze_bench_compare.log` 当长期当前结论。需要可复核当前状态时，应使用 run-labelled 目录、target-specific output、repeated summary 或 manifest / doctor pair，并在文档中写清 target、case-filter、run label 和 evidence role。

## Evidence Registry（证据登记表）

为了处理人工或 agent 复跑覆盖旧日志但忘记同步文档的问题，复杂 topic 和所有会覆盖证据文件的 board / bench target 应维护 topic-local evidence registry（证据登记表）。默认路径由 `artifact_layout.evidence_registry_template` 解析：

```text
{artifact_layout.evidence_registry_template}
```

通用脚本入口由 `artifact_layout.evidence_registry_script_template` 解析。它不是长期文档，也不替代 manifest / Evidence Doctor；它只记录“哪些证据文件由哪个动作生成，以及当前文件状态是否和上次登记一致”。registry 至少记录：

- evidence path（证据路径）、size、`mtime_ns`、`sha256` 或等价 summary digest；`mtime` 只能作为快速变化信号，不能单独作为最终判断。
- producer target / script（生产动作）、backend（`qemu` / `board` / `target`）、run label、case-filter、evidence role。
- summary、manifest、Evidence Doctor 路径，以及当前文档引用路径或 run label。
- freshness state：`fresh`、`recorded`、`unregistered_change`、`unregistered_file`、`stale_doc_pending_refresh`、`historical` 或 `manual_run_detected`。

官方 Make / script target 在写出或抓回日志、summary、manifest、doctor report 后，应调用 registry record 动作或等价逻辑。worker S0 恢复、phase loop 恢复和提交前检查应调用 registry check 动作或等价扫描：

- 如果 registry 中已登记的文件 hash / size / mtime 变化，写成 `unregistered_change`，先判断是否需要重建 summary / doctor 并刷新文档。
- 如果扫描到新 summary / manifest / doctor / analyze log 但 registry 没有记录，写成 `unregistered_file` 或 `manual_run_detected`，不能直接沿用旧 phase result。
- 如果 registry / summary 指向的 current run 未被 phase result、evaluation、topic 文档或 Handoff 引用，写成 `stale_doc_pending_refresh`。
- 如果 hook（例如 pre-commit）未安装，不得假设 registry 已经自动更新；恢复和提交前仍要显式运行检查。

registry 可以进入提交候选，但只有被 `evidence.committable_log_reference_roots` 解析出的文档根明确引用并确认不含私有路径时才提交。raw logs 仍按 raw log 策略处理。

## Bench Backend（bench 后端）

bench 类 target 的性能结论默认只来自 board（板卡）或 target hardware（目标硬件）。QEMU bench 只允许作为 build / correctness / log-shape smoke（编译、正确性和日志形状冒烟），并且必须是窄范围 smoke，不是完整 bench matrix：

- agent 默认不为了性能分析单独运行 QEMU bench compare；默认可以编译 bench binary，但不在 QEMU 上运行 `run_bench_compare`、完整计时统计或任何会生成 Std/RVV 数值对比表的 compare target。
- 若公共 Makefile 提供 guard，默认不得绕过；只有历史/窄范围 log-shape smoke 才能显式设置类似 `ALLOW_QEMU_BENCH_COMPARE=1` 的开关，并在文档中写明 `qemu_smoke_only`。
- 若需要检查 bench binary 能否启动、case label 是否完整或 checksum shape 是否可解析，可以运行小规模、少 case、少 iteration 的 QEMU smoke，并在文档中明确 `qemu_smoke_only`、case-filter、规模和它不能证明性能。
- QEMU 生成的 `analyze_bench_compare.log` 不能进入 performance evidence（性能证据）或 EvidenceDecision，只能作为可运行性 / 日志格式辅助证据。
- 同一份 bench compare 中，std / RVV 两侧必须来自同一 bench wrapper、row source、输入 corpus、size、iterations 和 checksum policy。若有意比较 production wrapper 与 test-only helper，必须命名为 mixed-boundary cross-check（混合边界交叉检查），并降级证据角色。

## 文档引用驱动的提交白名单

提交 `artifact_layout.qemu_output_subdir` 或 `artifact_layout.board_output_subdir` 解析目录下的证据文件前，先检查 `evidence.committable_log_reference_roots` 解析出的 Markdown 文档是否明确引用该证据。引用可以是仓库相对路径、run label（运行标签）、summary artifact（摘要产物）路径，或 Traceability Map 中的 evidence path（证据路径）。

该规则是提交候选的必要条件，不替代脱敏、summary-only 和用户授权规则：

- 文档只写某个输出目录时，不表示目录内所有文件都可提交；优先按具体文件名或受控 glob（通配模式）建立 allowlist（白名单）。
- 复杂 topic 应在 README、testing overview 或 evaluation 中维护当前提交白名单。白名单按具体文件列出，并说明每个文件的证据角色。文档未解释的数据日志不进入提交候选。
- 被文档引用的 summary、checksum、asm attribution（反汇编归因）、Evidence Doctor report（证据体检报告）或 sanitized log（脱敏日志）可以进入提交候选。
- 被文档引用的 correctness / unit test run log 可以进入提交候选，例如 QEMU `run_test_std.log` / `run_test_rvv.log`、board `run_test.log`，或 topic 明确采用的等价 correctness log。若文档只写 `run_test_compare` 命令而不写具体日志路径，可以改为提交小型 correctness summary，或在文档中补充被保留的具体日志路径。
- 被文档引用的 bench analyze log 可以进入提交候选，例如 `analyze_bench_compare.log`、repeated benchmark `summary.md`、Evidence Doctor report（证据体检报告）、trace summary 或 topic-local analyzer 生成的性能摘要。原始 `run_bench_*.log` 仍按 raw log 处理，只有文档明确引用且满足脱敏 / 用户授权时才提交。
- 被文档引用的 raw log（原始日志）仍需满足脱敏检查，或由用户明确要求保留原始文本并确认无私有信息风险。
- 未被文档引用的 raw run log、board env log（板卡环境日志）、collection manifest（采集清单）、临时 analyzer 输出和空表格摘要不提交；必要时在文档中先补证据角色和路径，再调整 `.gitignore` 或 staging allowlist。
- `.gitignore` 只应放开文档实际引用的文件或窄模式，不要因为 `log/board` 或 `log/qemu` 目录存在就整体放开。

默认 compare 输出是易覆盖产物。`log/board/analyze_bench_compare.log`、`log/qemu/analyze_bench_compare.log`、`log/board/run_bench_*.log` 和 `log/qemu/run_bench_*.log` 可能被下一次不同 `case-filter` 覆盖。长期文档不能只引用这类裸路径。若需要保留 bench analyze 结果，应写入 run-labelled 目录、target-specific output 或 repeated summary，例如 `log/board/run_board_bench_<alias>/analyze_bench_compare.log`、`log/qemu/analyze_bench_compare_<alias>.log` 或 `log/board/<evidence-label>/summary.md`。文档同时写明 target、case-filter、run label 和证据角色。

## Checksum 与可复现说明

文档引用 checksum、trace 或 analyzer summary 时，必须说明 checksum 的来源：

- 生成 checksum 的函数或脚本。
- 输入对象，例如 matrix、normal-equation、accepted point、iteration 序列或 final summary。
- warm-up iteration 是否进入 checksum。
- raw log 中被解析的行格式。
- summary 如何判断多轮序列一致。

checksum 是日志指纹或结果稳定性证据。它不能替代 numerical correctness test。若 checksum 只来自 ignored raw log，文档应说明 raw log 可再生成且默认不提交。

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
- 运行 topic 提供的 `make evidence_status` / `make check_evidence_freshness`，或直接调用 `artifact_layout.evidence_registry_script_template` 解析出的脚本执行 `check` 的等价入口；若 topic 尚未接入 registry，Handoff 必须说明 `evidence_registry_status=not_available` 并人工列出可能被覆盖的证据路径。
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
- `artifact_layout.qemu_output_subdir` 与 `artifact_layout.board_output_subdir` 解析目录下的 raw `*.log`、`run*.log`、`board_env_*.log` 和 `collection_manifest.json`，除非它们已被 `evidence.committable_log_reference_roots` 解析出的文档明确引用，并且满足脱敏检查或用户对 raw log / manifest 的明确授权。
- 本机 `config.mk`。
- 临时 deploy（部署）脚本。
- 聊天记录。
- raw board fetch（原始板卡抓回）目录。
