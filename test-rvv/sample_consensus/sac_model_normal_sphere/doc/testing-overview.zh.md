# sac_model_normal_sphere 测试总览

## 本文职责

本文只说明 test、bench（性能测试）、QEMU（仿真器）、board（板卡）和 evidence（证据）入口如何分类，以及每类入口能证明什么。每个 TEST 的输入和断言见 `doc/correctness-tests.zh.md`；bench 数值和 Evidence Doctor（证据体检）路径见 `doc/benchmark-and-evidence.zh.md`。

## 阅读路径

| 读者问题 | 主路径 |
| --- | --- |
| 当前 topic 结论和恢复入口是什么 | `README.zh.md`、`doc/phases/README.zh.md` |
| 每个 gtest 证明什么 | `doc/correctness-tests.zh.md` |
| bench label、board summary 和 registry 在哪里 | `doc/benchmark-and-evidence.zh.md` |
| 候选族哪些正向、哪些仍需 production | `doc/optimization-evidence.zh.md`、`doc/phases/optimization-matrix.zh.md` |
| helper、script、output 如何互相定位 | `doc/test-support-code-map.zh.md` |
| 函数级判断和诊断证据链 | `doc/sac_model_normal_sphere-evaluation.zh.md` |

## 运行入口分类

| 类别 | 当前入口 | 证明范围 | 不能证明 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_compare` | Std/RVV 两个构建下的 public entry、Standard helper、production RVV detail helper、fallback 和测试专用历史 candidate 输出一致。 | 不证明目标硬件性能。 |
| correctness alias（正确性细分入口） | `run_normal_sphere_public_tests`、`run_board_normal_sphere_public_tests` | 用 gtest filter 跑公开入口、production direct（真实生产路径）和 fallback 样本。 | 不是性能证据。 |
| diagnostic bench（诊断性能入口） | `make -C ... run_bench_rvv BENCH_ARGS='<points> <iters> <PointT>'` | 验证 bench binary 可运行、Dataset 行和单点型日志形状；历史 Phase000-030 用它生成候选数据。 | QEMU timing 不进入性能结论。 |
| board smoke（板卡小型验证） | `board_smoke OUTPUT_DIR_BOARD=... BENCH_ARGS='65536 200 <PointT>'` | 板卡 gtest、Std/RVV 日志、checksum 和单次性能信号。 | 不是当前 production repeated 结论；Phase000-030 只作历史诊断。 |
| board repeated（重复板卡测试） | `collect_phase060_repeated_board PHASE060_REPEATED_RUNS=5 BENCH_ARGS='65536 200 <PointT>'` | Phase060 按四种 source 点型生成 production-public 5-run summary。 | 只覆盖当前点型、规模、row source 和板卡。 |
| doctor / registry | `run_phase060_evidence_doctor`、`record_phase060_evidence_state`、`evidence_status`；历史阶段保留 `record_phase010_evidence_state` 等 target。 | 生成 manifest、summary、doctor 并登记 freshness。 | 不替代 correctness；raw log 默认不提交。 |
| historical probe guarded alias（历史探针保护入口） | 当前无旧 production probe 保护入口；历史 diagnostic label 保留在 bench binary 中。 | Phase000-030 只作候选历史，不作为当前生产性能 truth。 | 无法替代 Phase060。 |

## 输入数据总览

当前测试资产使用 direct indexed source/normal：`indices_` 同时索引 source cloud 和独立 `pcl::Normal` cloud。已验证并采纳的 source 点型为 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA`，`Scalar` 语义是 float xyz input 与 double output；未覆盖自定义 registered point type、其它 normal 点型、`Scalar=double` 和非 indexed 入口。

## 推荐执行顺序

1. `python3 -m py_compile test-rvv/sample_consensus/sac_model_normal_sphere/script/generate_normal_sphere_evidence_manifest.py`
2. `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_compare`
3. `make -C test-rvv/sample_consensus/sac_model_normal_sphere clean_bench_rvv dump_bench_rvv`
4. 生产性能复跑使用 `collect_phase060_repeated_board PHASE060_REPEATED_RUNS=5 BENCH_ARGS='65536 200 <PointT>'`
5. 按 phase 运行 `record_*_evidence_state`
6. `make -C test-rvv/sample_consensus/sac_model_normal_sphere evidence_status`

QEMU 只用于 correctness 和日志形状；性能结论必须来自 board 或目标硬件。

## 覆盖矩阵

| 路径 | 点型 / 布局 | correctness | bench / board | Evidence Doctor | production direct |
| --- | --- | --- | --- | --- | --- |
| public entries | `PointXYZ/PointXYZI/RGB/RGBA + Normal` | public 输出与参考链路一致 | Phase060 production-public 5-run 全部 positive | 0/0/0 | adopted |
| production RVV count | `PointXYZ/PointXYZI/RGB/RGBA + Normal` | covered | Phase060 count median `3.332x` / `3.358x` / `3.308x` / `3.411x` | 0/0/0 | adopted |
| production RVV select | `PointXYZ/PointXYZI/RGB/RGBA + Normal` | covered | Phase060 select median `3.039x` / `2.900x` / `2.973x` / `2.906x` | 0/0/0 | adopted |
| production RVV getDistances | `PointXYZ/PointXYZI/RGB/RGBA + Normal` | covered | Phase060 getDistances median `4.456x` / `4.157x` / `4.290x` / `4.161x` | 0/0/0 | adopted |
| Phase000-030 diagnostic candidates | `PointXYZ/PointXYZI/RGB/RGBA + Normal` | covered | historical positive smoke | low_run_count only | historical input |

## 当前结论边界

当前证据支撑 `production-adopted`：三入口和四种 source layout 在接入后的 production-public 边界内均为正向。
该结论只覆盖 Phase060 冻结的 direct indexed `indices_`、`pcl::Normal` normal cloud、float AoS layout、当前规模和当前板卡；
其它 normal 点型、自定义点型、`Scalar=double`、真实 workload 和其它硬件需要新证据。
