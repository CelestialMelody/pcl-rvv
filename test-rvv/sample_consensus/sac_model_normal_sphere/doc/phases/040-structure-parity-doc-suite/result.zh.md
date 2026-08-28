# Phase 040: topic-local doc suite 结构对齐结果

## 阶段范围

| 项 | 内容 |
| --- | --- |
| validated_scope | 当前 topic-local 文档套件、阶段索引、evaluation、roadmap、optimization matrix、筛选复筛表和 README 导航。 |
| unvalidated_scope | production dispatch（生产分流）、production direct（真实生产路径证据）、自定义点型、其它 normal 点型、`Scalar=double` 和 repeated board（重复板卡测试）。 |
| phase_closeout_boundary | 只关闭文档结构对齐，不新增性能证据，不改变 production 接入状态。 |

## 实现结果

本阶段新增 5 份 topic-local role 文档：

- `doc/testing-overview.zh.md`：测试入口分类、target 粒度和覆盖矩阵。
- `doc/correctness-tests.zh.md`：gtest 输入、断言和证明范围。
- `doc/benchmark-and-evidence.zh.md`：bench（性能测试）label、board summary（板卡摘要）、manifest、Evidence Doctor（证据体检）和 registry（证据登记表）。
- `doc/optimization-evidence.zh.md`：candidate family（候选实现族）到代码、测试、bench、board、asm 和决策的索引。
- `doc/test-support-code-map.zh.md`：test support（测试支撑）helper、bench、script 和 output 的调用图。

同时更新：

- `README.zh.md`：补 Phase 030/040 状态、doc-suite 阅读路径、Phase 030 命令和 summary-only（只提交摘要）证据边界。
- `doc/phases/README.zh.md`：补 Phase 030/040 行，并把默认恢复动作改为 production 授权边界。
- `doc/phases/optimization-matrix.zh.md`：补 RGB/RGBA 点型扩展和 structure-parity-doc-suite 行。
- `doc/optimization-roadmap.zh.md`：补 Phase 030/040 反思和默认恢复动作。
- `doc/sac_model_normal_sphere-evaluation.zh.md`：补 Phase 030 证据、Phase 040 文档归属和当时的 `PI1-candidate diagnostic / representative point type set` 判断；Phase 050 已把当前状态更新为 PI1 plan ready。
- `doc-rvv/library-screening/sample_consensus/sample_consensus-retained-candidate-rescreen.zh.md`：同步 normal-sphere 状态。

production 文件 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` 未修改，`doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md` 仍为 not_applicable。

## Doc Suite Role Inventory

| role | current shape | decision | evidence / blocker | next action |
| --- | --- | --- | --- | --- |
| topic_navigation | `README.zh.md` | adopted | 已列当前结论、阅读路径、常用命令和提交边界。 | 无。 |
| testing_overview | `doc/testing-overview.zh.md` | adopted | 已覆盖 target 粒度、输入范围、QEMU / board / doctor 边界。 | 无。 |
| correctness_tests | `doc/correctness-tests.zh.md` | adopted | 已列每个 TEST 的输入、断言、证明范围和未覆盖范围。 | 无。 |
| benchmark_and_evidence | `doc/benchmark-and-evidence.zh.md` | adopted | 已列 CLI、case label、summary、manifest、doctor、registry 和 raw log 排除边界。 | 无。 |
| optimization_evidence | `doc/optimization-evidence.zh.md` | adopted | 已按 candidate family 聚合 Phase 000-030 证据。 | 无。 |
| optimization_roadmap | `doc/optimization-roadmap.zh.md` | adopted | 已更新 Phase 030/040 反思和默认恢复动作。 | 无。 |
| test_support_code_map | `doc/test-support-code-map.zh.md` | adopted | 已列 src/include/include/impl/script/output 调用图和拆分审计。 | 无。 |
| phase_index / phase_plan / phase_result | `doc/phases/README.zh.md`、`doc/phases/*/plan.zh.md`、`doc/phases/*/result.zh.md` | adopted | Phase 000-040 均有 plan/result 或索引。 | 无。 |
| optimization_matrix | `doc/phases/optimization-matrix.zh.md` | adopted | 已补 Phase 030 和 Phase 040 状态。 | 无。 |
| evaluation_diagnostic | `doc/sac_model_normal_sphere-evaluation.zh.md` | adopted | 已补 Traceability Map、诊断证据链、Phase 030/040 和 production 边界。 | 无。 |
| evaluation_production | 同一 evaluation 的 production 判断段落 | not_applicable with evidence | 当前没有 production patch 或 PI5 production evidence。 | 用户授权 production integration loop 后再启用。 |
| production_topic_doc | `doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md` | not_applicable with evidence | 没有 adopted production behavior（已采用生产行为）、用户确认保留的 production patch 或 PI5 通过。 | 用户授权并确认采纳后再创建或更新。 |

## Target 粒度审计

| target 类别 | 当前入口 | decision | 证据 / 边界 | next action |
| --- | --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `run_test_compare` | adopted | Std/RVV 各 5 个 gtest 通过；覆盖 public / reference / candidate 对拍。 | 无。 |
| correctness aliases（正确性细分入口） | `run_normal_sphere_public_tests`、`run_board_normal_sphere_public_tests` | adopted | public gtest filter 可单独运行。 | production direct fallback 需 PI 阶段新增。 |
| bench diagnostic aliases | `run_bench_rvv BENCH_ARGS='<points> <iters> <PointT>'` | adopted | 支持 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`。 | 自定义点型需新 phase。 |
| QEMU smoke aliases | `run_bench_rvv BENCH_ARGS='128 1 <PointT>'` | adopted | 只作为可运行性和日志形状检查。 | 无。 |
| board smoke aliases | `board_smoke OUTPUT_DIR_BOARD=... BENCH_ARGS='65536 200 <PointT>'` | adopted | Phase 000-030 均有 board smoke 证据。 | production direct 需 PI4 重跑。 |
| board repeated aliases | 当前无专用 repeated target | not_applicable with evidence | 当前 closeout 是 diagnostic smoke，不声明 repeated performance。 | production integration loop 中按 PI1 预算新增或复用。 |
| doctor / registry aliases | `record_evidence_state`、`record_phase010_evidence_state`、`record_phase020_evidence_state`、`record_phase030_evidence_state`、`evidence_status` | adopted | Phase 000-030 summary / manifest / doctor 已登记，registry fresh。 | 无。 |
| historical probe guarded aliases | 当前无旧 production probe | not_applicable with evidence | 没有历史 production patch 或 guarded probe target。 | 无。 |

