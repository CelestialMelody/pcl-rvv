# Phase 010 Plan: cluster-growth-diagnostic

## 阶段意图和边界

本阶段把 `clusterCorrespondences()` 的 outer consensus growth loop 复刻到 test-support
层：先保留已排序 correspondences 的遍历和 `taken_corresps` 语义，再把 inner
pairwise predicate 组织进完整 cluster growth。范围仍不包含 RANSAC rejector、`
found_transformations_` 输出顺序或 production patch。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| predicate helper | Phase 000 已完成，pairwise RVV 候选 positive | `test-rvv/recognition/geometric_consistency/include/impl/gc_candidates.hpp` |
| board evidence | Phase 000 5-run positive | `test-rvv/recognition/geometric_consistency/log/board/repeated_phase000_pairwise_consistency_diagnostic/summary.md` |
| evidence doctor | Phase 000 `Errors=0`、`Warnings=1`、`Suggestions=1` | `test-rvv/recognition/geometric_consistency/log/board/repeated_phase000_pairwise_consistency_diagnostic/evidence_doctor.md` |
| registry | Phase 000 fresh after record | `test-rvv/recognition/geometric_consistency/log/evidence_registry.json` |
| production state | 尚未进入 production integration loop | 当前无 production patch |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `cluster-growth-rvv` | 把 outer growth loop 组织成同边界的诊断 helper 后，仍可保留 RVV 正向收益 | `taken_corresps` 和 consensus growth 的控制流更复杂 | planned |
| `production patch` | 当前不应直接改 production | 证据仍只覆盖诊断层 | rejected for this phase |

## 诊断到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic / component ablation |
| A/B boundary | sorted growth helper vs scalar reference |
| 当前决策问题 | RVV-vs-scalar diagnostic |
| diagnostic 是否可外推到 production | unknown；先补 full growth，再谈 production |
| comparison-boundary / baseline mismatch 风险 | yes；Phase 000 仅覆盖局部 predicate |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no |
| clean adoption 是否需要同一 production boundary 内 A/B | yes |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `cluster-growth-rvv` | correspondence-pair | `PointXYZ / float / AoS` packed to diagnostic SoA | full cluster growth loop | scalar reference vs RVV candidate | growth-mode `bench_gc` | planned 5-run board repeated if helper is worth continuing | RVV gather / sqrt / compare + growth control flow | planned | planned | write RED test first |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED test | `src/test_gc.cpp` | growth helper 的 scalar reference 与 candidate 对拍 |
| growth reference | `include/impl/gc_candidates.hpp` | outer cluster growth 和 taken_corresps 语义一致 |
| growth candidate | `include/impl/gc_candidates.hpp` | `__RVV10__` 下命中 RVV predicate path |
| growth bench | `src/bench_gc.cpp` | board / QEMU 能输出 growth mode summary 和 checksum |
| asm | `make -C ... check_gc_rvv_asm` | 看到预期 RVV 指令和 growth mode 的路径命中 |

## 板卡复跑预算和决策桶

默认 5-run。`median >= 1.20x` 且 `B/A < 1 = 0` 为 positive；`1.05x~1.20x`
为 weak_positive；`0.95x~1.05x` 为 neutral；`< 0.95x` 为 negative；预算内摇摆
为 unstable。

## 继续 / 停止条件

只要还有未阻塞的 test / bench / asm / board 动作，就继续下一步。只有用户限定范围、
dirty isolation 风险、工具/板卡不可用、必须扩大到 production 或证据矛盾时才停。

## 文档更新清单

本阶段更新 README、evaluation、testing-overview、correctness-tests、
benchmark-and-evidence、optimization-evidence、optimization-roadmap、
test-support-code-map、phase index、optimization matrix 和 current handoff。
当前不创建 `doc-rvv`。
