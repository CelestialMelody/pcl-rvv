# Phase 010 result: shape sample projection diagnostic

## 执行范围

本阶段完成 `GASDEstimation::computeFeature` 中 shape sample projection（形状样本投影）片段的 test-only diagnostic（测试专用诊断）闭环。候选把每个 `PointXYZ` 样本投影到 staging buffers（暂存缓冲区）：`grid_x/grid_y/grid_z/dbin`。本阶段不写真实 histogram，不覆盖 interpolation（插值）写回，不修改 production（生产源码），也不证明 public `compute()` 已经命中 RVV。

Phase 010 的输入域改为 production-like normalization（生产形态归一化）：`max_coord` 和 `distance_normalization_factor` 从 synthetic transformed cloud（合成变换后点云）推导，而不是固定为 `8/16`。旧固定归一化会把 4096 点 synthetic cloud 推到 production 不会采用的极大 ratio（距离除以步长）域，导致 `dbin` 小数部分在 float 精度边界上失真；该旧 run 已降级为历史诊断证据。

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| B1 test-first 红灯 | done | `GASDShapeProjection.ProjectsBenchSizedCloudWithProductionLikeNormalization` 曾在 RVV 侧暴露 4096 点 `dbin` mismatch | 测试能捕捉 projection 数值语义问题 |
| B2 scalar reference | done | `include/impl/gasd_reference.hpp` 的 `projectShapeSamplesStdToBuffers` 和 `computeShapeProjectionNormalization` | 标量参考复刻 production 的坐标缩放、距离归一化和 `std::modf` 小数 bin 语义 |
| B3 RVV candidate | done | `include/impl/gasd_copy_candidate.hpp` 的 `projectShapeSamplesRVVToBuffers` | RVV 路径使用 AoS stride load（结构数组跨步加载）、向量平方和、`vfsqrt`、`vfdiv`、RTZ 转换和 staging store；非 RVV 构建 fallback 到 scalar |
| B4 bench case | done | `src/bench_gasd.cpp`，case `candidate_shape_projection_rvv` | 输出 normalization、checksum 和 timing；checksum policy 与 correctness 容差对齐 |
| B5 asm | done | `make -C test-rvv/features/gasd dump_bench_rvv`，`build/asm/riscv/bench_gasd_rvv.asm` | filtered asm 命中 `vlse32.v`、`vfmacc.vv`、`vfsqrt.v`、`vfdiv.vf`、`vfcvt.rtz.xu.f.v`、`vse32.v` |
| B6 board repeated + Doctor | done | `log/board/repeated_phase010_shape_projection_fast/summary.md`、`evidence_doctor.md` | 5-run median 1.690x，range 1.690x-1.710x，bucket=`positive`；Doctor Errors=0、Warnings=0、Suggestions=2 |
| B7 文档回填 | done | 本文件、phase index、matrix、roadmap、evaluation、README | Phase 010 诊断正向，但仍不接 production；默认继续 Phase 020 |

## EvidenceDecision

`current_decision`: `diagnostic-positive / continue-phase-loop`。shape sample projection 在 test helper boundary（测试 helper 边界）上有稳定正向板卡证据，说明该逐样本 staging 片段比 Phase 000 copy tail 更值得继续作为 GASD 热点候选。它仍只是 component diagnostic（组件诊断），不能替代 production direct（真实生产路径）证据，也不能直接写成 production-ready（可接入生产）。

## 证据边界

| evidence | result | proves | does not prove |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/features/gasd run_test_compare`，Std/RVV 均 6/6 通过 | production-like normalization 下 projection staging 与标量参考在容差内一致 | 不证明 histogram interpolation 或 public compute dispatch |
| QEMU bench smoke | Std/RVV smoke 的 checksum 均为 `1.45433e+20` | bench wrapper、normalization 输出和 checksum policy 可运行且两侧一致 | QEMU timing 不用于性能结论 |
| asm | `build/asm/riscv/bench_gasd_rvv.asm` | projection RVV 指令形态存在，包含 stride load、sqrt、divide、float-to-int trunc 和 store | filtered asm 仍不是完整 public compute 热点占比证据 |
| board repeated | `log/board/repeated_phase010_shape_projection_fast/summary.md` | 目标硬件上 projection staging diagnostic 稳定正向，5-run median 1.690x | 不包含真实 histogram 写回、alignment、color hue 或 descriptor 输出 |
| Evidence Doctor | `log/board/repeated_phase010_shape_projection_fast/evidence_doctor.md` | Errors=0，Warnings=0，checksum 两侧一致，decision bucket=`positive` | environment metadata 和 binary identity 仍是建议项 |

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：scalar projection staging vs RVV projection staging |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 projection 是否值得继续到更接近 production 的 histogram probe |
| diagnostic 是否可外推到 production | no；本阶段只计算 staging buffer，不写 `addSampleToHistograms`，也没有 public dispatch |
| comparison-boundary / baseline mismatch 风险 | yes；真实 production 还包含 interpolation 写回、histogram boundary bins、descriptor copy 和对象状态 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes；但本阶段已经 positive，仍需先做后续 component / production-shaped probe 才能谈 production |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 异常和修正记录

| issue | status | resolution |
| --- | --- | --- |
| 固定 `max_coord=8`、`distance_normalization_factor=16` 导致 4096 点 `dbin` mismatch | fixed | 改为从 synthetic cloud 推导 production-like normalization，4096 点 RVV correctness 通过 |
| RVV ratio 使用乘倒数导致 farthest sample 在整数 bin 边界出现 `15.999...` vs `0` | fixed | 改为 `vfdiv`，对齐 production 标量 `distance / shape_grid_step` 的操作顺序 |
| projection checksum 过细导致 Evidence Doctor `checksum_mismatch` | fixed | 改为 tolerance-aligned coarse quantized checksum，QEMU smoke 和 board repeated 均两侧一致 |
| `lround` / 分支四舍五入污染 bench 计时 | fixed | 使用非负 staging 值上的轻量 `static_cast(value * 100)` 粗量化，并用新 tag `phase010_shape_projection_fast` 重跑 |

## Evidence Doctor 结果

| severity | count | 处理 |
| --- | ---: | --- |
| Errors | 0 | 无需修复。 |
| Warnings | 0 | 无需降级。 |
| Suggestions | 2 | `environment_metadata_missing` 和 `binary_identity_missing` 已记录为后续 evidence 增强项；不阻塞当前 diagnostic-positive。 |

## 旧 run 降级

| run tag | status | reason |
| --- | --- | --- |
| `phase010_shape_projection` | historical / blocked evidence | 初始固定 normalization 与 checksum mismatch，不能支撑性能结论 |
| `phase010_shape_projection_clean` | historical / polluted evidence | checksum 使用 `std::lround` 或分支 rounding，污染 timed loop，不作为当前结论 |
| `phase010_shape_projection_fast` | current evidence | checksum 一致、Doctor clean，作为 Phase 010 当前 truth |

## Optimization matrix 更新

`shape sample projection` 更新为 `attempted / diagnostic-positive`。Phase 020 默认进入 `color-hue-diagnostic`，因为 color path 中 hue projection 仍是逐样本分支热点，且仍在当前 topic 的 test-only diagnostic 授权范围内。

## continue / stop decision

`stop_condition_hit`: none。`next_phase_default`: `020-color-hue-diagnostic`。继续条件成立：下一候选仍在当前 topic 的 test-only assets 内，不需要 production 授权，板卡可用，当前 Evidence Doctor 无 Error。
