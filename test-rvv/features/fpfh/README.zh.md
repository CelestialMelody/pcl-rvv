# FPFH RVV 主题入口

本目录评估并验证 `features/include/pcl/features/impl/fpfh.hpp` 中
`FPFHEstimation::computePointSPFHSignature`、`weightPointSPFHSignature` 和 `computeFeature` 的 RVV 优化价值。
当前已完成 `weightPointSPFHSignature` 的有界 production patch（生产补丁），其它函数族仍按 roadmap
另开 phase 继续。

## 阅读路径

| 文档 | 作用 |
| --- | --- |
| `doc/fpfh-evaluation.zh.md` | S2/S11 函数级评估、Traceability Map（可追踪性地图）、fallback 矩阵和 production decision（生产接入判断）。 |
| `doc/phases/README.zh.md` | 阶段索引和默认恢复入口。 |
| `doc/phases/002-weighted-spfh-33-production-probe/result.zh.md` | Phase 002 PI5 事实主归属：QEMU、asm、board repeated、Evidence Doctor 和采纳边界。 |
| `doc/phases/optimization-matrix.zh.md` | 跨阶段优化矩阵。 |
| `doc/optimization-roadmap.zh.md` | topic-level optimization roadmap（主题级优化路线图）。 |
| `../../../doc-rvv/features/fpfh-RVV.zh.md` | 当前 adopted production behavior（已采用生产行为）的长期主题文档。 |

历史阶段：

- `doc/phases/000-current-state-and-caller-shaped-ablation/{plan,result}.zh.md`
- `doc/phases/001-weighted-spfh-33-diagnostic-candidate/{plan,result}.zh.md`
- `doc/phases/002-weighted-spfh-33-production-probe/plan.zh.md`

## 当前结论

`fpfh.hpp` 是建议队列中的 high-priority（高优先级）候选。Phase 002 已把 33-bin weighted FPFH
合成接入 production RVV path：

- production detail case `component_weighted_spfh_33`：5-run repeated 平均 4.804x，0/5 degradation。
- public case `public_fpfh_k`：5-run repeated 平均 1.262x，0/5 degradation。
- QEMU Std/RVV correctness：6/6 pass。
- Evidence Doctor：0 Error / 0 Warning / 9 Suggestions。

当前停止状态是 `topic_closeout_ready_for_topic_only_commit`：用户已确认当前 topic 可以结束并进入提交流程。
Phase 003 已说明 pair-feature batch 需要先补 bin-stability 和 scatter 语义证据，不建议在当前 closeout
中继续直接实现。

## 常用命令

```bash
make -B -C test-rvv/features/fpfh run_test_compare
make -B -C test-rvv/features/fpfh dump_bench_rvv
make -C test-rvv/features/fpfh board_smoke
make -C test-rvv/features/fpfh board_repeated \
  REPEATED_BOARD_OUTPUT_DIR=log/board/pi1-production-weighted/repeated
make -C test-rvv/features/fpfh evidence_doctor_repeated \
  REPEATED_BOARD_OUTPUT_DIR=log/board/pi1-production-weighted/repeated \
  EVIDENCE_MANIFEST_REPEATED=log/board/pi1-production-weighted/repeated/evidence_manifest.json \
  EVIDENCE_DOCTOR_REPEATED_MD=log/board/pi1-production-weighted/repeated/evidence_doctor.md \
  EVIDENCE_DOCTOR_REPEATED_JSON=log/board/pi1-production-weighted/repeated/evidence_doctor.json
```

`run_bench_compare` 默认受 guard（保护门）限制，不用于 QEMU 性能结论。QEMU（仿真器）只用于
correctness（正确性）、构建和日志形状；真实性能结论必须来自 board（板卡）或目标硬件。

## 证据提交边界

默认 evidence policy（证据策略）是 summary-only：raw logs（原始日志）不默认提交。

可审查的摘要证据路径：

- `test-rvv/features/fpfh/log/board/pi1-production-weighted/repeated/evidence_manifest.json`
- `test-rvv/features/fpfh/log/board/pi1-production-weighted/repeated/evidence_doctor.md`
- `test-rvv/features/fpfh/log/board/pi1-production-weighted/repeated/evidence_doctor.json`
- `test-rvv/features/fpfh/log/board/pi1-production-weighted/repeated/run_*/analyze_bench_compare.log`

当前提交阶段采用 topic-only commit（只提交本主题相关产物）：默认不提交上述生成证据、raw logs（原始日志）、
build 输出或其它 topic 改动。

## Doc Suite Role Inventory

| role | status |
| --- | --- |
| topic_navigation | `standalone:README.zh.md` |
| testing_overview | `merged:doc/fpfh-evaluation.zh.md#测试和 Bench 计划` |
| correctness_tests | `merged:doc/fpfh-evaluation.zh.md#测试和 Bench 计划` |
| benchmark_and_evidence | `merged:doc/fpfh-evaluation.zh.md#当前验证结果` |
| optimization_evidence | `merged:doc/optimization-roadmap.zh.md` and `doc/phases/optimization-matrix.zh.md` |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` |
| test_support_code_map | `merged:doc/fpfh-evaluation.zh.md#Traceability Map` |
| phase_index / phase_result / optimization_matrix | `standalone:doc/phases/**` |
| evaluation_production | `standalone:doc/fpfh-evaluation.zh.md` |
| production_topic_doc | `standalone:doc-rvv/features/fpfh-RVV.zh.md` |
