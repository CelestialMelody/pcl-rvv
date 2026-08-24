# Phase 010 结果：production-shaped compressed writer diagnostic

## 阶段范围回填

本阶段验证 `PCDWriter::writeBinaryCompressed(std::ostream&, const PCLPointCloud2&, ...)` 的 compressed
payload（压缩 payload）生成形态。测试专用 wrapper 覆盖字段过滤后的 AoS（结构数组）到 field-major
（字段连续）打包、LZF compression（LZF 压缩）和 8 字节 compressed / uncompressed size header。
production（生产源码）没有修改。

validated_scope：

- writer payload shaped path：4 字节 `xyzi` 连续布局和尾部 padding `xyzi` 布局。
- A/B boundary（对比边界）：同一个 test helper / production-shaped payload wrapper。
- timer boundary（计时边界）：pack 到 field-major 加 LZF；checksum 在计时后执行。
- evidence role（证据角色）：production-shaped diagnostic（生产形态诊断）。

unvalidated_scope：

- `generateHeaderBinaryCompressed` 文本 header、`std::ostream` flush、file-name overload、mmap / file lock / `msync`。
- reader unpack、finite scan（有限值扫描）、non-4-byte fields、`impl/pcd_io.hpp` templated writer。
- production dispatch（生产分流）、fallback（回退路径）、production direct（真实生产路径证据）和长期 `doc-rvv`。

## 计划动作结果

| action | status | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| B1 写 RED correctness test | done | `run_test_compare` 曾因 `makeCompressedWriterPayload*` helper 缺失编译失败。 | RED 命中缺失行为，不是测试拼写问题。 |
| B2 实现 test-only payload helper | done | `include/impl/pcd_io_support.hpp` | scalar 与 candidate compressed payload bytes 完全一致；RVV build 命中 pack RVV path。 |
| B3 扩展 bench case | done | `src/bench_pcd_io.cpp` | 新增 `writer_payload_xyzi_307k` 和 `writer_payload_padded_xyzi_307k`。 |
| B4 扩展 manifest wrapper | done | `script/generate_pcd_io_evidence_manifest.py` | writer payload case 输出 `production_shaped_diagnostic` comparison 和 top-level summary role。 |
| B5 扩展 Make / board target | done | `Makefile` | 新增 `run_board_pcd_io_writer_payload_repeated`，证据写入独立 run 目录。 |
| B6 验证 | done | 见下方验证表 | correctness、QEMU、asm、board、doctor 和 registry 均闭合。 |
| B7 文档回填 | done | 本文件、evaluation、roadmap、matrix | 本阶段结论为 `partial-production-candidate`，默认下一步是 PI1。 |

## 验证结果

