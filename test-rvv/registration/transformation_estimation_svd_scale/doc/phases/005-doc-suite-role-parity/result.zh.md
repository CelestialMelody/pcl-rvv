# Phase 005 结果：doc-suite role parity

## EvidenceDecision

`doc_parity_ready_for_PI1`。

本阶段只完善 `test-rvv/registration/transformation_estimation_svd_scale/**` 下的 topic-local 文档，没有修改 production 源码，也没有创建 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档。本阶段完成时 topic 仍是 diagnostic positive / partial production candidate；后续 Phase 010 已进入 PI5 用户确认点，Phase 020 又补充了 matrix-local implementation-shape diagnostic。

## 实际修改

| role | 状态 | 本阶段处理 |
| --- | --- | --- |
| topic_navigation | adopted | `README.zh.md` 补当前结论、阅读路径、常用命令、证据白名单和 `production_topic_doc` 适用性。 |
| testing_overview | adopted | 补文档阅读路径、测试类型定义、target granularity audit、测试流和 committable evidence 边界。 |
| correctness_tests | adopted | 补测试文件职责、TEST 字典、输入路径、断言边界和随机 / 边界样本策略。 |
| benchmark_and_evidence | adopted | 补 CLI 参数表、推荐 target、timing 边界、Evidence Doctor / manifest 边界和 ASM 归属边界。 |
| optimization_evidence | adopted | 补 scalar-vs-RVV 路径差异、代码级证据索引和细粒度 target 字典指针。 |
| optimization_roadmap | adopted | 恢复入口从历史确认点刷新为先完成 Phase 005，再进入 PI1；长期生产文档路径口径改为 `artifact_layout.topic_doc_template`。 |
| test_support_code_map | adopted | 补 scripts / evidence output 表和关键 helper traceability；`production_topic_doc` 写成当前 not_applicable。 |
| phase_suite | adopted | `doc/phases/README.zh.md`、`optimization-matrix.zh.md` 和本 result 形成恢复链路。 |
| evaluation_diagnostic | adopted | `transformation_estimation_svd_scale-evaluation.zh.md` 补 production dispatch/fallback 状态、PI 入口和 PI5 用户检查边界。 |
| production_topic_doc | not_applicable with evidence | 未接 production、没有 PI5 后用户确认采纳的 patch；不创建 production 长期主题文档。 |

## 角色裁剪规则

本阶段完成时 topic 仍未形成 adopted production behavior；Phase 010 之后已有 positive production patch，但仍停在 PI5 用户确认点。因此：

- `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档裁剪为 not_applicable，证据保存在 README、evaluation、Phase 000 result、optimization matrix 和 roadmap。
- topic-local doc suite 保留测试、bench、脚本、证据、target 粒度和恢复入口，便于后续 PI 阶段继续。
- Phase 000 的“用户确认点”保留为历史停止原因；当前恢复目标已经授权进入 PI1，但 PI5 后仍必须停到用户检查点，不能自动写成 adopted。

## target granularity audit

已在 `doc/testing-overview.zh.md` 中补 target 粒度审计。当前可审查的 target 层级为：

| 类别 | 代表 target / 文件 | 角色 |
| --- | --- | --- |
| QEMU correctness | `run_test_compare` / `src/test_tesvd_scale.cpp` | Std/RVV correctness 与 helper path shape。 |
| QEMU bench smoke | `run_bench_compare` / `src/bench_tesvd_scale.cpp` | CLI、checksum、label 和 manifest 解析 smoke；不写性能结论。 |
| asm attribution | `dump_bench_rvv` | candidate helper 或 bench 符号范围内的 RVV 指令形状证据。 |
| board repeated | `run_board_bench_ordered_cloud_pair_repeated` / `board.mk` | 4K/64K/256K repeated diagnostic performance。 |
| evidence scripts | `script/generate_tesvd_scale_*` | summary / manifest / Evidence Doctor 输入。 |

## closeout checks

| check | 结果 |
| --- | --- |
| role-based templates 是否只定义职责、不接管文件名 | pass；topic 文档按现有 role/path index 和 `artifact_layout` 口径解释。 |
| 是否仍把 diagnostic speedup 写成 production adoption | pass；本阶段所有 production 采用语义均标为 PI 后待验证；Phase 010 后当前状态已更新为 PI5 用户确认前的 pending adoption。 |
| `doc-rvv` 硬编码是否仍作为当前规范 | pass；当前规范口径改为 `artifact_layout.topic_doc_template` / `production_topic_doc`。 |
| production 源码是否被本阶段修改 | pass；本阶段没有 production diff。 |
| 下一恢复入口是否明确 | pass；进入 `010-production-integration-plan`，先冻结 PI1 范围和证据门禁。 |

## 下一步

进入 `010-production-integration-plan`。PI1 必须先冻结 production 接入范围、fallback 矩阵、测试 / bench / ASM / board gate，再决定是否进入 PI2 production patch。
