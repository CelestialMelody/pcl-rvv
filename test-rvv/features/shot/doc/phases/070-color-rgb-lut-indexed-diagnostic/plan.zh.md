# Phase 070 plan: color RGB/LUT indexed diagnostic

## 阶段意图和边界

本阶段做 test-only color RGB/LUT indexed diagnostic（测试专用 indexed RGB / 查找表颜色诊断）。它比 Phase 060 更接近
`SHOTColorEstimation::computePointSHOT` 的颜色前半段：从 indexed `PointXYZRGBA` surface（按索引访问的颜色点云）读取 RGB，
通过 PCL 的 `RGB2sRGB_LUT` 和 `XYZ2LAB_LUT` 得到归一化 LAB，再计算 `binDistanceColor`。

本阶段不修改 `features/include/pcl/features/impl/shot.hpp`，不接 production dispatch（生产分流），不覆盖完整
`interpolateDoubleChannel` / `interpolateSingleChannel`。RVV 候选允许把 indexed RGB 读取和 RGB2CIELAB LUT（查找表）先 staging（标量暂存）到连续
LAB arrays，并把 staging 成本计入 bench；这样能回答“加上 RGB/LUT 前半段后，Phase 060 的算术收益是否仍有继续价值”。本阶段不声明 LUT gather 已经 RVV 化；若 staging 版仍正向，后续再单独评估 direct LUT gather。

## 当前状态清单

| item | current state |
| --- | --- |
| production source | `shot.hpp` 未修改；颜色路径在 loop 中逐点 RGB -> CIELAB -> normalized LAB -> color bin distance -> push_back。 |
| previous phase | Phase 060 normalized LAB arithmetic 3 次板卡 run 为 1.10x-1.23x，Evidence Doctor Errors=0、Warnings=1。 |
| test support | `include/impl/shot_color.hpp` 已承载 color helper，可继续追加 RGB/LUT indexed helper。 |
| correctness target | `make -C test-rvv/features/shot run_test_compare` 通过 Std / RVV 各 10 个 gtest。 |
| bench target | `color_lab_distance_component` 已存在；本阶段新增 `color_rgb_lut_indexed_component`。 |

## 假设与候选族

| candidate family | hypothesis | risk / unknown |
| --- | --- | --- |
| indexed RGB/LUT scalar staging + RVV LAB distance | RGB/LUT 转换先暂存为连续 LAB arrays，随后复用 Phase 060 的 RVV LAB distance，可能保留部分收益。 | scalar staging、两级 LUT、float-to-int index 和多组临时数组可能吞掉收益。 |
| direct indexed PointXYZRGBA / LUT gather | 直接 RVV gather r/g/b byte 字段和 LUT 更接近 production。 | byte gather、LUT gather intrinsic 与结构 offset 复杂；若 staging 版已经退化，直接 production probe 风险更高。 |

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | correctness / fallback | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| indexed RGB/LUT scalar staging + RVV LAB distance | indexed color surface batch | `pcl::PointXYZRGBA` AoS + `pcl::Indices`, scalar LAB staging, double output | planned | planned: `color_rgb_lut_indexed_component` | planned | planned | planned |
| direct indexed PointXYZRGBA / LUT gather | indexed color surface batch | direct byte gather from AoS plus LUT gather | deferred | deferred | deferred | deferred | phase_deferred |

## 实现和测试动作

| action | dependency | expected artifact / command | completion criteria |
| --- | --- | --- | --- |
| F1 red test | plan exists | Add a gtest that calls `computeColorBinDistanceIndexedRGBScalar/RVV`; run `make -C test-rvv/features/shot run_test_compare`. | Build fails because helper does not exist. |
| F2 helper implementation | F1 | Extend `include/impl/shot_color.hpp`. | Scalar/RVV match production formula on indexed `PointXYZRGBA` samples, including repeated indices; RVV path records scalar LAB staging boundary. |
| F3 component bench | F2 | Add `color_rgb_lut_indexed_component` to `src/bench_shot.cpp`. | Bench binary builds and case emits time/checksum lines; staging cost is inside timed function. |
| F4 manifest metadata | F3 | Add case metadata in `script/generate_shot_evidence_manifest.py`. | Evidence Doctor classifies the case as diagnostic/test-helper with indexed RGB staging. |
| F5 asm | F3 | `make -C test-rvv/features/shot dump_bench_rvv` plus targeted asm scan. | RVV LAB distance arithmetic / store instructions are visible near helper callsite; LUT conversion remains scalar by design. |
| F6 board evidence | F4 | Targeted board compare with `--case-filter color_rgb_lut_indexed_component`, then Evidence Doctor. | One run gives a bucket; if >=1.10x, add up to 2 reruns; if negative, stop this candidate. |
| F7 docs | F6 | Result, README, matrix, roadmap, evaluation and queue. | Current evidence and next action recover without chat context. |

## Evidence Doctor 和 registry 规则

Manifest wrapper 仍是 `test-rvv/features/shot/script/generate_shot_evidence_manifest.py`。本阶段 evidence role 是
`diagnostic`，A/B boundary 是 `test_helper`，timer boundary 必须说明 scalar RGB/LUT staging 是否计入 RVV candidate。
当前 topic 没有独立 evidence registry（证据登记表），result 使用 run-labelled 目录做人工 freshness check。

## 板卡复跑预算和决策桶

板卡可用。默认先跑 1 次 targeted board compare：

- speedup >= 1.10x 且 doctor 无 Error：最多追加 2 次 rerun。
- speedup < 0.98x：标记 negative，不追加。
- 0.98x <= speedup < 1.10x：标记 neutral / weak，不进入 production probe。
- 方向反转且预算用完：标记 unstable。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic（组件诊断）。 |
| A/B boundary | test helper：indexed RGB/LUT scalar staging + RVV LAB distance helper。 |
| 当前决策问题 | RVV-vs-scalar component A/B 和 implementation-shape。 |
| diagnostic 是否可外推到 production | 仍不能直接外推。它不覆盖 protected estimator state、`std::vector::push_back` 和完整 color interpolation。 |
| comparison-boundary / baseline mismatch 风险 | 存在。RVV candidate 的 LAB staging 是显式临时数组，production 标量路径在同一个 loop 中边转换边 push。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许；只说明该 staging/LUT shape 当前不支持生产探针。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。 |

## 继续 / 停止条件

`stop_condition_hit` 只在继续需要 production 修改、板卡 / 工具不可用、Evidence Doctor Error 无法降级、dirty isolation 不安全，或 roadmap 无授权未阻塞下一动作时成立。

`next_phase_default` 暂定为：若 070 正向，写更窄 direct byte/LUT gather diagnostic 或 PI1 color production-shaped probe plan；若 070 中性 / 负向，回到 interpolation bin-selection / scalar-tail diagnostic。
