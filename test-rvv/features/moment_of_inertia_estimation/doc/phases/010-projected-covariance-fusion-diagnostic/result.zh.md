# Phase 010 result: projected covariance fusion diagnostic

## 执行范围

本阶段按 `plan.zh.md` 验证 `getProjectedCloud()` + projected `computeCovarianceMatrix()` 是否能在 test helper boundary（测试 helper 边界）融合为 RVV diagnostic（诊断）路径。实际范围保持为 `pcl::PointXYZ`、`float`、indexed AoS（结构数组索引访问）、单个法向量和 helper-only 计时；未修改 production（生产源码）。

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| B1 test-first 红灯 | done | 历史恢复记录：`computeProjectedCovarianceRVV` 缺失时 `run_test_compare` 编译失败 | 红灯来自候选 helper 缺失，范围符合计划 |
| B2 Std/RVV projected covariance helper | done | `include/impl/moi_reductions.hpp`；`make run_test_compare` | QEMU correctness（QEMU 正确性验证，不代表真实性能）通过，2 个 gtest 全部通过 |
| B3 bench case | done | `src/bench_moi.cpp`；QEMU smoke: `make run_bench_rvv BENCH_ARGS='--case-filter moi_projected_covariance --points 1024 --iterations 1 --warmup-iterations 1'` | `moi_projected_covariance` case-filter 可单独输出 label、us/iter 和 checksum；QEMU 只用于日志形状 |
| B4 asm / board / doctor | done | `make dump_bench_rvv`；`make run_board_moi_phase010_repeated`；`log/board/repeated_phase010_projected_covariance_diagnostic/{summary.md,evidence_manifest.json,evidence_doctor.md}` | projected covariance helper 范围内有 `vluxei32`、`vfmacc`、`vfnmsac`、`vfredusum`；5-run board bucket 为 `positive` |

## 正确性、反汇编和板卡证据

| evidence layer | result | boundary |
| --- | --- | --- |
| correctness | `make run_test_compare` exit 0；Std/RVV 两个 binary 各运行 2 个 gtest，全部通过 | 数值容差验收覆盖 reduction summary 和 projected covariance helper；不证明 public `compute()` dispatch |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS='--case-filter moi_projected_covariance --points 1024 --iterations 1 --warmup-iterations 1'` exit 0 | 只证明 bench 输出形状和 case-filter；不作为性能结论 |
| asm attribution（反汇编归属） | `build/asm/riscv/bench_moi_rvv.full.asm` 的 `measureProjectedCovariance` 符号范围内出现 `vluxei32`、`vfmacc`、`vfnmsac`、`vfredusum` | RVV 指令归属到 bench 内联 helper；不是 production 符号归属 |
| board repeated | `moi_projected_covariance,points=262144`，5 runs，median 1.247x，min 1.237x，max 1.353x，0/5 低于 1.0 | 性能结论只适用于 helper-only diagnostic，计时边界为 `helper_only_projected_covariance_no_full_compute` |
| Evidence Doctor（证据体检） | Errors=0，Warnings=0，Suggestions=1 | suggestion 是 `binary_identity_missing`；不阻塞本阶段 diagnostic 结论，生产接入前应补 binary identity 或用同轮 clean deploy 重跑 |
| registry freshness（证据登记新鲜度） | `make evidence_status_phase010` -> `evidence registry check: fresh` | phase010 summary / manifest / doctor 已登记到 `log/evidence_registry.json` |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：materialized projection scalar reference vs fused projected covariance RVV |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 projected covariance fusion 是否有足够正向信号进入生产接入计划 |
| diagnostic 是否可外推到 production | 不能直接外推。公式和数据流与 production 的 projected covariance 片段一致，但不包含完整 angle scan（角度扫描）、`PointCloud` 分配生命周期、`computeEccentricity()` 的 Eigen 求解和 public `compute()` 状态更新 |
| comparison-boundary / baseline mismatch 风险 | 存在。helper-only bench 没有计入 production 的 `projected_cloud` 分配 / resize / header 写回，也没有计入每个角度的 Eigen solver；因此它只能证明该数学片段有正向信号 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive；若后续 production direct 变弱或转负，应保留 diagnostic helper，不应把本阶段直接写成 no-production 或 clean-adopt |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。进入 production 后还要比较真实 public path 的标量 / RVV，并按 production 符号做反汇编和板卡证据 |

## Optimization matrix 更新

`projected cloud covariance fusion` 在当前窄范围内从 `phase_deferred + unblocked` 更新为 `attempted_positive_diagnostic`：正确性、反汇编、板卡 repeated 和 Evidence Doctor 都闭合，但 evidence role 仍是 diagnostic，不是 production direct。

## 阶段反思

phase 000 的 fused xyz reductions 和 phase 010 的 projected covariance fusion 都在 helper-only diagnostic boundary 下呈正向。下一阶段最值得做的是 PI1 production integration plan（生产接入计划）：冻结是否以 exact `PointXYZ` 先接入、还是用 `RVVXYZAoSFloatLayout<PointT>` 做 PointXYZ-like traits gate；同时列出 small input、non-RVV build、非 xyz float layout、32-bit indexed byte offset、indices 有效性、`Scalar=float` 和完整 `compute()` 语义保持的 fallback 矩阵。

本阶段也暴露了一个证据质量改进：summary manifest 仍缺 binary hash（或等价二进制身份）。这不是当前 positive bucket 的阻塞项，但生产接入证据重跑前应补齐。

## continue / stop decision

`continue_stop_decision`：phase 010 完成。当前 roadmap 仍有授权范围内的文档动作（PI1 计划）可继续，但实际 production patch（生产补丁）会修改 `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` / 可能还需声明调整，按仓库规则需要用户明确授权后才能进入 PI2。

`next_phase_default`：`020-pi1-production-integration-plan`。

`stop_condition_hit`：未命中工具或板卡阻塞；生产源码修改边界待授权。
