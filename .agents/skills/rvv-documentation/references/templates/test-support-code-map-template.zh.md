# Test Support Code Map Role Template

## Metadata

- `role`: test_support_code_map
- `applies_when`: topic 的 test support 代码包含三类以上角色，或 reviewer 难以定位 fixture、reference、candidate、bench wrapper、script 和 production 对照关系时。
- `default_path_source`: `artifact_layout.test_support_code_map_doc_template`；当前 topic 已有稳定 role 文档路径时可保留，并写入 `doc_suite_role_inventory`。
- `may_omit_or_merge_when`: 测试支撑只有一个短 test source，且 evaluation / correctness role 已能完整说明调用关系。
- `must_not_claim`: 不把 test-only helper 写成 production helper；不把 bench wrapper 写成 public API；不把当前拆分方式写成所有 topic 的必然模板。

## Required Sections

1. 本文职责：代码定位和角色边界，不承担性能结论。
2. 总调用图：production public entry、diagnostic reference、candidate helper、bench wrapper、analysis script 和 output 的关系。
3. 稳定聚合入口：Make target、test executable、bench executable 和 shared helper。
4. Fixtures 与输入构造：数据规模、点类型、row source、mask、indices、correspondences、weight 或 transform。
5. 标量 Reference：来源、与 production Std 的关系和不能证明的范围。
6. Candidate / Diagnostic Helper：每个 helper 的职责、输入输出、fallback 和当前状态。
7. Bench Harness 与 Case Registry：case-filter、计时边界、checksum 和输出合同。
8. Scripts 与 Evidence Output：summary、doctor、registry、sanitize 和 manifest 的调用关系。
9. Production 与 Test Support 边界：哪些代码只是专项测试，哪些对照真实 production。
10. 拆分审计：是否需要 internal header 拆分、命名调整或 helper ownership 清理。

## Trimming Rules

- 小 topic 可用一张表替代调用图，但必须保留 role、位置和证据角色。
- 不复制源代码；只写符号、路径、调用者、被调用者和证据职责。
- 旧 helper 若已废弃，保留状态和删除条件，不让 README 或 evaluation 继续把它当当前入口。

## Closeout Checks

- 每个被 README、evaluation、bench 或 phase result 引用的 helper 都能在 map 中定位。
- test-only、diagnostic、production-shaped diagnostic 和 production direct 的层级没有混淆。
- map 与 Makefile、source split、script 和 output summary 的实际路径一致。
- 拆分审计结果与 `.agents/config/defaults.yaml` 的 source / helper 命名规则不冲突。
