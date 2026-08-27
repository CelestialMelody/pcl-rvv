# Phase 030 result: interpolation ablation

## 当前结论

Phase 030 已闭合 trilinear interpolation arithmetic / index staging（三线性插值算术 / 索引暂存）
诊断边界。`candidate_trilinear_interpolation_rvv` 在板卡 5-run repeated benchmark（重复性能测试）中
median speedup 为 1.950x，range 为 1.560x-2.030x，5/5 checksum 一致，Evidence Doctor（证据体检）
为 Errors=0、Warnings=1、Suggestions=2。decision bucket 为 `positive`。

这个结果只证明 test helper boundary（测试 helper 边界）下的 `grid_idx/h_idx` 和 8 个空间权重暂存
适合继续向 histogram write probe（直方图写回探针）推进；它不证明真实 `Eigen::VectorXf` 写回、
scatter conflict（分散写冲突）、quadrilinear hue（四线性色相）维度或 production dispatch（生产分流）。

## 实际执行范围

| field | result |
| --- | --- |
| validated_scope | `PointXYZ` synthetic shape samples、`float`、Phase 010 projection staging、`INTERP_TRILINEAR`、dense finite samples |
| unvalidated_scope | `INTERP_NONE`、`INTERP_QUADRILINEAR`、真实 histogram writes、color interpolation、public compute dispatch、其它 PointT traits、`Scalar=double` |
| phase_closeout_boundary | 关闭 trilinear arithmetic / index staging 的 correctness、QEMU smoke、asm、board diagnostic、Evidence Doctor 条目 |
| production status | 未修改 `features/include/pcl/features/impl/gasd.hpp`；`doc-rvv/features/gasd-RVV.zh.md` 仍不适用 |

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| D1 test-first 红灯 | done | `make -C test-rvv/features/gasd run_test_rvv` 曾因缺少 `TrilinearInterpolationBuffers` / `computeTrilinearInterpolation*ToBuffers` 编译失败 | 红灯来自缺少本阶段 helper，符合 TDD（测试驱动开发）预期 |
| D2 scalar reference | done | `test-rvv/features/gasd/include/impl/gasd_reference.hpp` | scalar reference 生成 `grid_idx/h_idx/w000..w111`，保持 production trilinear 分支同构 |
| D3 RVV candidate | done | `test-rvv/features/gasd/include/impl/gasd_copy_candidate.hpp` | RVV path 使用 vector floor / index arithmetic / weight arithmetic；非 RVV 构建回退到 scalar |
| D4 bench case | done | `test-rvv/features/gasd/src/bench_gasd.cpp`、`script/generate_gasd_evidence_manifest.py` | `candidate_trilinear_interpolation_rvv` 输出 checksum 和 timing；QEMU 只作 smoke |
| D5 asm | done | `build/asm/riscv/bench_gasd_rvv.asm` | 命中 `vfcvt.rtz.x.f.v`、`vfcvt.f.x.v`、`vle32.v`、`vse32.v`、`vadd`、`vmul`、`vfsub`、`vfmul` 等本阶段所需指令 |
| D6 board repeated + Doctor | done | `log/board/repeated_phase030_trilinear_interpolation/summary.md`、`evidence_doctor.md` | 5-run median 1.950x，checksum 一致；Doctor Errors=0、Warnings=1、Suggestions=2 |
| D7 文档回填 | done | 本文件、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/gasd-evaluation.zh.md`、`README.zh.md` | 当前阶段可恢复，默认下一 phase 为 histogram write probe |

## Correctness / QEMU / asm / board 证据链

| evidence layer | command / path | result | boundary |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/features/gasd run_test_rvv` | RVV 构建 8/8 通过 | gtest 对拍 `grid_idx/h_idx` 与 8 个权重 buffer |
| correctness compare | `make -C test-rvv/features/gasd run_test_compare` | Std/RVV 两侧 8/8 通过 | 覆盖 Phase 000-030 当前测试资产 |
| QEMU smoke | `bench_gasd_* --case-filter candidate_trilinear_interpolation_rvv ...` | Std/RVV checksum 均为 `2.76313e+19` | 只证明可运行和日志形状，不参与性能结论 |
| asm attribution | `make -C test-rvv/features/gasd dump_bench_rvv` | RVV bench asm 命中本阶段 vector load/store、float/int convert、加减乘指令 | 归属到 test-only bench binary，不是 production symbol |
| board performance | `log/board/repeated_phase030_trilinear_interpolation/summary.md` | median 1.950x，min 1.560x，max 2.030x，B/A < 1 为 0/5 | 板卡性能证据，仅覆盖 trilinear arithmetic / index staging |
| Evidence Doctor | `log/board/repeated_phase030_trilinear_interpolation/evidence_doctor.md` | Errors=0，Warnings=1，Suggestions=2 | 可继续，但必须解释长尾和 metadata 缺口 |

