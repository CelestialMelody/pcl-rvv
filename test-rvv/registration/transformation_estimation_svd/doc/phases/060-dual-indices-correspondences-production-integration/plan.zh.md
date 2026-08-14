# Phase 060 计划：dual-indices / correspondences production integration

## 阶段意图和边界

本阶段把 `dual-indices-cloud-pair` 和 `correspondence-pair` 接入真实 production（生产源码）路径。只覆盖 `use_umeyama_ == true`、`Scalar=float`、dense、`RVVXYZAoSFloatLayout` 门控和 32-bit gather 边界。

本阶段不把 `Scalar=double` 拉回当前范围，不回滚已接入的 ordered-cloud-pair / source-indexed-cloud-pair，也不把代表性 correctness 误写成逐点型板卡性能。

## 当前状态清单

| area | 当前状态 | 路径 |
| --- | --- | --- |
| dual-indices candidate | diagnostic positive | `log/board/dual_indices_cloud_pair_repeated/summary.md` |
| correspondence candidate | diagnostic positive | `log/board/correspondence_pair_repeated/summary.md` |
| production helper patch | working tree 已加入 | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| test support | 已有 candidate / direct helper | `test-rvv/registration/transformation_estimation_svd/include/impl/tesvd_candidates.hpp` |
| board production targets | 已预埋 | `test-rvv/registration/transformation_estimation_svd/Makefile` |

## 假设与候选族

1. dual-indices 可以复用当前 source-indexed / ordered 的 RVV 累加形状，只是 source 和 target 两侧都改成 indexed gather。
2. correspondences 可以复用同一累加与求解 tail，只是从 `pcl::Correspondence` 中读取 query / match 索引。
3. 只要 public dispatch 能在 `use_umeyama_ == true`、dense、layout 和 32-bit gather 门控下短路到 RVV helper，就可以保留标量 iterator 作为 fallback。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `production_direct_dispatch_dual_indices` | dual-indices-cloud-pair | `PointXYZ` / `float` / dense xyz AoS | production public dual-indices overload | public iterator vs fused reference；production helper path-hit | `dual-indices-cloud-pair` production direct repeated board | pending | pending | pending | planned | 跑 `run_test_compare` 与 production direct board |
| `production_direct_dispatch_correspondence` | correspondence-pair | `PointXYZ` / `float` / dense xyz AoS | production public correspondence overload | public iterator vs fused reference；production helper path-hit | `correspondence-pair` production direct repeated board | pending | pending | pending | planned | 跑 `run_test_compare` 与 production direct board |
| `Scalar=double` | all row sources | double | 不在本阶段 | fallback already covered | not planned | not applicable | not applicable | not applicable | rejected / fallback | none |

## 实现和测试动作

1. 验证 production helper 和 public dispatch 在 `__RVV10__` 下都能编译并命中 dual-indices / correspondences RVV fast path。
2. 在 `src/test_tesvd.cpp` 补 production direct path-hit 和 fallback gate tests。
3. 运行 `run_test_compare` 复核 correctness。
4. 运行 `run_board_bench_production_dual_indices_cloud_pair_repeated` 和 `run_board_bench_production_correspondence_pair_repeated`。
5. 刷新 `README.zh.md`、evaluation、benchmark / evidence、optimization evidence、matrix、roadmap、test-support code map 和 phase result。

## Evidence Doctor 和 registry 规则

- 生产直连 board 必须使用 `evidence_role=production_direct`。
- 仍按 5-run、20-iteration、5 warm-up 的 repeated board 合同执行。
- 若 registry 报告 `doc_ref_missing`、`unregistered_change` 或 `stale_doc_pending_refresh`，先更新引用文档，再解释数值。

## 阶段完成条件

- dual-indices 与 correspondence 的生产 helper path-hit 和 fallback gate 已有 correctness 证据。
- production direct board summary、manifest 和 Evidence Doctor 都已刷新。
- optimization matrix 能清楚区分 adopted、deferred 和 rejected 路线。

## 继续 / 停止条件

默认继续到 correctness 与 board direct。只有板卡不可用、证据矛盾或用户限定范围时才停止。

## 文档更新清单

- `doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- `doc/transformation_estimation_svd-evaluation.zh.md`

## roadmap 同步动作

本阶段会把 `production_direct_dispatch_dual_indices` 和 `production_direct_dispatch_correspondence` 从 `planned` 推进到 `adopted`、`attempted` 或 `deferred`，并把 `dual_indices_fused_accum` / `correspondence_fused_accum` 的板卡正向结论升级为 production direct evidence。
