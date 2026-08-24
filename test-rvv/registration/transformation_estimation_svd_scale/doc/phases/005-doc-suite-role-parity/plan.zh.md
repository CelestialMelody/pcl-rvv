# Phase 005 计划：doc-suite role parity

## 阶段意图和边界

本阶段先按 `rvv-documentation` 的 role-based templates（基于职责的模板）完善 `transformation_estimation_svd_scale` 的 topic-local doc suite（主题本地文档套件）。目标是让当前 diagnostic topic（诊断主题）在进入 production integration loop（生产接入闭环）前，文档能清楚说明 role 归属、target 粒度、证据边界、production_topic_doc 适用性和下一阶段恢复入口。

本阶段只修改 `test-rvv/registration/transformation_estimation_svd_scale/**` 下的文档，不修改 production 源码、不新增生产长期主题文档。production patch 只能在后续 PI2 开始。

## 当前状态清单

| 对象 | 当前状态 | 证据 |
| --- | --- | --- |
| Phase 000 | `partial-production-candidate`，`direct-fused-scale-accum` 在 ordered `PointXYZ -> PointXYZ` / `float` / dense 诊断范围内 positive。 | `doc/phases/000-current-state-and-gaps/result.zh.md` |
| topic-local docs | 已有 topic_navigation、testing_overview、correctness_tests、benchmark_and_evidence、optimization_evidence、optimization_roadmap、test_support_code_map、phase suite 和 evaluation。 | `README.zh.md`、`doc/*.zh.md`、`doc/phases/**` |
| role template gap | 部分文档仍使用旧 `doc-rvv` 字面路径；target granularity audit、doc-suite parity audit 和 production integration 恢复状态不够明确。 | `rg "doc-rvv|blocked_by_user_confirmation|target 粒度"` |
| production 状态 | 尚未修改 production。用户当前目标允许先完善文档，然后进入有界 production integration loop。 | 当前 prompt / goal |

## 实现动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 更新配置化命名口径 | README、evaluation、roadmap、phase docs、code map | `artifact_layout.topic_doc_template` / `production_topic_doc` 替代旧的硬编码 production 文档路径。 |
| 补 doc-suite parity audit | phase result 或 topic 文档 | 按 role 列出 adopted / not_applicable / phase_deferred，并说明裁剪证据。 |
| 补 target granularity audit | testing overview | 从 Makefile、board.mk、test、bench、script、registry 抽取真实 target 分类。 |
| 刷新恢复入口 | phase README、roadmap、matrix、evaluation | `blocked_by_user_confirmation` 更新为本轮已授权进入 PI1；PI5 后仍需用户检查确认。 |
| 写 Phase 005 result | `doc/phases/005-doc-suite-role-parity/result.zh.md` | 记录实际修改、审计表、artifact tracking 和下一 phase。 |

## 继续 / 停止条件

若文档 role 和 target 粒度可审查，本阶段完成后默认进入 `010-production-integration-plan`。若发现 topic-local docs 缺失无法在当前范围补齐，写成 `phase_deferred + unblocked` 并继续补；只有 dirty isolation 不安全或需要修改 `.agents` / production 才能停在本阶段。
