# Phase 030 计划：row-source family carry-over audit

## 阶段意图和边界

本阶段只审计 `TransformationEstimationSVD` 里剩余的 row source policy（行来源策略）是否能够迁移当前 ordered-cloud-pair 已采用的 math family（数学族）。审计对象是：

- `source-indexed-cloud-pair`
- `dual-indices-cloud-pair`
- `correspondences-pair`

本阶段不修改当前 ordered-cloud-pair production path，不回滚 PI1-PI5，不把 `Scalar=double` 拉回当前 RVV 范围，也不把 `PointXYZI` / `PointXYZRGB` 的代表性 correctness 误写成逐类型性能结论。

## 当前状态清单

| area | current state | 证据 |
| --- | --- | --- |
| ordered-cloud-pair production | adopted / production-ready | `doc/optimization-evidence.zh.md`、`doc/transformation_estimation_svd-evaluation.zh.md` |
| generic xyz AoS correctness | adopted for representative layouts | `src/test_tesvd.cpp` |
| source-indexed / dual-indices / correspondences | deferred | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` 的其余 overload 仍走 iterator 标量路径 |
| board access | available | Phase 020 board repeated 已完成 |

## 假设与候选族

1. source-indexed 可能复用当前 ordered-cloud-pair 的前端累加结构，只是把 source 侧改成 indexed gather。
2. dual-indices 需要双 gather，可能比 source-indexed 更容易被 cache locality 反噬。
3. correspondences 需要确认 query/match 的 local offset 是否值得 RVV 化；如果输出顺序和边界成本太高，可能只适合 diagnostic 或 fallback。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | decision | next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `source_indexed_fused_accum` | source-indexed-cloud-pair | `PointXYZ` / `float` / xyz AoS | source-indexed overload | 与 iterator 标量语义对拍 | indexed gather bench | 需要 | 需要 | planned | 先写测试支撑和 row-source helper |
| `dual_indices_fused_accum` | dual-indices-cloud-pair | `PointXYZ` / `float` / xyz AoS | dual-indices overload | 双 index 对拍 | 双 gather bench | 需要 | 需要 | planned | 先审计索引合法性和对拍输入 |
| `correspondence_fused_accum` | correspondences-pair | `PointXYZ` / `float` / xyz AoS | correspondence overload | correspondence 对拍 | correspondence bench | 需要 | 需要 | planned | 先确认 query/match 语义和偏移边界 |

## 实现和测试动作

1. 为三个 overload 各补一组 row-source correctness gtest，先证明语义不漂移。
2. 复用 `rvv_point_load` 的 indexed gather primitive，先做最窄 source-indexed candidate。
3. 若 source-indexed positive，再分别评估 dual-indices 和 correspondences，不合并结论。
4. 为每个 candidate 补 board repeated / Evidence Doctor；如果 board 证据不足，只停留在 diagnostic。

## Evidence Doctor 和 registry 规则

- manifest 和 summary 继续走 topic-local `log/`。
- 任何新 board summary 都要经过 Evidence Doctor。
- 若证据只足够证明 correctness，不得写成 production-ready。
- `evidence_status` 必须检查新产物是否登记。

## 阶段完成条件

- 至少一个 row-source family 获得同边界 correctness + board 证据。
- 其余 family 必须明确为 `deferred`、`rejected with evidence` 或 `not_applicable with evidence`。
- optimization matrix 与 roadmap 要回填恢复条件。

## 继续 / 停止条件

默认下一动作是 `source-indexed-fused_accum` 的最小 scaffold。若在实现前发现索引语义、数据布局或 board 证据无法在当前 topic 内闭合，则降级为 `turn_stop_deferred`，并在 Handoff 写清 blocker。
