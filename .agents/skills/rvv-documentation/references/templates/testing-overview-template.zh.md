# Testing Overview Role Template

## Metadata

- `role`: testing_overview
- `applies_when`: topic 有多个 test target、QEMU / board target、bench entry、Evidence Doctor 或 reviewer 需要快速判断测试覆盖边界时。
- `default_path_source`: `artifact_layout.topic_test_dir_template` 和 `artifact_layout.evaluation_doc_subdir` 解析范围；具体文件名由配置或当前 topic role/path index 决定。
- `may_omit_or_merge_when`: 只有一个 correctness target、没有 bench / board / Evidence Doctor，且 evaluation 已完整列出运行入口和证据边界。
- `must_not_claim`: QEMU timing 不是性能证据；aggregate target 不能冒充 production direct、fallback 或 board repeated 覆盖。

## Required Sections

1. 本文职责：测试入口总览和证据边界，不解释每个 TEST 细节。
2. 文档阅读路径：README、correctness、benchmark/evidence、evaluation、phase result 的职责分工。
3. 测试类型定义：correctness aggregate、correctness alias、diagnostic bench、QEMU smoke、board smoke、board repeated、doctor / registry、historical probe。
4. 运行入口分类：Make target、脚本、case-filter、gtest filter、guard 条件和默认 target。
5. Target 粒度审计：每类 target 是否存在、证明什么、缺口和下一步。
6. 测试流程：本地 build、QEMU、board、summary、doctor、registry 的推荐顺序。
7. 输入数据总览：点类型、Scalar、规模、row source、indices / correspondences / mask / weight 等入口语义。
8. 覆盖矩阵：路径、数据布局、fallback、诊断候选和 production direct 的覆盖关系。
9. 当前可提交证据和默认排除项。
10. 当前结论边界：哪些证据可以支撑 closeout，哪些只能支撑下一 phase。

## Trimming Rules

- 若没有 board 资源，保留 board evidence 缺口和 stop condition；不要删掉性能证据边界。
- 若只有 diagnostic path，production direct 行可以合并成 `not_applicable with evidence`。
- 若 correctness 细节较多，迁到 correctness role；overview 只保留分类和覆盖矩阵。

## Closeout Checks

- target 粒度审计覆盖当前 Makefile、board.mk、test source、bench source、script 和证据输出。
- 每个运行入口都说明证明范围和不能证明的范围。
- QEMU、board smoke、board repeated 和 Evidence Doctor 的证据角色没有混用。
- overview 与 README 的命令列表、evaluation 的 EvidenceDecision、phase result 的下一步保持一致。