## Test Support Shape Scan

| area | current shape scan | decision | evidence / reason | next action |
| --- | --- | --- | --- | --- |
| test/bench source layout | 测试和 bench 源码位于 `src/`，符合 `artifact_layout.source_subdir`。 | adopted | `src/test_sac_model_normal_sphere.cpp` 与 `src/bench_sac_model_normal_sphere.cpp` 只保留入口。 | 无。 |
| aggregator and internal helpers | 聚合头位于 `include/`，内部 helper 位于 `include/impl/`。 | adopted | 与 `test_support.aggregator_directory` / `internal_directory` 一致。 | 无。 |
| internal helper layout | 没有旧 `test_support/` 目录或 compatibility alias（兼容别名）。 | adopted | `find` 和 git path scan 均未发现旧目录。 | 无。 |
| script and bench registry | topic-local manifest generator 位于 `script/`，通用 doctor / registry 位于 `test-rvv/script/`。 | adopted | Make target 已接 Phase 000-030。 | 无。 |
| legacy compatibility | 没有 root evaluation pointer、旧 wrapper 或重复正文。 | adopted | 新 role docs 均在 `doc/` 下，由 README 引用。 | 无。 |
| evidence freshness | registry 已记录 Phase 000-030 summary / manifest / doctor。 | adopted | `make -C ... evidence_status` 为 fresh。 | 完成后最终验证再跑一次。 |

## Artifact Tracking

本阶段新增或更新的 README / evaluation / roadmap / phase result 引用都指向 topic-local 文件。提交边界上，这些文件属于当前 topic test asset（测试资产）或 topic-local doc（主题本地文档）；raw logs、build 二进制和 QEMU / board 原始日志仍排除。

## Evidence Doctor 解释

本阶段没有新性能数据，因此不新增 Evidence Doctor 报告。当前 Evidence Doctor 状态来自 Phase 000-030：

- Phase 000：`Errors=0`，Warning 为 `low_run_count`。
- Phase 010：`Errors=0`，Warning 为 `low_run_count`。
- Phase 020：`Errors=0`，Warning 为 `low_run_count`。
- Phase 030：`Errors=0`、`Warnings=8`、`Suggestions=0`，Warning 均为 `low_run_count`。

这些 warning 不阻塞 diagnostic closeout，但所有性能结论都保持 single board smoke 边界，不写成 production performance。

## 阶段结论

Phase 040 决策为 `complete-doc-suite-parity`。当前 topic-local 文档结构已能从 README 恢复测试、候选、证据、代码地图和阶段状态；没有剩余只触碰 topic-local 文档或测试资产的高优先级未阻塞动作。

## 继续 / 停止判断

当前未命中板卡不可用、Evidence Doctor Error、证据矛盾或 dirty isolation 不安全。合法停止条件是：继续推进需要用户明确授权 production integration loop（生产接入闭环），或另行指定自定义点型、其它 normal 点型、`Scalar=double` / 非 indexed 入口扩展范围。

默认下一动作：若用户授权 production integration loop，进入 PI1，冻结 `countWithinDistance` mask count、`selectWithinDistance` `vcompress` 写回、`getDistancesToModel` dense store、`PointXYZ/PointXYZI/PointXYZRGB/PointXYZRGBA + Normal` 代表点型范围、fallback、dispatch 和 PI2-PI5 验证计划。
