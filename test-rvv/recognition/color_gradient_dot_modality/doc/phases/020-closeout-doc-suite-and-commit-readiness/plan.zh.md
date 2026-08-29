# Phase 020: closeout doc suite and commit readiness plan

## 目标

本阶段不再改 `recognition/include/pcl/recognition/color_gradient_dot_modality.h` 的算法行为，只做 closeout
收尾：把 topic-local doc suite（主题本地文档套件）补齐到当前复杂度所需的粒度，修正 evidence freshness
（证据新鲜度）扫描范围，并把当前 adopted production behavior（已采用生产行为）的提交边界写清。

## 当前状态

| area | current shape scan | decision | next action |
| --- | --- | --- | --- |
| production behavior | `processInputData()` 已接入 `computeMaxColorGradientsRVV()`；`computeDominantQuantizedGradients()` 保持标量。 | adopted production behavior | 保持不再扩展算法范围。 |
| topic_navigation | `README.zh.md` 已存在，但还需补齐 role-based 文档入口。 | phase_deferred + unblocked | 更新 README 指向 topic-local role 文档。 |
| testing_overview | 尚未单独拆出。 | phase_deferred + unblocked | 新建 `doc/testing-overview.zh.md`。 |
| correctness_tests | 尚未单独拆出。 | phase_deferred + unblocked | 新建 `doc/correctness-tests.zh.md`。 |
| benchmark_and_evidence | 尚未单独拆出。 | phase_deferred + unblocked | 新建 `doc/benchmark-and-evidence.zh.md`。 |
| optimization_evidence | 尚未单独拆出。 | phase_deferred + unblocked | 新建 `doc/optimization-evidence.zh.md`。 |
| test_support_code_map | 尚未单独拆出。 | phase_deferred + unblocked | 新建 `doc/test-support-code-map.zh.md`。 |
| optimization_roadmap | 已有，但需在 closeout 语境下确认不再有未阻塞优化动作。 | adopted | 保持当前路线图，只补充 closeout 说明。 |
| phase_index / plan / result / matrix | 已有，但需加入 Phase 020 索引。 | phase_deferred + unblocked | 更新 phase README 与结果文档。 |
| evaluation | 已有，但仍承担文档归属审计入口。 | phase_deferred + unblocked | 更新 evaluation 的 doc suite inventory。 |
| production_topic_doc | 已存在，且当前状态为 adopted production behavior。 | adopted | 仅做 freshness 对齐，不改长期结论。 |

## 证据与门禁

| gate | plan |
| --- | --- |
| freshness | 更新 `Makefile` 的 `check_evidence_freshness`，覆盖 Phase 000、Phase 010 和 `doc-rvv` 长期文档。 |
| registry | 让 `record_evidence_state_repeated` 同时登记 topic-local docs 与 `doc-rvv` 生产文档引用。 |
| doc suite | 按 `doc-suite-quality-bar.zh.md` 补齐 role-based 文档。 |
| commit boundary | 只整理当前 topic 相关文件，继续忽略其它 topic 的 dirty changes。 |

## 完成条件

1. 新增的 topic-local role 文档存在，并能从 README / evaluation / phase index 找到。
2. `check_evidence_freshness` 覆盖 Phase 010 production direct evidence 和 `doc-rvv` 文档。
3. `git diff --check` 通过，且当前 topic 的 untracked 产物边界清楚。
4. 没有新的优化候选需要继续推进；若以上都成立，本阶段结束后进入提交流程。
