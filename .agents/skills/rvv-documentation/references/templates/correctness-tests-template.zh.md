# Correctness Tests Role Template

## Metadata

- `role`: correctness_tests
- `applies_when`: topic 有 gtest、reference path、candidate correctness、fallback correctness、production direct correctness 或随机 / 边界样本需要解释时。
- `default_path_source`: `artifact_layout.topic_test_dir_template` 和 `artifact_layout.evaluation_doc_subdir` 解析范围；具体文件名由配置或当前 topic role/path index 决定。
- `may_omit_or_merge_when`: correctness 只有一个小型 smoke，且 testing overview 或 evaluation 已写清输入、断言和证明范围。
- `must_not_claim`: 不把 helper-level pass 写成 public semantics pass；不把 checksum 一致写成性能或 asm attribution 证据。

## Required Sections

1. 本文职责：解释 correctness 语义，不承担 bench 统计或 production decision。
2. 测试文件分工：test source、fixtures、reference helper、candidate helper 和 production direct test 的位置。
3. 共同输入和断言：点类型、Scalar、规模、随机种子、误差阈值、row source、mask、indices、correspondences、weight 或 transform 语义。
4. TEST / 测试族字典：每个 TEST 或 filter 的输入、被测路径、断言、证明范围、不能证明的范围和默认 target。
5. 边界和随机样本策略：小规模、tail、空输入、退化矩阵、NaN / Inf、fallback 或 solver failure。
6. 验证命令：本地、QEMU、production direct 或 fallback 入口。

## Trimming Rules

- 没有对应边界样本时写 `not_applicable with evidence` 或 `phase_deferred + unblocked`，说明缺少的对象。
- 多个 TEST 共享输入时先写共同输入，再在字典里只写差异。
- 若 TEST 名来自历史实验，必须解释当前是否仍默认运行，guard 条件是什么。

## Closeout Checks

- 每个 correctness target 都能对应到测试文件和 TEST / filter。
- 每个断言都说明它证明的是数值等价、路径命中、fallback、输出形状还是异常处理。
- helper-level、diagnostic-level 和 production direct 的证明边界没有混淆。
- correctness 结论与 benchmark/evidence 的 checksum 来源一致。
