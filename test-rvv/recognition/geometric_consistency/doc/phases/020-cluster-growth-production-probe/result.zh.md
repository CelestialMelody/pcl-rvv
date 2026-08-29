# Phase 020 Result: cluster-growth-production-probe

## 执行范围

本阶段把 `clusterGrowthCandidate()` 单独作为 growth-mode board probe 重新登记，
验证 outer cluster growth 形态是否也保留正向收益。范围仍不包含 `std::sort`、
RANSAC rejector、transformations 输出顺序或公开 API 变更。

## 当前结论

growth-mode repeated board 结果为正向，且比前一轮更像一个可重复的稳定信号，而不是单次偶发：

- median `2.51x`
- min `2.50x`
- max `2.56x`
- `B/A < 1 = 0/5`
- checksum stable

Evidence Doctor 报告：

- `Errors=0`
- `Warnings=1`
- `Suggestions=1`

Warning 只是在提醒：`cluster_growth` 这个名字含有 production，但 evidence_role 仍是
diagnostic，不能直接外推成 production evidence。Suggestion 仍是补环境 metadata
（taskset / governor / freq / temperature）。

## 动作回填

| action | status | evidence |
| --- | --- | --- |
| growth board target | done | `make -C test-rvv/recognition/geometric_consistency board_repeated_growth BENCH_ARGS='4096 200 5 0.03 growth'` |
| growth summary | done | `log/board/repeated_phase020_cluster_growth_production_probe/summary.md` |
| growth evidence doctor | done | `log/board/repeated_phase020_cluster_growth_production_probe/evidence_doctor.md` |
| growth evidence manifest | done | `log/board/repeated_phase020_cluster_growth_production_probe/evidence_manifest.json` |
| registry freshness | done | `make -C test-rvv/recognition/geometric_consistency check_evidence_freshness` |

## 证据路径

- Summary: `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/summary.md`
- Manifest: `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/evidence_manifest.json`
- Doctor: `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/evidence_doctor.md`
- Doctor JSON: `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/evidence_doctor.json`
- Registry: `test-rvv/recognition/geometric_consistency/log/evidence_registry.json`

## 诊断到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic / production-shaped diagnostic |
| A/B boundary | growth helper vs scalar reference |
| 当前决策问题 | RVV-vs-scalar growth probe |
| diagnostic 是否可外推到 production | unknown；当前只能说明 growth 形态本身正向 |
| comparison-boundary / baseline mismatch 风险 | yes；这仍是 test-support helper，不是新的 production dispatch |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no；当前已经足够正向，但还没形成新 production boundary |
| clean adoption 是否需要同一 production boundary 内 A/B | yes |

## 优化矩阵更新

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `cluster-growth-rvv` | correspondence-pair | `PointXYZ / float / AoS` growth helper probe | full cluster growth benchmark shape | scalar reference vs candidate | growth-mode `bench_gc` / board repeated | phase 020 positive | RVV gather / sqrt / compare + growth control flow | warning only, no errors | deferred | keep as probe; no new production-direct boundary yet |

## 继续 / 停止条件

本阶段已经闭环，可以停止。下一步不应继续硬拽同一 growth probe，而应保留当前 adopted
pairwise production patch，并在需要时重新开启新的 production-direct growth phase。
如果不打算继续，就按 closeout 处理当前 topic；如果要继续，必须先找到一个真正独立的新候选，
不能把 phase 020 的 growth 信号再外推一次。

