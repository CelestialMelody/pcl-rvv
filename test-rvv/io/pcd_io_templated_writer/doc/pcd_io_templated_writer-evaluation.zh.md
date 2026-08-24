# pcd_io_templated_writer 函数级评估

## S2 函数级评估

目标源码是 `io/include/pcl/io/impl/pcd_io.hpp`，首要评估对象是
`PCDWriter::writeBinaryCompressed<PointT>`。这个模板入口先用
`pcl::getFields<PointT>()` 获取点类型字段，过滤名称为 `_` 的 padding 字段，计算每个字段的字节数，
再把 `PointCloud<PointT>` 的 point-major（按点连续，类似 XYZRGBXYZRGB）内存布局改写为
field-major（按字段连续，类似 XXYYZZRGBRGB），随后交给 `pcl::lzfCompress`。

Phase 040 还对 `PCDWriter::writeBinary<PointT>(file_name, cloud)` 做了二级评估。binary writer
过滤字段后仍按 point-major packed layout（按点连续的有效字段）写入输出 buffer；它没有 LZF，
但真实路径仍包含 header、mmap、write、sync 和 error handling。本阶段只评估 test-only packed loop。

当前可 RVV 化片段是压缩前置字段布局转换。它有规整的大批量跨步加载和连续写入形态，但有三类风险：

1. 完整入口后续仍包含 LZF compression（LZF 压缩）、文件映射 / 写入和 header 生成，组件收益可能被稀释。
2. 模板点类型可能包含不同 datatype（数据类型）、count（字段元素数）、padding（填充）和 alignment（对齐）组合；第一阶段只覆盖 4 字节字段。
3. `writeASCII` 主要是 stream formatting（流式格式化）和 locale（区域设置）成本，本阶段只记录风险，不作为首个 RVV 目标。

## 初步判断

当前判断是 `compressed-writer adopted production behavior / binary-writer tuple-segment adopted production behavior`。
Phase 060 已把 compressed writer 的
4 字节字段 pack 接入真实 `PCDWriter::writeBinaryCompressed<PointT>` public overload（公开入口），并完成
production direct（真实生产入口直连）正确性、fallback、反汇编和板卡 production-public（真实公开入口）
证据；用户确认接入后板卡测试有收益即可采纳后，Phase 070 创建了正式
`doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`，并把 Phase 060 接入后的板卡数据作为 production closeout
证据。Phase 080 已把 binary writer 接入真实 `PCDWriter::writeBinary<PointT>` public overload，但
production-public 板卡结果为 mean `0.9842x / 0.9727x / 0.9810x` 且 Evidence Doctor Errors=3；
用户确认负收益可回滚后，该 field-outer binary writer production patch 和旧 `production_binary_*`
测试 / bench 入口已移除。Phase 090/100 随后验证 tuple / segment 新实现族：Phase 100 接入后
production-public 板卡 compact mean `1.3046x`、padding mean `1.3240x`、small smoke mean `1.1082x`，
Evidence Doctor Errors=0 Warnings=1；当前 `writeBinary<PointT>` 对 4 个连续 4-byte effective fields
采用 compact memcpy / padding segment path，其它布局 fallback。

## 文档归属矩阵

