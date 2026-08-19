# GICP 优化证据

| candidate family | idea source | applies to | required evidence | current status | decision boundary |
| --- | --- | --- | --- | --- | --- |
| residual dense-row RVV reduction | `OptimizationFunctorWithIndices::fdf()` | post-correspondence dense rows、`Scalar=double` 组件 | correctness、QEMU smoke、RVV asm、board repeated、Evidence Doctor、public profile | board positive after `f64m4`: median `1.347x`，0/5 B/A < 1 | 已作为生产探针线索，不单独采纳 |
| residual indexed-gather RVV reduction | `OptimizationFunctorWithIndices::fdf()` indices path | `tmp_idx_src_` / `tmp_idx_tgt_` style gather | correctness、QEMU smoke、RVV asm、board repeated、Evidence Doctor、public profile | board positive: median `1.255x`，0/5 B/A < 1 | 索引访问信号已用于 Phase 002 风险判断 |
| covariance post-KNN RVV reduction | `computeCovariances()` k-loop | KNN 后 k 邻居局部协方差 | correctness、QEMU smoke、board repeated | board weak_positive with `f64m1`: median `1.151x`，0/5 B/A < 1 | 次级候选；当前不优先于 `dfddfLoopRVV()` |
| Mahalanobis matrix update | `R*C1*R^T+C2` + invert3x3 | correspondence 后 3x3 小矩阵 | component ablation、asm、board | deferred | 小矩阵 / Eigen helper 可能稀释收益 |
| Newton Hessian full component | `OptimizationFunctorWithIndices::dfddf()` | default optimizer | residual 结果、profile、Hessian candidate、production public repeated | diagnostic median `1.410x` positive；Phase 002 1024 median `1.068x` weak_positive；Phase 002 4096 median `1.070x` weak_positive；clean 1024 median `1.080x` weak_positive；clean 4096 median `1.058x` weak_positive | 收益偏低，最终 rollback / no-production |
| cost-only production probe | `OptimizationFunctorWithIndices::operator()` | public GICP `PointXYZ -> PointXYZ` | production direct correctness、board repeated、Evidence Doctor | 1024 点 median `1.019x` neutral；4096 点 median `1.011x` neutral | 不建议采纳当前 patch |
| clean production public `dfddfLoopRVV()` | `OptimizationFunctorWithIndices::dfddf()` | public GICP `PointXYZ -> PointXYZ` | production direct correctness、board repeated、Evidence Doctor | 1024 点 median `1.080x` weak_positive；4096 点 median `1.058x` weak_positive | weak_positive 不足以接入，已回退生产补丁 |
| `dfddfLoopRVV()` matrix gather width | `dfddfLoopRVV()` helper 内部实现形态 | public GICP `PointXYZ -> PointXYZ` | QEMU correctness、board repeated、Evidence Doctor | 1024 点 median `1.073x` weak_positive，未优于 Phase 003 clean `1.080x` | 不采纳；源码已回到 `vluxei64` matrix gather |
| BFGS direction update | `test-rvv/registration/bfgs` | `useBFGS()` optional path | historical board negative | rejected with evidence | 小向量局部更新已负向，不作为本轮候选 |

## 经验迁移审计

| sibling 经验维度 | sibling topic 里的机制 | 当前 topic 是否适用 | 状态 | 证据 / 理由 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| residual / matrix accumulation | LLS / dual quaternion 的批量累加 | residual dense-row 组件相似 | adopted | 当前源码也有 per-correspondence `M*d` 与矩阵累加 | 先做组件诊断 |
| search dilution（搜索稀释） | TVE / correspondence 主题显示 search 可能吞掉收益 | GICP 主入口强相关 | adopted | `computeTransformation()` 每轮先做 transform + correspondence search | 需要 public profile |
| BFGS 小向量 | bfgs topic board negative | GICP `useBFGS()` 可选路径 | rejected | 6 维 state update 不作为批量 RVV 目标 | 不接入本 topic |
| row source policy | transformation estimation topics 逐 policy 审计 | 当前 residual 是 post-correspondence dense rows | deferred | 真实 production 仍有 PointCloud + indices + correspondence | PI1 前补入口映射 |

## Phase 000 证据索引

| path | 用途 |
| --- | --- |
| `test-rvv/registration/gicp/log/qemu/run_test_std.log` | 标量正确性对照 |
| `test-rvv/registration/gicp/log/qemu/run_test_rvv.log` | RVV 正确性对照 |
| `test-rvv/registration/gicp/log/qemu/run_bench_all_rvv.log` | QEMU smoke，仅日志形状 |
| `test-rvv/registration/gicp/build/asm/riscv/bench_gicp_rvv.asm` | bench binary RVV 指令存在性 |
| `test-rvv/registration/gicp/log/qemu/evidence_doctor.md` | QEMU raw summary doctor，metadata-limited |
| `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/summary.md` | 板卡 repeated 性能摘要 |
| `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/evidence_manifest.json` | 板卡 evidence metadata |
| `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/evidence_doctor.md` | 板卡 Evidence Doctor |
| `test-rvv/registration/gicp/log/board/dfddf_loop_dense_repeated/summary.md` | `dfddf()` 主循环 dense-row 诊断 |
| `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_repeated/summary.md` | cost-only 生产公开入口 1024 点 repeated |
| `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_4096_repeated/summary.md` | cost-only 生产公开入口 4096 点扩展复跑 |
| `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_repeated/summary.md` | `dfddfLoopRVV()` 生产公开入口 1024 点 repeated |
| `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_4096_repeated/summary.md` | `dfddfLoopRVV()` 生产公开入口 4096 点扩展复跑 |
| `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_repeated/summary.md` | clean adoption 1024 点 repeated |
| `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_4096_repeated/summary.md` | clean adoption 4096 点扩展复跑 |
| `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_gather32_repeated/summary.md` | Phase 004 gather width 尝试，1024 点 no-improvement |

## 当前取舍

residual dense-row 和 indexed-gather 支持进入生产探针，但 Phase 001 的 cost-only public entry
结果只有 neutral，不建议单独采纳 `operatorCostRVV()` patch。Phase 003 清理 cost-only helper
后，`dfddfLoopRVV()` 生产路径重新复测仍为 `weak_positive`。Phase 004 的 32-bit matrix gather
微调没有改善。最终按用户确认回退整个生产补丁；当前 topic 结论是 no-production。

## LMUL 取舍

| component | tried shape | board result | decision |
| --- | --- | --- | --- |
| residual dense-row | `f64m1` | historical negative, median `0.013x` | rejected |
| residual dense-row | `f64m4` | current positive, median `1.347x` | keep as diagnostic evidence |
| residual indexed-gather | `f64m4` + `vluxei64` | current positive, median `1.255x` | keep as diagnostic evidence |
| covariance post-KNN | `f64m4` | intermediate negative, median `0.924x` | rejected |
| covariance post-KNN | `f64m1` | current weak_positive, median `1.151x` | keep as secondary candidate |
| `dfddfLoopRVV()` matrix gather | `vluxei32` | no improvement, 1024 median `1.073x` vs Phase 003 `1.080x` | rejected / reverted |
