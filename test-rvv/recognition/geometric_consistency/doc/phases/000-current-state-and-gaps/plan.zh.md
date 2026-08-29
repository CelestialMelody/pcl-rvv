# Phase 000 Plan: current-state and gaps

## 阶段意图和边界

本阶段把 `recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp`
的 RVV 机会收窄为 pairwise distance consistency predicate（成对距离一致性谓词）
诊断。范围只覆盖固定 consensus set 下的局部 predicate 和 batch count，不覆盖
`std::sort`、RANSAC rejector、cluster growth、`found_transformations_` 顺序或 production patch。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| source | 生产源码是 header-only 的 `recognize()` / `clusterCorrespondences()` 链路 | `recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp` |
| topic assets | 当前 topic 还未形成 test-rvv scaffold | `test-rvv/recognition/geometric_consistency/` |
| board availability | 还未在本轮确认板卡配置；若能用则继续，不把“需要板卡”当停止理由 | `PCL_RVV_BOARD_*` env / local override |
| production state | 尚未进入 production integration loop | 当前无 production patch |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `pairwise-consistency-rvv` | 固定 consensus set 上的成对距离比较能批量化 | gather / sqrt / early-exit 的成本未拆开 | planned |
| `cluster-growth-rvv` | 若局部 predicate 正向，外层 growth 可能再吃掉一些成本 | `taken_corresps` 和顺序敏感 | deferred |
| `production patch` | 当前不应直接改 production | 证据不足 | rejected for this phase |

## 诊断到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic / component ablation |
| A/B boundary | test helper packed predicate vs scalar reference |
| 当前决策问题 | RVV-vs-scalar diagnostic |
| diagnostic 是否可外推到 production | unknown；只覆盖局部谓词，不覆盖排序、RANSAC 和输出顺序 |
| comparison-boundary / baseline mismatch 风险 | yes；fixed consensus set 与 production full cluster 不同 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no，先把局部 predicate 做实再说 |
| clean adoption 是否需要同一 production boundary 内 A/B | yes |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `pairwise-consistency-rvv` | correspondence-pair | `PointXYZ / float / AoS` packed diagnostic view | test helper predicate | scalar reference vs candidate | `bench_gc` batch predicate | planned 5-run board repeated if helper is worth continuing | RVV gather / sqrt / compare | planned | planned | write RED test first |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED test | `src/test_gc.cpp` | RVV build 命中 candidate path 之前，测试先能说明自己在守门 |
| scalar reference | `include/impl/gc_candidates.hpp` | predicate 语义和 production inner loop 一致 |
| RVV candidate | `include/impl/gc_candidates.hpp` | `__RVV10__` 下命中 RVV gather / math path |
| bench harness | `src/bench_gc.cpp` | board / QEMU 都能输出 summary 和 checksum |
| asm | `make -C ... check_gc_rvv_asm` | 看到预期 RVV 指令 |

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

