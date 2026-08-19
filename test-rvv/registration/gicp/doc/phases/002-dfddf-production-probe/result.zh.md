# Phase 002 Result: dfddf Production Probe

## 当前状态

本阶段已把 `OptimizationFunctorWithIndices::dfddf()` 的逐 correspondence 累加主循环接入
`registration/include/pcl/registration/impl/gicp.hpp`，作为 `dfddfLoopRVV()` 有界生产探针。
探针只在 `__RVV10__`、`Scalar=float`、source / target 都满足 RVV xyz AoS float layout、
match 数不少于 32 且 32-bit byte offset 可表达时命中；否则保留原标量循环。

`dfddfLoopRVV()` 只替换循环内的 gradient、translation Hessian、`dCost_dR_T`、
`dCost_dR_T*b` 和 `hessian_rot_tmp` 累加。旋转导数、Hessian 组装、6x6 eigensolver 和
Newton 外围控制流仍沿用原标量路径。

## 已完成动作

| action | evidence | result |
| --- | --- | --- |
| 新增 production helper | `registration/include/pcl/registration/gicp.h`、`registration/include/pcl/registration/impl/gicp.hpp` | `dfddfLoopRVV()` 已接入 `dfddf()` fallback 前 |
| 强化 production direct smoke | `test-rvv/registration/gicp/src/test_gicp.cpp` | public align 现在检查收敛、fitness、输出规模和 final transform 参考值 |
| QEMU correctness | `make run_test_compare` | Std/RVV 均 10 tests pass |
| QEMU smoke / asm | `make run_bench_all_smoke dump_bench_rvv` | bench 可运行；full asm 中 `dfddf()` 调用 `dfddfLoopRVV()`，helper 内有 RVV 指令 |
| board correctness | `make run_board_test_smoke` | RVV 10 tests pass |
| board public repeated 1024 点 | `log/board/production_public_align_pointxyz_dfddf_repeated/summary.md` | median `1.068x`，min `1.051x`，max `1.078x`，bucket `weak_positive` |
| board public repeated 4096 点扩展 | `log/board/production_public_align_pointxyz_dfddf_4096_repeated/summary.md` | median `1.070x`，min `1.068x`，max `1.073x`，bucket `weak_positive`；3-run expansion |

## Evidence Doctor

| evidence | Errors | Warnings | Suggestions | 处理 |
| --- | ---: | ---: | ---: | --- |
| `production_public_align_pointxyz_dfddf_repeated/evidence_doctor.md` | 0 | 1 | 0 | reduction order mismatch；作为 production-public 弱正向证据，不写成泛型 clean adoption |
| `production_public_align_pointxyz_dfddf_4096_repeated/evidence_doctor.md` | 0 | 2 | 0 | reduction mismatch + 3-run；只作规模扩展确认 |

## 数值一致性说明

public bench 的 raw checksum 对 double raw bits 很敏感，Std/RVV 因 RVV chunk reduction（分块规约）
会出现 checksum 不同。为避免只靠 checksum 判断，本阶段用临时 1024 点 QEMU probe 对比了
Std/RVV final transform：fitness 差约 `9.37e-12`，transform 最大绝对差约 `1.40e-8`。
正式 correctness 中新增的 `GICPProductionDirect.PublicAlignPointXYZSmoke` 也把小点云 public align
锁定到标量参考 transform，Std/RVV 和板卡 smoke 均通过。

## EvidenceDecision

Phase 002 结论是 `production_public / weak_positive / recommend_adoption_with_user_confirmation`。
与 Phase 001 的 cost-only probe 相比，`dfddfLoopRVV()` 接入后 public GICP `PointXYZ -> PointXYZ`
从 neutral 提升到稳定弱正向：1024 点 5-run median `1.068x`，4096 点 3-run median `1.070x`，
两组均无 B/A < 1。

建议将 `dfddfLoopRVV()` 作为正式生产候选保留；`operatorCostRVV()` 单独收益不足，但当前 public
证据测的是二者同时启用后的生产 RVV path。最终是否采纳当前生产 patch、是否保留 cost-only helper
作为叠加优化，以及是否进入提交阶段，按 PI5 规则等待用户确认。

## 未覆盖范围

- 非 `PointXYZ -> PointXYZ`、非 `Scalar=float`、非 xyz AoS float layout。
- `PointNormal` / `PointXYZINormal`、混合 source/target 点型和泛型 traits 扩展。
- correspondence-pair 入口、其它 row source policy 和 `Scalar=double`。
- RVV family 内部的 detail A/B：当前没有单独测 “只开 dfddf、关闭 cost-only” 与 “二者同时开”。

## 下一步

默认停止在 PI5 用户检查点：保留当前 production patch，不提交、不回滚、不创建
`doc-rvv/registration/gicp-RVV.zh.md`。若用户确认采纳，下一步应做 production closeout：

- 决定是否保留 `operatorCostRVV()` 或只保留 `dfddfLoopRVV()`。
- 刷新长期 `doc-rvv/registration/gicp-RVV.zh.md`。
- 补提交前 evidence registry / diff / status 检查。
