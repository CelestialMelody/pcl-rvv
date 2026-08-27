# GASD RVV Topic

本目录保存 `features/include/pcl/features/impl/gasd.hpp` 的 RVV（RISC-V Vector，可变长度向量）topic-local（主题本地）测试、bench（性能测试）、阶段文档和证据摘要。

## 当前状态

当前 EvidenceDecision（证据决策）是 `no-production for current staged shape family / stop-for-user-review`。Phase 060 的
production-shaped shape combined diagnostic（生产形态 shape 组合诊断）在板卡 5-run repeated benchmark
（重复性能测试）上得到 median 0.590x、range 0.590x-0.600x，checksum 一致。Evidence Doctor
（证据体检）为 Errors=1、Warnings=0、Suggestions=2；这个 Error 是 5/5 退化频率，用于把该候选降级为
稳定负向诊断。Phase 070 已把当前 staged shape family 收口为不接入 production（生产源码）。这个结果不能外推成
整个 GASD topic 的全局拒绝。Phase 080 补充了 public compute profile（公开入口性能剖析）：未改 production
源码的 `public_gasd_shape_compute` 为 median 1.220x，`public_gasd_color_compute` 为 median 1.110x，Doctor 均为
Errors=0、Warnings=0、Suggestions=2；该信号只能说明当前 RISC-V / RVV build 下公开入口有 build-level
profile signal（构建层级性能信号），不能归因为当前手写 staged shape family。

默认下一步是用户审查。继续 production probe（生产探针）需要用户授权；继续 color quadrilinear interpolation
（颜色四线性插值）应作为独立 follow-up。

## 阅读路径

| 目的 | 入口 |
| --- | --- |
| 先读当前判断和 Traceability Map（可追踪性地图） | `doc/gasd-evaluation.zh.md` |
| 恢复当前阶段 | `doc/phases/README.zh.md` |
| 查看 Phase 000 计划 | `doc/phases/000-current-state-and-gaps/plan.zh.md` |
| 查看 Phase 010 计划 | `doc/phases/010-shape-sample-projection-diagnostic/plan.zh.md` |
| 查看 Phase 010 结果 | `doc/phases/010-shape-sample-projection-diagnostic/result.zh.md` |
| 查看 Phase 020 计划 | `doc/phases/020-color-hue-diagnostic/plan.zh.md` |
| 查看 Phase 020 结果 | `doc/phases/020-color-hue-diagnostic/result.zh.md` |
| 查看 Phase 030 计划 | `doc/phases/030-interpolation-ablation/plan.zh.md` |
| 查看 Phase 030 结果 | `doc/phases/030-interpolation-ablation/result.zh.md` |
| 查看 Phase 040 计划 | `doc/phases/040-histogram-write-probe/plan.zh.md` |
| 查看 Phase 040 结果 | `doc/phases/040-histogram-write-probe/result.zh.md` |
| 查看 Phase 050 计划 | `doc/phases/050-eigen-backed-histogram-write-probe/plan.zh.md` |
| 查看 Phase 050 结果 | `doc/phases/050-eigen-backed-histogram-write-probe/result.zh.md` |
| 查看 Phase 060 计划 | `doc/phases/060-production-shaped-shape-combined-diagnostic/plan.zh.md` |
| 查看 Phase 060 结果 | `doc/phases/060-production-shaped-shape-combined-diagnostic/result.zh.md` |
| 查看 Phase 070 计划 | `doc/phases/070-no-production-closeout-profile-audit/plan.zh.md` |
| 查看 Phase 070 结果 | `doc/phases/070-no-production-closeout-profile-audit/result.zh.md` |
| 查看 Phase 080 计划 | `doc/phases/080-public-compute-profile-audit/plan.zh.md` |
| 查看 Phase 080 结果 | `doc/phases/080-public-compute-profile-audit/result.zh.md` |
| 查看路线图 | `doc/optimization-roadmap.zh.md` |
| 查看跨 phase 证据矩阵 | `doc/phases/optimization-matrix.zh.md` |
| 查看 Phase 000 board summary | `log/board/repeated_phase000_shape_copy/summary.md` |
| 查看 Phase 000 Evidence Doctor | `log/board/repeated_phase000_shape_copy/evidence_doctor.md` |
| 查看 Phase 010 board summary | `log/board/repeated_phase010_shape_projection_fast/summary.md` |
| 查看 Phase 010 Evidence Doctor | `log/board/repeated_phase010_shape_projection_fast/evidence_doctor.md` |
| 查看 Phase 020 board summary | `log/board/repeated_phase020_color_hue/summary.md` |
| 查看 Phase 020 Evidence Doctor | `log/board/repeated_phase020_color_hue/evidence_doctor.md` |
| 查看 Phase 030 board summary | `log/board/repeated_phase030_trilinear_interpolation/summary.md` |
| 查看 Phase 030 Evidence Doctor | `log/board/repeated_phase030_trilinear_interpolation/evidence_doctor.md` |
| 查看 Phase 040 board summary | `log/board/repeated_phase040_histogram_write_probe/summary.md` |
| 查看 Phase 040 Evidence Doctor | `log/board/repeated_phase040_histogram_write_probe/evidence_doctor.md` |
| 查看 Phase 050 board summary | `log/board/repeated_phase050_eigen_histogram_write_probe/summary.md` |
| 查看 Phase 050 Evidence Doctor | `log/board/repeated_phase050_eigen_histogram_write_probe/evidence_doctor.md` |
| 查看 Phase 060 board summary | `log/board/repeated_phase060_shape_combined/summary.md` |
| 查看 Phase 060 Evidence Doctor | `log/board/repeated_phase060_shape_combined/evidence_doctor.md` |
| 查看 Phase 080 shape public summary | `log/board/repeated_phase080_public_shape_compute/summary.md` |
| 查看 Phase 080 shape public Evidence Doctor | `log/board/repeated_phase080_public_shape_compute/evidence_doctor.md` |
| 查看 Phase 080 color public summary | `log/board/repeated_phase080_public_color_compute/summary.md` |
| 查看 Phase 080 color public Evidence Doctor | `log/board/repeated_phase080_public_color_compute/evidence_doctor.md` |

