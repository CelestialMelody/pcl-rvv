# 000 current-state-and-diagnostic-plan 结果

## 执行范围

本阶段按计划建立 DON（Difference of Normals，法线差分）逐点热点的 test-only RVV candidate（仅测试使用的 RVV 候选）。本阶段没有修改 production（生产源码），也没有声明 `features/include/pcl/features/impl/don.hpp` 已经接入 RVV。

| 计划动作 | 状态 | 证据 | 缺口 |
| --- | --- | --- | --- |
| RED 测试 | historical_done | 计划记录了先让 `computeDoNRVV()` 缺失导致 RVV test 失败；当前恢复时该代码已存在，不能重新证明红灯时序 | 只能作为历史 TDD 记录；本轮不把它当 fresh RED |
| GREEN candidate | done | `test-rvv/features/don/include/impl/don_core.hpp` 提供 `computeDoNScalar()` 和 `computeDoNRVV()`；QEMU Std/RVV correctness 均为 3 tests passed | 只覆盖 `pcl::Normal` ordered normal cloud |
| bench 入口 | done | `test-rvv/features/don/src/bench_don.cpp` 输出 `don_normal_pair`、iterations、warmup、checksum 和 us/iter | 计时边界是 helper-only，不包含 production dispatch |
| 反汇编 | done | `make -C test-rvv/features/don dump_bench_rvv` 生成 `build/asm/riscv/bench_don_rvv.asm`；可见 `vlse32.v`、`vfsub.vv`、`vfmul.vf`、`vmerge.vvm`、`vfmacc.vv`、`vfsqrt.v`、`vsse32.v` | helper 被内联到 bench hot path，当前归属是 bench candidate 边界 |
| 板卡 smoke | done | `log/board/analyze_bench_compare.log`：Milkv-Jupiter，262144 points，20 iterations，Std `13366.1731 us`，RVV `12191.6293 us`，speedup `1.10x` | 只有 1-run smoke，不能支撑强稳定性能结论 |
| Evidence Doctor | done_with_warning | `log/board/evidence_doctor.md`：Errors=0，Warnings=1，Suggestions=0；warning 为 `low_run_count` | 已降级为 weak-positive diagnostic |
| evidence registry | done | `log/evidence_registry.json` 登记 board smoke summary / manifest / doctor | 本阶段 result 和 evaluation 需引用这些路径 |

## Correctness 与语义边界

QEMU correctness（QEMU 正确性验证）覆盖三类语义：正常三分量差、NaN / Inf 输出置零、非整 VL tail（向量尾段）尺寸。`HelperMatchesProductionEstimatorForOrderedNormalCloud` 还用真实 `DifferenceOfNormalsEstimation` 对象调用 `initCompute()` / `computeFeature()`，证明 test-only helper 与当前 production 标量语义在 ordered normal cloud 下同构。

本阶段不覆盖 `initCompute()` 的失败路径，也不覆盖其它 `PointNT` / `PointOutT` 字段布局；这些属于 production-shaped diagnostic（生产形态诊断）或 PI1 计划的后续边界。

## 反汇编归属

`build/asm/riscv/bench_don_rvv.asm` 中的关键指令簇包含跨步加载（`vlse32.v`）、三分量差（`vfsub.vv`）、0.5 缩放（`vfmul.vf`）、finite mask（有限值掩码）和置零选择（`vmerge.vvm`）、curvature 计算（`vfmacc.vv` / `vfsqrt.v`）以及跨步写回（`vsse32.v`）。这说明 RVV build 的 bench helper hot path 命中了预期 RVV 指令。

## Board 证据与 Evidence Doctor

| 证据 | 路径 | 结论 |
| --- | --- | --- |
| board correctness | `test-rvv/features/don/log/board/run_test.log` | RVV build 3 tests passed |
| board smoke summary | `test-rvv/features/don/log/board/analyze_bench_compare.log` | `don_normal_pair` 为 `1.10x` |
| manifest | `test-rvv/features/don/log/board/evidence_manifest.json` | `evidence_role=diagnostic`，`run_count=1` |
| Evidence Doctor | `test-rvv/features/don/log/board/evidence_doctor.md` | Errors=0，Warnings=1（`low_run_count`），Suggestions=0 |
| registry | `test-rvv/features/don/log/evidence_registry.json` | 当前 board smoke 三个摘要文件已登记 |

`low_run_count` 的处理方式是降级证据边界：当前 board smoke 只能说明 DON helper-only RVV candidate 有弱正向信号，不能说明生产入口性能稳定成立。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic（诊断） |
| A/B boundary | test helper（测试 helper） |
| 当前决策问题 | RVV-vs-scalar 与 implementation-shape（实现形态） |
| diagnostic 是否可外推到 production | 不能直接外推。helper-only bench 不包含 `Feature::compute()` 输出准备、公开入口对象状态、生产 fallback 或前置 normal estimation |
| comparison-boundary / baseline mismatch 风险 | 有。Std/RVV 对比只改变 `__RVV10__` build 下的 test helper，不是真实 production dispatch |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许，但必须先完成 repeated board 稳定性检查和 PI1 production scope；若 repeated 降为 neutral/negative，应保持 diagnostic |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已采用 DON RVV family，不需要 RVV-vs-RVV family selection；但仍需要 production direct 证据才能采纳 production patch |

## 阶段决策

当前阶段结论为 `partial-production-candidate`。理由是：正确性、反汇编和单次 board smoke 均支持候选继续；但性能证据只有 1-run，且边界是 test-only helper，不能直接进入 production adoption。

`next_phase_default`：进入 `010-diagnostic-repeated-board`，做 5-run repeated board summary、Evidence Doctor 和 registry 刷新。如果 repeated 仍为 weak-positive 或 positive 且无 Error，再进入 PI1 production integration plan（生产接入计划）；如果 repeated 降为 neutral/negative/unstable，则保持 diagnostic，不修改 production。
