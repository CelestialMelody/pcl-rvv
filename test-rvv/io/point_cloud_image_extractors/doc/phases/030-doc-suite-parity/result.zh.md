# Phase 030 Result: doc-suite-parity

## 实际执行范围

本阶段只补齐 topic-local doc suite（主题本地文档套件）和导航归属。production 源码未修改，
没有新增 RVV helper，也没有重跑板卡；Phase 020 的 repeated board（重复板卡性能测试）和
Evidence Doctor（证据体检）仍是当前性能结论来源。

## 产物回填

| action | status | 证据 |
| --- | --- | --- |
| 创建 role 文档 | done | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` 均已存在。 |
| 更新导航 | done | `README.zh.md` 已把 role 文档纳入“先读什么”；evaluation 已把测试 / bench 细节主归属指向 role 文档。 |
| 记录审计 | done | 本文件包含 `doc_suite_role_inventory`、审计表和继续 / 停止判断。 |
| 验证 | done | 本阶段结束后运行尾随空白、`git diff --check`、topic artifact scan 和 `make run_test_compare`。 |

## doc_suite_role_inventory

| role | status | evidence |
| --- | --- | --- |
| topic_navigation | `standalone:README.zh.md` | 入口文档列出当前结论、常用命令、role 文档和长期 `doc-rvv` 不适用边界。 |
| testing_overview | `standalone:doc/testing-overview.zh.md` | 覆盖运行入口分类、输入族、target 粒度审计和当前 diagnostic 结论边界。 |
| correctness_tests | `standalone:doc/correctness-tests.zh.md` | 覆盖 5 个 gtest 的输入、路径、断言、证明范围和不能证明的范围。 |
| benchmark_and_evidence | `standalone:doc/benchmark-and-evidence.zh.md` | 覆盖 bench label、case-filter、board repeated、manifest、Doctor、asm 和提交边界。 |
| optimization_evidence | `standalone:doc/optimization-evidence.zh.md` | 覆盖 `rgb_segment_store_v1`、`rgb_u32_stride_unpack_v0`、`scaling_reduction_v1` 和 `scaling_float_stride_v0` 的证据索引。 |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` | 已更新 Phase 030 反思和 `production_probe` 的用户确认边界。 |
| test_support_code_map | `standalone:doc/test-support-code-map.zh.md` | 覆盖 `src/`、`include/`、`include/impl/`、topic-local script 和 evidence output。 |
| phase_index | `standalone:doc/phases/README.zh.md` | `030-doc-suite-parity` 状态已更新为 done，默认恢复入口指向用户确认后的 PI1。 |
| phase_plan / phase_result | `standalone:doc/phases/030-doc-suite-parity/{plan,result}.zh.md` | 本阶段计划和结果均已存在。 |
| optimization_matrix | `standalone:doc/phases/optimization-matrix.zh.md` | 当前 matrix 仍以候选实现族证据为主；doc-suite parity 不新增 RVV candidate row。 |
| evaluation_diagnostic | `standalone:doc/point_cloud_image_extractors-evaluation.zh.md` | 保留 EvidenceDecision、Traceability Map（可追踪性地图）和 production 接入判断，测试细节改由 role 文档承载。 |
| evaluation_production | `not_applicable with evidence` | 尚未进入 production integration loop（生产接入闭环），没有 production patch 或 production direct 证据。 |
| production_topic_doc | `not_applicable with evidence` | 尚无 adopted production behavior（已采用生产行为）或 PI5 后用户确认采纳；不创建 `doc-rvv/io/point_cloud_image_extractors-RVV.zh.md`。 |

