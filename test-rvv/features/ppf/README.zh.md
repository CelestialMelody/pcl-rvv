# PPF RVV Topic

本目录保存 `features/include/pcl/features/impl/ppf.hpp` 的 RVV topic-local（主题本地）测试、
bench（性能测试）、阶段文档和证据摘要。本主题已完成 production integration loop
（生产接入闭环）：`alpha_m` RVV 生产补丁已从 Phase 040 的 exact
`PointXYZ + Normal + PPFSignature` 扩展到 Phase 060 的 traits-gated source xyz AoS +
normal AoS + exact `PPFSignature`，正式长期文档见 `doc-rvv/features/ppf-RVV.zh.md`。

## 当前状态

当前 EvidenceDecision（证据决策）是
`adopted production behavior / S11 production closeout / Phase 070 ready for review`。
Phase 000 建立
S2 函数级评估、phase loop（阶段循环）和最小 correctness（正确性）入口；Phase 010 的
SoA-staged pair-feature batch RVV（数组结构暂存的点对特征批量 RVV）候选正确性成立，但 5-run
board repeated benchmark（板卡重复性能测试）稳定退化，因此不修改 production。Phase 030 的
`alpha_m` batch RVV 候选 5-run board speedup 为 `1.57, 1.57, 1.56, 1.57, 1.57`，
支持有界生产探针。Phase 040 已把该探针接入真实 public `PPFEstimation::compute` 路径，并用
production-public 5-run board 数据重新验证：`public_ppf_compute` speedup 为
`1.35, 1.35, 1.37, 1.39, 1.38`，mean Std `390.0784 ms`，mean RVV `284.9594 ms`。
Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=2`。Phase 050 已完成采纳确认后的
S11 文档收尾。Phase 060 又完成 point type expansion（点类型扩展）：`PointXYZI + Normal`
production-public 5-run speedup 为 `1.40, 1.43, 1.33, 1.41, 1.35`，mean Std `429.5662 ms`、
mean RVV `310.3166 ms`；`PointXYZ + PointNormal` speedup 为 `1.33, 1.33, 1.31, 1.33, 1.35`，
mean Std `412.4258 ms`、mean RVV `310.2756 ms`。两组 Evidence Doctor 均为
`Errors=0, Warnings=0, Suggestions=2`。当前没有值得在 PPF topic 内继续推进的高优先级未阻塞优化动作；
其它 row source、`Scalar=double`、PPFRGB / CPPF 和 evidence hardening（证据增强）需另开 phase 或 topic。

## 阅读路径

| 目的 | 入口 |
| --- | --- |
| 先读当前判断和 Traceability Map（可追踪性地图） | `doc/ppf-evaluation.zh.md` |
| 查看测试入口、target 粒度和证据边界 | `doc/testing-overview.zh.md` |
| 查看每个 gtest 的证明范围 | `doc/correctness-tests.zh.md` |
| 查看 bench case、板卡 summary 和 Evidence Doctor 边界 | `doc/benchmark-and-evidence.zh.md` |
| 查看候选族与 adopted / rejected / deferred 决策 | `doc/optimization-evidence.zh.md` |
| 查看测试支撑代码、聚合头和内部 helper 职责 | `doc/test-support-code-map.zh.md` |
| 恢复当前阶段 | `doc/phases/README.zh.md` |
| 查看 Phase 000 计划 | `doc/phases/000-current-state-and-gaps/plan.zh.md` |
| 查看 Phase 010 结果和板卡证据解释 | `doc/phases/010-pair-feature-and-output-staging-ablation/result.zh.md` |
| 查看 Phase 030 alpha RVV 候选和 PI1 边界 | `doc/phases/030-alpha-m-rvv-candidate/result.zh.md` |
| 查看 Phase 040 生产接入证据和 PI5 状态 | `doc/phases/040-production-alpha-m-rvv-integration/result.zh.md` |
| 查看 Phase 050 采纳收尾和正式文档同步 | `doc/phases/050-production-closeout-doc-rvv/result.zh.md` |
| 查看 Phase 060 点类型扩展和接入后板卡证据 | `doc/phases/060-point-type-expansion/result.zh.md` |
| 查看 Phase 070 文档套件 closeout 和 role inventory | `doc/phases/070-doc-suite-parity-closeout/result.zh.md` |
| 查看正式生产长期文档 | `../../../doc-rvv/features/ppf-RVV.zh.md` |
| 查看候选搜索空间 | `doc/optimization-roadmap.zh.md` |
| 查看证据矩阵 | `doc/phases/optimization-matrix.zh.md` |

## 常用命令

| 命令 | 作用 |
| --- | --- |
| `make -C test-rvv/features/ppf run_test_std` | 运行标量构建 correctness 测试。 |
| `make -C test-rvv/features/ppf run_test_rvv` | 运行 RVV 构建 correctness 测试；QEMU 结果不代表真实性能。 |
| `make -C test-rvv/features/ppf run_test_compare` | 顺序运行 Std/RVV 两侧 correctness 测试。 |
| `make -C test-rvv/features/ppf dump_bench_rvv` | 生成 RVV bench 反汇编，用于确认 candidate helper 的 RVV 指令存在。 |
| `make -C test-rvv/features/ppf REPEATED_BOARD_RUNS=5 BENCH_ARGS="--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2 --case-filter public_ppf_compute" board_repeated evidence_doctor_repeated` | 在板卡上隔离 production-public public case，并生成 Evidence Doctor（证据体检）摘要。 |
| `make -C test-rvv/features/ppf REPEATED_BOARD_RUNS=5 REPEATED_BOARD_OUTPUT_DIR=log/board/phase060-pointxyzi-normal/repeated BENCH_ARGS="--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2 --case-filter public_ppf_compute_pointxyzi_normal" board_repeated evidence_doctor_repeated` | 在板卡上验证 source 点型扩展代表 case。 |
| `make -C test-rvv/features/ppf REPEATED_BOARD_RUNS=5 REPEATED_BOARD_OUTPUT_DIR=log/board/phase060-pointxyz-pointnormal/repeated BENCH_ARGS="--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2 --case-filter public_ppf_compute_pointxyz_pointnormal" board_repeated evidence_doctor_repeated` | 在板卡上验证 normal 点型扩展代表 case。 |

## 证据提交边界

`doc/`、`include/`、`src/`、`Makefile` 和 `board.mk` 属于 topic 测试资产候选。
`features/include/pcl/features/impl/ppf.hpp` 是已采纳的 production patch。`build/`、
`log/qemu/` 和 `log/board/` 默认 local-only（仅本机保留），只有被 evaluation、phase result 或
Handoff 明确引用的 summary-only（仅摘要）证据才进入后续提交候选。正式
`doc-rvv/features/ppf-RVV.zh.md` 已创建并刷新，采用 Phase 060 接入后的板卡数据作为当前扩大覆盖的结论。
