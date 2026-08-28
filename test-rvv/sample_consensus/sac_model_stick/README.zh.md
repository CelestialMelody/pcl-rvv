# sac_model_stick RVV topic

本目录保存 `SampleConsensusModelStick` 的 RVV test-rvv（专项测试）资产。Phase 080 已把 `countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel` 三个公开入口接入 production direct（真实生产入口直连）RVV 路径；Phase 100 已把 `getDistancesToModelRVV` 的 staged scalar lane（暂存后逐向量通道标量写回）改成 RVV vector writeback（向量写回），并用接入后的 5-run board repeated（板卡重复采集）数据完成采纳。

## 阅读路径

| 文档 | 作用 |
| --- | --- |
| `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md` | production 长期主题文档，说明当前采纳的生产实现、fallback（回退路径）、证据链和后续边界。 |
| `doc/sac_model_stick-evaluation.zh.md` | 函数级评估、Traceability Map（可追踪性地图）、诊断证据链、production closeout 和文档归属主入口。 |
| `doc/testing-overview.zh.md` | 测试入口分类、target（Make 目标）粒度审计、QEMU / board / production direct 证据边界。 |
| `doc/correctness-tests.zh.md` | 11 个 gtest 的输入、断言、证明范围和不能证明的范围。 |
| `doc/benchmark-and-evidence.zh.md` | bench（性能测试）输出、case label（用例标签）、manifest、Evidence Doctor 和 registry 边界。 |
| `doc/optimization-evidence.zh.md` | count/select/getDistances 候选族到代码、测试、bench、asm、board 和决策的索引。 |
| `doc/test-support-code-map.zh.md` | 测试支撑代码、production helper、script 和 evidence output（证据输出）定位。 |
| `doc/phases/README.zh.md` | phase suite（阶段文档套件）入口和默认恢复动作。 |
| `doc/phases/080-stick-production-integration/plan.zh.md` | Phase 080 production integration plan（生产接入计划）。 |
| `doc/phases/080-stick-production-integration/result.zh.md` | Phase 080 PI2-PI5 结果、EvidenceDecision（证据决策）和 doc-suite closeout。 |
| `doc/phases/090-stick-point-type-expansion/plan.zh.md` | Phase 090 代表点型 correctness（正确性）扩展计划。 |
| `doc/phases/090-stick-point-type-expansion/result.zh.md` | Phase 090 点型扩展执行结果，不包含 dedicated board performance（专门板卡性能）结论。 |
| `doc/phases/100-stick-getdistances-vector-writeback/plan.zh.md` | Phase 100 getDistances 向量写回计划。 |
| `doc/phases/100-stick-getdistances-vector-writeback/result.zh.md` | Phase 100 接入后正确性、asm、板卡和 Evidence Doctor 结果。 |
| `doc/phases/optimization-matrix.zh.md` | 候选族、证据状态和未阻塞下一步。 |
| `doc/optimization-roadmap.zh.md` | 跨阶段候选搜索空间。 |

## 常用命令

```bash
make -C test-rvv/sample_consensus/sac_model_stick run_test_compare
make -C test-rvv/sample_consensus/sac_model_stick run_stick_count_tests
make -C test-rvv/sample_consensus/sac_model_stick run_stick_select_tests
make -C test-rvv/sample_consensus/sac_model_stick run_stick_getdistances_tests
make -C test-rvv/sample_consensus/sac_model_stick run_stick_point_type_tests
make -C test-rvv/sample_consensus/sac_model_stick clean_bench_rvv
make -C test-rvv/sample_consensus/sac_model_stick check_production_asm
make -C test-rvv/sample_consensus/sac_model_stick collect_production_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_stick record_production_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_stick collect_vector_writeback_board_evidence
make -C test-rvv/sample_consensus/sac_model_stick record_vector_writeback_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_stick production_evidence_status
make -C test-rvv/sample_consensus/sac_model_stick vector_writeback_evidence_status
```

板卡命令需要在当前 shell 注入 `SSH_AUTH_SOCK`，并通过 `test-rvv/config.mk` 或命令行设置 `REMOTE_USER`、`REMOTE_IP` 和 `REMOTE_DIR`。QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状，不写成性能结论。

## 当前证据白名单

以下 summary evidence（摘要证据）可作为提交候选；raw board logs（原始板卡日志）、build 二进制和反汇编完整输出默认只留本机：

- `doc/phases/000-stick-count-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/000-stick-count-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/000-stick-count-diagnostic/repeated-evidence-doctor.json`
- `doc/phases/020-stick-select-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/020-stick-select-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/020-stick-select-diagnostic/repeated-evidence-doctor.json`
- `doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-doctor.json`
- `doc/phases/080-stick-production-integration/production-repeated-evidence-manifest.json`
- `doc/phases/080-stick-production-integration/production-repeated-evidence-doctor.md`
- `doc/phases/080-stick-production-integration/production-repeated-evidence-doctor.json`
- `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-manifest.json`
- `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-doctor.md`
- `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-doctor.json`
- `log/evidence_registry.json`

提交前需要用路径限定 staging（加入暂存区）逐项加入当前 topic 产物；`log/evidence_registry.json`
受 `test-rvv/.gitignore` 的 `**/log/**` 规则影响，必须显式 `git add -f`。raw board logs、
QEMU logs、`build/` 和本机配置仍保持 local-only（仅本机保留）。

## 当前边界

当前 production-adopted（已采纳生产实现）范围覆盖 `PointXYZ` direct indexed row source（直接索引行来源）的公开入口性能证据，并通过 traits-gated xyz AoS（结构数组）gate 允许满足布局条件的 `PointT` 命中 RVV。Phase 090 已补 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal` 的代表性 correctness；这些点型的独立性能仍未闭合，若要写成性能结论需要另建 board performance phase。

Phase 060 已补齐 topic-local doc suite（主题本地文档套件），Phase 070 已补齐 `run_stick_getdistances_tests` 细分入口。Phase 080 已完成生产接入、接入后板卡证据、正式 `doc-rvv` 和队列表同步；Phase 090 已补代表点型 correctness；Phase 100 已补 getDistances 向量写回和接入后板卡证据。
