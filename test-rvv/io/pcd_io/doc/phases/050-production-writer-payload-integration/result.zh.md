# Phase 050 结果：writer payload production integration

本阶段完成 PI2-PI5 production integration loop（生产接入闭环）。当前采用范围是
`PCDWriter::writeBinaryCompressed(std::ostream&, const PCLPointCloud2&, ...)` 中
binary_compressed payload 的 4 字节字段 pack：RVV build 在字段布局满足 gate 时使用 stride load
（跨步加载）把 AoS（结构数组）点数据打包成 field-major（字段连续）buffer，然后继续调用原有
`pcl::lzfCompress`；其它情况回退到标量 helper。

用户已给出本 topic 的采纳偏好：接入后的板卡测试如果显示有收益即可采纳。因此本阶段把该窄边界写成
`adopted production behavior`，并创建长期文档 `doc-rvv/io/pcd_io-RVV.zh.md`。该结论不扩大到 reader、
filename overload、templated writer、ASCII writer、non-4-byte fields 或其它 PCD layout。

## 计划执行回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| P1 production direct correctness test | done | `src/test_pcd_io.cpp`；`make -C test-rvv/io/pcd_io run_test_compare` | Std/RVV 两侧各 10 个 gtest 通过；public ostream payload 与 scalar oracle 一致。 |
| P2 production patch | done | `io/src/pcd_io.cpp` | 新增 Std helper、RVV helper、4 字节对齐 gate 和 public overload 短路分流；公开 API 不变。 |
| P3 production public bench | done | `src/bench_pcd_io.cpp`、`Makefile`、`script/generate_pcd_io_evidence_manifest.py` | 新增 `production_writer_*` case、5-run board target、manifest / doctor / registry。 |
| P4 验证 | done | correctness、QEMU smoke、asm、board repeated、Evidence Doctor、registry | production-public 证据闭合；QEMU timing 未用于性能结论。 |
| P5 文档 closeout | done | 本文件、matrix、roadmap、evaluation、topic-local docs、`doc-rvv/io/pcd_io-RVV.zh.md`、Handoff | 当前窄 production 行为已同步；未覆盖范围保留为 fallback / deferred。 |

## Production Diff 范围

生产源码只修改 `io/src/pcd_io.cpp`：

- 新增 `canUseRvvCompressedWriterPack`，要求 point count 非零、有效字段非空、字段 size 为 4、field offset 和
  `cloud.point_step` 按 4 字节对齐。
- 把原 `writeBinaryCompressed(std::ostream&, ...)` 中 AoS -> field-major 的双重循环抽成
  `packCompressedWriterFieldsStd`，并由 `writeBinaryCompressedPayloadStd` 保留标量路径。
- 在 `__RVV10__ && __riscv_vector` 下新增 `packCompressedWriterFieldsRVV`，每个有效字段用 `vlse32.v`
  从 AoS stride 读取，再用 `vse32.v` 连续写入 field-major buffer。
- public overload 先尝试 RVV payload pack；RVV gate 不满足或 LZF 压缩失败时自然回退 Std。
- `PCL_PCD_IO_RVV_TEST_HOOKS` 只服务 topic-local gtest 路径命中检查，不改变公开 API。

## 正确性与 fallback

| target / test | result | 证明范围 |
| --- | --- | --- |
| `make -C test-rvv/io/pcd_io run_test_compare` | Std/RVV 各 10 个 gtest 通过 | test-only component、writer shaped、reader shaped 和 production public ostream correctness。 |
| `PCDIOProductionWriter.PublicOstreamPayloadMatchesScalarOracleAndHitsRvvPath` | pass | 4 个 4-byte 字段时 public ostream payload 与 scalar oracle 一致；RVV build 命中 RVV path。 |
| `PCDIOProductionWriter.PublicOstreamFallsBackForMixedFieldSizes` | pass | mixed 4 / 2 / 1 / 4 字段回退 Std。 |
| `PCDIOProductionWriter.PublicOstreamFallsBackForUnalignedPointStep` | pass | `point_step` 非 4 字节对齐时回退 Std。 |
| `PCDIOProductionWriter.PublicOstreamFallsBackForUnalignedFieldOffset` | pass | field offset 非 4 字节对齐时回退 Std。 |
| `PCDIOProductionWriter.PublicOstreamIgnoresPaddingField` | pass | `_` padding field 被过滤，仍可在有效字段满足 gate 时命中 RVV。 |

## QEMU、反汇编和板卡证据

