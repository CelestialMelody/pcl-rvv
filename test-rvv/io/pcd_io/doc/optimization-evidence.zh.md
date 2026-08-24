# pcd_io 优化证据索引

本文索引当前 topic 已尝试、暂缓和计划中的优化方式。搜索空间和优先级归到 `optimization-roadmap.zh.md`；
阶段流水归到 `doc/phases/`。

## 当前结论摘要

| status | candidate |
| --- | --- |
| attempted / diagnostic-positive | 4-byte field pack/unpack RVV stride path。 |
| partial-production-candidate | writer payload production-shaped diagnostic。 |
| adopted production behavior | writer `std::ostream` 4 字节对齐有效字段 production RVV pack。 |
| attempted / weak-positive | reader unpack + scalar finite scan production-shaped path。 |
| deferred | finite scan RVV mask、reader production probe、filename overload、templated writer 和 non-4-byte fields。 |
| applicable | `doc-rvv/io/pcd_io-RVV.zh.md` 记录 Phase 050 后 adopted production behavior。 |

## 优化方式总表

| candidate family | code path | correctness | bench / board | asm | doctor | decision | boundary |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 4-byte pack RVV stride path | `include/impl/pcd_io_support.hpp::packFieldsCandidate` | `PackCandidateMatchesScalarForFourByteFields` | `pack_xyzi_307k` 1.20x；`pack_padded_xyzi_307k` 1.12x | `vlse32.v` / `vse32.v` in bench path | Phase 000 Errors=0 / Warnings=0 | diagnostic-positive | test helper component |
| 4-byte unpack RVV stride path | `include/impl/pcd_io_support.hpp::unpackFieldsCandidate` | `UnpackCandidateMatchesScalarWithTrailingPadding` | `unpack_xyzi_307k` 1.11x；`unpack_padded_xyzi_307k` 1.03x | `vle32.v` / `vsse32.v` in bench path | Phase 000 Errors=0 / Warnings=0 | attempted / weaker | reader production deferred |
| mixed field size fallback | `canUseRvvFourByteFields` | `CandidateFallsBackForMixedFieldSizes` | no bench | not_applicable | not_applicable | fallback diagnostic | test helper only |
| writer payload RVV pack + LZF | `makeCompressedWriterPayloadCandidate` | `CompressedPayloadCandidateMatchesScalar` | `writer_payload_xyzi_307k` 1.18x；`writer_payload_padded_xyzi_307k` 1.12x | `vlse32.v` / `vse32.v` in bench path | Phase 010 Errors=0 / Warnings=0 | partial-production-candidate | production-shaped diagnostic |
| reader payload RVV unpack + scalar finite scan | `readCompressedBodyCandidate` | `CompressedReaderBodyCandidateMatchesScalarAndDenseFlag` | `reader_payload_xyzi_307k` 1.07x；`reader_payload_padded_xyzi_307k` 1.03x | `vle32.v` / `vsse32.v` in bench path | Phase 040 Errors=0 / Warnings=0 | attempted / weak-positive | production-shaped diagnostic，不进入 reader production |
| writer production RVV pack | `io/src/pcd_io.cpp::packCompressedWriterFieldsRVV` and public overload dispatch | 5 个 `PCDIOProductionWriter.*` tests | `production_writer_xyzi_307k` 1.33x；`production_writer_padded_xyzi_307k` 1.25x | public writer path 可见 `vlse32.v` / `vse32.v` | Phase 050 Errors=0 / Warnings=0 | adopted production behavior | production-public |

## 标量路径与 RVV 路径差异

| path | scalar behavior | RVV candidate behavior | fallback |
| --- | --- | --- | --- |
| pack | nested point × field loop with small `memcpy` | per field `vlse32` from AoS, `vse32` to field-major | non-4-byte field or unaligned layout |
| unpack | per field contiguous load, strided store to AoS | `vle32` from field-major, `vsse32` to AoS | non-4-byte field or unaligned layout |
| writer payload | pack scalar then `pcl::lzfCompress` | pack candidate then same `pcl::lzfCompress` | LZF failure or unsupported layout |
| production writer public overload | header + pack + LZF + ostream payload write | same public overload，4-byte aligned fields use RVV pack before same LZF / ostream path | non-RVV build、mixed size、unaligned layout、empty data 或 LZF failure |
| reader payload | `pcl::lzfDecompress` then scalar unpack and scalar finite scan | same decompression and finite scan，unpack uses candidate | malformed payload or unsupported layout |

当前 RVV candidate 不改变 LZF、header、ostream 或 mmap 语义。

## 代码级证据索引

| object | role | path |
| --- | --- | --- |
| `packFieldsScalar` / `unpackFieldsScalar` | scalar reference（标量参考） | `include/impl/pcd_io_support.hpp` |
| `packFieldsCandidate` / `unpackFieldsCandidate` | RVV diagnostic candidate（诊断候选） | `include/impl/pcd_io_support.hpp` |
| `makeCompressedWriterPayload*` | production-shaped writer payload diagnostic | `include/impl/pcd_io_support.hpp` |
| `readCompressedBody*` | production-shaped reader payload diagnostic | `include/impl/pcd_io_support.hpp` |
| `runPackCase` / `runUnpackCase` / `runWriterPayloadCase` | bench wrapper | `src/bench_pcd_io.cpp` |
| `generate_pcd_io_evidence_manifest.py` | manifest wrapper | `script/` |
| `PCDWriter::writeBinaryCompressed(std::ostream&, ...)` | adopted production public boundary | `io/src/pcd_io.cpp` |
| `packCompressedWriterFieldsStd` / `packCompressedWriterFieldsRVV` | production scalar / RVV pack helpers | `io/src/pcd_io.cpp` |

## 结论边界

Phase 050 的 board evidence 支持当前窄范围 production adoption。该结论只覆盖 public `std::ostream`
binary_compressed writer 的 4-byte aligned effective fields；reader、filename overload、templated writer、
ASCII writer、non-4-byte fields 和 generic PCD layout 仍需独立证据。
