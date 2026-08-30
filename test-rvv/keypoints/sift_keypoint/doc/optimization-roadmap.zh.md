# SIFT Keypoint Optimization Roadmap

## 当前边界

当前 adopted production behavior（已采纳生产行为）只覆盖 `computeScaleSpace()` 中的
Gaussian weight loop（高斯权重循环）。RVV 路径批量计算 weight，后续 numerator /
denominator 仍按原邻域顺序走 scalar tail（标量尾段），以保持 public output trace（公开入口输出跟踪）
完全一致。

`findScaleSpaceExtrema()`、其它点型、真实 workload 和更大规模都不是当前 adopted 范围。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `production-public-gaussian-weight-rvv` | Phase 000 diagnostic + Phase 010 production-public board | `public_sift_keypoint_320x240` true public `compute()` | public median speedup 1.309x | 只覆盖当前 synthetic organized dense case | public trace、asm、5-run board、Evidence Doctor、registry | adopted | none |
| `full-vector-reduction` | 尝试把 weight 和 numerator / denominator 都规约到 RVV | `computeScaleSpace()` | 可能减少 scalar tail 成本 | 输出顺序和浮点规约树改变，曾破坏 public trace | order-preserving design、public trace、board A/B | rejected | only with new correctness plan |
| `extrema-scan-rvv` | `findScaleSpaceExtrema()` local extrema scan（局部极值扫描） | DoG matrix + nearest-k neighborhood | 可能减少极值扫描成本 | tie 语义、输出顺序、search 稀释风险高 | post-adoption profile、standalone fixture、public trace、board | deferred with stop condition | `020-post-adoption-profile` |
| `point-type-expansion` | SIFT 模板入口可用于其它 `PointInT` | broader public entry | 扩大 production 覆盖面 | intensity field、output type、layout 和 fallback 未证明 | traits / accessor audit、fallback tests、public trace、board | deferred with stop condition | `020-point-type-expansion` |
| `larger-public-workloads` | 当前 case 只用 320x240 synthetic cloud | 641x481 或真实业务输入 | 验证收益是否随 workload 保持 | search / extrema / output assembly 可能稀释收益 | workload definition、public repeated board、Evidence Doctor | deferred with stop condition | `020-larger-public-workloads` |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 010 | `post-adoption-profile` | 当前 production-public 证据已经 positive，但不能证明未优化阶段是否成为新瓶颈 | public profile + board repeated | medium |
| 010 | `point-type-expansion` | production patch 是模板函数内的 RVV 分支，但证据只覆盖 `PointXYZI -> PointWithScale` | fallback / public trace / board | medium |

## 默认恢复队列

| action | 状态 | 恢复条件 |
| --- | --- | --- |
| closeout current adopted scope | completed | phase 010 result、evaluation、doc suite、`doc-rvv` 和 screening 表已同步 |
| continue extrema scan optimization | turn_stop_deferred with stop_condition_hit | 需要新的 post-adoption profile 证明 extrema scan 是值得优先优化的热点 |
| continue point type expansion | turn_stop_deferred with stop_condition_hit | 需要用户或新 phase 明确扩大点型 / layout 证据范围 |
