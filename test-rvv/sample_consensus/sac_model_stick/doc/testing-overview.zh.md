# sac_model_stick testing overview

## 本文职责

本文说明 `test-rvv/sample_consensus/sac_model_stick/` 的测试入口、target（Make 目标）粒度、QEMU（仿真器）和 board（板卡）证据边界。每个 gtest（GoogleTest 测试用例）的语义细节放在 `doc/correctness-tests.zh.md`；bench（性能测试）、manifest（证据清单）、Evidence Doctor（证据体检）和 registry（证据登记表）细节放在 `doc/benchmark-and-evidence.zh.md`。

## 阅读路径

| 文档 | 职责 |
| --- | --- |
| `README.zh.md` | 入口导航、常用命令、当前证据白名单和 production 边界。 |
| `doc/sac_model_stick-evaluation.zh.md` | 函数级评估、Traceability Map（可追踪性地图）、诊断证据链和 production closeout。 |
| `doc/correctness-tests.zh.md` | 11 个 correctness test（正确性测试）的输入、断言和证明边界。 |
| `doc/benchmark-and-evidence.zh.md` | bench 输出合同、board repeated、Evidence Doctor 和提交边界。 |
| `doc/optimization-evidence.zh.md` | 三个 RVV candidate family（候选族）、Phase 080 production adopted family 和 Phase 100 vector writeback family 的代码、测试、bench、asm 和决策索引。 |
| `doc/test-support-code-map.zh.md` | 测试支撑代码、production helper、script（脚本）和输出产物定位。 |
| `doc/phases/README.zh.md` | phase suite（阶段文档套件）和默认恢复入口。 |

## 运行入口分类

| 类别 | 当前入口 | 证明内容 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | Std/RVV 两个构建的 `test_sac_model_stick` 均通过 11 个 gtest，覆盖 candidate 回归、public production direct（真实生产入口直连）语义和代表点型 correctness。 | 不证明目标硬件性能。 |
| correctness aliases（正确性细分入口） | `run_stick_count_tests`、`run_stick_select_tests`、`run_stick_getdistances_tests`、`run_stick_point_type_tests` | 分别用 gtest filter（测试过滤器）只跑 count、select、getDistances 或代表点型扩展 case。 | 这些 alias 只跑 RVV build；完整 Std/RVV 对拍仍以 `run_test_compare` 为准。 |
| production bench（生产性能入口） | `bench_sac_model_stick.cpp`，共享 target `run_bench_std` / `run_bench_rvv` | 输出三条 public entry 计时行，Phase 080 后作为 production direct bench。 | QEMU 上的 bench timing（计时）不作为性能结论。 |
| QEMU smoke（QEMU 小型验证） | `dump_bench_rvv`、`run_test_compare` | 构建、correctness、日志形状和反汇编归属。 | 不支撑 board performance（板卡性能）结论。 |
| board smoke（板卡小型验证） | `board_smoke` | 部署测试和 bench，证明远端可运行并拉回日志。 | 单次 smoke 不是 repeated performance。 |
| board repeated（板卡重复采集） | `collect_repeated_board_evidence`、`collect_production_repeated_board_evidence`、`collect_vector_writeback_board_evidence` | 5-run bounded budget（有界预算）采集 Std/RVV board logs；Phase 080 target 覆盖三条 production entry，Phase 100 target 覆盖当前 getDistances 写回形态。 | 不自动登记 registry；需继续运行 record target。 |
| doctor / registry（证据体检和登记） | `record_repeated_board_evidence_state`、`repeated_evidence_status`、`record_production_board_evidence_state`、`production_evidence_status`、`record_vector_writeback_board_evidence_state`、`vector_writeback_evidence_status` | 生成 manifest、Evidence Doctor、JSON，并检查登记文件新鲜度和文档引用。 | 不会修复语义错误；异常必须在 phase result 中解释或降级。 |
| historical probe（历史探针） | not_applicable with evidence | 当前 topic 没有已接 production 后撤回的历史 probe target。 | 不适用。 |

## 输入数据总览

当前 bench 使用 `PointXYZ`、float xyz AoS（结构数组）布局、`Eigen::VectorXf` 模型系数和 direct indexed row source（直接索引行来源）。correctness tests 还覆盖 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal` 的 public entry vs Standard helper 对拍。`makeStickDistanceCloud` / `makeBenchCloud` 构造贴近 stick 中轴的合成点云，`indices_` 刻意打乱相邻点顺序，用来暴露 gather（离散加载）和输出顺序错误。

count/select 把系数 0-2 当第一个端点、3-5 当第二个端点；getDistances 把系数 0-2 当 line point（线上的点）、3-5 直接当 line direction（线方向）。这两个语义不能混用。

## 覆盖矩阵

| 路径 | correctness | bench / board | asm | production direct |
| --- | --- | --- | --- | --- |
| public `countWithinDistance` | public production test | Phase 080 positive-stable | `countWithinDistanceRVV` | adopted |
| candidate `countWithinDistanceCandidate` | 诊断回归，覆盖内外圈和 `nr_i <= nr_o` | Phase 000 candidate positive-stable | `countWithinDistanceCandidateRVV` | 只作历史诊断输入 |
| public `selectWithinDistance` | public production test | Phase 080 positive-stable | `selectWithinDistanceRVV` | adopted |
| candidate `selectWithinDistanceCandidate` | 诊断回归，覆盖保序 inliers 和 stale error 清理 | Phase 020 candidate positive-stable | `selectWithinDistanceCandidateRVV` | 只作历史诊断输入 |
| public `getDistancesToModel` | public production test | Phase 100 positive-stable for current vector writeback shape | `getDistancesToModelRVV` | adopted |
| candidate `getDistancesToModelCandidate` | 诊断回归，覆盖方向系数、dense 输出和 penalty | Phase 040 candidate positive-stable | `getDistancesToModelCandidateRVV` | 只作历史诊断输入 |
| representative xyz AoS point types | public vs Standard helper | not_applicable: correctness-only | same production helpers by template instantiation | adopted for correctness |

## Target 粒度审计

当前 target 粒度足以恢复 Phase 080 production closeout、Phase 090 点型 correctness 和 Phase 100 getDistances vector writeback：有一个 aggregate correctness target、四个细分 correctness alias、共享 bench、board smoke、diagnostic repeated target、production repeated target、vector writeback repeated target、manifest / doctor / registry target。Phase 070 已补齐 `run_stick_getdistances_tests`，Phase 080 已补 `check_production_asm` 和 production repeated evidence targets，Phase 090 已补 `run_stick_point_type_tests`，Phase 100 已补 `collect_vector_writeback_board_evidence` 和 `vector_writeback_evidence_status`。

## 当前可提交证据和默认排除项

可提交候选只包括 README / doc / phase 文档、topic-local test/bench/script/source、`log/evidence_registry.json` 和五组 repeated summary evidence。`log/board/repeated-*` raw logs（原始日志）、`build/`、QEMU `.log` 和本机配置默认排除。

## 当前结论边界

当前结论是 `production-adopted`。Phase 080 证明 `PointXYZ` direct indexed public count/select/getDistances 在板卡上均稳定正向；Phase 100 证明当前 vector writeback 形态下 public getDistances 仍稳定正向并优于 Phase 080 historical baseline（历史基线）；Phase 090 证明 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal` 的三条 public entry 与 Standard helper 输出一致。代表点型 correctness 已闭合，但这些点型的独立性能仍需新的 board performance phase 证明。
