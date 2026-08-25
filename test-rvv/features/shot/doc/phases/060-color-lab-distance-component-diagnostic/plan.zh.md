# Phase 060 plan: color LAB distance component diagnostic

## 阶段意图和边界

本阶段做 test-only color LAB distance component diagnostic（测试专用颜色 LAB 距离组件诊断）。目标是隔离
`SHOTColorEstimation::computePointSHOT` 中已经归一化的 L/a/b 三个颜色通道，验证这一段算术：

```text
colorDistance = (abs(LRef - L) + (abs(aRef - a) + abs(bRef - b)) / 2) / 3
binDistanceColor = clamp(colorDistance, 0, 1) * nr_color_bins
```

本阶段不修改 `features/include/pcl/features/impl/shot.hpp`，不调用 protected `RGB2CIELAB`，也不覆盖
RGB 到 CIELAB 的 LUT（查找表）离散加载成本。它只回答“归一化 LAB 数组上的距离 / bin distance 算术是否值得继续探索”，不能单独证明 SHOT1344 color production path（生产颜色路径）可接入 RVV。

## 当前状态清单

| item | current state |
| --- | --- |
| production source | `features/include/pcl/features/impl/shot.hpp` 保持未修改；颜色路径先把中心点和邻域点 RGB 转为 LAB，再归一化并计算 color bin distance。 |
| previous phase | Phase 050 interpolation geometry staging arrays 修正后仍为 0.97x，已标记 `attempted / not recommended for production probe`。 |
| test support | 当前 topic 使用 `src/`、`include/`、`include/impl/`、topic-local `script/`，适合新增单职责 helper。 |
| correctness target | `make -C test-rvv/features/shot run_test_compare` 当前通过 Std / RVV 各 9 个 gtest。 |
| bench target | `src/bench_shot.cpp` 已支持 `--case-filter` 隔离 component case。 |
| Evidence Doctor | `run_evidence_doctor` 当前对 Phase 050 数据报 Errors=1、Warnings=1；060 会补新的 manifest metadata 后重跑。 |

## 假设与候选族

| candidate family | hypothesis | risk / unknown |
| --- | --- | --- |
| normalized LAB distance RVV | 三个连续 float 数组的 abs / add / clamp / widen store 适合 RVV，可能为 SHOT1344 color path 提供低风险子候选。 | 真实 production 还包含 RGB2CIELAB LUT、`std::vector::push_back` 和后续插值，本阶段不覆盖这些成本。 |
| RGB-to-LAB indexed gather RVV | 更接近 production，但需要处理 PCL LUT、PointXYZRGBA traits 和 protected helper 语义。 | 范围大，容易和本阶段算术诊断混淆；本阶段暂不做。 |

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | correctness / fallback | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| normalized LAB distance RVV | contiguous normalized LAB arrays | float L/a/b input, double color-bin output | planned: scalar/RVV same-chain gtest | planned: `color_lab_distance_component` board A/B | planned: abs/add/clamp/store RVV attribution | planned | planned |
| RGB-to-LAB indexed gather RVV | indexed `PointXYZRGBA` cloud | RGB AoS + LUT gather | deferred | deferred | deferred | deferred | phase_deferred |

## 实现和测试动作

| action | dependency | expected artifact / command | completion criteria |
| --- | --- | --- | --- |
| F1 red test | plan exists | Add a gtest that calls `computeColorBinDistanceScalar/RVV`; run `make -C test-rvv/features/shot run_test_compare`. | Build fails because the helper does not exist, proving the test is active. |
| F2 helper implementation | F1 | Add `include/impl/shot_color.hpp` and include it from `include/shot.h`. | Std/RVV helper match within tight tolerance, including clamp boundaries. |
| F3 component bench | F2 | Add `color_lab_distance_component` to `src/bench_shot.cpp`. | Bench binary builds and case emits time/checksum lines. |
| F4 manifest metadata | F3 | Add case metadata in `script/generate_shot_evidence_manifest.py`. | Evidence Doctor can classify the new case as diagnostic/test-helper boundary. |
| F5 asm | F3 | `make -C test-rvv/features/shot dump_bench_rvv` plus targeted `rg` on asm. | RVV build contains expected vector abs/arithmetic/clamp/store instructions attributable to this topic helper. |
| F6 board evidence | F4 | `make -C test-rvv/features/shot run_board_bench_compare fetch_board_logs BENCH_ARGS='--case-filter color_lab_distance_component'`; then `run_evidence_doctor`. | Single targeted board run gives a decision bucket; rerun only if bucket is weak or contradictory. |
| F7 docs | F6 | Update result, phase README, optimization matrix, roadmap, evaluation, README and queue row as needed. | Current conclusion and next action can be recovered without chat context. |

