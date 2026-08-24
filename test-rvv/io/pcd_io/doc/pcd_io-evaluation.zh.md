# pcd_io 函数级评估

## S2 函数级评估

`io/src/pcd_io.cpp` 里的当前目标入口是 `PCDReader::readBodyBinary` 和
`PCDWriter::writeBinaryCompressed`。二者都处理 `pcl::PCLPointCloud2` 的 binary_compressed
（压缩二进制）路径：

- reader 先通过 `pcl::lzfDecompress` 解压出 field-major（字段连续）buffer，再按点写回
  `cloud.data` 的 AoS（结构数组）布局，并随后扫描所有字段判断 `cloud.is_dense`。
- writer 先按 field 列表把 `cloud.data` 从 AoS 打包成 field-major buffer，再调用
  `pcl::lzfCompress`，最后写出 compressed / uncompressed size 和压缩 payload（负载）。

当前可 RVV 化片段是 pack/unpack 的字段布局转换。它有清楚的大批量内存搬运形态，但也有三个风险：

1. 完整 `writeBinaryCompressed` 可能被 LZF compression（LZF 压缩）、mmap / stream I/O（映射或流式 I/O）
   和 header 生成稀释。
2. PCD 字段列表支持不同 datatype（数据类型）、count（字段元素数）和 padding（填充字段）。第一阶段只覆盖
   4 字节、顺序 offset、`point_step == fsize` 或明确 padding 的组件边界，不能外推到所有 PCD layout。
3. `readBodyBinary` 还包含 finite scan（有限值扫描）。第一阶段只隔离 layout conversion，不证明 finite scan
   已经优化。

## 当前判断

当前判断是 `adopted production behavior`（已采纳生产行为）。
Phase 000 的 component ablation（组件消融）证明 4 字节字段 pack/unpack 组件有收益；Phase 010 的
production-shaped diagnostic（生产形态诊断）进一步证明 writer compressed payload 在包含 LZF compression
（LZF 压缩）的边界内仍有 positive（正向）板卡信号；Phase 020 已冻结 `std::ostream` overload 的
production patch 范围、fallback 矩阵和 PI2-PI5 证据计划。Phase 040 对 reader compressed payload 做了
LZF 解压、unpack 和标量 finite scan 的 production-shaped diagnostic，结果只有 weak-positive（弱正向），
不建议进入 reader production probe。Phase 050 已完成 writer `std::ostream` overload 的 production
integration（生产接入）：`production_writer_xyzi_307k` 5-run median 为 1.33x，
`production_writer_padded_xyzi_307k` 5-run median 为 1.25x，Evidence Doctor 为 Errors=0 / Warnings=0。

该判断只适用于 writer `std::ostream` overload 的 4 字节对齐有效字段 compressed payload pack。它不能外推到
reader、filename overload、templated writer、ASCII writer、non-4-byte fields、generic PCD layout 或其它 I/O
文件格式。

## 文档归属矩阵

| 信息类型 | 主归属 | 说明 |
| --- | --- | --- |
| 函数职责、初步候选和生产接入判断 | 本文 | 当前生产决策以 Phase 050 接入后证据为准。 |
| production 长期主题文档 | `doc-rvv/io/pcd_io-RVV.zh.md` | 只记录已采纳的窄 production 行为、fallback 和接入后证据。 |
| 阶段计划、阶段结果和继续 / 停止条件 | `doc/phases/` | 每个 phase 先写 plan，再写 result。 |
| 搜索空间和后续候选 | `doc/optimization-roadmap.zh.md` | pack/unpack、finite scan、production-shaped diagnostic 分开排队。 |
| 证据矩阵 | `doc/phases/optimization-matrix.zh.md` | 不把 planned 或 diagnostic 证据写成 production adopted。 |
| 测试入口和 target 粒度 | `doc/testing-overview.zh.md` | 运行入口分类、QEMU / board 边界和 target audit。 |
| TEST 语义 | `doc/correctness-tests.zh.md` | 每个 gtest 的输入、断言和不能证明的范围。 |
| bench、summary、doctor、registry | `doc/benchmark-and-evidence.zh.md` | 性能统计和证据提交边界。 |
| 候选证据索引 | `doc/optimization-evidence.zh.md` | candidate family 到代码、测试、bench、board 和 decision 的映射。 |
| 测试支撑代码地图 | `doc/test-support-code-map.zh.md` | helper、src、script、output 和 production 对照关系。 |
| 生成日志和 board summary | `log/` | raw log 默认不提交；summary 只有被文档引用才进入提交候选。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `PCDReader::readBodyBinary` | production public helper | 读取 binary / binary_compressed body，compressed 路径做 field-major 到 AoS 解包 | `PCDReader::read` | `pcl::lzfDecompress`、finite scan | production boundary（生产边界） | `io/src/pcd_io.cpp` |
| `PCDWriter::writeBinaryCompressed(std::ostream&, ...)` | production public helper | 生成 header，把 AoS 打包成 field-major buffer，再压缩写出 | PCD writer public overload | `pcl::lzfCompress` | production boundary | `io/src/pcd_io.cpp` |
| `packCompressedWriterFieldsStd` / `packCompressedWriterFieldsRVV` | production detail helpers | 标量或 RVV 执行 writer payload field-major pack | `writeBinaryCompressedPayload*` | LZF input buffer | adopted production behavior / fallback baseline | `io/src/pcd_io.cpp` |
| `tryWriteBinaryCompressedPayloadRVV` | production dispatch helper | RVV pack + LZF，失败时交回 Std fallback | public writer overload | ostream payload write | production-public candidate path | `io/src/pcd_io.cpp` |
| `rvv_test::pcd_io::*Scalar` | diagnostic reference | 复刻 pack/unpack 的标量参考链路 | `test_pcd_io`、`bench_pcd_io` | 测试断言和 bench Std build | correctness reference（正确性参考） | `include/impl/pcd_io_support.hpp` |
| `rvv_test::pcd_io::*Candidate` | candidate formula | 在 `__RVV10__` 下尝试 RVV stride load/store；非 RVV 构建回退参考链路 | `test_pcd_io`、`bench_pcd_io` | QEMU、asm、board bench | component ablation（组件消融） | `include/impl/pcd_io_support.hpp` |
| `rvv_test::pcd_io::makeCompressedWriterPayload*` | production-shaped diagnostic | 生成 compressed writer payload，包含 field-major 打包、LZF 和 8 字节 size header | `test_pcd_io`、`bench_pcd_io` | writer payload board summary / Evidence Doctor | production-shaped diagnostic | `include/impl/pcd_io_support.hpp` |
| `rvv_test::pcd_io::readCompressedBody*` | production-shaped diagnostic | 解码 compressed reader payload，执行 LZF 解压、unpack 和标量 finite scan | `test_pcd_io`、`bench_pcd_io` | reader payload board summary / Evidence Doctor | production-shaped diagnostic | `include/impl/pcd_io_support.hpp` |
| `test_pcd_io` | correctness gate | 对比 pack/unpack candidate、shaped helper 和 production public writer 与 scalar reference | Makefile `run_test_compare` | gtest 输出 | correctness / fallback gate | `src/test_pcd_io.cpp` |
| `bench_pcd_io` | bench wrapper | 输出 checksum 和 case 计时 | Makefile board targets | repeated summary / Evidence Doctor | board performance（板卡性能） | `src/bench_pcd_io.cpp` |
| topic-local role docs | documentation section | 分拆测试、bench、候选和代码地图职责 | README / evaluation | reviewer / next worker | recovery pointer（恢复入口） | `doc/*.zh.md` |