## 常用命令

| 命令 | 作用 |
| --- | --- |
| `make -C test-rvv/features/gasd run_test_std` | 运行标量构建 correctness（正确性）测试。 |
| `make -C test-rvv/features/gasd run_test_rvv` | 运行 RVV 构建 correctness 测试；QEMU（仿真器）结果不代表真实性能。 |
| `make -C test-rvv/features/gasd run_test_compare` | 顺序运行 Std/RVV 两侧 correctness 测试。 |
| `make -C test-rvv/features/gasd dump_bench_rvv` | 生成 RVV bench 反汇编，用于确认 candidate helper 命中 RVV 指令。 |
| `make -C test-rvv/features/gasd board_repeated` | 在板卡上执行当前 repeated bench 默认 case，并拉回 summary 日志。 |
| `make -C test-rvv/features/gasd evidence_doctor_repeated` | 从 repeated board summary 生成 manifest 并运行 Evidence Doctor。 |
| `make -C test-rvv/features/gasd run_bench_compare` | 默认在本 topic 禁用；bench 结论优先来自板卡或目标硬件。 |

## 证据提交边界

`doc/`、`include/`、`src/`、`Makefile` 和 `board.mk` 都是 topic 测试资产候选。`log/evidence_registry.json`
登记的 summary / Evidence Doctor / manifest 是 summary-only（仅摘要）证据候选；因为 `test-rvv/.gitignore`
默认忽略 `log/`，提交流程需要用 `git add -f` 精确加入登记表列出的摘要证据。`build/`、`log/qemu/`、
`log/board/*/run_*/`、临时历史 probe 日志和板卡 raw `.log` 默认 local-only（仅本机保留）。`doc-rvv/features/gasd-RVV.zh.md`
目前不适用，等 adopted production behavior（已采纳生产行为）或 PI5 生产证据闭环后再考虑。