## Evidence Doctor 和 registry 规则

Evidence Doctor（证据体检）输入仍使用 topic-local manifest wrapper：
`test-rvv/features/shot/script/generate_shot_evidence_manifest.py`。
本阶段的 evidence role 是 `diagnostic`，A/B boundary 是 `test_helper`。

当前 topic 没有独立 `log/evidence_registry.json`，因此本阶段 result 和 Handoff 使用人工 freshness check（新鲜度检查）：列出 run label、summary 路径、doctor 摘要，以及哪些 topic 文档已同步。

## 板卡复跑预算和决策桶

板卡可用性：当前会话已确认板卡可用，topic Makefile 已有 board compare / fetch 入口。

默认预算：

- 先跑 1 次 targeted board compare。
- 若 speedup >= 1.10x 且 Evidence Doctor 仅有 low-run warning，最多追加 2 次 targeted rerun 验证是否稳定。
- 若 speedup < 0.98x，直接标记 negative，不扩大复跑。
- 若 0.98x <= speedup < 1.10x，标记 neutral / weak，不追加 production probe。
- 若 run 间方向反转且预算用尽，标记 unstable 并降级结论。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic（组件诊断）。 |
| A/B boundary | test helper：连续 LAB 数组的算术 helper。 |
| 当前决策问题 | RVV-vs-scalar component A/B 和 implementation-shape。 |
| diagnostic 是否可外推到 production | 不能直接外推。它不含 RGB2CIELAB LUT、indexed PointXYZRGBA load、`std::vector::push_back` 和后续 interpolation。 |
| comparison-boundary / baseline mismatch 风险 | 存在。production 标量路径边转换 RGB 边 push，测试 helper 使用预归一化连续数组和预分配输出。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许仅凭本组件进入 production probe；若结果强正向，可作为后续 RGB/LUT 或 production-shaped color diagnostic 的输入。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。当前阶段不可能 clean-adopt。 |

## 阶段完成条件

本阶段只关闭 `normalized LAB distance RVV × contiguous array × test helper` 矩阵项。若 correctness、asm、board 和 doctor 都闭合：

- speedup >= 1.10x 且无未处理 Error：`partial-production-candidate`，下一阶段应做更接近 production 的 indexed RGB/LUT 或 color path probe plan。
- 0.98x <= speedup < 1.10x：`attempted / neutral`，不建议 production probe，可转向更窄 interpolation subcandidate 或结构文档补齐。
- speedup < 0.98x 或 doctor Error 无法解释：`attempted / not recommended`，拒绝该 arithmetic-only helper 作为 production probe 输入。

## 继续 / 停止条件

`stop_condition_hit` 只在继续需要修改 production、板卡 / 工具不可用、Evidence Doctor Error 无法降级、dirty isolation 不安全，或当前 roadmap 没有授权的未阻塞下一动作时成立。否则按 phase loop 继续。

`next_phase_default` 暂定为：若 060 正向，继续 `070-color-rgb-lut-indexed-diagnostic`；若 060 中性或负向，优先回到 roadmap 中的 `interpolation bin-selection / scalar-tail diagnostic` 或补 doc-suite/evidence registry 结构动作。

## 文档更新清单

- 写 `060-color-lab-distance-component-diagnostic/result.zh.md`。
- 更新 `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`。
- 更新 `doc/optimization-roadmap.zh.md` 和 `doc/shot-evaluation.zh.md`。
- 更新 `README.zh.md` 和 `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md` 的当前 topic 摘要。
