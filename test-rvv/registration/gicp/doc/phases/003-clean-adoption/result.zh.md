# Phase 003 Result: Clean Adoption

## 历史阶段状态

Phase 005 已按用户决策回退生产源码并删除长期生产文档；本文件只保留 Phase 003 当时的
clean adoption 候选证据，不再表示当前生产状态。

本阶段已把 `registration/include/pcl/registration/impl/gicp.hpp` 的生产补丁收敛成干净接入形态：
`OptimizationFunctorWithIndices::operator()` 回到标量 cost path，只保留 `dfddfLoopRVV()` 作为
`OptimizationFunctorWithIndices::dfddf()` 的 RVV 主循环加速点。RVV 命中条件仍然是
`__RVV10__`、`Scalar=float`、source / target 均满足 xyz AoS float layout、match 数不少于 32，
且 32-bit byte offset 可表达；未命中时仍回退到原标量循环。

`dfddfLoopRVV()` 只接管 `dfddf()` 中逐 correspondence 的 gradient、translation Hessian、
`dCost_dR_T`、`dCost_dR_T*b` 和 `hessian_rot_tmp` 累加。旋转导数后处理、Hessian 组装、
6x6 eigensolver 和 Newton 外围控制流继续走标量路径。

## 已完成动作

| action | evidence | result |
| --- | --- | --- |
| 清理 cost-only dispatch | `registration/include/pcl/registration/gicp.h`、`registration/include/pcl/registration/impl/gicp.hpp` | `operatorCostRVV()` 已删除，public `operator()` 回到标量 cost path |
| 保留 `dfddfLoopRVV()` | `registration/include/pcl/registration/gicp.h`、`registration/include/pcl/registration/impl/gicp.hpp` | `dfddf()` 仍可在 gate 命中时短路主累加循环 |
| 强化 production direct smoke | `test-rvv/registration/gicp/src/test_gicp.cpp` | public align 检查收敛、fitness、输出规模和 final transform 参考值 |
| QEMU correctness | `make run_test_compare` | Std/RVV 均 10 tests pass |
| QEMU smoke / asm | `make record_qemu_smoke_evidence_state` | bench 可运行；full asm 中 `dfddf()` 调用 `dfddfLoopRVV()`，helper 内有 RVV 指令 |
| board correctness | `make run_board_test_smoke` | RVV 10 tests pass |
| board public repeated 1024 点 | `log/board/production_public_align_pointxyz_dfddf_clean_repeated/summary.md` | median `1.080x`，min `1.065x`，max `1.083x`，bucket `weak_positive` |
| board public repeated 4096 点扩展 | `log/board/production_public_align_pointxyz_dfddf_clean_4096_repeated/summary.md` | median `1.058x`，min `1.048x`，max `1.060x`，bucket `weak_positive`；3-run expansion |

## Evidence Doctor

| evidence | Errors | Warnings | Suggestions | 处理 |
| --- | ---: | ---: | ---: | --- |
| `production_public_align_pointxyz_dfddf_clean_repeated/evidence_doctor.md` | 0 | 1 | 0 | reduction order mismatch；作为 production-public 弱正向证据，不外推到泛型点型 |
| `production_public_align_pointxyz_dfddf_clean_4096_repeated/evidence_doctor.md` | 0 | 2 | 0 | reduction mismatch + 3-run；只作规模扩展确认 |

## 数值一致性说明

public bench 的 raw checksum 对 double raw bits 很敏感，Std/RVV 因 RVV chunk reduction（分块规约）
会出现 checksum 不同。为避免只靠 checksum 判断，本阶段保留了 public align 的正确性 smoke，
同时让 QEMU / board smoke 都命中同一 `PointXYZ` public 路径。10/10 QEMU correctness、10/10
board smoke 都通过。

## EvidenceDecision

Phase 003 结论是 `production_public / weak_positive / recommend_adoption_with_user_confirmation`。
与 Phase 001 的 cost-only probe 相比，清理后的 `dfddfLoopRVV()` 接入后 public GICP
`PointXYZ -> PointXYZ` 仍保持稳定弱正向：1024 点 5-run median `1.080x`，4096 点 3-run median
`1.058x`，两组均无 B/A < 1。

当时的 production patch 只保留 `dfddfLoopRVV()`，不再携带 `operatorCostRVV()`。这说明接入收益来自
`dfddf()` 主循环，而不是 cost-only helper。Phase 005 已把该候选回退为 no-production closeout，
长期 `doc-rvv/registration/gicp-RVV.zh.md` 因没有 adopted production behavior 而删除。

## 未覆盖范围

- 非 `PointXYZ -> PointXYZ`、非 `Scalar=float`、非 xyz AoS float layout。
- `PointNormal` / `PointXYZINormal`、混合 source/target 点型和泛型 traits 扩展。
- correspondence-pair 入口、其它 row source policy 和 `Scalar=double`。
- RVV family 内部的 detail A/B：当前没有单独测 “只开 dfddfLoopRVV” 与其它 production family 的同边界比较。

## 下一步

本阶段的 clean adoption 候选已被 Phase 005 的 no-production 决策覆盖；当前默认恢复入口不再是保留
production patch，而是结束本 topic 或另开新的候选探索。下一步如果继续扩大范围，应另开 phase：

- 若要扩展到其它 row source、点型或 `Scalar`，另开 phase。
- 若要提交当前补丁，先执行提交前 evidence registry / diff / status 检查并等待提交授权。
