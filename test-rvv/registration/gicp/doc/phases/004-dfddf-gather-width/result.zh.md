# Phase 004 Result: dfddf Gather Width

## 当前状态

本阶段尝试把 `dfddfLoopRVV()` 中读取 `mahalanobis_` 的 9 个 `Eigen::Matrix3d`
元素从 64-bit indexed gather（64 位索引离散加载，`vluxei64`）改成 32-bit indexed
gather（32 位索引离散加载，`vluxei32`）。这个尝试没有改变 production dispatch
（生产分流）边界：仍只覆盖 public GICP `PointXYZ -> PointXYZ`、`Scalar=float`、
xyz AoS float layout，并且只作用于 `OptimizationFunctorWithIndices::dfddf()` 的逐
correspondence 累加主循环。

1024 点板卡 repeated（重复板卡性能测试）没有显示新增收益：Phase 004 median 为
`1.073x`，低于 Phase 003 clean adoption 的 `1.080x`。因此本阶段的小改动已回退，
当前源码回到 Phase 003 的 `vluxei64` matrix gather 形态。

## 已完成动作

| action | evidence | result |
| --- | --- | --- |
| matrix gather 改成 `vluxei32` | `registration/include/pcl/registration/impl/gicp.hpp` 临时改动 | QEMU correctness 通过，但板卡没有改善 |
| QEMU correctness | `make -C test-rvv/registration/gicp run_test_compare` | Std/RVV 均 10 tests pass |
| board production public 1024 点 | `log/board/production_public_align_pointxyz_dfddf_gather32_repeated/summary.md` | median `1.073x`，min `1.061x`，max `1.096x`，bucket `weak_positive` |
| Evidence Doctor | `log/board/production_public_align_pointxyz_dfddf_gather32_repeated/evidence_doctor.md` | Errors=0，Warnings=1 |
| 回退 Phase 004 小改动 | `registration/include/pcl/registration/impl/gicp.hpp` | `mahalanobis_` gather 恢复为 `vuint64m4_t` offset + `vluxei64` |

## Evidence Doctor

| evidence | Errors | Warnings | Suggestions | 处理 |
| --- | ---: | ---: | ---: | --- |
| `production_public_align_pointxyz_dfddf_gather32_repeated/evidence_doctor.md` | 0 | 1 | 0 | reduction order mismatch；不影响“无新增收益”的判断 |

## EvidenceDecision

Phase 004 结论是 `attempted / no_improvement / do_not_adopt`。

与 Phase 003 clean adoption 相比，32-bit gather 版本仍只是 `weak_positive`，但 1024 点
median 从 `1.080x` 变为 `1.073x`，没有达到“同边界更优”的采纳条件。由于本阶段目标只是
低维护成本的小幅实现形态优化，且 1024 点已经没有改善，本阶段不继续跑 4096 点扩展。

当前保留的生产形态仍是 Phase 003：`OptimizationFunctorWithIndices::dfddf()` 命中
`dfddfLoopRVV()`，matrix gather 使用 64-bit indexed gather。当前公开入口收益仍属于
弱正向：1024 点 median `1.080x`，4096 点 median `1.058x`。

## 未覆盖范围

- 未验证 32-bit gather 在 4096 点或其它点型上的收益；由于 1024 点已无改善，本阶段按预算停止。
- 未扩大到 covariance post-KNN、Mahalanobis 3x3 update、其它点型、其它 `Scalar` 或其它 row source。

## 下一步

当前 GICP topic 在同一 production boundary（生产边界）内没有更高置信、低维护成本且已准备好的
RVV 优化点。若项目对接入门槛要求明显高于 `1.05x` 左右的弱正向，则建议不要接入或回退整个
`dfddfLoopRVV()` 生产补丁；若接受小幅稳定收益，可以保留 Phase 003 形态，但应明确它只覆盖
`PointXYZ -> PointXYZ` 的当前公开入口。
