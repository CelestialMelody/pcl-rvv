# point_coding 正确性测试说明

默认入口是：

```bash
make run_test_compare
```

该 target 分别构建 Std / RVV binary。Phase 070 后，Std / RVV 各 11 个 gtest 通过。Std build 证明标量语义没有被生产补丁改变；RVV build 覆盖测试专用 candidate（候选实现）、production-shaped diagnostic（生产形态诊断）和真实 `PointCoding<PointT>::decodePoints` production dispatch（生产分流）。

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `EncodeCandidateMatchesScalarReference` | 257 点合成 cloud，10 个非连续 indices。 | source-indexed leaf encode。 | candidate 输出 byte vector 与标量参考完全相同。 | RVV gather + same-chain quantize（同构量化链路）在普通 indices 下保持 production 公式语义。 | 不覆盖真实 `PointCoding` 对象状态和 `push_back` 成本。 |
| `EncodeCandidateClampsLikeScalarReference` | 6 个对抗点，覆盖超出 `[-127,127]` 和边界截断。 | encode clamp。 | candidate 输出 byte vector 与标量参考完全相同。 | clamp 和 `static_cast<int>` 截断语义一致。 | 不证明完整 f32 RVV 量化公式安全。 |
| `DecodeCandidateMatchesScalarReference` | 513 点 synthetic encoded diff。 | contiguous decode。 | 每个点 `x/y/z` 与标量参考 `EXPECT_FLOAT_EQ`。 | diff byte 扩展、`+0.5`、乘 resolution 和写回 AoS 字段一致。 | 不覆盖 production iterator 生命周期。 |
| `DecodeCandidateMatchesPointCodingObjectState` | 513 点 diff，`begin_index=5`，输出 cloud 带 sentinel padding（哨兵填充）。 | production-shaped decode context。 | 全 cloud `x/y/z` 与真实标量对象输出一致。 | begin/end offset 和对象状态 reference 语义保持。 | 不证明真实 production dispatch 或完整 decompression pipeline。 |
| `DecodeCandidateMatchesMultiLeafObjectState` | 1537 点 diff，多 leaf counts 和多个 reference point。 | multi-leaf production-shaped decode context。 | 全 cloud `x/y/z` 与真实 `PointCoding<PointXYZ>` 多次调用输出一致。 | 多 leaf point-count staging 和 byte offset 推进保持对象状态语义。 | 不证明 tree traversal、entropy decoding 或 stream input。 |
| `DecodePointXYZMatchesIndependentScalarReference` | 513 点 diff，`begin_index=5`。 | production-direct `PointCoding<PointXYZ>::decodePoints`。 | 与独立标量公式逐点 `EXPECT_FLOAT_EQ`。 | Phase 060 exact `PointXYZ` 生产入口语义保持。 | 不证明其它点型。 |
| `DecodePointXYZKeepsDoubleReferenceRounding` | 3 点对抗 diff，reference 含 f32 quick path 会产生末位差异的 double 值。 | production-direct `PointCoding<PointXYZ>::decodePoints`。 | 与独立标量公式逐点一致。 | f64 vector arithmetic（双精度向量运算）保持 production double reference rounding。 | 不证明 encode quantize。 |
| `DecodePointXYZIKeepsExtraFieldWithTraitsGate` | 129 点 `PointXYZI` cloud，保留 `intensity` sentinel。 | traits-gated production `PointCoding<PointXYZI>::decodePoints`. | `x/y/z` 符合标量公式，`intensity` 不变。 | Phase 070 traits gate 命中，额外字段不被 RVV store 覆盖。 | 不证明所有自定义点型。 |
| `DecodePointXYZRGBKeepsColorFieldsWithTraitsGate` | 257 点 `PointXYZRGB` cloud，保留 `r/g/b` sentinel。 | traits-gated production `PointCoding<PointXYZRGB>::decodePoints`。 | `x/y/z` 符合标量公式，`r/g/b` 不变。 | Phase 070 color representative 证明 traits offset / stride 写回不破坏 color 字段。 | 不证明 RGBA 或非标准布局点型的性能。 |
| `DecodeDoubleXYZFallsBackToScalarSemantics` | 65 点已注册 double `x/y/z` 点型，保留 `untouched` sentinel。 | non-compatible traits fallback。 | `x/y/z` 符合标量公式，`untouched` 不变。 | RVV build 下已注册但非 single-float xyz 的点型不会误走 RVV helper。 | 不证明所有自定义点型的性能。 |
| `RoundtripSmokeProducesFiniteOutput` | 128 点 synthetic `PointXYZ` cloud，使用无 color compression profile。 | public `OctreePointCloudCompression<PointXYZ>` encode/decode roundtrip。 | 输出点数等于输入、压缩流非空、输出 xyz 有限且 checksum 非零。 | 公开压缩/解压入口可构造。 | 不证明 RVV performance、坐标逐点误差或其它点型。 |

## 正确性风险

完整 f32 RVV encode 量化公式已暂缓。production 标量表达式会将 `float` 坐标与 `double` reference 混合后进入 double 除法，再 `static_cast<int>` 截断；纯 f32 RVV 链路在靠近整数边界时可能产生差 1 的量化结果。当前 encode candidate 只把 indexed gather 向量化，再用 `encodeOneScalar` 保持同构量化语义。
