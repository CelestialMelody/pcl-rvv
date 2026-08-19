# Phase 003 Plan: Clean Adoption

## 阶段意图和边界

本阶段把 Phase 002 的生产探针收敛成正式接入候选：保留有收益的
`OptimizationFunctorWithIndices::dfddf()` 主循环 RVV 路径，移除 Phase 001 中收益不足的
cost-only `operatorCostRVV()` 路线，然后重新采集 correctness、asm 和 board production-public
证据。

validated_scope：

- entry：public `GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ>::align()`。
- optimized function：`OptimizationFunctorWithIndices::dfddf()`。
- row source：`tmp_idx_src_` / `tmp_idx_tgt_` indexed source / target pair。
- point type / Scalar / layout：`PointXYZ -> PointXYZ`、`Scalar=float`、xyz AoS float layout。

unvalidated_scope：

- 非 xyz AoS layout、非 `Scalar=float`、其它 point type 或混合 source/target 组合。
- correspondence-pair 入口、`Scalar=double`、`PointNormal` / `PointXYZINormal` 泛型扩展。
- covariance post-KNN 和 Mahalanobis 小矩阵更新。

## 当前状态

| item | current evidence |
| --- | --- |
| cost-only `operatorCostRVV()` | 1024 点 median `1.019x`，4096 点 median `1.011x`，均 `neutral`，不建议单独采纳 |
| `dfddfLoopRVV()` + cost-only combined path | 1024 点 median `1.068x`，4096 点 median `1.070x`，均 `weak_positive` |
| correctness | QEMU Std/RVV 10 tests pass；board RVV smoke 10 tests pass |
| Evidence Doctor | combined path Errors=0；Warnings 为 reduction order 和 4096 low-run-count |

## 实现动作

| action | artifact | completion |
| --- | --- | --- |
| 移除 cost-only dispatch | `gicp.h`、`gicp.hpp` | `operator()` 回到原标量 cost path；删除 `operatorCostRVV()` helper |
| 保留 `dfddfLoopRVV()` | `gicp.h`、`gicp.hpp` | `dfddf()` 仍可在 gate 命中时短路主累加循环 |
| 减少 RVV 命中前冗余准备 | `gicp.hpp` | 标量 fallback 才构造 fallback 所需 transform float |
| 更新 test/bench metadata | topic-local docs / summary | production adopted shape 不再写 cost-only helper |

## 测试和证据动作

| evidence | command | completion |
| --- | --- | --- |
| QEMU correctness | `make run_test_compare` | Std/RVV 10 tests pass |
| QEMU smoke / asm | `make record_qemu_smoke_evidence_state` | registry fresh；asm 可定位 `dfddfLoopRVV()` |
| board correctness | `make run_board_test_smoke` | RVV 10 tests pass |
| board production public 1024 | `make run_board_bench_gicp_production_public_repeated GICP_PRODUCTION_BOARD_RUN_LABEL=production_public_align_pointxyz_dfddf_clean_repeated ...` | 5-run summary + doctor |
| board production public 4096 | 若 1024 仍 weak-positive 或 positive，追加 3-run expansion | summary + doctor |

## 决策桶

- `positive`：median >= `1.20x` 且无 B/A < 1。
- `weak_positive`：median >= `1.05x` 且 B/A < 1 不超过 1/5。
- `neutral`：`0.95x <= median < 1.05x`。
- `negative`：median < `0.95x`。
- `unstable`：方向跨桶且复跑预算耗尽。

若 clean path 仍为 `weak_positive` 或更好，则进入 adopted production closeout，创建
`doc-rvv/registration/gicp-RVV.zh.md` 并更新 topic-local 证据。若降为 `neutral` 或 `negative`，
停止并报告不建议正式接入。

## 文档更新

若结果仍好，更新：

- `doc/phases/003-clean-adoption/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/gicp-evaluation.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc-rvv/registration/gicp-RVV.zh.md`

不触碰其它 registration topic 的已有 dirty 文档。
