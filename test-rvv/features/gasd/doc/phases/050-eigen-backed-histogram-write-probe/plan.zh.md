# Phase 050 plan: Eigen-backed histogram write probe

## 阶段意图和边界

本阶段把 Phase 040 的 flat histogram write probe（扁平直方图写回探针）推进到
Eigen-backed histogram write probe（使用 `std::vector<Eigen::VectorXf>` 的直方图写回探针）。
目标是验证在更接近 production（生产源码）的 histogram container（直方图容器）布局下，
RVV（RISC-V Vector，可变长度向量）trilinear staging（三线性暂存）是否仍能保留收益。

本阶段仍不修改 `features/include/pcl/features/impl/gasd.hpp`。RVV candidate 只计算
`grid_idx/h_idx/w000..w111` staging buffer（暂存缓冲区），写回仍使用 scalar `Eigen::VectorXf::operator[]`
累加。这样可以审计 Phase 040 的 flat-vs-Eigen mismatch（扁平布局和 Eigen 布局不一致），但不声称实现了
vector scatter accumulate（向量分散累加）。

## validated_scope / unvalidated_scope

| field | scope |
| --- | --- |
| validated_scope | `PointXYZ` synthetic shape samples、`float`、Phase 010 projection staging、`INTERP_TRILINEAR`、`std::vector<Eigen::VectorXf>` histogram grid、dense finite samples |
| unvalidated_scope | public `computeFeature` dispatch、descriptor output copy、`INTERP_NONE`、`INTERP_QUADRILINEAR`、color interpolation、其它 PointT traits、`Scalar=double` |
| phase_closeout_boundary | 只能关闭 trilinear staging + scalar Eigen-backed write probe 的 correctness、asm、board diagnostic 条目 |

## 当前状态清单

| area | state | evidence |
| --- | --- | --- |
| Phase 040 flat histogram write probe | diagnostic weak-positive | `doc/phases/040-histogram-write-probe/result.zh.md`、`log/board/repeated_phase040_histogram_write_probe/summary.md` |
| production histogram layout | `addSampleToHistograms` 写 `std::vector<Eigen::VectorXf>`，每个 grid cell 有一个 Eigen vector | `features/include/pcl/features/impl/gasd.hpp` |
| remaining mismatch | Phase 040 计时没有覆盖 Eigen per-cell allocation / layout / operator[] 写回 | 本 plan 的 diagnostic-to-production mismatch audit |
| board | 当前会话确认可用 | Phase 040 repeated board 已完成 |

## 假设与候选族

| hypothesis | candidate | risk | validation |
| --- | --- | --- | --- |
| Eigen-backed 写回后仍可能保留 Phase 040 的弱正向 | `accumulateTrilinearHistogramEigenRVVStaged`：RVV staging + scalar Eigen write | Eigen per-cell layout 更接近 production，可能让 weak-positive 变成 neutral / negative | same-chain correctness、QEMU smoke、asm、board repeated、Doctor |
| scalar direct baseline 必须直接计算并写 Eigen histogram | `accumulateTrilinearHistogramEigenStd` | 如果 baseline 也复用 staging，会重复 Phase 040 初始错误并虚高收益 | correctness 对拍和 result 中 freshness 说明 |
| Eigen layout 仍不是完整 public compute | test-only helper 只覆盖 addSampleToHistograms 的 trilinear write 子段 | 不覆盖 transform、normalization、descriptor copy 和 public wrapper | EvidenceDecision 保持 diagnostic boundary |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Eigen-backed histogram write probe | per-sample interpolation write | `PointXYZ` / `float` / `std::vector<Eigen::VectorXf>` | planned `make run_test_compare` | planned `candidate_trilinear_eigen_histogram_write_rvv` | pending | pending | pending | phase_deferred + unblocked |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| E1 scalar Eigen reference | `include/impl/gasd_reference.hpp` | 直接计算 bin / 权重并写 `HistogramGrid` | 与 production trilinear write 公式同构，不共享 staging baseline |
| E2 RVV staged Eigen candidate | `include/impl/gasd_copy_candidate.hpp` | RVV staging + scalar Eigen write helper | `run_test_compare` Std/RVV 通过 |
| E3 correctness test | `src/test_gasd.cpp` | `GASDEigenHistogramWrite` 对拍 | 证明 Eigen-backed path 与 scalar reference 一致 |
| E4 bench case | `src/bench_gasd.cpp` | case `candidate_trilinear_eigen_histogram_write_rvv` 输出 timing / checksum | QEMU 只作 smoke，性能等 board |
| E5 manifest metadata | `script/generate_gasd_evidence_manifest.py` | case metadata 写清 evidence role、A/B boundary、timer boundary 和 Eigen layout | Evidence Doctor 不误判为 copy case |
| E6 asm | `make dump_bench_rvv` | RVV bench asm 命中 staging helper；Eigen write 仍为 scalar boundary | 写入 result |
| E7 board repeated + Doctor | `make board_repeated ... --case-filter candidate_trilinear_eigen_histogram_write_rvv ...`、`make evidence_doctor_repeated` | 5-run summary + manifest + doctor | Errors=0；Warnings 解释或降级 |
| E8 文档回填 | result、matrix、roadmap、evaluation、README | 当前 phase 可恢复 | 写清继续 / 停止条件 |

## Evidence Doctor 和 registry 规则

本阶段继续使用 topic-local manifest wrapper。新增 case `candidate_trilinear_eigen_histogram_write_rvv`：
evidence role 为 `diagnostic`，A/B boundary 为 `test helper`，timer boundary 为
`trilinear interpolation staging plus scalar Eigen-backed histogram accumulation`。当前仍没有正式
`log/evidence_registry.json`；本阶段结束时先用 manifest + artifact tracking 承担 freshness（新鲜度）
检查，并把 registry 接入保留为结构增强项。

## 板卡复跑预算和决策桶

预算为 5 次 repeated board run，iterations=10、warmup=2。decision bucket：
`positive` >= 1.10x 且无反向；`weak_positive` 为 median 1.03x-1.10x 且反向不超过 1/5；
`neutral` 为 0.97x-1.03x；`negative` < 0.97x；跨桶摇摆标 `unstable`。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：scalar direct trilinear compute+Eigen write vs RVV staging+scalar Eigen write |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 production-like histogram layout 下是否仍值得继续到 public-shaped combined path |
| diagnostic 是否可外推到 production | no；虽使用 `Eigen::VectorXf` 容器，但仍没有 public dispatch、descriptor copy、object state 和完整 compute |
| comparison-boundary / baseline mismatch 风险 | yes；bench wrapper 预先构造 projection staging，不覆盖 transform、normalization、sample loop 上游成本 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但必须先解释 Eigen-backed diagnostic 与 public compute 的剩余 mismatch；负向时不能直接写 no-production |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 继续 / 停止条件

默认继续到 E1-E8。合法停止条件：helper 无法在当前工具链编译、QEMU correctness 失败且无法修复、
板卡不可达、Evidence Doctor Error 无法消除、或继续需要 production 授权。

如果 Eigen-backed result 为 `positive` 或稳定 `weak_positive`，下一 phase 默认审计 production-shaped
combined shape path（生产形态组合 shape 路径）：projection + trilinear + Eigen-backed write + copy boundary。
如果为 `neutral` / `negative`，下一 phase 仍不能直接 no-production；需要先把结果写成
diagnostic-to-production mismatch audit，并判断是否改做 profile / public-shaped smoke。