| 信息类型 | 主归属 | 说明 |
| --- | --- | --- |
| 函数职责、初步候选和生产接入判断 | 本文 | compressed writer 的长期生产说明在 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。 |
| 阶段计划、阶段结果和继续 / 停止条件 | `doc/phases/` | 每个 phase 先写 plan，再写 result。 |
| 搜索空间和后续候选 | `doc/optimization-roadmap.zh.md` | compressed writer、binary writer、ASCII writer 风险分开记录。 |
| 证据矩阵 | `doc/phases/optimization-matrix.zh.md` | 不把 planned 或 diagnostic 证据写成 production adopted。 |
| 测试入口和 target 粒度 | `doc/testing-overview.zh.md` | 区分 correctness、QEMU smoke、board repeated、Doctor / registry 和 production direct 缺口。 |
| gtest 输入和断言 | `doc/correctness-tests.zh.md` | 逐个说明 helper-level correctness 和 fallback 证明范围。 |
| bench case、summary 和提交边界 | `doc/benchmark-and-evidence.zh.md` | 解释 case-filter、计时边界、Evidence Doctor 和 registry。 |
| candidate 证据索引 | `doc/optimization-evidence.zh.md` | 把 pack-only、pack+LZF、PI1、binary writer、ASCII writer 状态分开。 |
| 测试支撑代码定位 | `doc/test-support-code-map.zh.md` | 定位 aggregator、internal helper、src、script 和 output。 |
| 生成日志和 board summary | `log/` | raw log 默认不提交；summary 只有被文档引用才进入提交候选。 |
| compressed / binary writer 长期 production 行为 | `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md` | 只保存已采纳生产事实、fallback、证据链和后续边界。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `PCDWriter::writeBinaryCompressed<PointT>` | production public helper | 把 `PointCloud<PointT>` 写成 binary_compressed PCD | PCD writer public overload | `pcl::lzfCompress`、mmap / file write | production boundary（生产边界） | `io/include/pcl/io/impl/pcd_io.hpp` |
| `rvv_test::pcd_io_templated_writer::packFieldsScalar` | diagnostic reference | 复刻模板 writer 的标量布局转换 | `test_pcdtw` | 测试断言和 bench Std build | correctness reference（正确性参考） | `include/impl/pcdtw_support.hpp` |
| `rvv_test::pcd_io_templated_writer::packFieldsCandidate` | candidate formula | 在 `__RVV10__` 下尝试 RVV stride load + contiguous store | `test_pcdtw`、后续 `bench_pcdtw` | QEMU、asm、board bench | component ablation（组件消融） | `include/impl/pcdtw_support.hpp` |
| `rvv_test::pcd_io_templated_writer::packAndCompressCandidate` | production-shaped diagnostic | 先运行 RVV pack candidate，再调用 LZF，模拟 compressed writer 的主要前置链路 | `test_pcdtw`、`bench_pcdtw` 的 `compressed_*` case | Phase 010 correctness、QEMU smoke、board repeated summary | production-shaped diagnostic（生产形态诊断） | `include/impl/pcdtw_support.hpp` |
| `rvv_test::pcd_io_templated_writer::packBinaryFieldsCandidate` | binary component candidate | 在 `__RVV10__` 下尝试 RVV stride load + stride store，生成 binary writer 的 packed point-major 输出 | `test_pcdtw`、`bench_pcdtw` 的 `binary_*` case | Phase 040 correctness、QEMU smoke、board repeated summary | component ablation（组件消融） | `include/impl/pcdtw_support.hpp` |
| `pcl::io::detail::packBinaryCompressedFieldsRVV` | production detail helper | 真实 compressed writer 的 4 字节字段 RVV pack helper | `PCDWriter::writeBinaryCompressed<PointT>` | LZF、mmap / file write | production-public candidate | `io/include/pcl/io/impl/pcd_io.hpp` |
| Phase 080 binary writer production probe | historical production probe | 回滚前真实 binary writer 的 field-outer RVV packed output helper；回滚后生产源码中已移除。 | `PCDWriter::writeBinary<PointT>` | mmap / file write | PI5 negative / rollback evidence | `doc/phases/080-binary-writer-PI2-production-patch/result.zh.md` |
| `pcl::io::detail::packBinaryFieldsTupleRVV` | production detail helper | 当前 binary writer 的 compact memcpy / padding segment output helper。 | `PCDWriter::writeBinary<PointT>` | mmap / file write | adopted production behavior | `io/include/pcl/io/impl/pcd_io.hpp` |
| `test_pcdtw` | correctness gate | 对比 candidate 与 scalar reference，并检查 RVV build 是否命中候选路径 | Makefile `run_test_compare` | gtest 输出 | correctness gate（正确性验收） | `src/test_pcdtw.cpp` |
| `bench_pcdtw` | bench wrapper | 输出 pack-only、compressed、binary component、compressed production public 和 binary tuple production public case 的 Std/RVV A/B 数据；`production_binary_*` 仅作为 Phase 080 历史探针保留在文档 / 证据里。 | Makefile `run_qemu_smoke`、board repeated targets | summary、manifest、Evidence Doctor | board performance（板卡性能）和 QEMU log-shape（日志形状） | `src/bench_pcdtw.cpp` |
| `generate_pcdtw_evidence_manifest.py` | analysis script | 为 pack-only、compressed、binary tuple repeated logs 生成 Evidence Doctor manifest | Makefile evidence targets | `../../script/evidence_doctor.py`、registry | evidence boundary（证据边界） | `script/generate_pcdtw_evidence_manifest.py` |

