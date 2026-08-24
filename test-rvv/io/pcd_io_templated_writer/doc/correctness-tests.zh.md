# pcd_io_templated_writer 正确性测试说明

## 本文职责

本文解释 `src/test_pcdtw.cpp` 中每个 gtest（Google Test 单元测试）的输入、被测路径、断言和证据边界。
它不承担 bench 统计、板卡性能结论或 production decision（生产判断）。

## 测试文件分工

| 文件 / 符号 | 职责 |
| --- | --- |
| `include/pcdtw.h` | 聚合入口，供 test 和 bench include。 |
| `include/impl/pcdtw_support.hpp` | 测试专用 reference、candidate、fixture 和 compression helper。 |
| `src/test_pcdtw.cpp` | correctness aggregate gtest binary。 |

## 共同输入和断言

测试使用 `makePointMajorCloud` 生成 deterministic（确定性）字节数据。字段描述来自
`FieldLayout{offset, size}`，模拟 `PCDWriter::writeBinaryCompressed<PointT>` 或
`writeBinary<PointT>` 过滤 `_` 字段后的有效字段列表。

主要断言：

- 标量 reference（参考链路）和 RVV candidate（候选链路）输出 byte-equal。
- RVV build 在可覆盖 4 字节字段布局下命中 `PathKind::PackRvv`。
- 非覆盖 mixed field size（混合字段大小）回退到 `PathKind::PackScalarFallback`。
- binary writer candidate 在可覆盖 4 字节字段布局下命中 `PathKind::BinaryRvv`。
- binary writer mixed field size 回退到 `PathKind::BinaryScalarFallback`。
- Phase 080 回滚前 binary writer production probe 曾在真实 `writeBinary<PointT>` 公开入口下验证 RVV 命中、
  mixed-size fallback 和 indices overload 标量行为；回滚后这些 production-direct 测试已移除，当前正确性集合只保留
  compressed public direct、binary component ablation 和 Phase 100 binary tuple production-direct tests。

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明的范围 |
| --- | --- | --- | --- | --- | --- |
| `PackCandidateMatchesScalarForPointXYZRgbLikeLayout` | 257 points，`point_step=16`，四个 4 字节字段 | `packFieldsScalar` vs `packFieldsCandidate` | packed bytes 相等；RVV build 命中 `PackRvv` | 无 padding 的 4 字节字段 AoS 到 field-major pack correctness。 | 不证明 LZF、file write 或真实 public entry。 |
| `CandidateHandlesTrailingPointPadding` | 193 points，`point_step=20`，四个 4 字节字段 | 同上 | packed bytes 相等；RVV build 命中 `PackRvv` | 点尾部 padding 不参与有效字段拷贝。 | 不证明任意 padding / 非对齐字段。 |
| `CandidateFallsBackForMixedFieldSizes` | 31 points，字段大小为 4/2/1/4 | `packFieldsCandidate` fallback | 输出等于标量；lastPath 是 fallback | 非 4 字节字段不会误走 RVV candidate。 | 不证明 production fallback dispatch。 |
| `PackAndCompressCandidateMatchesScalarPayload` | 4096 points，`point_step=16`，四个 4 字节字段 | `packAndCompressScalar` vs `packAndCompressCandidate` | compressed payload byte-equal；RVV build 命中 `PackRvv` | pack+LZF production-shaped helper correctness。 | 不证明真实 file header、mmap、write 或 file lock。 |
| `CompressedPublicWriterMatchesScalarPackAndHitsRvvPath` | 2049 个 `PointXYZRGBA`，真实 `PCDWriter::writeBinaryCompressed` | public writer production direct | 写临时 compressed PCD，解压 payload 后与标量 pack byte-equal；RVV build hook 为 Rvv，Std build hook 为 Scalar | 证明 production patch 的公开入口输出和 RVV 分流。 | 不覆盖 mixed field fallback 或 binary writer。 |
| `CompressedPublicWriterFallsBackForMixedFieldSizes` | 自定义点型含 `std::uint16_t ring` 字段 | public writer fallback | 解压 payload 等于标量 pack；hook 为 Scalar | 非 4 字节字段不会误走 production RVV。 | 不覆盖所有用户自定义点型。 |
| `PackBinaryCandidateMatchesScalarForTrailingPadding` | 257 points，`point_step=20`，四个 4 字节字段 | `packBinaryFieldsScalar` vs `packBinaryFieldsCandidate` | packed binary bytes 相等；RVV build 命中 `BinaryRvv` | binary writer point-major packed output 在 tail padding 下正确。 | 不证明真实 header、mmap、write、sync 或 public dispatch。 |
| `PackBinaryCandidateFallsBackForMixedFieldSizes` | 47 points，字段大小为 4/2/1/4 | `packBinaryFieldsCandidate` fallback | 输出等于标量；lastPath 是 binary fallback | binary writer 非 4 字节字段不会误走 RVV candidate。 | 不证明 production fallback dispatch。 |
| `PackBinaryTupleCandidateMatchesScalarForCompactLayout` | 257 points，`point_step=16`，offset `0/4/8/12` | tuple / segment candidate | packed binary bytes 相等；RVV build 命中 tuple path | Phase 090 test-only tuple path 的 compact correctness。 | 不证明真实 public dispatch。 |
| `PackBinaryTupleCandidateHandlesTrailingPadding` | 257 points，`point_step=20`，offset `0/4/8/12` | tuple / segment candidate | packed binary bytes 相等；RVV build 命中 segment path | Phase 090 test-only segment path 的 padding correctness。 | 不证明真实 public dispatch。 |
| `BinaryTuplePublicWriterMatchesScalarPackAndHitsMemcpyPath` | 真实 `PointXYZRGBA` compact layout | `PCDWriter::writeBinary<PointT>` public writer | payload 与标量 pack byte-equal；hook 命中 memcpy path | Phase 100 compact production direct correctness。 | 不覆盖 padding RVV path 或 indices overload。 |
| `BinaryTuplePublicWriterMatchesScalarPackAndHitsRvvPathForPadding` | 真实 tail-padding point type | `PCDWriter::writeBinary<PointT>` public writer | payload 与标量 pack byte-equal；hook 命中 RVV path | Phase 100 padding production direct correctness 和 path hit。 | 不覆盖 arbitrary layout。 |
| `BinaryTuplePublicWriterFallsBackForMixedFieldSizes` | 自定义 mixed-size point type | binary public writer fallback | payload 与标量 pack byte-equal；hook 为 scalar | 非 4 字节字段不会误走 tuple / segment production path。 | 不覆盖所有 datatype 组合。 |
| `BinaryTupleIndicesOverloadKeepsScalarPath` | compact point type + indices overload | `writeBinary(file_name, cloud, indices)` | payload 顺序符合 indices；hook 不命中 RVV | indices overload 保持标量，不受 Phase 100 dispatch 影响。 | 不提供 indices RVV 性能结论。 |

## 验证命令

```bash
make -C test-rvv/io/pcd_io_templated_writer run_test_compare
```

当前记录：Phase 100 后 Std/RVV build 都运行 15 个测试并通过。QEMU correctness（QEMU 正确性）只能证明构建、
路径和 helper-level / production-direct 功能，不证明目标硬件性能。Phase 080 的 3 个 field-outer
binary production-direct gtest 属于 historical evidence，已随负收益 production patch 回滚移除；Phase 100
新增的 binary tuple production-direct gtest 是当前 adopted path 的正确性证据。
