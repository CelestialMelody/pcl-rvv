# Phase 030 计划：structure parity doc suite

## 阶段意图和边界

本阶段补齐 topic-local doc suite（主题本地文档套件）和 structure parity（结构对齐）审计。
Phase 020 已经完成 PI1 production integration plan（生产接入计划），继续到 PI2 会修改
`io/include/pcl/io/impl/pcd_io.hpp` production（生产源码），需要用户明确授权；因此本阶段只推进
当前 topic 测试资产和文档边界内的未阻塞动作。

本阶段不修改 production，不新增板卡复跑，不改变 Phase 010 / PI1 的 EvidenceDecision（证据决策）。

## 当前状态清单

- README 仍描述为 pack-only component ablation（组件消融），未反映 Phase 010 positive 和 PI1 状态。
- 已有 evaluation：`doc/pcd_io_templated_writer-evaluation.zh.md`。
- 已有 roadmap：`doc/optimization-roadmap.zh.md`。
- 已有 phase index / matrix：`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`。
- 缺少独立 role 文档：`testing-overview.zh.md`、`correctness-tests.zh.md`、
  `benchmark-and-evidence.zh.md`、`optimization-evidence.zh.md`、`test-support-code-map.zh.md`。
- `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md` 仍 not_applicable：没有 adopted production behavior（已采用生产行为）。

## Doc Suite Role Inventory 计划

| role | planned status | path / section |
| --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` |
| testing_overview | standalone | `doc/testing-overview.zh.md` |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` |
| phase_index / phase_plan / phase_result / optimization_matrix | standalone | `doc/phases/**` |
| evaluation_diagnostic | standalone | `doc/pcd_io_templated_writer-evaluation.zh.md` |
| evaluation_production | not_applicable with evidence | production patch 未授权且未存在 |
| production_topic_doc | not_applicable with evidence | 没有 adopted production behavior 或 PI5 用户确认 |

## Target 粒度审计计划

本阶段从当前 topic 的真实 `Makefile`、`board.mk`、`src/test_pcdtw.cpp`、`src/bench_pcdtw.cpp`、
topic-local `script/` 和 `log/evidence_registry.json` 抽取 target，不从 sibling topic 复制名称。

需要覆盖：

- correctness aggregate：`run_test_compare`。
- QEMU smoke：`run_qemu_smoke`，仅日志形状 / correctness，不作为性能证据。
- bench diagnostic aliases：`--case-filter all`、`--case-filter compressed_*`。
- board repeated aliases：`run_board_pcdtw_repeated`、`run_board_pcdtw_shaped_repeated`。
- doctor / registry aliases：`run_board_pcdtw_evidence_doctor`、`record_board_pcdtw_evidence_state`、
  `run_board_pcdtw_shaped_evidence_doctor`、`record_board_pcdtw_shaped_evidence_state`。
- production direct：planned only，需 PI2/PI3 后新增 target。

## 实现动作

| action | artifact | completion criteria |
| --- | --- | --- |
| D1 更新 README 导航 | `README.zh.md` | 当前状态、阅读路径、常用命令、证据白名单和 production_doc 适用性准确。 |
| D2 补 testing overview | `doc/testing-overview.zh.md` | target 粒度、QEMU/board/production direct 边界清楚。 |
| D3 补 correctness tests 文档 | `doc/correctness-tests.zh.md` | 每个 gtest 的输入、断言、证明范围和不可证明范围清楚。 |
| D4 补 benchmark/evidence 文档 | `doc/benchmark-and-evidence.zh.md` | case label、计时边界、summary/manifest/Doctor/registry 和提交边界清楚。 |
| D5 补 optimization evidence 文档 | `doc/optimization-evidence.zh.md` | candidate family 与 test、bench、board、asm、Doctor、decision 对齐。 |
| D6 补 test support code map | `doc/test-support-code-map.zh.md` | include、impl、src、script、output 和 production 对照能互相定位。 |
| D7 回填 phase result / roadmap / matrix | `result.zh.md`、roadmap、matrix、phase index | doc-suite audit 有 adopted / not_applicable / turn-stop 状态。 |
| D8 验证 | `git diff --check`、路径限定 status、topic 文档空白扫描 | 无空白错误；artifact tracking 可复核。 |

## Continue / Stop Conditions

- 若 D1-D8 全部完成，本阶段可关闭 topic-local doc suite parity。
- 若用户授权 production integration loop，下一阶段进入 PI2 production patch。
- 若未授权 production，合法停止点是 `pending-production-authorization`；不能自行修改 production。
