# Phase 060 result: color LAB distance component diagnostic

## 阶段结论

本阶段完成 test-only color LAB distance component（测试专用颜色 LAB 距离组件）诊断，没有修改
`features/include/pcl/features/impl/shot.hpp`。新增 helper 只覆盖 `SHOTColorEstimation::computePointSHOT`
中 RGB2CIELAB 之后的归一化 L/a/b 距离算术和 `binDistanceColor` 写出：

```text
(abs(LRef - L) + (abs(aRef - a) + abs(bRef - b)) / 2) / 3
clamp 到 [0, 1] 后乘 nr_color_bins
```

EvidenceDecision（证据决策）：`partial-production-candidate / arithmetic-only`。三次 targeted board run
稳定正向，约 1.10x-1.23x；但它不覆盖 RGB2CIELAB LUT（查找表）、`PointXYZRGBA` indexed gather
（按索引离散加载）、`std::vector::push_back` 或完整 color interpolation（颜色插值）。因此它只支持下一阶段
更接近 production 的 color path diagnostic（颜色路径诊断），不能直接进入 production patch（生产补丁）。

## 计划动作回填

| action | status | command / evidence | result |
| --- | --- | --- | --- |
| F1 red test | done | `make -C test-rvv/features/shot run_test_compare` | 缺少 `computeColorBinDistanceScalar/RVV` 时 Std 编译失败，红灯符合预期。 |
| F2 helper implementation | done | `include/impl/shot_color.hpp`、`include/shot.h` | 新增 scalar reference 和 RVV candidate。初版 RVV 先 float 乘 bins 再 widen，RVV 测试暴露 1e-7 级差异；已改为先 widen 到 double 再乘 bins，保持 production-shaped same-chain。 |
| F3 component bench | done | `src/bench_shot.cpp` | 新增 `color_lab_distance_component` case，输出 time/checksum。 |
| F4 manifest metadata | done | `script/generate_shot_evidence_manifest.py` | 新增 `color_lab_distance_component` metadata，Evidence Doctor 能识别为 diagnostic / test-helper boundary。 |
| F5 asm | done | `make -C test-rvv/features/shot dump_bench_rvv` | asm 可见 `vfsub`、`vfabs`、`vfadd`、`vfmax`、`vfmin`、`vfwcvt` 和 `vse64` 组合，归属到 test-only helper / bench callsite。 |
| F6 board evidence | done | targeted `run_board_bench_compare fetch_board_logs` ×3 | 1.22x、1.10x、1.23x，checksum 一致；预算用完且 bucket 未反转。 |
| F7 docs | done | 本 result、phase README、matrix、roadmap、evaluation、README 和队列表 | 当前结论、证据边界和后续路线已同步。 |

## Correctness（正确性）

TDD red / green 已闭合。`run_test_compare` 在新增 helper 后通过 Std / RVV 两侧各 10 个 gtest。新增测试
`ShotColorLabDistanceComponent.RvvMatchesScalarReferenceForNormalizedLabArrays` 覆盖：

- 连续 normalized LAB arrays（归一化 LAB 数组）；
- reference L/a/b 与邻域 L/a/b 的绝对值距离；
- `[0, 1]` clamp（夹紧）边界；
- float 中间值和 double 输出的 same-chain（同构链路）顺序。

该测试只证明 color LAB distance arithmetic helper 的正确性，不证明 `RGB2CIELAB` 或 SHOT1344 公开入口生产分流。

## 反汇编归属

`dump_bench_rvv` 通过。`build/asm/riscv/bench_shot_rvv.asm` 中可见本候选需要的 RVV 指令形态：

| 指令 | 证据含义 |
| --- | --- |
| `vfsub` / `vfabs` | 三个 LAB 通道批量差值和绝对值。 |
| `vfadd` | 合并 L/a/b 距离项。 |
| `vfmax` / `vfmin` | clamp 到 `[0, 1]`。 |
| `vfwcvt.f.f.v` | float distance 拓宽到 double bin output。 |
| `vse64.v` | 写回 double `binDistanceColor`。 |

该归属限定在 test-only helper / bench callsite；production `SHOTColorEstimation::computePointSHOT` 未命中 RVV。

## Board evidence（板卡证据）

| run label | Std avg | RVV avg | speedup | doctor |
| --- | ---: | ---: | ---: | --- |
| `phase060_color_lab_distance_rerun0` | 0.4820 ms | 0.3957 ms | 1.22x | historical targeted run |
| `phase060_color_lab_distance_rerun1` | 0.4393 ms | 0.3982 ms | 1.10x | historical targeted run |
| `phase060_color_lab_distance_rerun2` | 0.4416 ms | 0.3604 ms | 1.23x | Errors=0, Warnings=1, Suggestions=0 |

当前 doctor Warning 是 `low_run_count`：manifest 只解析到最后一次 run 的 1 个 repeated value。处理动作是保留三次
run-labelled 目录作为人工 repeated evidence（重复板卡证据），但不把本组件写成 strong production performance（强生产性能）结论。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component diagnostic（组件诊断）。 |
| A/B boundary | test helper / normalized LAB arrays component bench。 |
| 当前决策问题 | RVV-vs-scalar component A/B 和 implementation-shape。 |
| diagnostic 是否可外推到 production | 不能直接外推。它不含 RGB2CIELAB LUT、indexed `PointXYZRGBA` load、`std::vector::push_back` 和后续 interpolation。 |
| comparison-boundary / baseline mismatch 风险 | 存在。production 标量路径在同一循环里完成 RGB 转换和 push，测试 helper 使用预归一化连续数组和预分配输出。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段结果正向，但仍不允许直接 production probe；只允许作为下一阶段 RGB/LUT 或 color production-shaped diagnostic 的输入证据。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。当前证据不足以 clean-adopt。 |

## 矩阵更新

`color LAB distance RVV` 从 `planned` 更新为 `partial-production-candidate / arithmetic-only`：

- validated scope（已验证范围）：连续 float L/a/b 输入、单个 reference LAB、`nr_color_bins=30`、double 输出、clamp 边界。
- unvalidated scope（未验证范围）：RGB2CIELAB LUT、`PointXYZRGBA` indexed gather、protected estimator state、`std::vector::push_back`、完整 SHOT1344 color interpolation、production direct。
- next phase：如果 production 仍未授权，默认继续 `070-color-rgb-lut-indexed-diagnostic`，评估 RGB/LUT + indexed RGBA 的生产形态错配；如果用户授权 production，仍优先从已写好的 PI1 shape-bin indexed plan 进入，不把 060 直接插入 production。

## 继续 / 停止判断

`stop_condition_hit`：未命中。当前 phase 已闭合，但 roadmap 仍有授权范围内的 test-only 下一动作。

`next_phase_default`：`070-color-rgb-lut-indexed-diagnostic`。该阶段应保留 production 未修改边界，先评估 RGB2CIELAB + indexed `PointXYZRGBA` 访问是否吞掉 060 的算术收益；若该阶段成本过高或 correctness 风险大，再回到 interpolation bin-selection / scalar-tail diagnostic。
