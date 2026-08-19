# GICP Optimization Roadmap

## 当前边界

当前 topic 已完成 no-production closeout。cost-only 探针不建议采纳；`dfddfLoopRVV()` clean
production probe 只有弱正向：1024 点 median `1.080x`，4096 点 median `1.058x`。Phase 004
额外尝试把 matrix gather 从 `vluxei64` 改为 `vluxei32`，1024 点 median `1.073x`，没有优于
Phase 003。最终按用户确认回退全部 GICP 生产源码 RVV 接入，并删除不再适用的长期 `doc-rvv`
生产文档。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| residual dense-row RVV reduction | current source + LLS sibling | post-correspondence residual | 可能覆盖 optimizer functor 的批量扫描 | production 调用频率未知 | correctness、asm、board、public profile | positive diagnostic after `f64m4` | evidence retained |
| residual indexed-gather RVV reduction | current source | `tmp_idx_src_` / `tmp_idx_tgt_` style gather | 验证 dense-row 收益是否穿过 production-like 索引访存 | 仍未覆盖 PointT traits 和 solver | correctness、asm、board、public profile | positive diagnostic with indexed load | evidence retained |
| covariance post-KNN RVV reduction | current source | KNN 后 k-loop | k=20 默认邻域可批量规约 | KdTree、SVD 和长尾波动可能吞掉收益 | correctness、board component、public profile | weak_positive diagnostic with `f64m1` | secondary candidate |
| Mahalanobis 3x3 update | current source | correspondence 后矩阵更新 | 小矩阵重复多次 | 3x3 invert 和 Eigen 表达式不易 RVV 化 | component ablation | deferred | future bounded phase |
| Newton Hessian full component | current source | `dfddf()` | 默认 optimizer 热路径 | 公式更多、误差预算更复杂 | residual positive + profile + production public repeated | production public weak_positive: 1024 median `1.068x`，4096 median `1.070x` | PI5 user checkpoint |
| cost-only production probe | phase 001 source edit | public GICP entry / `operator()` | 真实生产接入 | 只覆盖 line-search cost，完整 GICP 中被稀释 | production public board repeated | neutral：1024 median `1.019x`，4096 median `1.011x` | do not adopt current patch |
| production integration loop | phase 003 result | public GICP entry | 真实生产接入 | 泛型点型、其它 `Scalar` 和其它 row source 尚未闭合 | clean production evidence | rolled back due to low benefit | no-production closeout |
| `dfddfLoopRVV()` matrix gather width | phase 004 result | public GICP entry / `dfddfLoopRVV()` detail | 可能减少 matrix gather offset 宽度开销 | 实际机器码收益被访存和规约吞掉 | QEMU correctness + board repeated 1024 | attempted no-improvement：1024 median `1.073x`，低于 Phase 003 `1.080x` | do not adopt |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | residual-first PI1 profile/probe | dense residual diagnostic `1.347x` positive；indexed residual `1.255x` positive；covariance `1.151x` weak_positive，但仍排除了 production dispatch / KdTree / SVD | public-entry profile、同一 production boundary A/B、fallback、asm attribution | high |
| 001 | public-entry profile before source edit | indexed gather 已保持正向，下一风险是 optimizer functor 在完整 GICP 中占比不足 | `doc/phases/001-production-profile-prerequisite/plan.zh.md`、production profile、Evidence Doctor | high |
| 001 | `dfddf()` 主循环生产探针 | cost-only 生产 public 为 neutral，但 `dfddf-loop-dense` 诊断 median `1.410x`，默认 Newton 每轮都会执行 | indexed-gather `dfddf()` production helper、public smoke、asm、board repeated、Evidence Doctor | high |
| 002 | production public 弱正向后进入 PI5 | `dfddfLoopRVV()` 1024 点 5-run median `1.068x`，4096 点 3-run median `1.070x`，均无退化 | 用户确认采纳 / 回滚 / 提交；若采纳再更新长期 `doc-rvv` | high |
| 003 | clean adoption 后重新复测 | 只保留 `dfddfLoopRVV()`，删除 cost-only helper；1024 点 median `1.080x`，4096 点 median `1.058x` | 当前范围 adopted；后续扩展需另开 phase | high |
| 004 | gather width 微调没有新增收益 | 32-bit matrix gather 1024 点 median `1.073x`，没有优于 Phase 003 clean `1.080x` | 不采纳，源码回到 Phase 003 形态 | high |
| 005 | 用户确认按低收益回退 | 生产源码回到标量实现，删除长期生产文档 | topic no-production closeout | high |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| residual dense-row `f64m1` | board repeated median `0.013x`，5/5 B/A < 1，Evidence Doctor Error | 已由 `f64m4` 替代 |
| covariance post-KNN `f64m4` | board repeated median `0.924x`，5/5 B/A < 1 | 保持 covariance `f64m1` |
| `dfddfLoopRVV()` matrix gather `vluxei32` | Phase 004 1024 点 median `1.073x`，低于 Phase 003 clean `1.080x`，没有同边界新增收益 | 已回退到 `vluxei64` |
| BFGS direction update | `test-rvv/registration/bfgs` 已显示小向量 board negative | 用户明确要求 bounded negative-analysis |

## 当前建议

当前 cost-only production probe 已完成，但 public GICP entry 只有 neutral：1024 点 median `1.019x`，4096 点 median `1.011x`。不建议单独采纳 cost-only patch。`OptimizationFunctorWithIndices::dfddf()` 主循环 clean probe 也只是 `weak_positive`：1024 点 median `1.080x`，4096 点 median `1.058x`。Phase 004 的 gather width 微调没有改善。当前同一轮内没有更高置信、同等成熟且无需扩大范围的 GICP 生产优化点；topic 已按 no-production 回退并可结束。
