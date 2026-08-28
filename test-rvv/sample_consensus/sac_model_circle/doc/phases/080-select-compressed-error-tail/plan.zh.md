# Phase 080 Plan: select compressed error tail

## 阶段意图和边界

本阶段评估 `selectWithinDistance` 的命中点误差写回是否值得继续 RVV 化。当前已采纳 production RVV（生产 RVV）路径已经在向量循环中用 `vcompress` 保序写出 inlier index，但随后把命中点平方距离落回标量 `std::sqrt`，逐个写 `error_sqr_dists_`。本阶段新增 test-only candidate（仅测试使用候选），在压缩后的 active lanes（有效向量通道）上继续执行 `vfsqrt + abs + vfwcvt + vse64`，并和当前 adopted select RVV 做 RVV-vs-RVV detail A/B（两个 RVV 实现族在同一生产边界内直接比较）。

本阶段不改 production dispatch（生产分流），不改变 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` 中已采纳的 `selectWithinDistanceRVV`。若候选没有稳定跑赢当前生产 RVV，只把它作为 rejected / attempted 实现族记录；若候选正向，只进入后续 production probe 计划，不在本阶段直接采纳。

validated_scope：`SampleConsensusModelCircle2D<PointT>::selectWithinDistance`，direct indexed `indices_`，`PointXYZ` board performance，float x/y AoS（结构数组）字段布局，signed 32-bit `pcl::index_t`，u32 byte offset gate。

unvalidated_scope：更多 PointXYZ-like 点型 dedicated board、`Scalar=double`、自定义 x/y layout、其它 SAC 模型、其它 row source 和 production 替换。

## 当前状态清单

| 对象 | 当前证据 |
| --- | --- |
| baseline | 当前 production `selectWithinDistanceRVV` 为 adopted，Phase 000 board median `1.6702x`，Evidence Doctor（证据体检）0/0/0。 |
| candidate idea | 只替换 `vcompress` 后的误差写回：从标量 `std::sqrt` 改为 compressed active lanes 上的 RVV `vfsqrt + vfwcvt + vse64`。 |
| correctness（正确性） | 需要新增 gtest，证明 public select、direct RVV baseline 和 test-only candidate 的 inliers 顺序与误差数组一致。 |
| asm attribution（反汇编归属） | 需要确认候选 helper 符号中有 `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`。 |
| board performance（板卡性能） | 需要 5-run repeated RVV-vs-RVV A/B，baseline 为 public adopted select，candidate 为 test-only full-RVV error tail。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| select compressed error full-RVV tail | direct indexed `indices_` | `PointXYZ` / float x-y AoS / dense active double error output | `run_circle_select_error_tail_candidate_test` | `bench_sac_model_circle` public select vs diagnostic select full-RVV error tail | 5-run repeated board planned | candidate helper must contain `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v` | required before decision | planned |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 新增候选 helper | `test-rvv/sample_consensus/sac_model_circle/include/impl/sac_model_circle_candidates.hpp` | test-only helper 不触碰 production，RVV 构建下可直接调用，非覆盖条件自然走 public fallback。 |
| 新增 correctness gate | `test-rvv/sample_consensus/sac_model_circle/src/test_sac_model_circle.cpp` | public select、direct RVV baseline 和 candidate 的 inliers 与误差值按 `1e-6` tolerance 对齐。 |
| 新增 bench row | `test-rvv/sample_consensus/sac_model_circle/src/bench_sac_model_circle.cpp` | 输出 `diagnostic select full-rvv error tail` 行和 checksum。 |
| 新增 Makefile / manifest | `Makefile`、`generate_circle_board_evidence_manifest.py` | 有 candidate test、asm gate、collect / doctor / registry / status target。 |
| 板卡验证 | `collect_select_error_tail_repeated_board_evidence` 和 `record_select_error_tail_board_evidence_state` | 5-run repeated board，Evidence Doctor Errors/Warnings/Suggestions 解释完毕。 |

## Evidence Doctor 和 registry 规则

本阶段 evidence role 是 `production-detail`，因为 baseline 和 candidate 都在 RVV build 内比较同一 `selectWithinDistance` 入口语义，但 candidate 是 test-only helper。若 board B/A 稳定大于 1 且 doctor 0/0/0，可进入后续 production probe；若 B/A 小于 1 或 doctor 报退化 Error，则拒绝该实现族，不影响当前 adopted select RVV。

`positive`：5-run median B/A >= 1.03，且 `B/A < 1` 为 0/5。`weak-positive`：median > 1 但低于 1.03 或有轻微波动。`neutral`：median 约等于 1。`negative`：median < 1 或 5-run 多数低于 1。复跑预算为 5 run；若决策桶稳定不追加复跑，若摇摆则标为 `unstable` 并暂停。

## 继续 / 停止条件

若候选正向，下一阶段默认进入 bounded production probe（有界生产探针）计划，仍需真实 production patch、production direct correctness、asm、board 和 Evidence Doctor 后再决定是否采纳。

若候选负向或中性，则 Phase 080 关闭当前可见的同 topic 高优先级优化方向；剩余更多点型、`Scalar=double`、自定义 layout 和 `circle3d` 均属于新 scope。