| evidence | command / path | result |
| --- | --- | --- |
| QEMU correctness | `make -C test-rvv/io/pcd_io run_test_compare` | Std/RVV correctness 通过；QEMU timing 不作为性能证据。 |
| QEMU bench smoke | `make -C test-rvv/io/pcd_io run_bench_rvv BENCH_ARGS="--case-filter production_writer --iterations 1 --warmup-iterations 0"` | production writer case 可运行；仅用于日志形状和路径 smoke。 |
| asm attribution | `make -C test-rvv/io/pcd_io dump_bench_rvv` | `bench_pcd_io_rvv.full.asm` 中 public writer path 可见 `vlse32.v` / `vse32.v`，随后进入 `pcl::lzfCompress`。 |
| board repeated | `make -C test-rvv/io/pcd_io run_board_pcd_io_production_writer_repeated` | Milkv-Jupiter，5 runs，20 iterations，3 warmup。 |
| board summary | `log/board/production_writer_repeat_5/summary.md` | `production_writer_xyzi_307k` median 1.33x；`production_writer_padded_xyzi_307k` median 1.25x。 |
| checksum | manifest / board logs | 两个 case Std/RVV checksum 均为 `15998081802654718142`。 |
| Evidence Doctor | `log/board/production_writer_repeat_5/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=4。 |
| registry | `log/evidence_registry.json` | production writer summary / manifest / doctor 已登记，freshness check 通过。 |

## Production Board Summary

| Benchmark Item | runs | median | min | max | values |
| --- | ---: | ---: | ---: | ---: | --- |
| `production_writer_xyzi_307k` | 5 | 1.33x | 1.32x | 1.33x | 1.32x, 1.33x, 1.33x, 1.33x, 1.33x |
| `production_writer_padded_xyzi_307k` | 5 | 1.25x | 1.24x | 1.28x | 1.25x, 1.25x, 1.26x, 1.28x, 1.24x |

Decision bucket（决策桶）：positive。5-run 预算已用完，方向稳定，无需继续自动复跑。

## Evidence Doctor 解释

Evidence Doctor 报告 Errors=0、Warnings=0、Suggestions=4。Suggestions 是环境 metadata（taskset、governor、
freq、temperature）和 binary identity（binary hash）缺失。它们不阻塞当前采纳，因为：

- repeated board 的两个 production-public case 方向稳定，min/median/max 都为正向；
- checksum 一致；
- manifest 已记录 device、iterations、warmup_iterations、run_count、wrapper、timer boundary 和 public overload
  boundary；
- 若未来出现方向反转或长尾，应优先补充环境字段和 binary hash 后重跑。

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_public` |
| A/B boundary | `public_overload`：`PCDWriter::writeBinaryCompressed(std::ostream&, ...)`。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path，且 fallback 是否保持语义。 |
| diagnostic 是否可外推到 production | Phase 010 只作为进入 probe 的依据；最终采纳使用本阶段 production-public 板卡数据。 |
| comparison-boundary / baseline mismatch 风险 | manifest 已把 baseline / candidate wrapper、row source、timer boundary 和 gate 对齐到同一 public overload。 |
| weak / negative / neutral / unstable 时动作 | 本轮不是 weak / neutral / negative / unstable；若未来复跑变成不稳定，应降级为 `stale_doc_pending_refresh` 并重新进入 PI5。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前 production 之前没有已采纳 RVV family；决策是 RVV-vs-scalar，不是 RVV-family-selection。 |

## Optimization Matrix 更新

| candidate family | evidence role | decision | unblocked next action |
| --- | --- | --- | --- |
| writer production RVV pack | production-public | adopted for `std::ostream` 4-byte aligned fields | none inside current authorized production boundary |
| reader shaped unpack + finite scan | production-shaped diagnostic | attempted / weak-positive | lower priority；不建议 reader production probe |
| finite scan RVV mask | diagnostic planned only | deferred / lower priority | 需要用户明确继续 reader 方向或 profile 指向 scan |
| filename overload / templated writer / ASCII writer / non-4-byte fields | out of scope | not_adopted | 另开 topic 或另建 phase 重新授权 |

## Continue / Stop Decision

continue_stop_decision：`turn_stop_deferred with stop_condition_hit`。

停止理由：

- 当前授权 production 边界内的 PI2-PI5 已闭合，production-public 板卡证据为 positive，且用户已给出“有收益即可采纳”的偏好。
- 当前没有值得在同一 production patch 内继续尝试的高优先级 code shape。`packCompressedWriterFieldsRVV` 已覆盖本阶段最清楚的 4-byte stride load/store 热点；再继续会扩大到 reader、finite scan、filename overload、templated writer、non-4-byte fields 或新的环境 metadata 采集，这些都超出本阶段窄采纳边界。
- Evidence Doctor 的 metadata suggestions 可作为后续证据质量增强，不足以要求本轮重跑。

next_phase_default：暂停。若用户要求继续同一 topic，优先级较高的可选方向是另建 phase 评估 reader finite scan RVV mask 或 filename overload public boundary；默认不在当前 production patch 内继续扩大。
