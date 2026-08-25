# Phase 040 plan: projected covariance production probe

## 阶段意图和边界

本阶段在真实 `MomentOfInertiaEstimation<PointT>::compute()` 入口中接入 phase 010 的 projected covariance fusion（投影协方差融合）候选，验证它是否比 phase030 的 mean/AABB-only（仅质心和轴对齐包围盒）更值得进入 production（生产源码）。

范围冻结：

- production entry（生产入口）：`MomentOfInertiaEstimation<PointT>::compute()` 的 angle scan（角度扫描）内部。
- 替换片段：`getProjectedCloud(current_axis, mean_value_, projected_cloud)` + projected `computeCovarianceMatrix(projected_cloud, covariance_matrix)`。
- 保留标量片段：`computeMeanValue()`、主 covariance、Eigen eigen-solver（特征求解）、`calculateMomentOfInertia()`、`computeEccentricity()` 和 `computeOBB()`。
- 点类型 / layout（布局）：`RVVXYZAoSFloatLayout<PointT>`，即 PointXYZ-like 单 `float x/y/z` AoS（结构数组）布局。
- row source（行来源）：当前 `indices_` indexed cloud（索引点云）。
- 不证明：其它点类型、其它 `Scalar`、无 indices 的独立入口、完整 `compute()` 全量 RVV 替换。

## 当前状态清单

| item | status | path |
| --- | --- | --- |
| phase 010 diagnostic | helper-only board median 1.247x，0/5 低于 1，Evidence Doctor 0 Error / 0 Warning | `log/board/repeated_phase010_projected_covariance_diagnostic/summary.md` |
| phase 030 production probe | mean/AABB-only public compute median 1.018x，2/5 低于 1，bucket `neutral`，已按用户确认回滚 | `doc/phases/030-production-mean-aabb-pi2-pi5/result.zh.md` |
| current production baseline | `compute()` 每个 angle materialize projected cloud，再求 projected covariance | `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` |
| current tests | diagnostic projected covariance 对拍已存在；本阶段新增 production private helper hit test | `src/test_moi.cpp` |

## 假设与候选族

候选族为 `projected covariance production fusion`。对中心化点 `r = p - mean_value_` 和单位法向量 `n`，投影后点相对 `mean_value_` 的向量为：

```text
r_projected = r - dot(r, n) * n
```

因此 projected covariance 可以直接在原始 indexed point cloud 上规约 6 个对称矩阵项，不必为每个 angle 分配、写入、再读取 projected cloud。预期收益来自减少 angle scan 内重复的临时点云内存流量；风险是完整 public `compute()` 仍包含 Eigen 求解和 moment-of-inertia 标量循环，可能稀释收益。

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | test | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| projected covariance production fusion | `indices_` indexed cloud | PointXYZ-like traits gate，`float`，AoS，32-bit byte offset gate | RED 缺 helper；GREEN private helper 对拍；`run_test_compare` | `moi_public_compute` production-public repeated board，angle step 45 | public bench binary 中归属到 `computeProjectedCovarianceRVV` 或内联 RVV 指令 | phase040 manifest + Evidence Doctor | PI5 根据接入后板卡收益决定采纳、回滚或继续 |

## 实现和测试动作

1. RED：在 `src/test_moi.cpp` 增加 `computeProjectedCovarianceRVV()` production helper 命中测试；实现前 `make run_test_rvv` 应编译失败。
2. GREEN：在 production 私有区添加 `computeProjectedCovarianceRVV()`，只在 `__RVV10__` 下编译；失败时 public `compute()` 走原标量 projected cloud 路径。
3. 正确性：运行 `make run_test_rvv` 和 `make run_test_compare`，Std 构建不暴露 RVV helper，RVV 构建多一个 production helper hit test。
4. smoke / asm：运行 `make run_bench_rvv BENCH_ARGS='--case-filter moi_public_compute --points 256 --iterations 1 --warmup-iterations 1'` 和 `make dump_bench_rvv`。
5. 板卡：运行 `make run_board_moi_phase040_projected_covariance_repeated`，默认 5 runs。
6. 证据：运行 `make evidence_status_phase040`，同步 phase result、matrix、roadmap、evaluation、README 和 Handoff。
7. adoption（采纳）：若 production-public 板卡结果显示有收益且 Evidence Doctor 无阻塞 Error，按用户偏好可建议采纳；采纳后创建正式 `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md`，数据使用接入后的 phase040 板卡结果。

## Evidence Doctor 和 registry 规则

phase040 production summary 使用 `log/board/repeated_phase040_projected_covariance_production/summary.md`、`evidence_manifest.json` 和 `evidence_doctor.md`。若 Evidence Doctor 有 Error，必须解释并暂停在 PI5；若只有 Suggestion，说明是否影响采纳。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | phase010 是 diagnostic；phase040 是 production-public |
| A/B boundary | helper-only projected covariance vs public `MomentOfInertiaEstimation::compute()` |
| 当前决策问题 | projected covariance RVV production dispatch 是否值得保留 |
| diagnostic 是否可外推到 production | 只能作为进入 bounded production probe 的依据；采纳必须看 phase040 public board evidence |
| comparison-boundary / baseline mismatch 风险 | 有；public compute 还包含 mean、主 covariance、moment、Eigen 和 OBB |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | phase010 已 positive；phase040 若 neutral / negative / unstable，则回滚或暂停，不创建正式 `doc-rvv` |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前无 adopted RVV family；按用户偏好，接入后 public board 显示有收益即可建议采纳，但仍记录 phase030 已回滚 |

## 板卡复跑预算和决策桶

默认 5 runs，`points=65536`、`iterations=5`、`warmup=1`，case `moi_public_compute`。若 median > 1.00 且 Evidence Doctor 无阻塞 Error，则按用户偏好可认为有收益；若低于 1 的 run 频率较高或 Doctor 报退化 Error，则暂停在 PI5 并建议回滚或降级证据。

## 继续 / 停止条件

若 phase040 有收益：保留 production patch，创建正式 `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md` 并进入 production closeout 文档同步。若无收益或证据不稳定：回滚 phase040 production patch，保留 phase010 diagnostic 结果，继续检查 roadmap 是否还有未阻塞候选。若板卡或工具失败：记录 blocker 和恢复命令，不把 QEMU 当性能结论。

## 文档更新清单

- `doc/phases/040-projected-covariance-production-probe/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/moment_of_inertia_estimation-evaluation.zh.md`
- `README.zh.md`
- `tmp/rvv-work-logs/features/moment_of_inertia_estimation/current-handoff/current-handoff.zh.md`
- 仅在采纳时创建：`doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md`