| 证据层 | 命令 / 路径 | 结果 |
| --- | --- | --- |
| RED | `make -C test-rvv/io/pcd_io run_test_compare` | 新测试先因 payload helper 不存在编译失败。 |
| correctness | `make -C test-rvv/io/pcd_io run_test_compare` | Std/RVV 两侧各 4 个 gtest 通过。 |
| QEMU smoke | `make -C test-rvv/io/pcd_io run_qemu_smoke` | 新增 writer payload case 可运行并输出 checksum；QEMU timing 不作为性能证据。 |
| asm attribution | `make -C test-rvv/io/pcd_io dump_bench_rvv`；`build/asm/riscv/bench_pcd_io_rvv.asm` | 可见 `vlse32.v` 和 `vse32.v`，归属到 `makeCompressedWriterPayloadCandidate` 内联 bench path。 |
| board repeated | `make -C test-rvv/io/pcd_io run_board_pcd_io_writer_payload_repeated` | 5-run summary 稳定 positive。 |
| Evidence Doctor | `log/board/writer_payload_repeat_5/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=4。 |
| evidence registry | `log/evidence_registry.json`；`evidence_registry.py check ... --fail-on any` | writer payload summary / manifest / doctor 为 fresh。 |

## Board performance

| case | median | min | max | values | decision bucket |
| --- | ---: | ---: | ---: | --- | --- |
| `writer_payload_xyzi_307k` | 1.18x | 1.14x | 1.19x | 1.19x, 1.14x, 1.18x, 1.18x, 1.18x | positive |
| `writer_payload_padded_xyzi_307k` | 1.12x | 1.09x | 1.13x | 1.13x, 1.09x, 1.11x, 1.13x, 1.12x | positive |

LZF 没有吞掉 4 字节字段 pack 的收益。padded writer payload 仍稳定高于 1.08x positive 阈值。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| actual evidence role | production-shaped diagnostic |
| actual A/B boundary | test helper / production-shaped payload wrapper |
| timer boundary | pack 到 field-major 加 LZF；checksum 在计时后执行 |
| row source | PCLPointCloud2 writer payload AoS 到 LZF compressed payload |
| point type / layout | synthetic PCLPointCloud2，4 字节字段，连续 `xyzi` 和尾部 padding `xyzi` |
| baseline / candidate | scalar pack plus LZF vs RVV stride pack plus LZF |
| 能证明什么 | writer payload 形态在板卡上仍有 positive 性能信号。 |
| 不能证明什么 | 不能证明真实 public overload、file-name overload、fallback、non-4-byte fields 或 production asm 已闭合。 |
| weak / negative / neutral / unstable 时 production probe 条件 | 本阶段不是 weak / negative / neutral / unstable；若下一阶段 production direct 变弱，应以 production direct 证据降级。 |
| clean adoption 是否需要 production boundary 内 A/B | 需要。当前只能支持进入 PI1 production integration plan。 |

## Evidence Doctor 和 registry

Evidence Doctor 输入：

- `log/board/writer_payload_repeat_5/evidence_manifest.json`
- `log/board/writer_payload_repeat_5/summary.md`

结果：

- Errors=0
- Warnings=0
- Suggestions=4

Suggestions 是环境 metadata（taskset / governor / freq / temperature）和 binary identity（binary hash）缺失。
当前 5-run decision bucket 稳定，checksum 一致，因此不降级 writer payload diagnostic。若 PI4 production
direct 出现方向反转或长尾，应补二进制身份和环境字段后重跑。

registry 状态：

- `python3 test-rvv/script/evidence_registry.py check --registry test-rvv/io/pcd_io/log/evidence_registry.json --scan-glob 'test-rvv/io/pcd_io/log/board/writer_payload_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}' --fail-on any`
- 输出 `evidence registry check: fresh`。

## S10 EvidenceDecision

EvidenceDecision：`partial-production-candidate`。

理由：

- writer component 和 writer payload shaped path 在板卡上均 positive。
- correctness、QEMU smoke、asm、board repeated、Evidence Doctor 和 registry 已闭合到 test-only / production-shaped boundary。
- production direct、fallback、non-RVV build、layout gate、real public overload 和 production asm 尚未闭合。

production_decision：

- 本阶段不修改 `io/src/pcd_io.cpp`。
- 默认下一步是 PI1 production integration plan（生产接入计划），只冻结 writer `std::ostream` overload 的
  4 字节字段 RVV pack 候选范围。
- PI1 之后是否进入 PI2 production patch 需要用户确认或后续明确授权。

## 阶段反思和后续队列

| candidate / action | status | reason | resume condition |
| --- | --- | --- | --- |
| writer `std::ostream` production integration | partial-production-candidate | component 和 payload shaped 证据均 positive。 | 用户确认进入 PI1/PI2，先冻结 fallback 和 layout gate。 |
| reader unpack production-shaped diagnostic | deferred | unpack component 正向较弱，且 reader 还有 finite scan。 | writer PI1 或用户选择暂缓 writer 后再排。 |
| finite scan RVV mask | deferred | 与 layout conversion 是不同数据流和类型 switch 风险。 | 单独 phase 建 NaN / Inf oracle 和 board summary。 |
| topic-local doc suite split | phase_deferred + unblocked | 当前 README / evaluation 已能恢复，但复杂度已上升。 | production gate 暂停或 PI1 前后补 `testing-overview`、`benchmark-and-evidence`、`test-support-code-map`。 |

continue_stop_decision：

- Phase 010 完成。
- 当前没有证据或板卡 blocker。
- 继续到 PI2 会修改 production 源码，命中 production authorization checkpoint（生产授权检查点）。
- next_phase_default：`PI1 production_integration_plan for writer std::ostream payload`。