## Doc-suite parity 审计表

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已有 README，Phase 030 前缺 role 文档链接 | README 应承载阅读路径、常用命令、当前结论和证据白名单 | adopted | README 已补齐 role 文档入口 | 无 |
| testing_overview | Phase 030 前缺独立测试总览 | 复杂 topic 需要 target 粒度审计、QEMU / board 边界和输入族矩阵 | adopted | `doc/testing-overview.zh.md` 已存在 | 无 |
| target granularity audit | Makefile / board target、case-filter 和 Doctor target 已存在 | 应区分 correctness aggregate、bench diagnostic alias、QEMU smoke、board repeated 和 doctor / registry | adopted | testing overview 已记录；当前不提交 raw log，registry 裁剪为人工 freshness scan，不阻塞本阶段 | Phase 040 已把 registry 是否接入留给 PI2-PI5 证据计划 |
| correctness_tests | Phase 030 前缺 TEST 字典 | 每个 TEST 需说明输入、被测路径、断言和不能证明的范围 | adopted | `doc/correctness-tests.zh.md` 覆盖 5 个 TEST | 无 |
| benchmark_and_evidence | Phase 030 前 bench label 和 Doctor 边界分散在 evaluation / phase result | 需要 case-filter 字典、repeated board、manifest、Doctor 和提交边界 | adopted | `doc/benchmark-and-evidence.zh.md` 已覆盖 Phase 020 当前证据 | 无 |
| optimization_evidence | candidate 取舍分散在 matrix、roadmap 和 evaluation | 需要 candidate -> test / bench / board / asm / decision 索引 | adopted | `doc/optimization-evidence.zh.md` 已覆盖当前四个主要 family | 无 |
| test_support_code_map | 代码地图只在 evaluation 的 Traceability Map 中部分出现 | 需要稳定定位聚合头、内部 helper、bench wrapper、script 和 output | adopted | `doc/test-support-code-map.zh.md` 已存在 | PI2 若新增 production helper 和 production bench，再评估职责拆分 |
| evaluation | 已存在，但承担过多测试细节 | evaluation 应承载决策审计和 Traceability Map，细节指向 role 文档 | adopted | evaluation 已新增 role 文档归属说明 | 无 |
| long-term doc-rvv | 当前不存在 | 只适用于 adopted production behavior 或 PI5 后用户确认采纳 | not_applicable with evidence | 当前仍是 diagnostic partial-production-candidate，未改 production | 用户确认并完成 PI5 后再创建 |
| phase index / result | Phase 030 plan 已有，result 缺失 | doc-suite parity 必须写入 phase result，不能只写在 roadmap | adopted | 本文件已创建，phase index 已标 done | 无 |
| artifact tracking | topic 目录有未跟踪 topic assets 和本地 build / log 输出 | README / evaluation / phase result 引用的 role 文档必须在 topic artifact boundary 内 | adopted | `git status --short --untracked-files=all -- test-rvv/io/point_cloud_image_extractors` 可看到新增 role 文档；build/log 属本地生成证据，不默认提交 | 提交前只选择 topic 源码 / 文档和被引用 summary，排除 build、raw log、`__pycache__` |

## Evidence 和 registry 状态

- 当前 performance truth（性能事实）仍来自 `log/board/repeated_phase020/summary.md`。
- 当前 Evidence Doctor 路径仍是 `log/board/repeated_phase020/evidence_doctor.md`，结果为
  `Errors=1, Warnings=0, Suggestions=0`。唯一 Error 指向旧
  `scaling_full_range_intensity_640x480`，不指向 `rgb_segment_store_v1` 或
  `scaling_reduction_v1`。
- topic-local registry 尚未接入；本阶段通过路径限定 `git status --short --untracked-files=all`
  做 artifact tracking scan。若进入 production integration loop，建议把 registry 接入列入 PI1
  证据计划或明确继续使用 manifest + Doctor 的人工 freshness scan。

## Continue / Stop Decision

`continue_stop_decision=turn_stop_deferred with stop_condition_hit`。

停止条件是：当前 topic 内的 diagnostic candidate、doc-suite parity 和板卡 evidence 已推进到
PI1 production integration plan（生产接入计划）检查点；继续到 PI2 会修改
`io/include/pcl/io/impl/point_cloud_image_extractors.hpp`，需要用户明确确认生产接入范围。

`next_phase_default=PI1-production-integration-plan after user confirmation`，候选范围为：

- `rgb_segment_store_v1`：`PointXYZRGB` / `PointXYZRGBA` RGB/RGBA unpack 的优先 production probe。
- `scaling_reduction_v1`：`PointXYZI::intensity` full-range scaling 的优先 production probe。

PI5 后用户确认采纳前，`doc-rvv/io/point_cloud_image_extractors-RVV.zh.md` 仍为 not applicable（不适用）。
