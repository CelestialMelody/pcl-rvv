# Phase 050 计划：dual-indices / correspondences row-source audit

## 阶段意图和边界

本阶段只审计 `TransformationEstimationSVD` 剩余两个 row source policy（行来源策略）是否值得迁移当前已经采用的数学族：

- `dual-indices-cloud-pair`
- `correspondence-pair`

本阶段不修改 production（生产源码），不回滚 Phase 020 / 040，不把 `Scalar=double` 拉回当前 RVV 范围，也不把 `PointXYZI` / `PointXYZRGB` 的代表性 correctness 误写成逐类型性能结论。

## 当前状态清单

| area | 当前状态 | 路径 |
| --- | --- | --- |
| ordered-cloud-pair production | adopted / production-ready | `doc/transformation_estimation_svd-evaluation.zh.md`、`doc-rvv/registration/transformation_estimation_svd-RVV.zh.md` |
| source-indexed-cloud-pair production | adopted / production-ready | 同上 |
| dual-indices-cloud-pair | 未实现 | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` 的对应 overload 仍走 iterator 标量路径 |
| correspondence-pair | 未实现 | 同上 |
| existing test support | ordered/source-indexed 已有 helper | `include/impl/tesvd_candidates.hpp` |
| existing board evidence | ordered/source-indexed 已有 repeated board | `log/board/production_ordered_cloud_pair_repeated/summary.md`、`log/board/production_source_indexed_cloud_pair_repeated/summary.md` |

## 假设与候选族

1. dual-indices 可能直接复用 ordered/source-indexed 的前端累加结构，只是 source / target 两侧都改成 indexed gather。
2. correspondences 可能需要从 `pcl::Correspondence` 里直接读取 query / match index，再做双 gather。
3. 如果双 gather 的收益明显不足，correspondence 可能只保留为 diagnostic 或 fallback candidate。

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | scope and entry | correctness target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `dual_indices_fused_accum` | dual-indices-cloud-pair | `PointXYZ` / `float` / dense xyz AoS | test-only dual-index candidate | public iterator vs fused reference；candidate vs scalar | `dual-indices-cloud-pair` QEMU smoke + board repeated | pending | pending | pending | planned | 先补 dual-index scaffold |
| `correspondence_fused_accum` | correspondence-pair | `PointXYZ` / `float` / dense xyz AoS | test-only correspondence candidate | public iterator vs fused reference；candidate vs scalar | `correspondence-pair` QEMU smoke + board repeated | pending | pending | pending | planned | 先补 correspondence scaffold |
| `Scalar=double` | all row sources | double | not in scope | fallback already covered | not planned | not applicable | not applicable | not applicable | rejected / fallback | none |

## 实现和测试动作

1. 在 `include/impl/tesvd_candidates.hpp` 补 dual-indices / correspondences 的标量 reference、RVV candidate、fixtures 和 stats。
2. 在 `src/test_tesvd.cpp` 补 dual-indices / correspondences correctness、fallback 和 representative layout tests。
3. 在 `src/bench_tesvd.cpp` 补 dual-indices / correspondences bench case-filter 和 checksum 输出。
4. 生成新的 QEMU smoke manifest / Evidence Doctor，并补证据登记。
5. 若 candidate 正向，再决定是否进入 production integration loop；若 board 证据不足，则保留为 diagnostic 或 deferred。

## Evidence Doctor 和 registry 规则

- QEMU smoke 只证明构建和日志形状，不写性能结论。
- board repeated 需要 5-run、20-iteration、5 warm-up 的 summary、manifest 和 Evidence Doctor。
- 若 registry 出现 `unregistered_change`、`unregistered_file` 或 `stale_doc_pending_refresh`，先刷新文档引用再解释数值。

## 阶段完成条件

- dual-indices / correspondences 的正确性、fallback 和 bench smoke 已有可审查证据。
- Evidence Doctor 没有未解释 Error。
- optimization matrix 能清楚标出哪个 row source 继续、哪个仍然 deferred。

## 继续 / 停止条件

默认继续到 test / bench scaffold 完成，再判断是否需要 board repeated。只有当 board 不可用、证据矛盾、dirty isolation 风险或用户限定范围时才停止。

## 文档更新清单

- `doc/phases/050-dual-indices-correspondences-audit/result.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- 适用时的 `doc-rvv/registration/transformation_estimation_svd-RVV.zh.md`

## roadmap 同步动作

本阶段会把 `dual_indices_fused_accum` 和 `correspondence_fused_accum` 从 `planned` 推进到 `diagnostic`、`attempted`、`rejected` 或 `deferred`，并记录恢复条件。若板卡证据闭合，再决定是否值得新建生产接入阶段。
