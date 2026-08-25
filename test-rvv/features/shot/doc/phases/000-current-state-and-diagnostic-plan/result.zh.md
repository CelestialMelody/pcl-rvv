# Phase 000 result: current-state and diagnostic scaffold

## 执行范围

本阶段按 `plan.zh.md` 建立了 `features/include/pcl/features/impl/shot.hpp` 的 production-shaped diagnostic（生产形态诊断）入口。实际覆盖范围与计划一致：`SHOTEstimation<PointXYZ, Normal, SHOT352>` 和 `SHOTColorEstimation<PointXYZRGBA, Normal, SHOT1344>`，`Scalar=float`，合成 AoS（结构数组）输入，外部提供 local reference frame（局部参考系），radius-search 邻域。

本阶段没有修改 production（生产源码），没有接入 `shot.hpp` 的 RVV dispatch（分流逻辑），也没有把 QEMU（仿真器）计时写成性能结论。

## 动作回填

| action | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| A1 scaffold | done | `Makefile`、`board.mk`、`include/shot.h`、`include/impl/shot_fixtures.hpp` | topic-local 测试支撑可构建；`-llz4` 和 `PCL_SOURCE_ROOT` 展开问题已修复。 |
| A2 correctness | done | `make -C test-rvv/features/shot run_test_compare` | Std 与 RVV under QEMU 均通过 3 个 gtest。 |
| A3 bench wrapper | done | `src/bench_shot.cpp`，case: `public_shot352_fixed_lrf` / `public_shot1344_fixed_lrf` | 输出 `Dataset`、`Iterations`、`Warmup Iterations`、case timing 和 checksum，可被 board compare parser 解析。 |
| A4 asm | partial | `make -C test-rvv/features/shot dump_bench_rvv` | RVV bench 反汇编生成成功，`bench_shot_rvv.asm` 中有 RVV 指令；尚未归因到 SHOT 热点 helper，只能作为路径 / 二进制形状证据。 |
| A5 board smoke | done | `make -C test-rvv/features/shot board_smoke` | 板卡测试通过；公开入口 fixed-LRF bench 约为 1.00x，当前没有已有 RVV 收益。 |
| A6 docs | done for Phase 000, continue required | 本文件、optimization matrix、roadmap、evaluation、README | Phase 000 已闭合为诊断脚手架；下一阶段仍有未阻塞组件诊断。 |

## 当前证据

| 证据层级 | 路径 | 结果 | 能证明什么 |
| --- | --- | --- | --- |
| QEMU correctness（QEMU 正确性） | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | Std / RVV 均通过 3 个测试。 | 固定 LRF 下公开入口 descriptor 有限、L2 归一，非法 LRF NaN fallback 可复现。 |
| 反汇编 | `build/asm/riscv/bench_shot_rvv.asm` | 生成成功，含 RVV 指令。 | 只证明 RVV binary 存在 RVV 指令；不证明 `shot.hpp` 热点已经命中专门 RVV helper。 |
| 板卡 smoke | `log/board/run_test.log`、`log/board/analyze_bench_compare.log` | Milkv-Jupiter；30 iterations，3 warmup；SHOT352 Std 0.1231 ms / RVV 0.1237 ms；SHOT1344 Std 0.2669 ms / RVV 0.2679 ms。 | 板卡闭环可用；公开入口在没有 SHOT 专门 RVV helper 时没有收益。 |
| Evidence Doctor（证据体检） | `log/board/evidence_manifest.json`、`log/board/evidence_doctor.md` | Errors=2，Warnings=2，Suggestions=0。 | 单次 smoke 且 B/A 均低于 1，不能支撑 production performance（生产性能）或严格正向结论。 |

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | diagnostic / production-shaped diagnostic。 |
| A/B boundary | public overload shape with test-owned synthetic input；Std / RVV 主要比较已有库构建宏，不包含新的 SHOT RVV helper。 |
| 当前决策问题 | Phase 000 只回答 scaffold、correctness、bench parser、board smoke 和 Evidence Doctor 能否形成闭环。 |
| diagnostic 是否可外推到 production | no for performance adoption。固定 LRF、合成输入和无专门 helper 的公开入口 smoke 不能外推成真实 production 接入结论。 |
| comparison-boundary / baseline mismatch 风险 | yes。当前 RVV binary 中的 RVV 指令未归因到 `shot.hpp` 热点，Std/RVV 近似 1.00x 只能说明现状没有收益。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes, but only after component ablation（组件消融）识别出窄 helper 并通过 same-chain correctness、asm 和板卡 A/B。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。若后续提出新 helper family，必须补同一生产边界内的 detail A/B，不能用本阶段 public Std/RVV smoke 直接 clean-adopt。 |

## Evidence Doctor 处理

`ba_degradation_frequency` 的两个 Error 来自单次 B/A 均小于 1：SHOT352 B/A 约 0.9955，SHOT1344 B/A 约 0.9965。处理动作是降级证据边界：这些数据只作为 Phase 000 smoke 和负向线索，不作为 production evidence 或严格性能结论。

`low_run_count` 的两个 Warning 来自每个 case 只有一个 repeated value。由于本阶段目标是验证板卡闭环和日志形状，而不是采纳生产实现，不追加无限复跑。下一阶段若某个组件候选出现正向或接近阈值信号，应按计划补 repeated board summary 和 Evidence Doctor。

## 阶段反思

Phase 000 证明了测试/bench/板卡通道可用，也说明公开入口 smoke 会被 search、插值和未归因 RVV 指令稀释。下一步不宜直接改 production；应先做 component ablation（组件消融）把候选收窄。

优先级调整：先尝试 `normalizeHistogram` 组件诊断。理由是 352 / 1344 维 descriptor 连续数组更容易建立 same-chain 对拍、RVV intrinsic 归因和板卡 A/B；`createBinDistanceShape` 需要穿过 estimator 对象状态、normal cloud 和 protected helper，风险更高，适合作为后续阶段。

## Continue / Stop Decision

`continue_stop_decision`: continue。

`stop_condition_hit`: none。板卡可用，dirty isolation 当前限定在 `test-rvv/features/shot/**` 和队列表，继续 Phase 010 不需要修改 production 或其它 topic。

`next_phase_default`: `010-normalize-component-diagnostic`，先写 test-only normalization reference / RVV candidate，完成 correctness、component bench、asm、board smoke 和 Evidence Doctor。