## 诊断证据链要求

Phase 060 之前的证据链只能写作“诊断证据链”：correctness 证明 test-only candidate 与参考链路一致；
QEMU 证明构建和路径可运行；反汇编证明 bench binary 中存在预期 RVV 指令；板卡 repeated summary 证明
layout conversion 组件是否有真实性能信号。Phase 060 新增的 production-public 证据才覆盖真实 production
dispatch（生产分流）和公开入口计时边界。

## 当前证据

Phase 000 pack-only component ablation 已完成。`run_test_compare`、`run_board_test fetch_board_logs` 均通过；
反汇编在 `build/asm/riscv/bench_pcdtw_rvv.asm` 中可见 `vlse32.v` / `vse32.v`；板卡 repeated summary
`log/board/component_ablation_repeat_5/summary.md` 显示 `pointxyzrgb_4f_262k` mean `1.4892x`、
`pointxyzrgb_4f_padding_262k` mean `1.4466x`、`pointxyzrgb_4f_small_512` mean `1.5739x`。
Evidence Doctor 报告 `log/board/evidence_doctor.md` 为 Errors=0，Warnings=0，Suggestions=0。

Phase 010 pack+LZF production-shaped diagnostic 也已完成。`run_test_compare` 通过 4 个 gtest；
`run_qemu_smoke BENCH_ARGS="--case-filter compressed_* --iterations 2 --warmup-iterations 1"` 证明
compressed case 的日志形状可解析；`dump_bench_rvv` 仍可见 `vlse32.v` / `vse32.v`。
板卡 repeated summary `log/board/production_shaped_repeat_5/summary.md` 显示：
`compressed_pointxyzrgb_4f_262k` mean `1.3904x`、`compressed_pointxyzrgb_4f_padding_262k`
mean `1.3526x`、`compressed_pointxyzrgb_4f_small_512` mean `1.3493x`。
Evidence Doctor `log/board/production_shaped_repeat_5/evidence_doctor.md` 为
Errors=0，Warnings=0，Suggestions=0；`log/evidence_registry.json` 已登记该 run。

Phase 040 binary writer component ablation 已完成。`run_test_compare` 通过 6 个 gtest；
`run_qemu_smoke BENCH_ARGS="--case-filter binary_* --iterations 2 --warmup-iterations 1"` 证明
binary case 的日志形状可解析；`dump_bench_rvv` 可见 `vlse32.v` / `vsse32.v`。板卡 repeated summary
`log/board/binary_component_repeat_5/summary.md` 显示：
`binary_pointxyzrgb_4f_262k` median `1.2738x`，`binary_pointxyzrgb_4f_padding_262k`
median `1.1842x`，`binary_pointxyzrgb_4f_small_512` median `6.8125x`。
Evidence Doctor `log/board/binary_component_repeat_5/evidence_doctor.md` 为
Errors=0，Warnings=1，Suggestions=0；Warning 指出 small case 是 group outlier，因此 small case
只作为 smoke-shaped positive 单独报告。

Phase 060 production-public evidence 已完成。`run_test_compare` 通过 Std/RVV 各 8 个 gtest；新增 public
writer test 会写临时 compressed PCD 文件、解压 payload 并与标量 pack byte-equal 对拍，同时验证 RVV build
命中 production RVV helper，mixed-size 自定义点型走 scalar fallback。`dump_bench_rvv` 在
`writeBinaryCompressed<PCDTWPointXYZRGBPadding>` 和 `<PCDTWPointXYZRGBCompact>` 符号附近可见
`vlse32.v` / `vse32.v`。板卡 repeated summary
`log/board/production_compressed_repeat_5/summary.md` 显示：
`production_compressed_pointxyzrgba_4f_compact_262k` mean `1.3122x`，
`production_compressed_pointxyzrgba_4f_padding_262k` mean `1.2495x`，
`production_compressed_pointxyzrgba_4f_compact_small_512` mean `1.1239x`。
Evidence Doctor `log/board/production_compressed_repeat_5/evidence_doctor.md` 为
Errors=0，Warnings=4，Suggestions=0；大规模 warning 是 long-tail / variance 但 min 仍高于 `1.19x`，
small case 有 1/5 退化，降级为 smoke-only。

