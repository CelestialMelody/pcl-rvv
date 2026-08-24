# Correctness Tests

## 本文职责

本文解释 `src/test_pcie.cpp` 中的 TEST 字典、输入构造和断言边界。性能、反汇编和板卡证据见
`doc/benchmark-and-evidence.zh.md`。

## 测试文件分工

| 文件 / helper | 作用 | 证据角色 |
| --- | --- | --- |
| `src/test_pcie.cpp` | gtest 入口，运行 Std/RVV 输出对拍。 | correctness gate（正确性验收） |
| `include/pcie.h` | 聚合头，暴露 test-only helper。 | 测试支撑入口 |
| `include/impl/pcie_support.hpp` | fixture、标量参考、RVV candidate 和测试专用 production probe gate。 | diagnostic helper |

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `RgbAndRgbaUnpackMatchScalarReference` | `PointXYZRGB(17x13)`、`PointXYZRGBA(19x11)` | `extractRgbCandidate` vs 真实 `PointCloudImageExtractorFromRGBField` | `std::vector<uint8_t>` 完全一致 | v0 RGB/RGBA unpack 语义一致。 | production dispatch、segment-store v1。 |
| `RgbSegmentStoreCandidateMatchesScalarReference` | `PointXYZRGB(31x10)`、`PointXYZRGBA(29x9)` | `extractRgbSegmentStoreCandidate` vs 真实标量 extractor | RGB 字节完全一致 | `vsseg3e8` 候选的通道顺序、tail 和字段 offset 正确。 | production dispatch、性能。 |
| `ScalingModesMatchScalarReference` | `PointXYZI(23x9)` | `extractScalingCandidate` vs `extractScalingScalar` | `uint16_t` 输出完全一致 | no-scaling、fixed-factor、full-range v0 语义一致。 | v0 full-range 性能收益。 |
| `FullRangeReductionCandidateMatchesScalarReference` | `PointXYZI(31x10)`，手工放置 min/max 和非满 tail | `extractScalingFullRangeReductionCandidate` vs 标量 full-range | `uint16_t` 输出完全一致 | `vfredmin` / `vfredmax` 候选的 min/max 和 tail 语义一致。 | NaN/Inf full-range 扩展语义。 |
| `PaintNaNsWithBlackPostPassMatchesScalarReference` | `PointXYZI(9x7)`，一个 NaN 点 | 标量 post-pass vs candidate post-pass | NaN 像素清零一致 | post-pass 仍可保持标量语义。 | RGB NaN、真实 extractor 对所有 encoding 的 post-pass。 |
| `NormalFieldCandidateMatchesScalarReference` | `PointNormal(23x11)` | `extractNormalCandidate` vs 真实 normal extractor | RGB 字节完全一致 | normal_x/y/z 到 `rgb8` 的截断语义、字段顺序和 tail 正确。 | production dispatch、泛型 normal traits、性能。 |
| `LabelMono16CandidateMatchesScalarReference` | `PointXYZL(29x9)`，包含大于 65535 的 label | `extractLabelMono16Candidate` vs 真实 label mono extractor | `uint16_t` 输出完全一致 | label mono16 的字段读取和低 16 位截断语义正确。 | random / Glasbey RGB、production dispatch、性能。 |
| `ProductionProbeGatePolicyMatchesPi1Plan` | exact 和非 exact 点类型、`intensity` / `z` 字段名、full-range / fixed-factor mode | `rgbProductionProbeGate` 和 `scalingProductionProbeGate` | Phase 040 冻结范围 accept，非覆盖范围 reject | PI2 production probe gate 的窄范围策略可失败验收。 | production dispatch、真实 fallback 命中、板卡性能。 |
| `RgbAndRgbaHitProductionRvvPath` | `PointXYZRGB(23x17)`、`PointXYZRGBA(19x13)` | 真实 `PointCloudImageExtractorFromRGBField` | 输出与标量参考一致，RVV hook 命中 | production RGB/RGBA RVV dispatch 和输出语义。 | 泛型 RGB 点型。 |
| `RgbNonExactPointTypeFallsBackToScalar` | `PointXYZRGBL(17x11)` | 真实 RGB extractor | 输出与标量参考一致，scalar hook 命中 | 非 exact RGB 点型 fallback。 | 是否未来适合扩到 RGBL。 |
| `IntensityFullRangeHitsProductionRvvPath` | `PointXYZI(29x13)` | 真实 `PointCloudImageExtractorFromIntensityField` full-range | 输出与标量参考一致，RVV hook 命中 | production intensity full-range RVV dispatch 和输出语义。 | 其它 intensity 点型。 |
| `ScalingGateMissesFallBackToScalar` | fixed-factor intensity、`z` full-range | 真实 scaling extractor | scalar hook 命中 | 非冻结 scaling gate fallback。 | 其它字段的性能。 |
| `LabelMono16HitsProductionRvvPath` | `PointXYZL(31x13)` | 真实 `PointCloudImageExtractorFromLabelField` `COLORS_MONO` | 输出与标量参考一致，RVV hook 命中 | production label mono16 RVV dispatch 和 uint32 -> uint16 截断语义。 | generic label-like 点型、RGB label modes。 |
| `LabelRgbModesStayOnExistingScalarBranches` | `PointXYZL(17x11)` | 真实 label extractor 的 `COLORS_RGB_GLASBEY` 和 `COLORS_RGB_RANDOM` | 输出 encoding 为 `rgb8`，hook 保持 `None` | mono16 RVV gate 不误接管 RGB label modes。 | random RGB 字节稳定性、RGB label mode 性能。 |
| `RvvExtractImplStillUsesBaseNaNPostPass` | `PointXYZI(11x7)`，含 NaN | 真实 intensity extractor + base `extract` | NaN 像素清零且 RVV hook 命中 | RVV `extractImpl` 后仍执行 base NaN post-pass。 | RGB NaN 扩展。 |

## 验证命令

```bash
make run_test_compare
```

当前结果：Std/RVV 各 15 个 TEST 通过。日志路径：

- `log/qemu/run_test_std.log`
- `log/qemu/run_test_rvv.log`

## 边界说明

前 8 个 TEST 是 helper-level 或 production-shaped diagnostic（生产形态诊断）正确性证据。
Phase 080/090 的 7 个 `PointCloudImageExtractorsProductionDirect` TEST 是真实 production direct
（真实生产路径）正确性证据，证明当前工作树中的 production header 已经有窄范围 RVV dispatch
（RVV 分流）、fallback、label RGB mode non-hit 和 NaN post-pass。
