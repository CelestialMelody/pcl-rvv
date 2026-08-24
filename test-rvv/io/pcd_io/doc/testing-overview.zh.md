# pcd_io 测试总览

本文说明 `test-rvv/io/pcd_io` 的测试入口、证据边界和 target 粒度。每个 TEST 的输入和断言归到
`correctness-tests.zh.md`；bench case、summary、Evidence Doctor（证据体检）和 registry（证据登记表）
归到 `benchmark-and-evidence.zh.md`。

## 阅读路径

| 问题 | 主入口 |
| --- | --- |
| 当前 EvidenceDecision（证据决策） | `pcd_io-evaluation.zh.md` |
| TEST 语义和断言 | `correctness-tests.zh.md` |
| bench label、board summary、doctor、registry | `benchmark-and-evidence.zh.md` |
| candidate 状态和后续路线 | `optimization-evidence.zh.md`、`optimization-roadmap.zh.md` |
| 测试支撑代码定位 | `test-support-code-map.zh.md` |

## 运行入口分类

| target / entry | 类别 | 证明范围 | 不能证明 |
| --- | --- | --- | --- |
| `make -C test-rvv/io/pcd_io run_test_compare` | correctness aggregate（正确性汇总入口） | Std / RVV build 中 test-only pack/unpack、writer / reader shaped helper，以及 production public writer payload 与 scalar reference 一致。 | 不证明 reader、filename overload 或 templated writer。 |
| `make -C test-rvv/io/pcd_io run_qemu_smoke` | QEMU smoke（仿真器小型验证） | correctness 和 RVV bench binary 日志形状可运行。 | QEMU timing 不作为性能证据。 |
| `make -C test-rvv/io/pcd_io dump_bench_rvv` | asm attribution（反汇编归属） | bench binary 中存在 `vlse32.v` / `vse32.v` 等 RVV 指令；Phase 050 归属到 public writer pack path。 | 不单独证明性能。 |
| `make -C test-rvv/io/pcd_io run_board_pcd_io_repeated` | board repeated（板卡重复采集） | Phase 000 component ablation 的 repeated board summary、manifest、doctor 和 registry。 | 不含 LZF、header、ostream 和 production dispatch。 |
| `make -C test-rvv/io/pcd_io run_board_pcd_io_writer_payload_repeated` | board repeated | Phase 010 writer payload production-shaped diagnostic。 | 不含真实 public overload 和 fallback。 |
| `make -C test-rvv/io/pcd_io run_board_pcd_io_reader_payload_repeated` | board repeated | Phase 040 reader payload production-shaped diagnostic。 | 不含真实 public reader、mmap / file I/O 和 finite scan RVV。 |
| `make -C test-rvv/io/pcd_io run_board_pcd_io_production_writer_repeated` | board repeated | Phase 050 writer public `std::ostream` overload production-public summary、manifest、doctor 和 registry。 | 不覆盖 reader、filename overload、templated writer 或 non-4-byte fields。 |
| `run_board_bench_compare` | board smoke / primitive target | 单次部署和采集 Std/RVV bench compare。 | 单次结果不替代 repeated summary。 |

## Target 粒度审计

| target 类别 | 当前状态 | 证据 / 说明 | 下一步 |
| --- | --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` 覆盖 Std/RVV test executable，当前 10 个 TEST 均在 aggregate 中运行。 | 可选拆分 gtest-filter alias，不阻塞当前采纳。 |
| correctness aliases | adopted by aggregate | 当前没有独立 gtest-filter target；production public / fallback tests 已纳入 aggregate。 | 若 reviewer 需要更细 CI 粒度，再补 alias。 |
| bench diagnostic aliases | adopted | CLI 支持 `--case-filter component`、`all`、单 case label、`writer_payload`、`reader_payload` 和 `production_writer` family。 | none。 |
| QEMU smoke aliases | adopted | `run_qemu_smoke` 组合 correctness 和 RVV bench 可运行性。 | 继续禁止 QEMU bench compare 作为性能结论。 |
| board smoke aliases | adopted | shared `run_board_bench_compare` 提供单次 board smoke。 | 只作为采集 primitive。 |
| board repeated aliases | adopted | component、writer payload、reader payload 和 production writer repeated targets 均存在。 | none inside current boundary。 |
| doctor / registry aliases | adopted | Makefile 中有 generate / doctor / record target，production writer evidence 已登记 fresh。 | none inside current boundary。 |
| historical probe guarded aliases | not_applicable with evidence | 当前无历史 production probe target。 | 无。 |

## 输入数据和覆盖矩阵

| path / case | row source（行来源） | layout | evidence role（证据角色） | 当前状态 |
| --- | --- | --- | --- | --- |
| pack component | PCLPointCloud2 contiguous rows | `xyzi` 4x u32，连续或尾部 padding | component ablation（组件消融） | positive |
| unpack component | PCLPointCloud2 contiguous rows | `xyzi` 4x u32，连续或尾部 padding | component ablation | positive / weak-positive |
| writer payload | PCLPointCloud2 writer payload AoS 到 LZF | `xyzi` 4x u32，连续或尾部 padding | production-shaped diagnostic（生产形态诊断） | positive |
| reader payload | PCLPointCloud2 reader payload LZF 到 AoS，再做 finite scan | `xyzi` 3x `FLOAT32` + 1x payload，连续或尾部 padding | production-shaped diagnostic | weak-positive |
| production writer public | `writeBinaryCompressed(std::ostream&, ...)` | 4 字节有效字段，offset 和 `point_step` 4 字节对齐 | production-public | adopted |

## 推荐测试流程

1. 本地 / QEMU correctness：`run_test_compare`。
2. QEMU smoke：`run_qemu_smoke`，只看可运行性和日志形状。
3. 反汇编：`dump_bench_rvv`。
4. 板卡 repeated：按 phase 选择 component、writer payload、reader payload 或 production writer target。
5. Evidence Doctor 和 registry：使用 Makefile 的 `run_board_pcd_io_*_evidence_doctor` 与 `record_board_pcd_io_*_evidence_state` 入口，或执行 repeated aggregate target 自动完成。

## 当前结论边界

当前测试套件支持 Phase 050 的窄范围 `adopted production behavior`：writer public `std::ostream`
overload 的 4 字节对齐有效字段 pack。它不支持 reader、filename overload、templated writer、
ASCII writer、non-4-byte fields 或 generic PCD layout 的 production 结论。