Phase 080 binary writer production-public evidence 已完成并已回滚。回滚前 `run_test_compare` 通过 Std/RVV
各 11 个 gtest；binary public writer test 会写临时 binary PCD 文件，payload 与 scalar pack byte-equal 对拍，同时验证
RVV build 命中 production RVV helper，mixed-size 点型走 scalar fallback，indices overload 保持原标量行为。
`dump_bench_rvv` 在 `writeBinary<PCDTWPointXYZRGBField>` 和
`writeBinary<PCDTWPointXYZRGBFieldPadding>` 符号内可见 `vlse32.v` / `vsse32.v`。板卡 repeated summary
`log/board/production_binary_repeat_5/summary.md` 显示：
`production_binary_pointxyzrgb_4f_compact_262k` mean `0.9842x`，
`production_binary_pointxyzrgb_4f_padding_262k` mean `0.9727x`，
`production_binary_pointxyzrgb_4f_compact_small_512` mean `0.9810x`。
Evidence Doctor `log/board/production_binary_repeat_5/evidence_doctor.md` 为
Errors=3，Warnings=0，Suggestions=0；三个 Error 均为 `ba_degradation_frequency`，因此该 binary writer
patch 不支持 production adoption。用户确认负收益可回滚后，当前生产源码已删除该 binary writer patch；
该历史 field-outer 实现族不再作为当前生产路径。

Phase 090 binary tuple / segment diagnostic 已完成。`run_test_compare` 通过 Std/RVV 各 11 个 gtest；
板卡 repeated summary `log/board/binary_tuple_segment_repeat_5/summary.md` 显示 diagnostic compact
mean `9.8247x`、padding mean `4.0905x`、small smoke mean `18.0526x`。Evidence Doctor
`log/board/binary_tuple_segment_repeat_5/evidence_doctor.md` 为 Errors=0，Warnings=2，Suggestions=0。
该结果只作为 Phase 100 的前置信号，不能替代 production evidence。

Phase 100 binary tuple / segment production-public evidence 已完成并采纳。`run_test_compare` 通过
Std/RVV 各 15 个 gtest；binary public writer test 会写临时 binary PCD 文件，payload 与 scalar
pack byte-equal 对拍，同时验证 compact layout 命中 memcpy path、padding layout 命中 RVV path、
mixed-size 点型 fallback、indices overload 保持原标量行为。QEMU smoke
`run_bench_rvv BENCH_ARGS="--case-filter production_binary_tuple_* --iterations 2 --warmup-iterations 1"`
只证明日志形状；`dump_bench_rvv` 在 production bench binary 中可见 padding path 的
`vlse32.v` / `vsseg4e32.v`。板卡 repeated summary
`log/board/production_binary_tuple_repeat_5/summary.md` 显示：
`production_binary_tuple_pointxyzrgba_4f_compact_262k` mean `1.3046x`，
`production_binary_tuple_pointxyzrgba_4f_padding_262k` mean `1.3240x`，
`production_binary_tuple_pointxyzrgba_4f_compact_small_512` mean `1.1082x`。
Evidence Doctor `log/board/production_binary_tuple_repeat_5/evidence_doctor.md` 为
Errors=0，Warnings=1，Suggestions=0；small case 的 long-tail warning 降级为 smoke-only，不影响大规模
compact / padding 的采纳判断。

## 生产接入判断

当前 production decision（生产判断）：

- compressed writer production patch 已采纳。正式文档为 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`，证据基于
  Phase 060 接入后的 production-public 板卡数据。
- `writeBinary(file_name, cloud)` field-outer RVV production patch 已完成 PI2-PI5，但板卡 production-public
  结果为负向；用户确认负收益可回滚后，该历史实现族已回滚。
- `writeBinary(file_name, cloud)` tuple / segment production path 已完成 Phase 100 接入后验证，当前对
  4 个连续 4-byte effective fields 采纳 compact memcpy / padding segment path，其它布局 fallback。
- `writeASCII`、indices overload 和 PCLPointCloud2 路径保持标量或独立 topic。

下一默认动作：停在当前 closeout，不继续自动扩大 production。若用户授权继续探索，应另开 narrow phase，
先冻结 arbitrary compact payload memcpy、non-4-byte fields、更多 point type / layout 或 ASCII profile
中的一个具体范围，并重新做 production-public 证据闭环。
