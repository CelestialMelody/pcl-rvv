# Phase 040 result: projected covariance production probe

## 执行范围

本阶段已按 `plan.zh.md` 完成 projected covariance fusion（投影协方差融合）的 production probe（生产探针），并按用户偏好在接入后用板卡结果决定是否采纳。

当前 production patch（生产补丁）只在 `__RVV10__` 下替换 public `MomentOfInertiaEstimation<PointT>::compute()` 的 angle scan（角度扫描）内 projected covariance 片段：RVV helper 成功时直接计算 3x3 projected covariance，失败时保留原 `getProjectedCloud()` + projected `computeCovarianceMatrix()` 标量路径。phase030 的 mean/AABB-only patch 已回滚，没有进入当前生产行为。

## 实现结果

| area | result |
| --- | --- |
| production entry | `compute()` 仍是唯一公开入口；angle scan 内先尝试 `computeProjectedCovarianceRVV()` |
| replaced scalar segment | `getProjectedCloud(current_axis, mean_value_, projected_cloud)` + projected `computeCovarianceMatrix(projected_cloud, covariance_matrix)` |
| retained scalar segment | `computeMeanValue()`、主 covariance、Eigen 求解、`calculateMomentOfInertia()`、`computeEccentricity()` 和 `computeOBB()` |
| point type gate | `pcl::rvv::RVVXYZAoSFloatLayout<PointT>`，即 PointXYZ-like xyz 单 float AoS layout（类似 PointXYZ 的 xyz 单 float 结构数组布局） |
| indexed load | 公共 `pcl::rvv_load::indexed_load3_f32m2`，indices 通过 32-bit byte offset gather（离散加载）读取 xyz |
| fallback | 非 RVV 构建、空 input / indices、layout gate 失败或 byte offset 上界失败时走原标量 projected cloud 路径 |
| not included | mean/AABB RVV dispatch、完整 `compute()` 替换、其它点类型 / `Scalar` / layout 扩展 |

## Correctness / smoke / asm

| gate | command | result |
| --- | --- | --- |
| RED | `make run_test_rvv` after test edit | failed as expected: `computeProjectedCovarianceRVV` missing |
| GREEN | `make run_test_rvv` after production patch | passed：RVV 4/4 |
| Std/RVV compare | `make run_test_compare` | Std 3/3 passed；RVV 4/4 passed |
| QEMU bench smoke | `make run_bench_rvv BENCH_ARGS='--case-filter moi_public_compute --points 256 --iterations 1 --warmup-iterations 1'` | produced parseable `moi_public_compute` output；QEMU timing 不作为性能证据 |
| asm | `make dump_bench_rvv` + grep | public bench binary contains `vluxei32`、`vfmacc`、`vfnmsac`、`vfredusum` in the RVV path |
| registry | `make evidence_status_phase040` | fresh |

## Board production-public evidence

path: `log/board/repeated_phase040_projected_covariance_production/summary.md`

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `moi_public_compute,points=65536` | 5 | 1.984x | 1.877x | 2.076x | 0/5 | `positive` |

Evidence Doctor（证据体检）路径：`log/board/repeated_phase040_projected_covariance_production/evidence_doctor.md`。

结果：Errors=0，Warnings=0，Suggestions=1。Suggestion 是 `binary_identity_missing`，不阻塞本阶段采纳；若后续性能方向反转，应补 binary hash 或等价 build identity 后重跑。

## EvidenceDecision

`current_decision`：`adopted_production_behavior`。

根据用户偏好，接入 production 后的板卡结果显示明确收益即可采纳。本阶段 public compute 5-run median 为 1.984x，min 为 1.877x，0/5 退化，Evidence Doctor 无 Error / Warning，因此采纳当前 projected covariance production patch，并创建正式 `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md`。性能数据只采用 phase040 接入后的 production-public 板卡结果；phase010 helper-only positive 仅作为进入本阶段 probe 的依据。

phase030 mean/AABB-only patch 的 production-public 证据为 `neutral` 且 Doctor 有 Error，已按用户确认回滚，保留为 historical rejected probe（历史拒绝探针）。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | phase010 是 diagnostic；phase040 是 production-public |
| A/B boundary | phase010 helper-only；phase040 public `MomentOfInertiaEstimation::compute()` |
| 当前决策问题 | projected covariance RVV dispatch 是否值得保留 |
| diagnostic 是否可外推到 production | 不直接外推；采纳只依赖 phase040 接入后的 public board evidence |
| comparison-boundary / baseline mismatch 风险 | 已通过 phase040 public boundary 复核；仍不外推到其它点类型或真实 workload 全集 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段 production-public 为 positive；无需降级 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有 adopted RVV family 被替换；phase030 rejected probe 不参与 family selection，故 public Std/RVV positive 支持当前有界采纳 |

## doc_suite_role_inventory

| role | status |
| --- | --- |
| topic_navigation | `standalone:README.zh.md` |
| testing_overview | `merged:doc/moment_of_inertia_estimation-evaluation.zh.md#文档归属矩阵`；当前 topic 仍可后续拆出独立 role 文档 |
| correctness_tests | `merged:doc/moment_of_inertia_estimation-evaluation.zh.md#正确性与高效性证据链` |
| benchmark_and_evidence | `merged:doc/moment_of_inertia_estimation-evaluation.zh.md#当前证据链` |
| optimization_evidence | `standalone:doc/phases/optimization-matrix.zh.md` |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` |
| test_support_code_map | `merged:doc/moment_of_inertia_estimation-evaluation.zh.md#Traceability Map` |
| phase_index | `standalone:doc/phases/README.zh.md` |
| evaluation_production | `standalone:doc/moment_of_inertia_estimation-evaluation.zh.md` |
| production_topic_doc | `standalone:doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md` |

拆分完整 topic-local doc suite 仍可改善 reviewer 定位，但当前生产采纳的关键事实、测试、bench、证据、Traceability Map 和 roadmap 已有稳定主归属；后续若进入泛型点类型扩展，建议先补 `testing-overview` / `benchmark-and-evidence` / `test-support-code-map` 独立文档。

## 阶段反思和下一候选

当前有界生产补丁已经采纳。仍可继续的优化方向是 point type expansion（点类型扩展）：把 `RVVXYZAoSFloatLayout<PointT>` 的泛型门控从 `PointXYZ` public bench 扩展到 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 normal 复合点型的 correctness / fallback / board 证据。该方向属于 scope expansion（范围扩展），需要新 phase 独立验证，不能从本阶段 `PointXYZ` 证据外推。

不建议继续的方向：

- mean/AABB-only：phase030 已证明 public boundary 中性且有 Doctor Error。
- full compute replacement：会把 Eigen 求解、moment、OBB 和 projection 全绑在一起，缺少单一归因。

## 文档和 closeout 边界

已创建正式 production 长期文档：`doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md`。本阶段完成后默认恢复动作是 `050-point-type-expansion-plan`，除非用户选择先提交 / review 当前有界 production patch。
