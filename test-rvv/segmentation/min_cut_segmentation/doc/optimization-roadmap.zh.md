# Optimization Roadmap

## 当前边界

本 topic 来自 `doc-rvv/library-screening/segmentation/segmentation-retained-candidate-rescreen.zh.md` 的执行清单。当前授权范围是 `segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp` 对应 topic 的测试资产、topic-local 文档和 component ablation。生产源码暂不修改。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| unary foreground min-distance batch | 当前源码 `calculateUnaryPotential`；APMF reduction/staging 经验 | `PointXYZ` / float xyz / dense AoS；按 input point 批处理，foreground 点循环标量展开 | foreground 点数较大时，用 RVV 跨 input points 同时更新最小距离 | foreground 很少时收益可能不足；production 里还会被 Boost graph mutation 和 max-flow 稀释 | same-chain correctness、bench component、asm、板卡 repeated、Evidence Doctor | diagnostic-positive: 5-run board median 2.08x | feeds Phase 010 |
| binary distance + expf approximation | 当前源码 `calculateBinaryPotential`；common `expf_RVV_f32m2` | `PointXYZ` / float xyz / dense AoS；边列表按 source/target index gather | KNN 后边数量大时，距离公式和 exp 可能有可见占比 | production 使用 double `std::exp`；`expf_RVV_f32m2` 是有限域 float 近似，不能直接替代生产语义 | 数值预算、same-chain tolerance、bench component、asm、板卡 repeated、Evidence Doctor | diagnostic-positive: 5-run board median 2.44x | feeds Phase 010 |
| production-shaped buildGraph timing | 筛选文档要求 solver/search 稀释审计 | public-shaped graph build；真实 KNN、Boost add_edge、edge_marker | 判断 potential loop 是否能穿透 search / graph 稀释 | test-only helper 与 production graph boundary 可能错配；不等于 production dispatch | production-shaped diagnostic、diagnostic-to-production mismatch audit、board evidence | complete-neutral: 5-run median 1.02x，2/5 退化，Evidence Doctor Error | stop current line |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| `000` | `buildgraph-potential-batch` | component diagnostic 两个候选均为 positive，但仍排除了 search / graph 成本 | production-shaped correctness、asm、5-run board、Evidence Doctor | high |
| `010` | none adopted | buildGraph-shaped 证据为 neutral，且退化频率触发 Evidence Doctor Error | 若未来重启，必须提出完全不同的 bounded hypothesis | not_applicable |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| production patch | Phase 010 buildGraph-shaped timing 为 neutral，且 Evidence Doctor 报退化频率 Error；真实 `extract()` 还会包含 max-flow，当前 potential batch 不建议接 production | 只有出现新的 bounded hypothesis，且重新完成 production-shaped positive / weak-positive 证据后才恢复 |
