# moment_invariants optimization evidence

## 当前结论摘要

当前 adopted production behavior（已采纳生产行为）是 traits-gated indexed moment accumulation RVV（由点类型字段特征 gate 保护的索引矩累加 RVV）：`computeFeature` 的 KdTree indexed neighbor path 在满足 `__RVV10__`、`PointOutT=pcl::MomentInvariants`、`RVVXYZAoSFloatLayout<PointT>`、dense 输入、邻域规模和 u32 byte offset gate 时，用 production RVV helper 累加六个中心矩。

## 优化方式总表

| candidate family | 代码路径 | 测试路径 | bench / board evidence | asm evidence | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- |
| helper indexed moment accumulation RVV | `include/impl/moment_invariants_reductions.hpp` | `src/test_moment_invariants.cpp` | Phase 000 summary median `1.141x` | `vluxei32`、`vfredusum` in bench asm | attempted, diagnostic positive | helper-only，不含 search / production dispatch。 |
| full-cloud moment accumulation RVV | `include/impl/moment_invariants_reductions.hpp` | `src/test_moment_invariants.cpp` | 未作为板卡主证据 | `vlse32`、`vfredusum` in bench asm | deferred with evidence | full-cloud production overload 当前保持标量。 |
| public-search-shaped helper replacement | `src/bench_moment_invariants.cpp` | Phase 000 correctness inherited | Phase 010 summary median `1.031x` | helper inlined into bench | attempted, near-threshold weak-positive | 生产形态诊断，不是真实 production dispatch。 |
| production indexed moment accumulation RVV | `features/include/pcl/features/impl/moment_invariants.hpp` | `src/test_moment_invariants.cpp` | Phase 030 `PointXYZ` summary median `1.067x` | `make check_production_rvv_asm` | adopted | `PointXYZ / float / dense AoS / MomentInvariants` public `computeFeature`。 |
| PointXYZ-like production expansion | `features/include/pcl/features/impl/moment_invariants.hpp` | `src/test_moment_invariants.cpp` typed tests | Phase 040 `PointXYZI 1.071x`、`PointXYZRGB 1.078x`、`PointXYZRGBA 1.078x` medians | typed production asm gates | adopted | 只覆盖三种常见 xyz AoS 点型；不外推到 normal 复合或自定义点型。 |
| evidence hardening | summary / manifest generator | not_applicable | 当前 Doctor 均 0E/0W/2S | not_applicable | deferred hygiene | 后续需要更强复现时补环境 metadata 和 binary hash。 |

## 标量路径与 RVV 路径差异

标量 production helper 先用 `compute3DCentroid`，再逐点累加六个中心矩。production RVV path（RVV 链路）复用标量 centroid，只替换 centroid 后的 indexed load/gather 和 reduction（规约）。`searchForNeighbors`、输出 NaN fallback、`output.is_dense` 更新和 full-cloud overload 仍按原标量或公开入口流程执行。

RVV reduction 改变浮点累加树，因此 correctness 用容差而不是 raw checksum strict equality（严格相等）闭合。Phase 030/040 board summary 的 raw checksum 不一致已按 manifest 标注为非结合规约的预期差异，语义等价由 gtest 容差 gate 证明。

## 采纳理由

| 维度 | 当前状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| RVV 是否比 std 好 | yes, weak-positive | 四个 production-public summary median `1.067x-1.078x`，均 0/5 退化。 | 支持有界采纳。 |
| 静态实现质量 | yes | public API 不变；helper 小；fallback gate 保守；生产 asm 可归属。 | 维护成本低。 |
| 平均情况 | yes | 每组 5-run median / min 均大于 1。 | 收益不大但稳定。 |
| 异常频率 | acceptable | Evidence Doctor 无 Error / Warning；只有 metadata suggestion。 | 记录为 evidence hardening，不阻塞。 |

## 暂缓路线

| 路线 | 当前状态 | 暂缓原因 | 恢复条件 |
| --- | --- | --- | --- |
| `PointXYZRGBNormal` / `PointXYZINormal` | deferred with evidence | 更大 stride 和实例化成本未由本阶段板卡证明。 | 新 phase 补 typed correctness、asm、board repeated 和 doctor。 |
| 自定义 traits-compatible 点型 | deferred with evidence | 没有代表性输入和 layout 分布。 | 用户提供或选定真实点型。 |
| full-cloud production overload | not_now | 当前 public `computeFeature` 主路径使用 indexed neighbor list。 | profile 或 caller 证明 full-cloud overload 是热点。 |
| `Scalar=double` 或其它输出类型 | not_applicable for current patch | 生产 helper 和输出语义只覆盖 float `MomentInvariants`。 | 另开范围审计和数值预算。 |
| KdTree/search 优化 | separate topic | 属于 search 或上层 pipeline，不应混入 moment accumulation patch。 | 独立 profile 证明 search 是优化目标。 |
