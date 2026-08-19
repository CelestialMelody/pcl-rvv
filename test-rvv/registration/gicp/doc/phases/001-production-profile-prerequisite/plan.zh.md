# Phase 001: Production Profile Prerequisite Plan

## 阶段意图和边界

本阶段只回答是否可以把 Phase 000 的 test-only RVV candidate 转化为 production probe（生产探针）。允许动作是 profiling（性能占比分析）、indexed gather mismatch audit（索引访存错配审计）、fallback 设计和 production direct test plan。默认不直接修改 `registration/include/pcl/registration/impl/gicp.hpp`；若必须插入 instrumentation（埋点）或 gated probe，需先在交接信息中明确请求用户确认。

## Phase 000 输入证据

| component | source mapping | diagnostic result | PI1 风险 |
| --- | --- | --- | --- |
| residual / Mahalanobis dense-row | `OptimizationFunctorWithIndices::{operator(),df,fdf}` | board median `1.347x`，0/5 B/A < 1，`positive` | optimizer 调用频率可能稀释 |
| residual indexed-gather | `OptimizationFunctorWithIndices::{operator(),df,fdf}` with `tmp_idx_src_` / `tmp_idx_tgt_` style gather | board median `1.255x`，0/5 B/A < 1，`positive` | 仍未覆盖 PointT traits、`computeRDerivative()` 和 solver |
| covariance post-KNN | `computeCovariances()` 中 `nearestKSearch` 之后 k-loop | board median `1.151x`，0/5 B/A < 1，`weak_positive` | KdTree search、3x3 SVD 和长尾波动可能吞掉局部收益 |

## 必须回答的问题

| question | required artifact | pass condition |
| --- | --- | --- |
| public GICP entry 中 residual / covariance 各占多少 | profile summary | 至少拆出 covariance、correspondence search、Mahalanobis update、optimizer functor / solver 的相对占比 |
| dense-row residual 收益是否能穿过 indices gather | indexed residual diagnostic 或 production profile | Phase 000 已通过：indexed median `1.255x`，仍需 production profile 确认 |
| `Scalar=float/double` 和 point type 如何 fallback | dispatch / fallback note | RVV path 只覆盖明确支持形态，其他路径稳定回标量 |
| asm attribution 是否能落到 production hot helper | asm report | 不只证明 bench binary 有 RVV 指令，还要能关联到 probe helper |
| correctness budget 是否接受 reduction order 差异 | production direct correctness plan | 明确误差预算和 public behavior 不变量 |

## 推荐动作顺序

| order | action | notes |
| ---: | --- | --- |
| 1 | 写 public-entry profile 方案 | 如果必须 touching source，只作为单独 PI1 confirmation checkpoint |
| 2 | 用户确认后插入 gated instrumentation | profile 必须能拆出 covariance、correspondence、Mahalanobis update、optimizer functor / solver |
| 3 | 用户确认后再进入 production probe | probe 必须 gated，且不可直接 clean adoption |

## 暂停条件

若 public-entry profile 显示 optimizer functor 占比过低，停止 production 接入建议，只保留 Phase 000 的组件结论。若占比足够，再请求用户确认是否允许做 production-source gated probe。
