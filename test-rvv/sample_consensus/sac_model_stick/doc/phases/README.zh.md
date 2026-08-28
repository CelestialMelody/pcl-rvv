# sac_model_stick phase index

本文是 `test-rvv/sample_consensus/sac_model_stick/` 的 phase suite（阶段文档套件）入口。当前 topic 已完成 Phase 000 的 `countWithinDistance` 诊断闭环、Phase 020 的 `selectWithinDistance` 诊断闭环和 Phase 040 的 `getDistancesToModel` 诊断闭环；Phase 080 已把三条公开入口接入 production direct（真实生产入口直连）RVV 路径。Phase 100 进一步把 `getDistancesToModelRVV` 的 penalty 写回改成全向量 mask / merge（掩码 / 合并）和 double 向量 store（双精度向量写回），并用接入后的 5-run board repeated（板卡重复采集）数据完成采纳。

| phase | 状态 | 默认恢复动作 |
| --- | --- | --- |
| `000-stick-count-diagnostic` | completed: `partial-production-candidate` | 已完成 RED/GREEN、QEMU correctness、asm、5-run board repeated、Evidence Doctor 和 registry。 |
| `010-stick-count-production-integration-plan` | completed: superseded by Phase 080 | PI1 计划已并入 Phase 080 三入口生产接入。 |
| `020-stick-select-diagnostic` | completed: `partial-production-candidate` | 已完成 RED/GREEN、QEMU correctness、asm、5-run board repeated、Evidence Doctor 和 registry。 |
| `030-stick-select-production-integration-plan` | completed: superseded by Phase 080 | PI1 计划已并入 Phase 080 三入口生产接入。 |
| `040-stick-getdistances-diagnostic` | completed: `partial-production-candidate` | 已完成 RED/GREEN、QEMU correctness、asm、5-run board repeated、Evidence Doctor 和 registry；public 行的 Error 已降级为未接 RVV 的 negative cross-check。 |
| `050-stick-getdistances-production-integration-plan` | completed: superseded by Phase 080 | PI1 计划已并入 Phase 080 三入口生产接入。 |
| `060-stick-doc-suite-structure` | completed: doc-suite structure | 已补 `testing-overview`、`correctness-tests`、`benchmark-and-evidence`、`optimization-evidence` 和 `test-support-code-map`，并同步 README、evaluation、roadmap、matrix、Makefile refs。 |
| `070-stick-getdistances-target-granularity` | completed: target granularity | 已补 `run_stick_getdistances_tests`，使 count/select/getDistances 三条 correctness alias 粒度一致；不改变 production 或性能结论。 |
| `080-stick-production-integration` | completed: production-adopted | 已完成 production patch、Phase 080 时 Std/RVV 10/10 gtest、production asm gate、5-run board repeated、Evidence Doctor 0/0/0、registry 和正式 `doc-rvv`；Phase 090 后当前 aggregate 已扩展为 11/11。 |
| `090-stick-point-type-expansion` | completed: representative point type correctness | 已验证 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal` 的 public entry vs Standard helper 输出一致；不写 dedicated board performance 结论。 |
| `100-stick-getdistances-vector-writeback` | completed: production-adopted implementation shape | 已移除 `getDistancesToModelRVV` 内的 staged scalar lane 写回，新增源码/asm 形态 gate；接入后 public getDistances 5-run board speedup 为 3.4122x / 3.6879x / 3.7150x，Evidence Doctor 0/0/0。 |

当前 `next_phase_default`：Phase 100 getDistances implementation-shape 已闭合。若维护者需要把性能结论从 `PointXYZ` 扩大到常见点型集合，另建 dedicated point-type board performance phase；当前 topic 内没有新的同边界 production implementation-shape candidate。
