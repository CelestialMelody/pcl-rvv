# Topic-local Doc Suite Role Templates

本文定义 topic-local doc suite（主题本地文档套件）的 role-based templates（基于职责的模板）入口。模板只规定文档职责、内容结构、裁剪规则和 closeout checks（收尾检查）；它不拥有最终文件名、目录名或固定文件集合。

## 路径解析边界

写入 topic 文档前，先从 `.agents/config/defaults.yaml` 的 `artifact_layout` 解析路径。模板中的 `default_path_source` 指向 role 的默认配置 key；模板文件名只说明职责，不参与 topic 文档命名。

路径决策顺序：

1. 若 `artifact_layout` 已有精确 role key，使用该 key 解析出的默认路径。
2. 若当前 topic 已有稳定 role 文档、README 链接或 Handoff 中确认的路径，可以保留该路径；路径必须仍在 `artifact_layout.topic_test_dir_template` 解析目录内，并在 `doc_suite_role_inventory` 记录。
3. 若某个新 role 需要跨 topic 复用且还没有精确 key，先更新 `.agents/config/defaults.yaml`，不要在模板正文里写死新文件名。
4. 若用户、reviewer 或旧 topic 文档使用了不同命名，以当前配置和当前 topic 的真实引用为准；模板只用于判断内容是否完整。

一个 role 可以由独立文档承载，也可以由现有文档中的稳定章节承载。合并时必须满足两个条件：读者能从 topic_navigation、phase index、evaluation 或 Handoff 找到该章节；closeout checks 没有因为合并而消失。复杂 topic 命中 `doc-suite-quality-bar.zh.md` 的拆分条件时，优先使用配置默认路径拆出独立 role 文档。

## 模板元数据

每个 role template 使用相同字段：

| 字段 | 含义 |
| --- | --- |
| `role` | 文档承担的职责，不是文件名。 |
| `applies_when` | 什么时候需要这个 role。 |
| `default_path_source` | 路径应从哪些 `artifact_layout` key 或配置解析范围取得。 |
| `may_omit_or_merge_when` | 可以省略或合并的条件。 |
| `must_not_claim` | 不能声称的结论或边界。 |
| `required_sections` | 默认内容结构，可按证据裁剪。 |
| `closeout_checks` | closeout、ready-for-review 或交接前必须核对的点。 |

## Role Registry

| role | template | default_path_source |
| --- | --- | --- |
| topic_navigation | [topic-navigation-template.zh.md](topic-navigation-template.zh.md) | `artifact_layout.topic_navigation_doc_template`。 |
| testing_overview | [testing-overview-template.zh.md](testing-overview-template.zh.md) | `artifact_layout.testing_overview_doc_template`。 |
| correctness_tests | [correctness-tests-template.zh.md](correctness-tests-template.zh.md) | `artifact_layout.correctness_tests_doc_template`。 |
| benchmark_and_evidence | [benchmark-and-evidence-template.zh.md](benchmark-and-evidence-template.zh.md) | `artifact_layout.benchmark_and_evidence_doc_template`。 |
| optimization_evidence | [optimization-evidence-template.zh.md](optimization-evidence-template.zh.md) | `artifact_layout.optimization_evidence_doc_template`。 |
| optimization_roadmap | [optimization-roadmap-template.zh.md](optimization-roadmap-template.zh.md) | `artifact_layout.optimization_roadmap_template`。 |
| test_support_code_map | [test-support-code-map-template.zh.md](test-support-code-map-template.zh.md) | `artifact_layout.test_support_code_map_doc_template`。 |
| phase_index | [phase-suite-template.zh.md](phase-suite-template.zh.md) | `artifact_layout.phase_index_template`。 |
| phase_plan | [phase-suite-template.zh.md](phase-suite-template.zh.md) | `artifact_layout.phase_plan_template`。 |
| phase_result | [phase-suite-template.zh.md](phase-suite-template.zh.md) | `artifact_layout.phase_result_template`。 |
| optimization_matrix | [phase-suite-template.zh.md](phase-suite-template.zh.md) | `artifact_layout.optimization_matrix_template`。 |
| evaluation_diagnostic | [evaluation-diagnostic-template.zh.md](evaluation-diagnostic-template.zh.md) | `artifact_layout.evaluation_doc_template`。 |
| evaluation_production | [evaluation-production-template.zh.md](evaluation-production-template.zh.md) | `artifact_layout.evaluation_doc_template`。 |

## Diagnostic Topic Validation

对 diagnostic topic（诊断主题），默认使用 `evaluation_diagnostic`、`optimization_roadmap`、`optimization_matrix`、`phase_result`、`testing_overview`、`correctness_tests`、`benchmark_and_evidence`、`optimization_evidence`、`test_support_code_map` 和 `topic_navigation`。如果尚无 adopted production behavior（已采用生产行为），`artifact_layout.topic_doc_template` 解析出的 production 长期主题文档应为 `not_applicable with evidence`。

模板应帮助 worker 回答：

- 诊断代码是否代表真实 production entry（生产入口）。
- 哪些候选有收益，收益来自 full diagnostic、局部 helper 还是 RVV-vs-RVV A/B。
- `optimization_matrix` 中的新想法来自源码 gap、bench 负向归因、asm/profile 线索、phase result 反思，还是 reviewer/user 反馈。
- 继续推进的 unblocked action 是优化搜索、production integration loop（生产接入闭环）还是 stop-for-review。

## Mature Sibling Calibration

成熟 sibling topic 只用于校准结构成熟度：读者路径、证据白名单、target 字典、代码地图、phase 恢复入口和 closeout 门禁。不要复制它的算法族、case-filter 名、性能数字、phase slug、生产结论或 topic-specific 文件名。若模板无法表达成熟 sibling 中反复出现的跨 topic 做法，应更新本目录或其它 `.agents` reference，而不是让 future worker 继续依赖那个 sibling 路径。
