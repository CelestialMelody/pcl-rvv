# pcd_io RVV 主题入口

本文是 `io/src/pcd_io.cpp` 的 RVV（RISC-V Vector，可变长度向量扩展）主题入口。当前主题来自
`doc-rvv/library-screening/io/io-retained-candidate-rescreen.zh.md` 的执行清单，第一阶段只做
PCLPointCloud2（二进制点云通用容器）的 field layout conversion（字段布局转换）组件消融。

## 当前结论

- production（生产源码）已在 Phase 050 接入窄范围 writer payload RVV pack。
- Phase 000 证明 PCLPointCloud2 compressed pack/unpack 的 4 字节字段组件有板卡收益。
- Phase 010 证明 writer compressed payload（字段打包加 LZF compression）在 production-shaped diagnostic
  （生产形态诊断）边界内仍有 positive（正向）收益：`writer_payload_xyzi_307k` median 1.18x，
  `writer_payload_padded_xyzi_307k` median 1.12x。
- Phase 020 已完成 PI1 production integration plan（生产接入计划），冻结 writer `std::ostream`
  overload 的 4 字节字段 production patch 范围、fallback 矩阵和 PI2-PI5 证据计划。
- Phase 040 证明 reader compressed payload（LZF 解压 + unpack + 标量 finite scan）只有 weak-positive
  （弱正向）收益：`reader_payload_xyzi_307k` median 1.07x，`reader_payload_padded_xyzi_307k`
  median 1.03x；当前不建议进入 reader production probe。
- Phase 050 完成 writer `std::ostream` overload 的 PI2-PI5 production integration（生产接入闭环）。
  接入后 production-public board summary 为 `production_writer_xyzi_307k` median 1.33x、
  `production_writer_padded_xyzi_307k` median 1.25x，Evidence Doctor 为 Errors=0 / Warnings=0。
- 当前 EvidenceDecision（证据决策）是 `adopted production behavior`，范围只覆盖
  `PCDWriter::writeBinaryCompressed(std::ostream&, ...)` 的 4 字节对齐有效字段 pack。

## 阅读路径

| 读者问题 | 主入口 |
| --- | --- |
| 这个 topic 为什么启动 | `doc/pcd_io-evaluation.zh.md` |
| 当前阶段证明了什么 | `doc/phases/050-production-writer-payload-integration/result.zh.md`；历史阶段见 `doc/phases/README.zh.md` |
| 当前 production 行为 | `doc-rvv/io/pcd_io-RVV.zh.md` |
| 测试入口和 target 粒度 | `doc/testing-overview.zh.md` |
| TEST 输入、断言和证明范围 | `doc/correctness-tests.zh.md` |
| bench case、board summary、Evidence Doctor 和 registry | `doc/benchmark-and-evidence.zh.md` |
| helper、script 和 output 代码地图 | `doc/test-support-code-map.zh.md` |
| 跨阶段还有哪些候选 | `doc/optimization-roadmap.zh.md` |
| 证据矩阵当前状态 | `doc/phases/optimization-matrix.zh.md` |
| 优化候选与证据索引 | `doc/optimization-evidence.zh.md` |
| 测试和 bench 源码 | `src/test_pcd_io.cpp`、`src/bench_pcd_io.cpp` |

## 常用命令

```bash
make -C test-rvv/io/pcd_io run_test_compare
make -C test-rvv/io/pcd_io run_qemu_smoke
make -C test-rvv/io/pcd_io dump_bench_rvv
make -C test-rvv/io/pcd_io run_board_pcd_io_repeated
make -C test-rvv/io/pcd_io run_board_pcd_io_writer_payload_repeated
make -C test-rvv/io/pcd_io run_board_pcd_io_reader_payload_repeated
make -C test-rvv/io/pcd_io run_board_pcd_io_production_writer_repeated
```

`run_board_pcd_io_repeated` 产生 Phase 000 组件性能证据。`run_board_pcd_io_writer_payload_repeated`
产生 Phase 010 writer payload 生产形态诊断性能证据。`run_board_pcd_io_reader_payload_repeated` 产生
Phase 040 reader payload 生产形态诊断性能证据。`run_board_pcd_io_production_writer_repeated` 产生
Phase 050 production-public（真实公开入口）性能证据。QEMU 上的 bench compare（性能对比）默认不作为结论来源。

## production topic doc 适用性

`doc-rvv/io/pcd_io-RVV.zh.md` 当前适用，记录 Phase 050 后已采纳的 production 行为、fallback 矩阵和
接入后板卡证据。topic-local 文档仍负责保存 phase 过程、测试 target、bench 字典和证据登记。
