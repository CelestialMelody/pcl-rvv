# Optimization Evidence

## 本文职责

本文索引 candidate family（候选实现族）的代码、测试、bench、board、asm 和 decision（决策）。
跨阶段搜索空间仍归 `doc/optimization-roadmap.zh.md`；阶段流水归 `doc/phases/**`。

## 当前结论摘要

| candidate family | 状态 | 结论 |
| --- | --- | --- |
| `rgb_segment_store_v1` | partial-production-candidate within diagnostic boundary | RGB/RGBA 优先 production probe。 |
| `rgb_u32_stride_unpack_v0` | positive fallback implementation family | 正向，但弱于 v1。 |
| `scaling_reduction_v1` | partial-production-candidate within diagnostic boundary | full-range scaling 优先 production probe。 |
| `scaling_float_stride_v0` full-range | rejected | repeated board 5/5 退化，Doctor Error 指向该 label。 |
| `scaling_float_stride_v0` fixed-factor | weak-positive / deferred | 只有弱正向，不单独触发 production probe。 |
| `pi2_gate_policy_test_support` | adopted as test support | 固化 Phase 040 PI2 gate 策略，但不产生 production dispatch。 |
| `normal_float_stride_v0` | rejected within diagnostic boundary | correctness 通过，但 Phase 060 repeated board median 0.61x 且 5/5 退化。 |
| `label_mono16_stride_v0` | diagnostic positive; promoted | Phase 070 repeated board median 1.21x，Doctor 无 finding；Phase 090 已补 production-public 证据。 |
| `production_rgb_segment_store_v1` | adopted | 真实 RGB/RGBA public entry repeated board median 1.54x / 1.55x，Doctor 无 finding。 |
| `production_scaling_reduction_v1` | adopted | 真实 intensity full-range public entry repeated board median 1.52x，Doctor 无 finding。 |
| `production_label_mono16_stride_v0` | weak-positive adopted | 真实 label `COLORS_MONO` public entry repeated board median 1.08x，min 1.05x，max 1.09x，Doctor 无 finding。 |
| label random / Glasbey | deferred | 状态结构和 LUT 成本不适合当前阶段。 |

## 优化方式总表

| candidate | helper | correctness | bench label | board evidence | asm evidence | decision |
| --- | --- | --- | --- | --- | --- | --- |
| `rgb_segment_store_v1` | `extractRgbSegmentStoreRvv` | `RgbSegmentStoreCandidateMatchesScalarReference` | `rgb_segment_store_pointxyzrgb_640x480`, `rgb_segment_store_pointxyzrgba_640x480` | median 1.74x / 1.75x | `vsseg3e8.v` | PI1 preferred RGB candidate |
| `rgb_u32_stride_unpack_v0` | `extractRgbRvv` | `RgbAndRgbaUnpackMatchScalarReference` | `rgb_unpack_pointxyzrgb_640x480`, `rgb_unpack_pointxyzrgba_640x480` | median 1.29x / 1.31x | `vlse32.v` | fallback implementation family |
| `scaling_reduction_v1` | `extractScalingFullRangeReductionRvv` | `FullRangeReductionCandidateMatchesScalarReference` | `scaling_full_range_reduction_intensity_640x480` | median 1.56x | `vfredmin.vs`, `vfredmax.vs` | PI1 preferred scaling candidate |
| `scaling_float_stride_v0` | `extractScalingRvv` | `ScalingModesMatchScalarReference` | `scaling_full_range_intensity_640x480`, `scaling_fixed_factor_intensity_640x480` | full-range median 0.92x; fixed-factor median 1.08x | `vlse32.v`, float arithmetic | full-range rejected; fixed-factor deferred |
| `pi2_gate_policy_test_support` | `rgbProductionProbeGate`, `scalingProductionProbeGate` | `ProductionProbeGatePolicyMatchesPi1Plan` | not_applicable | not_applicable | not_applicable | test support only; no EvidenceDecision change |
| `normal_float_stride_v0` | `extractNormalRvv` | `NormalFieldCandidateMatchesScalarReference` | `normal_field_pointnormal_640x480` | median 0.61x, 5/5 below 1.0 | `vlse32.v`, `vfadd.vf`, `vfmul.vf` | rejected within diagnostic boundary; not in current PI2 scope |
| `label_mono16_stride_v0` | `extractLabelMono16Rvv` | `LabelMono16CandidateMatchesScalarReference` | `label_mono16_pointxyzl_640x480` | median 1.21x, min 1.18x, max 1.27x | `vlse32.v`, `vnsrl.wi`, `vse16.v` | diagnostic positive; promoted by Phase 090 |
| `production_rgb_segment_store_v1` | `extractRgbFieldRVV` | production direct RGB/RGBA path-hit tests | `production_rgb_pointxyzrgb_640x480`, `production_rgb_pointxyzrgba_640x480` | median 1.54x / 1.55x | `vlse32.v`, `vsseg3e8.v` | adopted |
| `production_scaling_reduction_v1` | `extractScalingFullRangeIntensityRVV` | production direct intensity path-hit / fallback / NaN post-pass tests | `production_scaling_full_range_intensity_640x480` | median 1.52x | `vlse32.v`, `vfredmin.vs`, `vfredmax.vs` | adopted |
| `production_label_mono16_stride_v0` | `extractLabelMono16FieldRVV` | production direct label path-hit / RGB mode non-hit tests | `production_label_mono16_pointxyzl_640x480` | median 1.08x, min 1.05x, max 1.09x | `vlse32.v`, narrow, `vse16.v` | weak-positive adopted |

## 标量路径与 RVV 路径差异

- RGB 标量参考通过真实 `PointCloudImageExtractorFromRGBField` 生成 `PCLImage` 数据；RVV diagnostic 直接写 `std::vector<uint8_t>`，因此仍存在 production mismatch（生产错配）。
- `rgb_segment_store_v1` 与 v0 的主要差异是输出写回：v0 使用 scratch + per-lane store，v1 使用 `vsseg3e8`。
- full-range scaling v0 的第一遍 min/max 仍把 chunk 存到 scratch 后标量扫描；v1 用 `vfredmin` / `vfredmax` 在向量寄存器中规约。
- normal v0 使用三路 float stride load，再把每个 chunk 存回 scratch 后逐 lane 转 `uint8_t`。Phase 060
  的负向结果说明该形态在当前 board 和 test-helper 计时边界下不值得纳入当前 PI2。
- label mono16 v0 使用 `vlse32` 跨步读取 `PointXYZL::label`，用向量窄化保留低 16 位，再写 `uint16_t`。
  它没有覆盖 random / Glasbey 的 map/set 分支。

## 结论边界

Phase 080/090 已补齐 RGB/scaling/label mono16 的 production-public（公开入口生产证据）：public entry
范围、fallback、点类型 gate、production direct correctness、production asm 和 board repeated 均已闭合。
当前结论是 adopted production behavior（已采用生产行为）。label mono16 属于 weak-positive，但实现小、
fallback 简单，按本阶段计划和用户确认采纳。label random / Glasbey、normal field 和泛型点型仍未覆盖。