## 证据链

Phase 000-040 是诊断或 production-shaped evidence（生产形态证据）；Phase 050 是当前 production decision
的依据：

| phase | 证据路径 | 结果 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- | --- |
| 000 component ablation | `log/board/component_ablation_repeat_5/summary.md`、`evidence_doctor.md` | pack median 1.20x / 1.12x；unpack median 1.11x / 1.03x；Errors=0 / Warnings=0 | layout conversion 组件在板卡上有收益。 | 不含 LZF、header、ostream、mmap / file I/O 和 finite scan。 |
| 010 writer payload shaped | `log/board/writer_payload_repeat_5/summary.md`、`evidence_doctor.md` | writer payload median 1.18x / 1.12x；Errors=0 / Warnings=0 | writer payload 在包含 LZF 的生产形态边界内仍 positive。 | 不含真实 production dispatch、fallback、file-name overload、non-4-byte fields 和 production asm。 |
| 020 PI1 plan | `doc/phases/020-production-integration-plan-writer-payload/result.zh.md` | fallback 矩阵、production direct test、asm 和 board plan 已冻结。 | 可以恢复到 PI2 production patch 授权检查点。 | 不证明 production direct 性能或 adopted production behavior。 |
| 030 doc suite | `doc/phases/030-doc-suite-structure/result.zh.md` | topic-local role docs 已补齐。 | reviewer 可从文档定位 target、TEST、bench、script、证据和 helper。 | 不改变性能或 production 接入证据。 |
| 040 reader payload shaped | `log/board/reader_payload_repeat_5/summary.md`、`evidence_doctor.md` | reader payload median 1.07x / 1.03x；Errors=0 / Warnings=0 | reader payload shaped boundary 下仍有弱正向信号。 | 不支持 reader production probe；不覆盖 public read、mmap / file I/O、header parsing 或 finite scan RVV。 |
| 050 writer production public | `log/board/production_writer_repeat_5/summary.md`、`evidence_doctor.md` | production writer median 1.33x / 1.25x；Errors=0 / Warnings=0 | writer public `std::ostream` overload 的 4 字节字段 RVV pack 在板卡上 positive。 | 不覆盖 reader、filename overload、templated writer、ASCII writer、non-4-byte fields 或 generic PCD layout。 |

QEMU 证明构建和路径可运行；反汇编证明 bench binary 中存在预期 RVV 指令；板卡 repeated summary 才能证明
真实性能信号。这些证据不能直接替代 production dispatch 证据。

## 生产接入判断

Phase 050 已按用户偏好采纳当前窄 production patch。保留条件：

- `__RVV10__` 关闭时完全走标量 helper。
- 字段 layout gate 只覆盖 4 字节字段和当前测试过的 AoS / field-major 边界。
- mixed field sizes、non-4-byte datatype、reader、finite scan、templated writer 和 file-name overload 不纳入同一生产补丁。
- production public tests、fallback tests、production asm 和 board production bench 已在 Phase 050 闭合。

Reader 方向已由 Phase 040 降级为 `attempted / weak-positive`，当前不进入 reader PI1。当前没有同一授权边界内
值得继续自动推进的高优先级优化方式；若继续，应另建 phase 并重新冻结入口范围。
