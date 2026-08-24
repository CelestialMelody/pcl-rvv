# 030 production-direct-probe result

## 当前结论

本阶段完成 PI2-PI5 的 production direct（真实生产路径证据）闭环，但证据不支持采纳当前 RVV production path。本阶段完成时状态是 `pending_user_confirmation_rollback`：保留 production patch（生产补丁）供用户 / reviewer 检查，不能由 worker 自行回滚。

更新：用户已在后续 phase 040 确认回滚，当前生产源码已撤掉 DON RVV helper / dispatch；本文件保留为 phase 030 历史负向证据。

关键结论：

- QEMU correctness（QEMU 正确性验证）：Std/RVV 都通过 5 个 gtest。
- production public asm（公开入口反汇编）：`bench_don_production_rvv` 的 `DifferenceOfNormalsEstimation::computeFeature()` 符号范围内出现 `vlse32.v`、`vfsub.vv`、`vmerge.vvm`、`vfsqrt.v`、`vsse32.v`。
- production public board repeated（公开入口重复板卡性能）：5-run median `0.908x`，min `0.899x`，max `0.949x`，`B/A < 1 = 5/5`，decision bucket 为 `negative`。
- Evidence Doctor（证据体检）：Errors=1，Warnings=0，Suggestions=0；Error 是 `ba_degradation_frequency`，说明 5/5 退化频率不能支撑 production adoption（生产采纳）。

## 实际执行范围

| 计划动作 | 状态 | 证据路径 | 结论 |
| --- | --- | --- | --- |
| TDD 红灯：production asm gate | done | `make -C test-rvv/features/don check_production_rvv_asm` 在生产补丁前失败 | 红灯有效，当前生产源码无可归属 DON production RVV 指令组合 |
| production patch | done | `features/include/pcl/features/impl/don.hpp` | 增加 `computeFeatureStd()`、exact `pcl::Normal` RVV helper 和 `__RVV10__` 短路分流 |
| public entry 初始化修复 | done | `features/include/pcl/features/impl/don.hpp` | `initCompute()` 调用 `PCLBase::initCompute()`，修复真实 `Feature::compute()` 入口下 `indices_` 未初始化的段错误 |
| production direct / fallback gtest | done | `test-rvv/features/don/src/test_don.cpp`，`log/qemu/run_test_std.log`，`log/qemu/run_test_rvv.log` | exact normal public entry 和非 exact output fallback 都与标量 reference 一致 |
| production asm gate | done | `test-rvv/features/don/build/asm/riscv/bench_don_production_rvv.asm` | 反汇编 gate 通过，路径命中 RVV 指令 |
| production board repeated | done | `test-rvv/features/don/log/board/repeated_phase030_production_direct/summary.md` | production-public 5-run 为 `negative` |
| Evidence Doctor / registry | done | `test-rvv/features/don/log/board/repeated_phase030_production_direct/evidence_doctor.md`，`test-rvv/features/don/log/evidence_registry.json` | Doctor 有 1 个 Error；registry check 为 fresh |

## Diagnostic-to-Production Mismatch Audit 回填

| question | result |
| --- | --- |
| evidence role | helper-only 阶段是 `diagnostic`；本阶段是 `production-public` |
| A/B boundary | helper-only 阶段是 `test helper`；本阶段是 `public overload`，通过 `Feature::compute()` 触发 DON private virtual `computeFeature()` |
| 当前决策问题 | `RVV-vs-scalar`：当前 public RVV path 是否快于当前 public scalar path |
| diagnostic 是否可外推到 production | 不可直接外推。helper-only repeated 为 `weak_positive`，但 production public repeated 为 `negative`，二者方向不一致 |
| comparison-boundary / baseline mismatch 风险 | 已实际发生。public entry 包含初始化、输出准备和 dispatch 成本；该边界下 RVV helper 收益被抵消并转为退化 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已完成 bounded probe；结果不支持采纳当前 RVV 分流 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有进入 clean adoption；若继续探索，下一步应先做 production detail 消融，而不是扩大点类型 |

## 证据链

| 类别 | 命令 / 路径 | 结果 | 证据边界 |
| --- | --- | --- | --- |
| QEMU correctness | `make -C test-rvv/features/don run_test_compare` | Std/RVV 各 5 个 gtest 通过 | 正确性，不证明性能 |
| QEMU production smoke | `make -C test-rvv/features/don run_bench_production_rvv BENCH_ARGS='--points 4096 --iterations 1 --warmup-iterations 1 --case-filter don_production_direct'` | bench 可运行并输出 checksum | 日志形状，不证明性能 |
| production asm | `make -C test-rvv/features/don check_production_rvv_asm` | gate pass | 证明 production-direct RVV 指令存在；不证明性能 |
| board repeated | `make -C test-rvv/features/don run_board_don_production_repeated` | median `0.908x`，`B/A < 1 = 5/5` | 目标硬件性能证据，反对采纳当前 RVV path |
| Evidence Doctor | `test-rvv/features/don/log/board/repeated_phase030_production_direct/evidence_doctor.md` | Errors=1 | Error 未修正前不能写 production adoption |
| registry | `make -C test-rvv/features/don evidence_status` | fresh | 当前 summary / manifest / doctor 已登记 |

## Optimization matrix 更新

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback | board evidence | asm boundary | Evidence Doctor | decision | next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production direct DON RVV | ordered normal cloud | exact `PointNT=pcl::Normal` / `PointOutT=pcl::Normal`, float AoS | `Feature::compute()` -> `computeFeature()` | QEMU Std/RVV 5/5；非 exact `PointOutT` fallback 通过 | production-public repeated median `0.908x`，`B/A < 1 = 5/5` | production bench asm gate pass | Errors=1, Warnings=0, Suggestions=0 | `pending_user_confirmation_rollback` | 用户确认后回滚 RVV production path；可考虑单独保留 `PCLBase::initCompute()` 修复 |

## 阶段反思

本阶段把 helper-only `weak_positive` 证据降级为“只支持生产探针”的历史诊断证据。真实公开入口下，当前 RVV helper 的跨步 load/store、finite mask、sqrt 和写回成本没有形成收益，反而稳定慢于标量路径。下一步不建议扩大到泛型 normal-like 点类型，因为 exact `pcl::Normal` 的 production-public 边界已经负向。

可选后续方向：

- 推荐默认：用户确认后回滚 RVV production dispatch/helper，保留 topic-local diagnostic / bench 资产和负向证据。
- 可单独评审：`PCLBase::initCompute()` 修复是否作为非 RVV correctness patch 保留；它解决 `Feature::compute()` 入口下 `indices_` 未初始化的问题。
- 若继续探索：只做 production detail 消融，例如把 `Feature::compute()` 包装开销、finite mask、sqrt 和 strided store 分离；这应另建窄 phase，不应直接扩大点类型或 traits gate。
- 当前不建议：generic normal point type expansion；因为 exact production-public 性能未成立。

## Continue / Stop Decision

本阶段命中 PI5 用户确认停止条件。production evidence 为负向且 Evidence Doctor 有 Error；worker 不能自行采纳，也不能自行回滚当前 production patch。默认恢复动作是等待用户确认：

1. 回滚 RVV production dispatch/helper，并决定是否保留 `PCLBase::initCompute()` 修复。
2. 或要求补 production detail 消融，再决定是否回滚。
