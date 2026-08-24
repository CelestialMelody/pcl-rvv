# 040 rollback closeout plan

## 阶段意图和边界

本阶段执行用户在 PI5 后确认的 rollback（回滚）：撤掉 `features/include/pcl/features/impl/don.hpp` 中的 DON RVV production dispatch/helper（生产分流 / helper），让真实公开入口回到标量 `computeFeature()` 语义。production-public board repeated（公开入口重复板卡性能）在 phase 030 为 `negative`，因此本阶段不再尝试采纳该 RVV path。

本阶段只保留可与 RVV 分流分开评审的 `PCLBase<PointInT>::initCompute()` 初始化修复。它修复 `Feature::compute()` 公开入口下 `indices_` 未初始化的问题，不构成 RVV optimization adoption（RVV 优化采纳）。

## 当前状态清单

| area | 当前状态 | 本阶段动作 |
| --- | --- | --- |
| production RVV path | phase 030 为 `pending_user_confirmation_rollback`，用户已确认回滚 | 删除 RVV helper、`__RVV10__` dispatch 和新增 RVV include |
| init 修复 | `initCompute()` 已调用 `PCLBase<PointInT>::initCompute()` | 保留为 standalone correctness fix（独立正确性修复） |
| production asm gate | phase 030 曾用于证明 RVV path 存在 | 回滚后该 gate 预期失败，只作为历史 / negative probe 证据 |
| correctness | QEMU Std/RVV 需继续通过 | 运行 `make -C test-rvv/features/don run_test_compare` |
| evidence registry | phase 030 registry 为 fresh | 运行 `make -C test-rvv/features/don evidence_status` 确认登记不因文档刷新失效 |

## 优化矩阵和完成条件

| candidate family | scope and entry | decision | 完成条件 |
| --- | --- | --- | --- |
| production direct DON RVV | exact `pcl::Normal` public `Feature::compute()` | `rolled_back_no_production` | 源码不再包含 DON production RVV helper / dispatch；文档和队列状态同步 |
| initCompute public entry fix | DON `initCompute()` | `standalone_correctness_fix_retained` | correctness 通过；文档明确它不是 RVV production adoption |

## 验证计划

1. `make -C test-rvv/features/don run_test_compare`
2. `make -C test-rvv/features/don evidence_status`
3. `git diff --check -- features/include/pcl/features/impl/don.hpp test-rvv/features/don doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md`
4. 可选执行 `make -C test-rvv/features/don check_production_rvv_asm`，预期失败，用于确认生产 RVV path 已撤掉；失败不作为本阶段错误。

## Continue / Stop 条件

回滚完成后，当前 DON production RVV path 关闭为 no-production。仍可继续的优化方向只能作为新 phase：production detail 消融（例如 finite mask、sqrt、strided store、wrapper/dispatch 开销拆分）或重新设计输出布局 / batch helper；不能直接扩大到泛型点类型。
