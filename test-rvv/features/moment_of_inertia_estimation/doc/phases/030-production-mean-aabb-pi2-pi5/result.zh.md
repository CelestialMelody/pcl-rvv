# Phase 030 result: production mean/AABB PI2-PI5

## 执行范围

本阶段曾完成一个有界 production probe（生产探针）：在 `MomentOfInertiaEstimation<PointT>::compute()` 的 `computeMeanValue()` 内部接入 mean/AABB RVV helper（质心与轴对齐包围盒 RVV helper），公开 API 不变，非 `__RVV10__` 构建和 gate（门控）失败时回到 `computeMeanValueStd()`。

该 patch 已按用户确认回滚，现仅作为 historical rejected probe（历史拒绝探针）保留在 phase 记录中。当前生产行为由 phase040 projected covariance production probe 取代。

## 实现结果

| area | result |
| --- | --- |
| production entry | `compute()` 仍调用 `computeMeanValue()`；`computeMeanValue()` 先尝试 `computeMeanValueRVV()`，失败后调用 `computeMeanValueStd()` |
| point type gate | `pcl::rvv::RVVXYZAoSFloatLayout<PointT>`，即 PointXYZ-like xyz 单 float AoS layout（类似 PointXYZ 的 xyz 单 float 结构数组布局） |
| indexed load | 公共 `pcl::rvv_load::indexed_load3_f32m2`，indices 先按 i32 load 再 reinterpret 为 u32 byte offset |
| byte offset gate | `input_->size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()` |
| fallback | 非 RVV 构建、空 indices、空 input、layout gate 失败或 byte offset 上界失败时走 Std |
| not included | phase 010 projected covariance fusion 未接入；covariance、moment、OBB 和 angle scan 其它 helper 未接入 |

## Correctness / smoke / asm

| gate | command | result |
| --- | --- | --- |
| RED | `make run_test_rvv` after test edit | failed as expected: `computeMeanValueRVV` missing |
| GREEN | `make run_test_rvv` after production patch | passed：4/4 |
| Std/RVV compare | `make run_test_compare` | Std 3/3 passed；RVV 4/4 passed |
| QEMU bench smoke | `make run_bench_rvv BENCH_ARGS='--case-filter moi_public_compute --points 256 --iterations 1 --warmup-iterations 1'` | produced parseable `moi_public_compute` output |
| asm | `make dump_bench_rvv` + grep | `bench_moi_rvv.full.asm` contains `vluxei32` and `vfred*` instructions in the public bench binary |
| registry | `make evidence_status_phase030` | fresh |

## Board production-public evidence

path: `log/board/repeated_phase030_public_compute_production/summary.md`

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `moi_public_compute,points=65536` | 5 | 1.018x | 0.987x | 1.025x | 2/5 | `neutral` |

Evidence Doctor（证据体检）路径：`log/board/repeated_phase030_public_compute_production/evidence_doctor.md`。

结果：Errors=1，Warnings=0，Suggestions=2。

- Error：`ba_degradation_frequency`，5 次中 2 次低于 1.0，不能只凭 median 略高于 1 写成稳定收益。
- Suggestion：`binary_identity_missing` 和 `near_threshold_ba`。

## EvidenceDecision

`current_decision`：`rolled_back_after_user_confirmation`。

production-public 证据不支持采纳 mean/AABB-only production patch。原因是完整 public `compute()` 中，mean/AABB 只占较小比例，接入后 median 只有 1.018x，且 2/5 次退化，Evidence Doctor 有 Error。该结果把 phase000 helper-only 2.082x 的正向信号降级为“helper boundary 可加速，但 public boundary 不值得单独接入”。

用户已确认回滚当前 mean/AABB-only production patch。若继续优化，后续已由 phase040 的 projected covariance production probe 接续。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | phase000 是 diagnostic；phase030 是 production-public |
| A/B boundary | helper-only fused reduction vs public `MomentOfInertiaEstimation::compute()` |
| 当前决策问题 | mean/AABB-only RVV dispatch 是否值得保留 |
| diagnostic 是否可外推到 production | no；public compute 中其它 scalar/Eigen/allocation 成本稀释了 helper 收益 |
| comparison-boundary / baseline mismatch 风险 | 已发生；helper median 2.082x，但 public median 1.018x 且退化频率 2/5 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已完成 probe；结果不支持采纳 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前无既有 RVV family，但 production-public 证据未达采纳门槛，因此不进入 clean adoption |

## 阶段反思和下一候选

新增路线：

- `projected covariance fusion production probe`：phase010 diagnostic median 1.247x，作用在 angle scan 每个 axis 的 projected cloud allocation + covariance 组合上，比 mean/AABB 更接近 public compute 的重复热点。恢复条件是用户先确认当前 production patch 如何处理；若回滚当前 patch，再以新的 phase plan 做 test-first production probe。

拒绝路线：

- `mean/AABB-only clean adoption`：production-public neutral 且 Evidence Doctor Error，不建议保留，且已回滚。

## 文档和 closeout 边界

本阶段曾不创建 `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md`。该判断在 phase040 采纳后已变为历史记录；当前正式 `doc-rvv` 由 phase040 的 adopted production behavior 维护。
