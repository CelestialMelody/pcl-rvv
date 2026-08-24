# color_coding 正确性测试字典

本文说明 `src/test_color_coding.cpp` 中每个 TEST 的输入、断言和证明范围。它只负责 correctness（正确性）语义，不承担 bench 统计、板卡性能或 production adoption（生产采纳）结论。

## 测试文件分工

| 文件 | 职责 |
| --- | --- |
| `src/test_color_coding.cpp` | gtest 入口，构造小型颜色样本并断言 reference / candidate 语义。 |
| `include/color_coding.h` | 测试专用聚合头。 |
| `include/impl/color_coding_support.hpp` | reference helper、RVV candidate helper、`ColorPoint`、`EncodedColorData` 和 point-vector helper。 |
| `io/include/pcl/compression/color_coding.h` | production direct tests 调用真实 public methods；phase 080 后不再有 RVV 测试钩子，只验证完整回滚后的标量语义。 |

## 共同输入和断言

component tests 使用 5 个 `ColorPoint`，RGBA byte 值手工构造；production-shaped test 使用 5 个 `pcl::PointXYZRGBA`。共同 leaf indices 为 `{1, 2, 3}`，`color_bit_reduction=1`。断言覆盖：

- average bytes 先做整数平均，再右移 bit reduction。
- differential bytes 使用 `(average ^ color) >> reduction`。
- decode 先把 average 左移，再 XOR diff。
- default color 写入 `0x00ffffff`，不依赖 alpha。
- candidate 与 reference 输出 byte stream 或 RGBA 字段一致。

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `ColorCodingReference.EncodeAverageUsesIntegerAverageBeforeBitReduction` | `ColorPoint` 5 点、indices `{1,2,3}` | `encodeAverageOfPointsReference` | average bytes 等于 `{7,12,17}`，diff 为空 | 标量 reference 的 average 顺序和 bit reduction 语义 | RVV、production direct、性能 |
| `ColorCodingReference.EncodePointsUsesXorDiffBeforeBitReduction` | 同上 | `encodePointsReference` | average 和 9 个 diff bytes 与手算一致 | diff 先 XOR 再右移，且 average 追加到 average stream | vector gather、`push_back` 成本、public entry |
| `ColorCodingReference.DecodePointsRestoresShiftedAverageAndDiffSemantics` | 手写 average/diff stream，输出范围 `[1,4)` | `decodePointsReference` | 输出 RGBA 与 cursor 前进量一致 | decode cursor、average 左移和 diff XOR 语义 | entropy decode、连续大范围性能 |
| `ColorCodingReference.SetDefaultColorWritesWhiteWithoutAlpha` | 4 点输出，范围 `[1,3)` | `setDefaultColorReference` | 只写目标范围，颜色为 white without alpha | default color 语义和范围边界 | RVV store、production fallback |
| `ColorCodingCandidate.CandidateMatchesReferenceForAverageEncodeDecodeAndDefault` | `ColorPoint` 5 点和同一 indices | reference vs candidate | average、diff、decode 输出、default 输出一致 | RVV build 下 candidate 与 reference 的 component helper 等价 | 真实 `pcl::PointXYZRGBA` layout、production direct |
| `ColorCodingProductionShaped.PointXYZRGBAMatchesComponentSemantics` | `pcl::PointXYZRGBA` 5 点和同一 indices | point-vector reference vs candidate；phase 050 还调用 staged-store decode helper | average、diff、direct decode、staged decode、default 与 production-shaped reference 一致 | candidate 能处理真实 PCL RGBA offset、AoS stride，并证明 staged scratch 写回保持 byte stream 语义 | `OctreePointCloudCompression` public entry、fallback gate、性能；staged correctness 不代表 staged 性能正向 |
| `ColorCodingProductionDirect.PointXYZRGBAPublicMethodsMatchExpectedAfterFullRollback` | `pcl::PointXYZRGBA` 64 点同色 leaf | 真实 `ColorCoding` public methods | average、diff、default 语义正确；Std/RVV build 都不要求 RVV dispatch | production public 方法在完整回滚后保持标量语义 | 板卡性能、完整 compression public entry、其它点型 |
| `ColorCodingProductionDirect.ScalarSemanticsRemainForSmallAndNonExactPointTypes` | 小规模 `PointXYZRGBA` 和非 exact `PointXYZRGB` | 真实 `ColorCoding` public methods | encode / default 语义保持 | 小规模和非 exact 点型在完整回滚后仍保持标量语义 | 所有可能 layout / offset / cloud size fallback |

## 边界样本策略

当前 correctness 是小规模 deterministic（确定性）样本，目的是锁住 byte 语义和 RGBA offset。尚未覆盖：

| 缺口 | 状态 | 原因 / 下一步 |
| --- | --- | --- |
| empty leaf / size 1 fallback | partially covered | phase 060 覆盖小规模 leaf fallback；empty leaf 未单独覆盖。 |
| unsupported point type / layout | partially covered | phase 060 覆盖非 exact `PointXYZRGB` fallback；错误 offset 和极大 cloud size 未单独覆盖。 |
| full public compression entry | not recommended now | 当前没有 adopted color coder production RVV；除非新 profile 指向完整压缩入口，否则不继续。 |
| randomized leaf distribution | not_applicable with evidence for diagnostic phase | 当前问题是 component helper 语义和生产形态预检。 |
| NaN / Inf | not_applicable with evidence | color bytes 不使用浮点几何公式。 |

## 验证命令

```bash
make -C test-rvv/io/color_coding run_test_compare
```

期望 Std/RVV 两边都通过 8 个 TEST。QEMU 通过只能说明 correctness 和路径可运行，不能作为性能证据。
