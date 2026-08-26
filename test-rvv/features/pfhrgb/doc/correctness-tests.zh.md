# PFHRGB Correctness Tests

本文解释 `src/test_pfhrgb.cpp` 中 gtest 的输入、断言和证明范围。性能、反汇编和 board repeated
不归本文负责。

## 测试文件分工

| 文件 | 职责 |
| --- | --- |
| `src/test_pfhrgb.cpp` | gtest 入口，验证 reference、production exact path、fallback、pair-batch candidate、public-shaped wrapper 和 reusable workspace。 |
| `include/impl/pfhrgb_fixtures.hpp` | 构造 synthetic `PointXYZRGBNormal` 点云、法线、邻域和输出比较。 |
| `include/impl/pfhrgb_reference.hpp` | 测试专用 scalar reference（标量参考链路），复刻 production PFHRGB helper 语义。 |
| `include/impl/pfhrgb_pair_batch_candidate.hpp` | 测试专用 RVV candidate（候选实现）和 public-shaped wrappers；接入后只作为诊断 / 实现形态背景。 |
| `include/pfhrgb.h` | 聚合入口，供 test / bench 源码引用。 |

## 共同输入和断言

所有当前测试都使用 `pcl::PointXYZRGBNormal`、`Scalar=float`、AoS xyz / normal / rgb 字段和
`nr_split=5`。主要断言是 descriptor bins（描述子直方图区间）在容差内一致，并且输出大小、有限值和
histogram（直方图）语义没有被 RVV path（RVV 链路）破坏。

## TEST 字典

| TEST | 被测路径 | 断言 | 证明范围 | 不证明范围 |
| --- | --- | --- | --- | --- |
| `PFHRGBReference.ComputesPublicDescriptorLikeProductionHelper` | `computePointPFHRGBReference` 对拍 `PFHRGBEstimation::compute` 第一个 descriptor。 | bins 数值接近，证明 reference 复刻有向 pair order、RGB ratio、bin clamp 和 scatter 顺序。 | scalar reference 可作为后续 oracle。 | 不证明 RVV 性能或泛型点型。 |
| `PFHRGBProduction.ExactPointTypePublicEntryMatchesScalarReference` | exact `PointXYZRGBNormal -> PointXYZRGBNormal -> PFHRGBSignature250` 的 `PFHRGBEstimation::compute`。 | 所有输出 descriptor 与 scalar reference 接近。 | 接入后的 production exact path（精确点型生产路径）在公开 KSearch 入口下保持 descriptor 语义。 | 不证明非 exact 点型、非默认 `nr_split` 或 radius search。 |
| `PFHRGBProduction.NonExactSourcePointTypeKeepsScalarFallbackSemantics` | `PointXYZRGB` source + `PointXYZRGBNormal` normals 的非 exact source 模板实例。 | 输出与 mixed-point scalar reference 一致。 | exact gate 未覆盖的 source 点型保持标量 fallback（回退路径）语义。 | 不覆盖所有 fallback 原因，例如非默认 bin 数、小规模邻域和非 RVV build。 |
| `PFHRGBCandidate.PairBatchRVVComputesHistogramCloseToReference` | `computePointPFHRGBSignaturePairBatchRVV`。 | candidate histogram 接近 reference。 | fixed neighborhood pair-batch RVV 数值语义正确。 | 不证明 helper-only 性能；当前 board rerun 已显示该 case 退化。 |
| `PFHRGBCandidate.PublicShapedCandidateComputesDescriptorsCloseToEstimator` | `computePublicPFHRGBWithPairBatchCandidate`。 | public-shaped wrapper 输出 descriptor 与真实 estimator 接近。 | KSearch 外层循环 + candidate helper 的 topic-local 公开入口形态正确。 | 不证明真实 production `computeFeature` 已分流。 |
| `PFHRGBCandidate.ReusablePublicShapedCandidateComputesDescriptorsCloseToEstimator` | `computePublicPFHRGBWithReusablePairBatchCandidate`。 | 复用 `PairBatchWorkspace` 后输出 descriptor 与真实 estimator 接近。 | reusable workspace 没有破坏邻域顺序、staging 或 histogram 写回；生产实现采用了同类 workspace 复用思路。 | 不证明当前 production 采纳收益；收益以接入后的 `public_pfhrgb_k` board 为准。 |

## 边界样本状态

当前 fixture 覆盖 dense finite synthetic grid 和固定 KSearch 邻域。颜色除零、zero-distance、zero-cross
语义通过 candidate helper 内部 fallback 保持与 `computeRGBPairFeatures` 一致。当前已补一个非 exact source
production fallback test；非默认 `nr_split`、小规模邻域、非 RVV build 和其它点型组合仍没有独立拆成每个
fallback 原因的 gtest。若继续 point-type expansion 或 fallback matrix 扩展，需要新增对应 production direct
correctness。

## 验证命令

| 命令 | 作用 |
| --- | --- |
| `make -C test-rvv/features/pfhrgb run_test_std` | 运行标量构建 gtest。 |
| `make -C test-rvv/features/pfhrgb run_test_rvv` | 运行 RVV 构建 gtest。 |
| `make -C test-rvv/features/pfhrgb run_test_compare` | 当前 correctness aggregate，顺序运行 Std/RVV 两侧。 |
