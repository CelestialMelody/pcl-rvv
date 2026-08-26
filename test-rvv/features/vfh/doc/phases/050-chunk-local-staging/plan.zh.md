# Phase 050 Chunk-local Staging Plan

## 阶段意图和边界

本阶段继续 VFH（Viewpoint Feature Histogram，视点特征直方图）production RVV
优化，但不扩大 Phase 040 的生产覆盖范围。目标只验证一个实现形态候选：
把 `accumulateVFHSPFHRVV()` 和 `accumulateVFHViewpointRVV()` 中按整云大小分配的
staging buffer（暂存缓冲）改为按 VL chunk（可变向量长度分块）复用的小缓冲，并在每个
chunk 内按 lane 顺序立即执行标量 histogram scatter（直方图离散累加）。

本阶段覆盖的入口仍是 `VFHEstimation::computeFeature()` 默认公开入口：
`PointInT` 满足 `RVVXYZAoSFloatLayout`、`PointNT` 满足 normal AoS float layout、输出
`VFHSignature308`，dense full-cloud sequential indices（全点云顺序索引），
`normalize_distances=false`、`size_component=false`，且没有给定 centroid / normal。
非覆盖路径继续 fallback（回退）到原标量主体。

## 当前状态清单

| item | current state |
| --- | --- |
| Phase 040 production patch | 已接入 bounded RVV helper，production public repeated board 为正向。 |
| 正确性 | `make -C test-rvv/features/vfh run_test_compare` 在语义修复后通过，Std/RVV 各 7 个测试。 |
| production board | `log/board/repeated-production-phase040` 5-run checksum 一致，production case 约 `1.33x-1.38x`。 |
| Evidence Doctor | Phase 040 当前报告 `0E/1W/11S`，唯一 Warning 在历史 diagnostic case，不阻塞 production case。 |
| 当前实现缺口 | 生产 helper 为 `f1/f2/f3/valid/alpha` 分配 O(N) staging，再二次扫描；这会增加内存流量和 heap allocation（堆分配）成本。 |

## 候选族与假设

| candidate family | hypothesis | risk / unknown | evidence needed |
| --- | --- | --- | --- |
| `vfh-production-chunk-local-staging` | O(VLmax) chunk buffer 可减少大规模 staging 内存流量，不改变直方图累加顺序。 | 标量分箱移入 chunk loop 后可能影响编译器调度；收益可能被 `atan2_RVV` 或 histogram scatter 主成本稀释。 | production direct correctness、asm attribution、5-run board repeated、Evidence Doctor。 |

## 实现和测试动作

| action | 产物 | 验收 |
| --- | --- | --- |
| IMPL | 修改 `features/include/pcl/features/impl/vfh.hpp` 的两个 production RVV accumulate helper。 | 不改变 public API，不改变 fallback gate；只替换 staging 组织方式。 |
| CORRECTNESS | 运行 `make -C test-rvv/features/vfh run_test_compare`。 | Std/RVV correctness 全通过，production direct checksum 仍一致。 |
| ASM | 运行 `make -B -C test-rvv/features/vfh dump_bench_rvv` 并抽样确认 production helper RVV 指令仍存在。 | `computeVFHNormalCentroidRVV` / `accumulateVFHSPFHRVV` / `accumulateVFHViewpointRVV` 仍可归属 RVV 指令。 |
| BOARD | 运行 `board_repeated` 到新目录 `log/board/repeated-production-phase050`。 | 5-run production case checksum 一致，decision bucket 与 Phase 040 比较后决定 adopted / attempted。 |
| DOCTOR | 运行 Evidence Doctor。 | `Errors=0`；Warning / Suggestion 解释清楚，不阻塞时写入 result。 |
| DOC | 更新 phase result、README、optimization matrix、roadmap、evaluation、正式 `doc-rvv/features/vfh-RVV.zh.md` 和筛选清单。 | 文档使用 Phase 050 或明确选择的生产接入后板卡数据，不沿用早期 diagnostic 数据。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-public`。本阶段直接比较真实 `VFHEstimation::compute()` 的 Std/RVV build。 |
| A/B boundary | public overload + production dispatch，bench label 为 `production_vfh_compute_default`。 |
| 当前决策问题 | `implementation-shape` + `RVV-vs-scalar`：判断 chunk-local staging 是否值得替代 Phase 040 staging，并确认替代后 public RVV 仍快于 public scalar。 |
| diagnostic 是否可外推到 production | 不需要外推；最终用 production direct board 数据。 |
| comparison-boundary / baseline mismatch 风险 | 与 Phase 040 同一 bench label、同一输入规模和同一 production boundary 对比，仍需说明不同 run 目录之间存在环境波动。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已经是 bounded production probe；若收益不如 Phase 040 或不稳定，可保留 Phase 040 形态或暂停等待用户判断。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 本阶段是当前 production helper 的实现形态 A/B；若 Phase 050 production public 不低于 Phase 040 且 correctness / Doctor 闭合，可作为正式 production 形态。 |

## 板卡复跑预算和决策桶

板卡可用，本阶段预算为 5 次 repeated run，参数沿用 Phase 040：
`BENCH_ARGS='--side 80 --iterations 8 --warmup 2'`。

decision bucket（决策桶）：

- `positive`：production checksum 全一致，平均 speedup 大于 `1.2x`，且不低于 Phase 040 平均值的噪声区间。
- `weak-positive`：production checksum 全一致，平均 speedup 大于 `1.05x` 但低于 Phase 040 或波动明显。
- `neutral/negative`：production 平均 speedup 小于等于 `1.05x` 或低于 Phase 040 明显。
- `unstable`：checksum 不一致、run 间方向摇摆，或 Evidence Doctor 出现未修复 Error。

## 继续 / 停止条件

若 Phase 050 为 `positive` 或 `weak-positive` 且仍优于标量，继续刷新 production closeout 文档。
若 Phase 050 退化但 Phase 040 证据仍有效，恢复 Phase 040 staging 形态并记录 Phase 050 为
`attempted / rejected with evidence`。若证据矛盾或 Board / Doctor 出现阻塞，暂停并输出 Handoff。
