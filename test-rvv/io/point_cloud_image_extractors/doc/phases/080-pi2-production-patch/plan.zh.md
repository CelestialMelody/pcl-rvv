# Phase 080 Plan: PI2 production patch

## 阶段意图和边界

本阶段按 Phase 040/050 冻结范围推进 production integration loop（生产接入闭环）的
PI2-PI5。允许修改的 production 文件只有
`io/include/pcl/io/impl/point_cloud_image_extractors.hpp`；不修改
`io/include/pcl/io/point_cloud_image_extractors.h` 的公开 / protected API。

本阶段只接两个已有正收益候选：

- `rgb_segment_store_v1`：exact `PointXYZRGB` / `PointXYZRGBA` 的 RGB/RGBA 字段转 `rgb8`。
- `scaling_reduction_v1`：exact `PointXYZI`、`field_name == "intensity"`、
  `SCALING_FULL_RANGE` 的 intensity 转 `mono16`。

`label_mono16_stride_v0` 虽然在 Phase 070 为正向诊断结果，但未进入 Phase 040/050 冻结
PI2 范围。本阶段不把 label 接入 production，也不修改 normal / random label / Glasbey
分支。

## 当前状态清单

| item | state | evidence |
| --- | --- | --- |
| RGB diagnostic | positive | Phase 020 repeated board：`rgb_segment_store_v1` median 1.74x / 1.75x，asm 可见 `vsseg3e8.v`。 |
| scaling diagnostic | positive | Phase 010 repeated board：`scaling_reduction_v1` median 1.54x，asm 可见 `vfredmin.vs` / `vfredmax.vs`。 |
| gate policy | frozen | Phase 050 `ProductionProbeGatePolicyMatchesPi1Plan` 已固化 exact 点型范围。 |
| production source | scalar-only | `PointCloudImageExtractorFromRGBField::extractImpl` 和 `PointCloudImageExtractorWithScaling::extractImpl` 当前无 `__RVV10__` 分流。 |
| user authorization | granted for loop | 用户表示“如果有收益即可采纳”；本阶段可推进 PI2-PI5，但 PI5 后仍需用户确认保留 / 采纳。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 010/020 为 `production_shaped_diagnostic`；本阶段必须补 `production-public`。 |
| A/B boundary | 诊断 A/B 是 `test_helper`；PI4 bench 必须走真实 `PointCloudImageExtractor*` 公开入口。 |
| 当前决策问题 | `RVV-vs-scalar` 的有界 production probe；不做 `RVV-family-selection` clean adoption。 |
| diagnostic 是否可外推到 production | 只能支持进入有界 production probe；最终以 PI4 production-public 证据为准。 |
| comparison-boundary / baseline mismatch 风险 | 存在真实 `PCLImage` metadata、resize、field lookup、base `extract` organized check 和 NaN post-pass 差异。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已允许进入本阶段；若 PI4 证据不成立，PI5 停在用户确认回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前不是实现族选择，只能得出“bounded production candidate”；用户确认后才可视为 adopted behavior。 |

## 实现和测试动作

| action | artifact | expected evidence | completion |
| --- | --- | --- | --- |
| RED production-direct tests | `src/test_pcie.cpp` | RVV build 先因 production hook / path-hit 缺失失败；Std build 不要求 RVV path-hit。 | RED failure observed before production patch。 |
| production helper split | `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` | 原标量循环抽成 `detail::*Std`，public `extractImpl` 只做 field lookup、RVV short-circuit 和 Std fallback。 | 非 RVV 构建自然走 Std。 |
| RVV RGB path | same production header | exact RGB/RGBA + field metadata gate 命中时使用 `vlse32` + `vsseg3e8` 写 `PCLImage::data`。 | production-direct output equal，RVV hook hit。 |
| RVV scaling path | same production header | exact `PointXYZI/intensity/full-range` 使用 `vfredmin/vfredmax` 找范围，再写 `mono16`。 | production-direct output equal，RVV hook hit。 |
| production bench labels | `src/bench_pcie.cpp`、manifest script | bench 通过真实 extractor public entry，输出 checksum 和 case label。 | QEMU smoke、asm、board repeated 可解析。 |
| docs / matrix update | phase result、matrix、roadmap、evaluation、topic role docs | PI5 证据和用户确认边界写清。 | final stop at PI5 checkpoint。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `production_rgb_segment_store_v1` | organized cloud order | exact `PointXYZRGB/RGBA` / RGB field offset | `PointCloudImageExtractorFromRGBField::extract` public entry | planned production-direct output + path-hit / fallback tests | planned `production_rgb_*_640x480` | planned repeated PI4 | `extractRgbFieldRVV` | planned | planned |
| `production_scaling_reduction_v1` | organized cloud order | exact `PointXYZI::intensity` / float / full-range | `PointCloudImageExtractorFromIntensityField::extract` public entry | planned production-direct output + path-hit / fallback tests | planned `production_scaling_full_range_intensity_640x480` | planned repeated PI4 | `extractScalingFullRangeIntensityRVV` | planned | planned |

## Point type expansion queue

| queue item | status | resume condition |
| --- | --- | --- |
| RGB traits-gated generic color types | deferred | 当前 exact production probe 经 PI5 和用户确认后，另建 point-type expansion phase。 |
| `PointXYZRGBL` RGB fallback -> possible RGB production | deferred | 审计 RGB + label 复合语义，并补独立 production direct / board evidence。 |
| `PointXYZINormal` intensity | deferred | exact `PointXYZI` production probe 成立后，审计 intensity offset 和 POD layout。 |
| label mono16 production probe | deferred | 用户明确扩大范围或另建 label PI1 plan。 |

## 板卡复跑预算和决策桶

- repeated board budget：5 runs。
- bench 参数：`--iterations 20 --warmup-iterations 3`。
- production labels：`production_rgb_pointxyzrgb_640x480`、
  `production_rgb_pointxyzrgba_640x480`、
  `production_scaling_full_range_intensity_640x480`。
- positive：每个 production label median >= 1.20x 且 min > 1.05x，Evidence Doctor 无 Error。
- weak-positive：1.05x <= median < 1.20x，交给 PI5 用户确认是否保留。
- neutral / negative / unstable：PI5 停在用户确认回滚或补充诊断。

## Continue / Stop 条件

本阶段若 production direct correctness、asm、board repeated 和 Evidence Doctor 都闭合，必须停在
PI5 用户检查点：保留 production diff，报告证据，等待用户明确确认采纳 / 保留或授权回滚。用户
确认前不更新长期 `doc-rvv` 为 adopted production behavior，不创建 commit。
