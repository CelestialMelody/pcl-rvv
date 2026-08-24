# pcd_io_templated_writer 测试支撑代码地图

## 本文职责

本文定位 test support（测试支撑代码）、bench harness（性能测试框架）、script（脚本）、
evidence output（证据输出）和 production 对照关系。它不承担性能结论，也不把 test-only helper
写成 production helper。

## 总调用图

```text
io/include/pcl/io/impl/pcd_io.hpp
  PCDWriter::writeBinaryCompressed<PointT>
    -> production RVV 4-byte pack helper or scalar fallback
    -> pcl::lzfCompress

  PCDWriter::writeBinary<PointT>
    -> production tuple / segment helper or scalar fallback
    -> mmap/write output buffer

include/pcdtw.h
  -> include/impl/pcdtw_support.hpp
       makePointMajorCloud
       packFieldsScalar
       packFieldsCandidate
       packAndCompressScalar / Candidate
       packBinaryFieldsScalar / Candidate

src/test_pcdtw.cpp
  -> correctness assertions and path-hit checks

src/bench_pcdtw.cpp
  -> pack-only cases
  -> compressed_* cases
  -> binary_* cases
  -> production_compressed_* public writer cases
  -> production_binary_tuple_* public writer cases
  -> Phase 080 historical production_binary_* public writer cases kept only as evidence
  -> logs consumed by scripts

script/generate_pcdtw_repeat_summary.py
script/generate_pcdtw_evidence_manifest.py
  -> ../../script/evidence_doctor.py
  -> ../../script/evidence_registry.py
```

## 稳定入口

| 入口 | 作用 |
| --- | --- |
| `include/pcdtw.h` | topic-local aggregator header（聚合头）。 |
| `include/impl/pcdtw_support.hpp` | helper implementation，包含 fixtures、reference、candidate、compression wrapper。 |
| `src/test_pcdtw.cpp` | correctness executable source。 |
| `src/bench_pcdtw.cpp` | benchmark executable source。 |
| `Makefile` | QEMU、board、summary、Doctor、registry target。 |
| `board.mk` | 板卡远端运行参数。 |

## Helper 职责

| helper | role | 调用者 | 证据边界 |
| --- | --- | --- | --- |
| `makePointMajorCloud` | fixture / input generator（输入构造） | test、bench | 生成 synthetic byte layout，不代表真实 PCL point registration。 |
| `packFieldsScalar` | diagnostic reference（诊断参考链路） | test、bench Std build | 复刻 production pack loop，不处理 file write。 |
| `packFieldsCandidate` | RVV candidate（RVV 候选链路） | test、bench RVV build | 只接管 4 字节字段 pack；不证明 production dispatch。 |
| `canUseRvvFourByteFields` | candidate gate（候选准入） | `packFieldsCandidate` | helper-level gate；PI2 需重新落到 production field metadata。 |
| `compressPackedBytes` | LZF wrapper | pack+compress helper | 模拟 compressed payload 的 8 字节 header + LZF，不处理 PCD header。 |
| `packAndCompressScalar` / `Candidate` | production-shaped diagnostic | compressed test 和 bench | 支撑 Phase 010，不是 production direct。 |
| `packBinaryFieldsScalar` / `Candidate` | binary component ablation | binary test 和 bench | 支撑 Phase 040；复刻 binary writer packed output loop，不处理 mmap / write。 |
| `packBinaryCompressedFieldsStd` / `RVV` | production detail helper | `PCDWriter::writeBinaryCompressed<PointT>` | Phase 060 真实生产补丁；只覆盖 4 字节字段且 offset / stride 对齐的 pack 阶段。 |
| Phase 080 field-outer `packBinaryFieldsStd` / `RVV` | historical production probe helper | `PCDWriter::writeBinary<PointT>` | Phase 080 真实生产补丁已因 board negative 回滚；当前生产源码中不存在该 field-outer helper。 |
| current `packBinaryFieldsStd` / `packBinaryFieldsTupleRVV` | production detail helper | `PCDWriter::writeBinary<PointT>` | Phase 100 真实生产补丁；compact 16B 直接 `memcpy`，padding 20B 使用 `vlse32.v` + `vsseg4e32.v`。 |

## Scripts 与输出

| script / output | role |
| --- | --- |
| `script/generate_pcdtw_repeat_summary.py` | 从 repeated run logs 生成 Markdown summary。 |
| `script/generate_pcdtw_evidence_manifest.py` | 生成 Evidence Doctor manifest，并标注 evidence role / timer boundary。 |
| `log/board/*/summary.md` | 可提交摘要候选；保存 repeated board 统计。 |
| `log/board/*/evidence_manifest.json` | 可提交 manifest 候选。 |
| `log/board/*/evidence_doctor.md` | Evidence Doctor 摘要报告。 |
| `log/evidence_registry.json` | freshness registry（新鲜度登记表）。 |

## Production 与 Test Support 边界

当前 production 有 compressed writer patch 和 binary tuple / segment writer patch，均已 adopted。test support helper 的
pack / compress 形态仍用于历史诊断；真实 production direct tests 通过 public writer 写文件、解压 payload 和 hook
证明生产分流与 fallback，正式长期文档为 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。

测试支撑里的 `packBinaryFields*` component helper 对照 `PCDWriter::writeBinary<PointT>` 中有效字段 packed output
的循环，但只服务 Phase 040 component ablation；它不覆盖 header、mmap、write、sync、error handling 或真实
public entry dispatch。

Phase 060 的 compressed writer production helper 已按 PI1 计划抽出 Std helper、RVV helper 和 fallback gate；
Phase 070 已完成 production closeout。Phase 080 的 binary writer helper 曾接入真实 public entry 并完成
PI2-PI5 验证，但板卡 repeated summary 为负向，Evidence Doctor Errors=3；用户确认负收益可回滚后，该
field-outer production probe 已移除，不是 adopted production behavior。Phase 100 的 binary tuple / segment
helper 已接入真实 public entry，板卡 production-public summary 为正向，当前是 adopted production behavior。

## 拆分审计

当前 topic 已采用 `src/`、`include/`、`include/impl/`、`script/` 布局，没有旧 `test_support/`
目录。`include/impl/pcdtw_support.hpp` 同时包含 fixture、reference、candidate、compression wrapper、
binary component helper、tuple diagnostic helper、production-direct helpers 和 path-hit state，职责较多但当前仍低于硬拆分阈值。若用户授权
继续扩大 binary writer 范围，应重新评估是否拆成 `fixtures`、`references`、`candidates`、
`assertions` 和 `bench_cases` 内部头。
