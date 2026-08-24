# pcd_io 测试支撑代码地图

本文定位 test support（测试支撑代码）的职责边界。它不承担性能结论；性能和证据归到
`benchmark-and-evidence.zh.md`。

## 总调用图

```text
src/test_pcd_io.cpp
  -> include/pcd_io.h
    -> include/impl/pcd_io_support.hpp

src/bench_pcd_io.cpp
  -> include/pcd_io.h
    -> include/impl/pcd_io_support.hpp
  -> script/generate_pcd_io_evidence_manifest.py
  -> test-rvv/script/evidence_doctor.py
  -> test-rvv/script/evidence_registry.py
```

Production 对照入口是 `io/src/pcd_io.cpp` 中 `PCDReader::readBodyBinary` 和
`PCDWriter::writeBinaryCompressed(std::ostream&, ...)`。Phase 050 后 production 已接入 writer
`std::ostream` overload 的 4 字节字段 RVV pack；test support 仍不被 production 调用，只作为 oracle、
bench 和证据包装层。

## 稳定聚合入口

| object | role | location |
| --- | --- | --- |
| `pcd_io.h` | aggregator header（聚合头） | `include/pcd_io.h` |
| `pcd_io_support.hpp` | internal helper（内部 helper） | `include/impl/pcd_io_support.hpp` |
| `test_pcd_io.cpp` | correctness executable source | `src/` |
| `bench_pcd_io.cpp` | bench executable source | `src/` |
| `generate_pcd_io_evidence_manifest.py` | topic-local manifest script | `script/` |

当前结构已经使用配置建议的 `include/`、`include/impl/`、`src/` 和 `script/`。没有旧 `test_support/`
目录或 compatibility alias（兼容别名）。

## Fixtures 与输入构造

| helper | role | notes |
| --- | --- | --- |
| `FieldLayout` | 字段 offset / size 描述 | 代表过滤后的 PCLPointCloud2 有效字段。 |
| `makeInterleavedCloud` | synthetic fixture（合成测试夹具） | 构造 AoS byte buffer，并用 `0x5a` 填充未覆盖字节。 |
| `makeInterleavedFloatCloud` | finite scan fixture（有限值扫描夹具） | 对 `Float32/Float64` 字段写入确定性有限值，便于单点注入 NaN。 |
| `writeFloat32` | fixture mutation（夹具改写） | 用于 correctness / bench 注入 NaN。 |
| `packedSize` | size helper | 按 field size × point count 计算 field-major buffer。 |

production public writer tests 已在 `src/test_pcd_io.cpp` 中构造真实 `pcl::PCLPointCloud2::fields` metadata。
component / shaped helper 仍使用 `FieldLayout` 代表过滤后的字段布局。

## 标量 Reference

| helper | production relationship | limitation |
| --- | --- | --- |
| `packFieldsScalar` | 复刻 writer compressed path 的 field-major pack 双重循环。 | 不生成 text header，不写 ostream。 |
| `unpackFieldsScalar` | 复刻 reader compressed path 的 field-major unpack。 | 不运行 `lzfDecompress` 和 finite scan。 |
| `compressPackedPayload` | 调用现有 `pcl::lzfCompress` 并写 8-byte size header。 | 只处理 payload，不处理 public entry header。 |
| `readCompressedBodyScalar` | 复刻 reader compressed payload 的 LZF 解压、unpack 和标量 finite scan。 | 不处理 public read、mmap 或 file I/O。 |

## Candidate / Diagnostic Helper

| helper | role | fallback |
| --- | --- | --- |
| `canUseRvvFourByteFields` | test-only layout gate | point_count 0、field empty、field size 非 4、offset / point_step 非 4 字节对齐时 false。 |
| `packFieldsCandidate` | RVV pack candidate | 不满足 gate 或非 RVV build 时调用 `packFieldsScalar`。 |
| `unpackFieldsCandidate` | RVV unpack candidate | 不满足 gate 或非 RVV build 时调用 `unpackFieldsScalar`。 |
| `makeCompressedWriterPayloadCandidate` | production-shaped diagnostic | 使用 candidate pack 后调用 shared LZF payload helper。 |
| `readCompressedBodyCandidate` | production-shaped diagnostic | 使用 candidate unpack，LZF 解压和 finite scan 保持 shared scalar shape。 |
| `lastPath` / `resetLastPath` | correctness path marker（路径标记） | 仅 test support 使用，不是 production API。 |

## Bench Harness 与 Case Registry

| bench function | case family | timer boundary |
| --- | --- | --- |
| `runPackCase` | `pack_*` | layout conversion only |
| `runUnpackCase` | `unpack_*` | layout conversion only |
| `runWriterPayloadCase` | `writer_payload_*` | pack + LZF payload |
| `runReaderPayloadCase` | `reader_payload_*` | LZF decompress + unpack + scalar finite scan |
| `caseEnabled` | CLI case-filter | 支持 exact label、`component`、`all`、`writer_payload`、`reader_payload` 和 `production_writer` family |
| `runProductionWriterCase` | `production_writer_*` | public ostream header + pack + LZF + payload write |

## Scripts 与 Evidence Output

| script / output | role |
| --- | --- |
| `script/generate_pcd_io_evidence_manifest.py` | 解析 repeated board logs，生成 manifest。 |
| `log/board/*/summary.md` | repeated board summary。 |
| `log/board/*/evidence_manifest.json` | Evidence Doctor input。 |
| `log/board/*/evidence_doctor.md` | Evidence Doctor report。 |
| `log/evidence_registry.json` | freshness tracking。 |

## Production 与 Test Support 边界

Phase 050 后，production RVV candidate 位于 `io/src/pcd_io.cpp`。test support 仍提供 scalar oracle、
path marker 和 bench wrapper：

| object | role | boundary |
| --- | --- | --- |
| `canUseRvvCompressedWriterPack` | production layout gate | 只允许 4-byte aligned effective fields。 |
| `packCompressedWriterFieldsStd` | production scalar pack helper | 保留原双重循环语义。 |
| `packCompressedWriterFieldsRVV` | production RVV pack helper | `__RVV10__ && __riscv_vector` 下编译。 |
| `writeBinaryCompressedPayloadStd` | production fallback payload helper | pack + LZF + size header。 |
| `tryWriteBinaryCompressedPayloadRVV` | production RVV payload helper | RVV pack + LZF，失败时交回 Std fallback。 |
| `pcd_io_rvv_test::WriterCompressedPath` | test hook | 只在 `PCL_PCD_IO_RVV_TEST_HOOKS` 下编译，不是 public API。 |

## 拆分审计

| area | current state | decision |
| --- | --- | --- |
| source layout | `src/test_pcd_io.cpp` 和 `src/bench_pcd_io.cpp` 已拆分 | adopted |
| aggregator / internal helpers | `include/pcd_io.h` + `include/impl/pcd_io_support.hpp` | adopted |
| helper responsibilities | internal helper 同时含 fixtures、reference、candidate、payload helper 和 path marker，但仍低于 soft line limit | keep with map |
| script ownership | topic-local manifest wrapper 位于 `script/` | adopted |
| legacy alias | 无 | not_applicable with evidence |

若 PI3/PI4 增加 production direct test、production bench 和 manifest role，建议再拆 `pcd_io_support.hpp` 或新增 production-specific helper map。
