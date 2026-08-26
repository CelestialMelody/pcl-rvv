# Phase 010: public-search-shaped dilution check result

## 执行摘要

本阶段没有修改 production（生产源码）。`mi_public_search_shape` bench 使用真实 `pcl::search::KdTree<PointXYZ>` nearestKSearch（近邻搜索）外层，再把邻域 indices（索引）交给测试专用 Std/RVV helper replacement（helper 替换）。这是 production-shaped diagnostic（生产形态诊断），不是 `MomentInvariantsEstimation::computeFeature` 的 production direct（真实生产路径）证据。

板卡 5-run repeated 结果为 near-threshold weak-positive（接近阈值的弱正向）：median speedup 1.031x，min 1.026x，max 1.037x，0/5 run 低于 1.0。Evidence Doctor 为 0 Error / 0 Warning / 3 Suggestion，其中 `near_threshold_ba` 说明收益离 1.0 阈值不足 0.05。当前证据不建议直接进入 production patch；若未来要继续，只应作为用户确认后的有界生产探针。

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| 定义 public-search-shaped bench | done | `src/bench_moment_invariants.cpp` 的 `mi_public_search_shape` | 真实 KdTree search 外层计入 timing boundary（计时边界），moment helper 仍是 test-only replacement。 |
| QEMU bench smoke | done | `make run_bench_rvv BENCH_ARGS='--case-filter mi_public_search_shape --points 4096 --iterations 1 --warmup-iterations 0'` 历史 smoke | 只验证日志形状和 checksum 非零，不作为性能结论。 |
| 板卡 repeated | done | `log/board/repeated_phase010_public_search_shape_diagnostic/summary.md` | 5-run median 1.031x，decision bucket 为 `weak_positive`。 |
| summary metadata 修正 | done | `script/generate_mi_repeated_summary.py`、`Makefile` | Phase 010 manifest 标成 `production_shaped_diagnostic`、`production_shaped_helper`、`kd_tree_k_neighbor_query`。 |
| Evidence Doctor | done | `log/board/repeated_phase010_public_search_shape_diagnostic/evidence_doctor.md` | 0 Error / 0 Warning / 3 Suggestion；新增 near-threshold 提醒。 |
| registry 记录 | done | `log/evidence_registry.json` | Phase 010 summary、manifest、doctor 已登记为 fresh，并指向本 result。 |

## 证据分层

| 层级 | 当前证据 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness（正确性） | Phase 000 `run_test_compare` | Std/RVV helper replacement 在 `PointXYZ / float / AoS` 范围内数值一致。 | 不证明真实 production dispatch 或 fallback。 |
| QEMU path（QEMU 路径） | public-search-shaped smoke | case-filter、输出格式和 checksum 口径可被脚本解析。 | 不证明性能。 |
| asm（反汇编） | `build/asm/riscv/bench_moment_invariants_rvv.asm` | RVV helper 指令存在于 bench binary。 | 不证明 production symbol（生产符号）命中。 |
| board performance（板卡性能） | Phase 010 summary | KdTree search 外层计入后，helper 替换只剩 1.031x median 的弱信号。 | 不证明 `computeFeature` 加 RVV dispatch 后仍有同等收益。 |

## diagnostic-to-production mismatch audit

| 问题 | 回填结果 |
| --- | --- |
| evidence role | `production_shaped_diagnostic`。 |
| A/B boundary | `production-shaped helper`；真实 KdTree search 外层 + test-only helper replacement。 |
| 当前决策问题 | 是否值得进入 bounded production probe（有界生产探针）。 |
| 是否可外推到 production | 只能作为接近真实调用形态的弱线索，不能直接外推。production 仍需要公开入口 dispatch、fallback、非 dense 输入和真实 estimator 输出测试。 |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 两侧同一 bench wrapper，但 helper replacement 不是 production helper；baseline 不是真实 production scalar dispatch。 |
| weak 结果是否允许 probe | 允许用户确认后做 PI1 readiness audit（生产接入准备审计），但当前不推荐默认 production patch。 |
| clean adoption 是否需要 detail A/B | 需要真实 production direct correctness、fallback、asm、board repeated 和 PI5 用户确认；当前全部缺失。 |

## Evidence Doctor 解释

`environment_metadata_missing` 和 `binary_identity_missing` 与 Phase 000 相同，记录为证据卫生建议。`near_threshold_ba` 是当前决策的关键限制：median 1.031x 太接近阈值，继续 production patch 的维护成本、fallback 复杂度和泛型点类型边界风险可能高于收益。当前结论因此降级为 `bench-only/no-production`，不是 `production-ready`。

## 继续 / 停止决定

Phase 010 的证据动作已闭合。下一步进入 Phase 020 文档套件 closeout，整理不接 production 的诊断证据链、文档归属、Traceability Map（可追踪性地图）和筛选状态。继续生产接入会扩大到 production 文件和真实 dispatch，需要用户显式确认；本轮不自动进入。
