# sac_model_normal_sphere RVV 主题入口

## 当前结论

本 topic 针对 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` 的三个公开距离入口完成 RVV 生产接入：
`selectWithinDistance`、`countWithinDistance` 和 `getDistancesToModel`。Phase 060 使用接入后的真实 public entry（公开入口）
Std/RVV 板卡对比，四种 source 点型 × 三入口共 12 项 comparison 全部为 positive（正向），Evidence Doctor（证据体检）
为 `Errors=0`、`Warnings=0`、`Suggestions=0`，因此按当前用户偏好采纳为 `production-adopted`。

当前采纳范围是 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA + pcl::Normal`，direct indexed `indices_`，
float xyz / normal AoS（结构数组）布局，signed 32-bit `pcl::index_t` 和可用 32-bit byte offset（字节偏移）表达的规模。
其它 normal 点型、自定义 registered point type（已注册自定义点型）、`Scalar=double`、其它 layout、真实 workload（工作负载）
和其它硬件不在当前采纳范围内。

## 阅读路径

1. `doc/sac_model_normal_sphere-evaluation.zh.md`：函数级评估、Traceability Map（可追踪性地图）和诊断证据链主归属。
2. `doc/testing-overview.zh.md`：测试入口分类、target 粒度和覆盖矩阵。
3. `doc/correctness-tests.zh.md`：gtest 输入、断言和证明范围。
4. `doc/benchmark-and-evidence.zh.md`：bench label、board summary、Evidence Doctor（证据体检）和 registry（证据登记表）。
5. `doc/optimization-evidence.zh.md`：candidate family（候选实现族）证据索引。
6. `doc/test-support-code-map.zh.md`：测试支撑代码、script 和 output 调用图。
7. `doc/phases/000-normal-sphere-count-select-diagnostic/result.zh.md`：count/select 诊断结果和 Phase 000 证据边界。
8. `doc/phases/010-vcompress-select-ablation/result.zh.md`：select 写回实现族消融结果。
9. `doc/phases/020-getdistances-dense-store-audit/result.zh.md`：`getDistancesToModel` dense-store 诊断结果。
10. `doc/phases/030-rgb-rgba-point-type-expansion/result.zh.md`：RGB/RGBA 点型扩展结果。
11. `doc/phases/040-structure-parity-doc-suite/result.zh.md`：文档套件结构对齐审计。
12. `doc/phases/050-pi1-production-integration-plan/result.zh.md`：PI1 候选范围、fallback、production direct 测试计划和用户授权边界。
13. `doc/phases/060-production-integration-execution/result.zh.md`：production patch、生产直连测试、板卡 repeated 和 PI5 采纳结论。
14. `doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md`：正式 production 长期主题文档。
15. `doc/phases/optimization-matrix.zh.md`：候选、点型、测试、bench 和板卡证据状态。
16. `doc/optimization-roadmap.zh.md`：跨阶段候选搜索空间和默认恢复动作。

## 常用命令

```bash
make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_compare
make -C test-rvv/sample_consensus/sac_model_normal_sphere dump_bench_rvv
make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase010-pointxyz REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase010-pointxyz BENCH_ARGS='65536 200 PointXYZ'
make -C test-rvv/sample_consensus/sac_model_normal_sphere record_phase010_evidence_state
make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase020-pointxyz REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase020-pointxyz BENCH_ARGS='65536 200 PointXYZ'
make -C test-rvv/sample_consensus/sac_model_normal_sphere record_phase020_evidence_state
make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase030-pointxyzrgb REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase030-pointxyzrgb BENCH_ARGS='65536 200 PointXYZRGB'
make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase030-pointxyzrgba REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase030-pointxyzrgba BENCH_ARGS='65536 200 PointXYZRGBA'
make -C test-rvv/sample_consensus/sac_model_normal_sphere record_phase030_evidence_state
make -C test-rvv/sample_consensus/sac_model_normal_sphere collect_phase060_repeated_board PHASE060_REPEATED_RUNS=5 BENCH_ARGS='65536 200 PointXYZ'
make -C test-rvv/sample_consensus/sac_model_normal_sphere run_phase060_evidence_doctor
make -C test-rvv/sample_consensus/sac_model_normal_sphere record_phase060_evidence_state
make -C test-rvv/sample_consensus/sac_model_normal_sphere evidence_status
```

QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自 board（板卡）或目标硬件。

## 提交边界

`src/`、`include/`、`doc/`、`Makefile`、`board.mk`、`log/evidence_registry.json` 和
`doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md` 是当前 topic 的可审查产物。
`doc/phases/*/board-evidence-summary.md`、`board-evidence-manifest.json` 和 `board-evidence-doctor.md` / `.json`
是 summary-only（只提交摘要）证据候选。`build/`、`log/board*/*.log`、`log/qemu/*.log`、`output/`
和 raw logs（原始日志）默认不提交。
