# Phase 000 Plan: current state and component ablation

## 阶段意图和边界

本阶段只建立 `marching_cubes_rbf` 的 test-rvv diagnostic（诊断）闭环，回答：
RBF matrix fill（矩阵填充）和 voxel evaluation（体素求值）是否在包含 Eigen solve 的 full pipeline
里仍有板卡正向信号。阶段不修改 production 源码，不创建 `doc-rvv` production 长期主题文档。

## 范围

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断） |
| A/B boundary | test helper / bench wrapper |
| 当前决策问题 | 是否建议进入 production integration loop（生产接入闭环） |
| diagnostic 是否可外推到 production | unknown；只有 full pipeline 正向、asm 归属和数值误差闭合后才可作为 bounded production probe（有界生产探针）输入 |
| comparison-boundary / baseline mismatch 风险 | yes；test helper 使用 SoA centers 和 column-major matrix write，production 当前直接从 PointNT AoS 读取 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 默认 no；除非 matrix-only 强正向且 profile 证明 production solve 不主导 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；本阶段最多给出是否建议接入源码的确认材料 |

validated_scope：synthetic RBF centers、`Scalar=double`、matrix fill / voxel evaluation component、full pipeline diagnostic cases。

unvalidated_scope：真实 `PointNT` AoS load、真实 `MarchingCubesRBF::voxelizeData()` production dispatch、generic point type、`createSurface()` 生产收益、非 synthetic 输入。

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 创建 scaffold | `test-rvv/surface/marching_cubes_rbf/` | Makefile、board.mk、src、include、evaluation、roadmap、matrix 存在 |
| correctness | `make run_test_compare` | Std/RVV gtest 通过 |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS="--case-filter mcrbf_full_pipeline_n24_r18 --iterations 1 --warmup-iterations 0"` | RVV bench 可运行并输出 checksum；不作为性能结论 |
| asm | `make dump_bench_rvv` | RVV 指令存在；归属写入 result |
| board | `make run_board_component_smoke` | 板卡 Std/RVV compare 输出可解析 |
| Evidence Doctor | `python3 ../../script/evidence_doctor.py --summary-md log/board/analyze_bench_compare.log --output log/board/evidence_doctor.md --fail-on never` | summary-only doctor 产物存在；Warning 被解释 |

## 板卡复跑预算和决策桶

默认先跑一次 all-case board smoke。若 full pipeline 方向接近 1.0 或与 matrix-only 相反，最多追加一次
`run_board_matrix_n40` 和一次 `run_board_full_n40`。本阶段 decision bucket（决策桶）使用：

- `positive`：full pipeline 主要 case 稳定大于 `1.10x`，且 matrix-only 不退化。
- `weak_positive`：full pipeline 约 `1.03x-1.10x`，没有 checksum / doctor Error。
- `neutral`：full pipeline 约 `0.97x-1.03x`。
- `negative`：full pipeline 小于 `0.97x`。
- `unstable`：复跑后方向跨桶或 Evidence Doctor Warning 无法解释。

## 继续 / 停止条件

如果 full pipeline 为 `positive` 或可信 `weak_positive`，本轮停在用户确认点，建议是否进入 production
integration loop。若结果为 `neutral`、`negative` 或 `unstable`，不建议修改 production，结果写入 phase
result 和 evaluation 的诊断证据链。