## Evidence Doctor 解释

| severity | signal | handling |
| --- | --- | --- |
| Error | none | 无需修正。 |
| Warning | `long_tail_or_variance`：min=1.56x、median=1.95x、max=2.03x，max/min=1.30 | 保留 min/median/max，不剔除 run 02/03；本阶段仍为 positive，因为 5/5 均正向且不接近 1.0，但后续 histogram write probe 应记录同类波动并考虑环境字段。 |
| Suggestion | `environment_metadata_missing` | 不阻塞本阶段；下一次 board summary 优先补 taskset / governor / freq / temperature 或在 Handoff 写明缺口。 |
| Suggestion | `binary_identity_missing` | 不阻塞本阶段；若后续出现方向反转，应补 binary hash / build label 后清理旧日志重跑。 |

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：scalar trilinear staging vs RVV trilinear staging |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 interpolation arithmetic / index staging 是否值得继续到 histogram write probe |
| diagnostic 是否可外推到 production | no；没有真实 histogram write、没有 public dispatch、没有 production helper |
| comparison-boundary / baseline mismatch 风险 | yes；真实 production 还包含 `hists[grid_idx + offset][h_idx + offset] += weight`、边界 bins、descriptor copy 和对象状态 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但必须先完成 histogram write boundary 诊断；若写回吞掉收益，只能降级为局部正向证据 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；当前结果不能 clean-adopt |

## Optimization matrix 更新

`trilinear / quadrilinear interpolation` 中的 trilinear arithmetic / index staging 子项从 `planned`
更新为 `attempted / diagnostic-positive`。未闭合项是真实 histogram writes、color quadrilinear
和 production-shaped combined path（生产形态组合路径）。这些都是当前 topic 授权内的后续 phase，
不构成本轮停止条件。

## 阶段反思和新增路线

本阶段证明单独计算 `grid_idx/h_idx` 和 8 个空间权重有明显 RVV 收益，说明插值链路的算术段不是
天然不值得优化。下一阶段必须把 write boundary（写回边界）纳入计时，否则仍无法回答真实
`addSampleToHistograms` 的核心风险：分散更新 `Eigen::VectorXf` histogram 是否会吞掉算术收益。

新增 / 重排的路线：

| candidate family | reason | priority | next evidence |
| --- | --- | --- | --- |
| histogram write probe | Phase 030 positive 后的直接缺口，能验证 scatter/write cost | high | same-chain correctness、QEMU smoke、asm、board repeated、Doctor |
| production-shaped combined shape path | 需要在 projection + trilinear + write 组合边界复核收益 | medium | Phase 040 后再计划；不能跳过 write probe |
| quadrilinear color interpolation | color 默认 `INTERP_NONE`，但 public API 支持 quadrilinear | medium / deferred | 先完成 shape trilinear write；后续独立 phase |

## Continue / Stop Decision

`continue_stop_decision`：继续。未命中停止条件；板卡可用，Doctor 无 Error，生产源码未修改，且 roadmap /
matrix 仍有当前 topic 授权内的 high-priority unblocked action。

`next_phase_default`：`040-histogram-write-probe`。

`stop_condition_hit`：none。
