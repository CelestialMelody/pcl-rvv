# Phase 010: public-search-shaped dilution check plan

## 阶段意图和边界

Phase 000 证明 indexed helper-only moment accumulation（只计中心矩累加）在板卡上是 weak-positive diagnostic（弱正向诊断）。本阶段继续检查真实 KdTree search（KD 树邻域搜索）外层是否稀释该收益。范围仍是 test-only production-shaped diagnostic（仅测试使用的生产形态诊断）：bench 使用真实 `pcl::search::KdTree<PointXYZ>` 查询邻域，然后把邻域 indices 交给 Std/RVV helper。production 源码、public API（公开接口）和 `MomentInvariantsEstimation::computeFeature` 不修改。

## 当前状态清单

| 对象 | 当前状态 | 证据 |
| --- | --- | --- |
| helper-only candidate | QEMU `run_test_compare` Std/RVV 各 3/3 pass；板卡 5-run median 1.141x、min 1.095x、0/5 退化。 | `doc/phases/000-current-state-and-diagnostic-plan/result.zh.md` 待回填；summary 在 `log/board/repeated_phase000_moment_accumulation_diagnostic/summary.md` |
| Evidence Doctor | Phase 000 为 0 Error / 0 Warning / 2 Suggestion；缺少环境 metadata 和 binary hash。 | `log/board/repeated_phase000_moment_accumulation_diagnostic/evidence_doctor.md` |
| public shape risk | production 入口每个输出点先 search，再做 centroid 和 moment accumulation。 | `features/include/pcl/features/impl/moment_invariants.hpp` |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| public-search-shaped helper replacement | KdTree k-neighbor query | `PointXYZ / float / AoS` | production-shaped diagnostic | Phase 000 correctness inherited; bench checksum diagnostic only | `mi_public_search_shape` | planned, 5 runs | test-only helper inlined; no production symbol | planned | planned |

## 实现和测试动作

1. 将 `mi_public_search_shape` bench 定义为真实 KdTree search 外层 + `computeMomentSummaryStd/RVV` helper，而不是直接调用当前 production estimator。
2. 运行 QEMU bench smoke，确认日志格式、checksum 和 case-filter 可解析；不把 QEMU timing 写成性能结论。
3. 在板卡上运行 5-run repeated：`--case-filter mi_public_search_shape --points 4096 --iterations 8 --warmup-iterations 2`。
4. 生成 summary、Evidence Doctor 和 registry。若结果为 positive/weak-positive，S10 可给出 bounded production probe 候选；若 neutral/negative，则建议 no-production closeout。

## Evidence Doctor 和 registry 规则

输出目录为 `log/board/repeated_phase010_public_search_shape_diagnostic/`。本阶段结果仍是 diagnostic，不作为 production direct 证据；如果 summary 或 doctor 暴露 checksum、long-tail、metadata 或 boundary 问题，必须在 result 中降级或解释。

## 继续 / 停止条件

若 public-search-shaped 结果 positive 或 weak-positive，默认下一 phase 是 `020-production-integration-readiness-audit`，只写 PI1-ready 候选和缺口，不自动改 production。若结果 neutral/negative，默认进入 no-production closeout 并同步筛选状态。
