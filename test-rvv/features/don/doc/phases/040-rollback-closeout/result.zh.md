# 040 rollback closeout result

## 当前结论

用户已确认回滚 phase 030 中的 DON RVV production path（生产路径）。本阶段已删除 `features/include/pcl/features/impl/don.hpp` 中新增的 RVV helper、`__RVV10__` production dispatch（生产分流）和相关 include；`computeFeature()` 回到原标量逐点循环。

`PCLBase<PointInT>::initCompute()` 初始化修复被保留为 standalone correctness fix（独立正确性修复）。它解决 `Feature::compute()` 公开入口下 DON `indices_` 未初始化的问题，不代表 RVV production adoption（RVV 生产采纳）。

EvidenceDecision（证据决策）：`rolled_back_no_production`。

## 实际执行范围

| 计划动作 | 状态 | 证据路径 | 结论 |
| --- | --- | --- | --- |
| 回滚 production RVV helper / dispatch | done | `features/include/pcl/features/impl/don.hpp` | 已删除 `pcl::detail::don` RVV / Std helper 抽取和 `__RVV10__` 短路分流 |
| 保留 init 修复 | done | `features/include/pcl/features/impl/don.hpp` | `initCompute()` 仍调用 `PCLBase<PointInT>::initCompute()` |
| QEMU correctness | done | `make -C test-rvv/features/don run_test_compare` | Std/RVV build 各 5 个 gtest 通过 |
| production asm absence check | done | `make -C test-rvv/features/don check_production_rvv_asm` | 预期失败；binary / asm 生成成功，但 production RVV 指令组合不再匹配 |
| evidence registry | done | `make -C test-rvv/features/don evidence_status` | `fresh` |

## 证据解释

phase 030 的 production-public repeated board 仍是当前性能决策依据：median `0.908x`，`B/A < 1 = 5/5`，Evidence Doctor Errors=1（`ba_degradation_frequency`）。因此本阶段没有重跑板卡性能；回滚后的 production path 不含 DON RVV dispatch，继续跑 production A/B 不再回答 RVV adoption 问题。

QEMU correctness 只证明回滚后公开入口语义仍正确。`check_production_rvv_asm` 的失败在本阶段是预期结果，用于确认 production RVV path 已撤掉，不作为错误。

## Optimization matrix 更新

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback | board evidence | asm boundary | Evidence Doctor | decision | next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production direct DON RVV | ordered normal cloud | exact `PointNT=pcl::Normal` / `PointOutT=pcl::Normal`, float AoS | `Feature::compute()` -> `computeFeature()` | 回滚后 QEMU Std/RVV 5/5；公开入口保持标量语义 | phase 030 production-public repeated median `0.908x`，`B/A < 1 = 5/5` | 回滚后 production asm gate 预期失败 | phase 030 Errors=1 | `rolled_back_no_production` | 不采纳当前 RVV production path |
| initCompute public entry fix | DON public `Feature::compute()` | all current DON template instantiations using this header | `initCompute()` | QEMU Std/RVV 5/5，覆盖 exact normal public entry | not_applicable | not_applicable | not_applicable | `standalone_correctness_fix_retained` | 可作为非 RVV 正确性修复单独评审 |

## 阶段反思和后续方向

当前 DON 的直接 AoS（数组结构）跨步 load/store RVV path 不建议继续采纳到 production。若仍要探索，下一阶段不应扩大点类型，而应先做 production detail 消融：

- `finite-mask-ablation`：把 NaN/Inf 置零 mask 与正常 finite-only 输入拆开，判断 mask 成本是否主导。
- `sqrt-curvature-ablation`：单独测试 curvature `sqrt` 是否让 RVV 链路退化。
- `store-policy-ablation`：比较 strided store、临时 SoA buffer 后标量写回、只写 normal / 延迟 curvature 的形态。
- `public-wrapper-cost`：在同一 production bench 中比较 direct helper 与 `Feature::compute()` wrapper 边界，定位 helper-only weak-positive 与 public negative 的差异。

这些方向需要新 phase plan 和同边界 board evidence；不能复用 phase 010 helper-only positive 直接推出 production 可采纳。

## Continue / Stop Decision

本阶段关闭 phase 030 的 PI5 rollback checkpoint。当前 DON topic 对 production 的结论是 `rolled_back_no_production`，已无可采纳 RVV production patch。

可继续动作：若用户希望继续榨 DON，可进入 `050-production-detail-ablation`；否则建议把 features 队列推进到下一主题。
