# Phase 030 Plan: doc-suite-parity

## 阶段意图和边界

本阶段补齐 topic-local doc suite（主题本地文档套件）。前面三个 phase 已经形成多候选、多
bench label、repeated board（重复板卡性能测试）和 Evidence Doctor（证据体检）证据；只靠
README、evaluation、roadmap 和 phase result 不足以让下一轮 worker / reviewer 快速定位测试入口、
bench 证据、candidate 取舍和测试支撑代码。

本阶段只修改 `test-rvv/io/point_cloud_image_extractors/doc/**` 和 README 导航，不修改
production 源码、不新增 RVV helper、不重跑板卡。

## 当前状态清单

| role | 当前状态 | 缺口 |
| --- | --- | --- |
| topic_navigation | `README.zh.md` 已存在 | 需要链接新增 role 文档。 |
| evaluation_diagnostic | `doc/point_cloud_image_extractors-evaluation.zh.md` 已存在 | 已承担过多测试和证据说明，需要把细节迁到 role 文档。 |
| optimization_roadmap | `doc/optimization-roadmap.zh.md` 已存在 | 已更新到 Phase 020。 |
| phase suite | `doc/phases/**` 已存在 | 需要记录本次 doc-suite parity result。 |
| testing_overview | 缺失 | 需要 target 粒度审计和运行入口分类。 |
| correctness_tests | 缺失 | 需要 TEST 字典和 correctness 边界。 |
| benchmark_and_evidence | 缺失 | 需要 bench label、board repeated、Doctor 和提交边界。 |
| optimization_evidence | 缺失 | 需要 candidate 到证据的索引。 |
| test_support_code_map | 缺失 | 需要 helper、bench、script、output 的代码地图。 |
| production_topic_doc | 不适用 | 尚无 adopted production behavior。 |

## 实现动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 创建 role 文档 | `doc/testing-overview.zh.md`, `doc/correctness-tests.zh.md`, `doc/benchmark-and-evidence.zh.md`, `doc/optimization-evidence.zh.md`, `doc/test-support-code-map.zh.md` | 每份文档有职责、路径、证据边界和 closeout checks。 |
| 更新导航 | `README.zh.md`, evaluation | 新文档可从入口文档找到，evaluation 不承担所有细节。 |
| 记录审计 | `doc/phases/030-doc-suite-parity/result.zh.md` | `doc_suite_role_inventory` 覆盖所有 role。 |
| 验证 | `rg` / `git diff --check` / status scan | 无尾随空白，新增文档在 topic artifact boundary 内。 |

## Continue / Stop Criteria

本阶段完成后，当前 topic 内 test-only 搜索空间已经闭合到 PI1 检查点。下一步若继续，需要用户确认
production integration loop（生产接入闭环），因为 PI2 会修改
`io/include/pcl/io/impl/point_cloud_image_extractors.hpp`。
