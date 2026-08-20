# Phase 020 result: column-stride depth production probe

## 当前结论

本阶段已把 phase 010 的 `column-stride-depth-direct` 候选接入真实 `BilateralUpsampling<PointInT, PointOutT>::performProcessing`，并完成 PI2-PI5 的 production public（公开生产入口）证据采集。QEMU correctness（正确性）通过，反汇编能归属到生产 RVV helper，但最新 Milkv-Jupiter 板卡公开入口 Std/RVV 对比为 `0.96x`、`0.89x`、`0.95x`，Evidence Doctor（证据体检）报告 `Errors=3`、`Warnings=9`、`Suggestions=0`。

当前 EvidenceDecision（证据决策）：`pending_user_confirmation_rollback`。含义是：同一 production public 边界下，当前生产 RVV 补丁不建议采纳；但按照 PI5 规则，生产补丁必须先保留给用户检查，不能由 worker 自动回滚。默认建议是回滚该 production patch，保留 topic-local 测试、bench、phase 文档和负向证据。

## 计划回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| PI2 production patch | done | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` | 抽出 Std helper，新增 `__RVV10__` gated RVV helper；公开 API 不变。 |
| PI3 production public tests | done | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | QEMU std/RVV 两侧各 9 个测试通过，新增 `PointXYZRGB` / `PointXYZRGBA` public entry 对拍。 |
| PI4 asm | done | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | RVV bench 反汇编中 production helper 符号可见 `vlse32.v`、`vmfeq.vv`、`vmerge.vvm`、`vfmul.vv`、`vfredusum.vs`。 |
| PI4 board | done | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | production public 三项为 `0.96x`、`0.89x`、`0.95x`，整体 decision bucket 为 negative。 |
| PI4 doctor | done-with-errors | `evidence_manifest.json` -> `evidence-doctor.md` | `Errors=3` 来自三项 `ba_degradation_frequency`；结论降级为不建议采纳。 |
| PI5 decision | paused | 本文件、matrix、roadmap、evaluation、benchmark 文档和队列表已刷新 | 停在用户确认点；未自动采纳，也未自动回滚。 |

## 证据分层

Correctness：`run_test_compare` 在 QEMU 上通过，Std 和 RVV 构建各运行 9 个测试。新增 public-entry test（公开入口测试）覆盖 `PointXYZRGB` 的 NaN holes 和 `PointXYZRGBA` dense 输入，证明生产分流和标量 fallback 在当前误差预算内一致。QEMU 只证明功能、fallback 和路径，不参与性能判断。

Disassembly evidence（反汇编证据）：`dump_bench_rvv` 生成 `build/asm/riscv/bench_bilateral_upsampling_rvv.full.asm`。生产 helper 符号 `pcl::bilateralUpsamplingPerformProcessingRVV<pcl::PointXYZRGB, pcl::PointXYZRGB>` 和 `pcl::bilateralUpsamplingPerformProcessingRVV<pcl::PointXYZRGBA, pcl::PointXYZRGBA>` 可归属到 `vlse32.v`、`vmfeq.vv`、`vmerge.vvm`、`vfmul.vv`、`vfredusum.vs`。

Board performance（板卡性能）：`log/board/analyze_bench_compare.log` 来自 Milkv-Jupiter，5 iterations、2 warmup。phase 010 diagnostic direct-depth 同次 smoke 仍正向，但本阶段只用 production public 三项决定生产补丁是否建议采纳。

| case | Std avg | RVV avg | speedup | bucket |
| --- | ---: | ---: | ---: | --- |
| `production public PointXYZRGB 80x60 w3 dense` | 6.8092 ms | 7.0564 ms | 0.96x | negative |
| `production public PointXYZRGB 120x90 w4 holes` | 27.1604 ms | 30.5335 ms | 0.89x | negative |
| `production public PointXYZRGBA 180x120 w5 dense` | 80.2801 ms | 84.7961 ms | 0.95x | negative |

Evidence Doctor：phase 020 manifest 记录 production-public evidence role（公开生产入口证据角色）和 public overload A/B boundary（公开重载对比边界）。报告为 `Errors=3`、`Warnings=9`、`Suggestions=0`。三个 Error 都是 `ba_degradation_frequency`，说明每个 production public case 的 RVV 候选都低于标量；九个 Warning 来自 gate、mask、reduction 差异，这些差异是 RVV 分流的预期差异，但在负向结果下不能支撑采纳。

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | production-public；phase 010 direct-depth 仅作为进入本阶段的 diagnostic 输入。 |
| A/B boundary | public overload，Std 和 RVV 都从 `BilateralUpsampling::process` 进入真实 `performProcessing`。 |
| 当前决策问题 | RVV-vs-scalar，判断当前生产补丁是否比标量公开入口更快且正确。 |
| diagnostic 是否可外推到 production | no。phase 010 的最新 `1.01x/1.11x/1.26x` 只支持进入生产探针；phase 020 已用同边界 public entry 刷新结论。 |
| comparison-boundary / baseline mismatch 风险 | phase 020 已降低到公开入口同边界；仍存在预期 gate/mask/reduction 差异，Doctor 以 Warning 暴露。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已经是 bounded production probe。因三项 public case 均低于 1.0x，当前不建议继续接入。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有进入 clean adoption；无需再补 family-selection A/B 来证明拒绝。 |

## 板卡复跑预算和决策桶

计划允许当 production direct 三项中任一项落在 `0.97x..1.10x` 时最多追加一次同边界复跑。最新结果为 `0.96x`、`0.89x`、`0.95x`，三项都低于 1.0x，其中 `0.89x` 是明确负向；decision bucket 已稳定为 negative。为避免用复跑预算稀释明确负向生产证据，本阶段不追加复跑，直接进入 PI5 用户确认点。

## 未覆盖范围

当前 production gate 只覆盖 `PointXYZRGB` / `PointXYZRGBA` exact type（精确点型）和 `RVVXYZAoSFloatLayout`。其它 RGB-like 自定义点类型、非 organized 输入、真实传感器数据分布、更大规模、多次重复板卡 summary 和去除 `process` 既有 stdout 打印后的计时边界都未覆盖。因为公开入口证据已负向，这些扩展不作为当前补丁采纳前置项。

## 继续 / 停止决定

`continue_stop_decision`: stop_for_PI5_user_confirmation。

`stop_condition_hit`: PI5 用户确认点。生产证据不支持采纳，但 worker 不能自动回滚生产源码；需要用户确认是否回滚 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` 的生产补丁，或是否保留补丁继续做额外复跑 / 消融。

`next_phase_default`: 若用户确认回滚，进入 `030-production-rollback-and-no-production-closeout`，只回滚 production source，保留 topic-local 测试、bench 和证据文档；若用户要求继续调查，建议先做 `production-public-overhead-ablation`，拆分 public entry 对象构造、既有 stdout 打印、weight staging 和 RVV reduction 的开销边界。
