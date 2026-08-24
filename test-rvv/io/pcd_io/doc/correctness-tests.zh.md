# pcd_io 正确性测试说明

本文解释 `src/test_pcd_io.cpp` 中 gtest（Google Test 单元测试）的输入、断言和证明范围。性能统计、case-filter
和 board summary 见 `benchmark-and-evidence.zh.md`。

## 测试文件分工

| 文件 / helper | 职责 |
| --- | --- |
| `src/test_pcd_io.cpp` | 运行 test-only correctness，比较 scalar reference（标量参考链路）和 RVV candidate（候选链路）。 |
| `include/impl/pcd_io_support.hpp` | 提供 `FieldLayout`、输入构造、pack/unpack scalar、candidate、writer payload 和 reader payload helper。 |
| `include/pcd_io.h` | 稳定聚合入口。 |

## 共同输入和断言

测试使用 synthetic `PCLPointCloud2`-like byte buffer（合成二进制点云字节缓冲）。`makeInterleavedCloud`
按 point index、field index 和 byte index 写入确定性字节。断言使用 byte-for-byte equality（逐字节相等）。

当前没有浮点误差预算，因为 pack/unpack 和 payload helper 只搬运 byte、调用现有 LZF，并用 `std::isfinite`
复刻 dense scan（稠密扫描）判断。checksum（校验和）只用于 bench 输出，不作为 gtest 的主断言。

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `PCDIOComponentAblation.PackCandidateMatchesScalarForFourByteFields` | 257 points，`point_step=16`，4 个 4-byte field | `packFieldsScalar` vs `packFieldsCandidate` | packed bytes 相同；RVV build 命中 `PackRvv` | 4 字节连续 field pack 与 scalar 语义一致。 | 不含 LZF、header、ostream、production dispatch。 |
| `PCDIOProductionWriter.PublicOstreamPayloadMatchesScalarOracleAndHitsRvvPath` | 128 points，4 个 4-byte PCL fields | public `PCDWriter::writeBinaryCompressed(std::ostream&, ...)` | compressed payload bytes 与 scalar oracle 相同；RVV build 命中 `WriterCompressedPath::Rvv` | production public ostream writer 的 4-byte aligned field pack 语义一致并命中 RVV。 | 不覆盖 filename overload、templated writer 或 non-4-byte fields。 |
| `PCDIOProductionWriter.PublicOstreamFallsBackForMixedFieldSizes` | 64 points，field sizes = 4 / 2 / 1 / 4 | public writer fallback | payload bytes 与 scalar oracle 相同；path 为 `Std` | mixed field sizes 不误走 production RVV helper。 | 不提供 RVV 性能结论。 |
| `PCDIOProductionWriter.PublicOstreamFallsBackForUnalignedPointStep` | 48 points，4 个 4-byte fields，`point_step=18` | public writer fallback | payload bytes 与 scalar oracle 相同；path 为 `Std` | unaligned `point_step` 回退标量。 | 不覆盖所有异常 PCD layout。 |
| `PCDIOProductionWriter.PublicOstreamFallsBackForUnalignedFieldOffset` | 48 points，4 个 4-byte fields，offset 非 4 字节对齐 | public writer fallback | payload bytes 与 scalar oracle 相同；path 为 `Std` | unaligned field offset 回退标量。 | 不覆盖所有异常 PCD layout。 |
| `PCDIOProductionWriter.PublicOstreamIgnoresPaddingField` | 96 points，含 `_` padding field，4 个有效 4-byte fields | public writer padding case | payload bytes 与 scalar oracle 相同；RVV build 命中 `Rvv` | `_` padding field 被过滤，剩余有效字段满足 gate 时可 RVV。 | 不覆盖 non-4-byte 有效字段。 |
| `PCDIOComponentAblation.UnpackCandidateMatchesScalarWithTrailingPadding` | 193 points，`point_step=20`，尾部 padding | `unpackFieldsScalar` vs `unpackFieldsCandidate` | interleaved bytes 相同；RVV build 命中 `UnpackRvv` | 带尾部 padding 的 4 字节 field unpack 与 scalar 语义一致。 | 不含 finite scan 和 reader production path。 |
| `PCDIOComponentAblation.CandidateFallsBackForMixedFieldSizes` | 31 points，field sizes = 4 / 2 / 1 / 4 | `packFieldsCandidate` fallback | bytes 相同；path 为 `PackScalarFallback` | mixed field sizes 不误走 4-byte RVV candidate。 | 不覆盖 production fallback gate。 |
| `PCDIOWriterShapedDiagnostic.CompressedPayloadCandidateMatchesScalar` | 128 points，4 个 4-byte field | `makeCompressedWriterPayloadScalar` vs `makeCompressedWriterPayloadCandidate` | compressed payload bytes 相同；RVV build 命中 `PackRvv` | writer payload 的 pack + LZF + 8-byte size header 与 scalar 一致。 | 不证明 public `writeBinaryCompressed` 已分流。 |
| `PCDIOReaderShapedDiagnostic.CompressedReaderBodyCandidateMatchesScalarAndDenseFlag` | 128 points，3 个 `FLOAT32` finite 字段 + 1 个 payload 字段，单点注入 NaN | `readCompressedBodyScalar` vs `readCompressedBodyCandidate` | unpacked bytes 相同；dense flag 均为 false；RVV build 命中 `UnpackRvv` | reader payload 的 LZF 解压、unpack 和标量 finite scan 与 scalar 一致。 | 不证明 public `readBodyBinary` 或 finite scan RVV。 |

## 边界策略

| boundary | 当前状态 | 后续要求 |
| --- | --- | --- |
| empty data | production helper 回退 Std，保留原 empty payload 语义 | 由源码 fallback 说明；未单独构造 gtest。 |
| empty fields | 过滤后有效字段为空时回退 Std | 由源码 fallback 说明；未单独构造 gtest。 |
| unaligned offset / point_step | production public fallback 已覆盖 | `PublicOstreamFallsBackForUnalignedPointStep`、`PublicOstreamFallsBackForUnalignedFieldOffset`。 |
| `_` padding field | production public padding case 已覆盖 | `PublicOstreamIgnoresPaddingField`。 |
| overflow guard | public entry 保留原 data size overflow guard | 当前 test 未构造大 allocation；源码 guard 未被 RVV patch 改动。 |

## 验证命令

```bash
make -C test-rvv/io/pcd_io run_test_compare
make -C test-rvv/io/pcd_io run_qemu_smoke
```

`run_qemu_smoke` 包含 RVV bench 可运行性检查，但其中 timing 不参与性能结论。
