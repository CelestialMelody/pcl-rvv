# Phase 004 Plan: dfddf Gather Width

## 阶段意图和边界

本阶段只尝试一个低风险实现形态优化：`dfddfLoopRVV()` 已经 gate 了 source / target cloud size
可由 32-bit byte offset 表达，因此 `mahalanobis_` 的 `Eigen::Matrix3d` gather 也可以用
32-bit byte offset。目标是去掉当前 `u32 -> u64` 扩展和 `vluxei64` matrix gather，改为
`vluxei32`，观察 public GICP clean production entry 是否有可见收益。

validated_scope：

- entry：public `GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ>::align()`。
- optimized function：`OptimizationFunctorWithIndices::dfddf()`。
- helper：`dfddfLoopRVV()`。
- point type / Scalar / layout：`PointXYZ -> PointXYZ`、`Scalar=float`、xyz AoS float layout。

unvalidated_scope：

- 其它点型、`Scalar=double`、其它 row source policy。
- covariance post-KNN、Mahalanobis 3x3 update 和 cost-only path。

## 当前状态

Phase 003 clean adoption 已完成，1024 点 median `1.080x`，4096 点 median `1.058x`，均为
`weak_positive`。收益偏低，因此本阶段只接受低维护成本的小改动；若无改善，则建议回退当前生产补丁或结束 topic。

## 实现动作

| action | artifact | completion |
| --- | --- | --- |
| matrix gather offset 改为 32-bit | `registration/include/pcl/registration/impl/gicp.hpp` | `mahalanobis_` gather 使用 `vuint32m2_t` byte offset 和 `vluxei32` |
| 保持 gate 不扩大 | `gicp.hpp` | 继续使用现有 cloud-size 32-bit offset gate |

## 测试和证据动作

| evidence | command | completion |
| --- | --- | --- |
| QEMU correctness | `make run_test_compare` | Std/RVV 10 tests pass |
| QEMU smoke / asm | `make record_qemu_smoke_evidence_state` | asm 中可看到 32-bit indexed gather |
| board production public 1024 | `run_board_bench_gicp_production_public_repeated` 新 label | 与 Phase 003 clean 结果同桶或更好 |
| board production public 4096 | 若 1024 不退化，追加 3-run expansion | 与 Phase 003 clean 结果同桶或更好 |

## 决策桶

- 若 1024 和 4096 都保持 `weak_positive` 且 median 不低于 Phase 003 clean 明显范围，保留本形态。
- 若 1024 或 4096 降到 `neutral`，回退本阶段小改动。
- 若仍只是 `weak_positive` 且收益没有改善，向用户说明当前 topic 没有继续接 production 的充分性，可按倾向回退整个 production patch。
