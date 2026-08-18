# Benchmark And Evidence Role Template

## Metadata

- `role`: benchmark_and_evidence
- `applies_when`: topic 有 bench executable、case-filter、QEMU smoke、board run、summary / manifest、Evidence Doctor、asm attribution 或 registry 证据时。
- `default_path_source`: `artifact_layout.topic_test_dir_template` 和 `artifact_layout.evaluation_doc_subdir` 解析范围；具体文件名由配置或当前 topic role/path index 决定。
- `may_omit_or_merge_when`: topic 没有 bench、board、summary、doctor 或 registry，且 evaluation 已说明为什么只能做 correctness closeout。
- `must_not_claim`: 不把 QEMU timing 写成性能结论；不把单次 board smoke 写成 repeated evidence；不把 raw log 当成默认提交产物。

## Required Sections

1. 本文职责：bench、summary、doctor、registry 和提交边界。
2. Bench 输出格式：label、checksum、timing、case-filter、run label 和异常标记。
3. CLI 参数和 case-filter 字典：每个参数和 case 证明什么，不能证明什么。
4. 推荐 target：本地 bench、QEMU smoke、board smoke、board repeated、summary、doctor、registry。
5. 计时边界：setup、load/gather、staging、solver、store、checksum、warmup 和 repeat 统计是否计入。
6. Checksum 来源：reference、Std、RVV、production direct 或 diagnostic helper。
7. 当前 QEMU 证据：build、correctness、log-shape、路径命中。
8. 当前 board 证据：target、summary、manifest、doctor、repeat budget 和 decision bucket。
9. Evidence Doctor / Manifest 边界：warning / error、降级动作和人工复核。
10. ASM Attribution 口径：符号、关键指令、是否能归属到 production 或 diagnostic path。
11. 复现命令和提交边界：可提交 summary、sanitized log、registry 和默认排除项。

## Trimming Rules

- 没有 board repeated 时保留 board 缺口和所需 target，不把 smoke 结果升级。
- 没有 asm 时说明是否因为 diagnostic 阶段暂缓、符号归属困难或工具不可用。
- bench case 很多时把完整统计留给 summary，本 role 只保留 case 字典和结论索引。

## Closeout Checks

- 每个性能结论都有目标硬件或板卡证据路径。
- summary、manifest、doctor 和 registry 的路径可定位，且角色描述一致。
- checksum 与 correctness role、evaluation EvidenceDecision 没有冲突。
- raw output 的提交边界、脱敏状态和 excluded 状态明确。
