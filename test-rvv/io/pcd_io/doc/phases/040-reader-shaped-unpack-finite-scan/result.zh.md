# Phase 040 结果：reader shaped unpack + finite scan 诊断

## 阶段范围回填

本阶段验证 reader compressed payload（读取压缩 payload）生产形态诊断：LZF decompression（LZF 解压）
之后执行 field-major（字段连续）到 AoS（结构数组）unpack，再执行标量 finite scan（有限值扫描）。
production（生产源码）没有修改。

validated_scope：

- reader payload shaped path：4 字节 `xyzi` 连续布局和尾部 padding `xyzi` 布局。
- finite scan：`x/y/z` 三个 `FLOAT32` 字段，夹具默认写入有限值，并在一个点注入 NaN 证明 dense flag（稠密标记）一致。
- A/B boundary（对比边界）：同一个 test helper / production-shaped payload wrapper。
- timer boundary（计时边界）：LZF 解压、unpack、finite scan；checksum 在计时后执行。
- evidence role（证据角色）：production-shaped diagnostic（生产形态诊断）。

unvalidated_scope：

- 真实 `PCDReader::read` public overload、header parsing、mmap range、file I/O、uncompressed binary path。
- finite scan RVV mask、`FLOAT64`、non-4-byte fields、mixed datatype count、reader production dispatch。
- writer production patch。PI2 仍需要用户明确确认。

## 计划动作结果

| action | status | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| R1 reader-shaped correctness test | done | `run_test_compare` 先因 helper 缺失失败，修复后 Std/RVV 各 5 个 gtest 通过。 | payload decode、unpack、finite scan 和 dense flag 与 scalar reference 一致。 |
| G1 reader helper | done | `include/impl/pcd_io_support.hpp` | 新增 `FiniteKind`、finite float fixture、`readCompressedBodyScalar/Candidate`。 |
| B1 reader payload bench | done | `src/bench_pcd_io.cpp` | 新增 `reader_payload_xyzi_307k` 和 `reader_payload_padded_xyzi_307k`，支持 `--case-filter reader_payload`。 |
| E1 manifest / board target | done | `script/generate_pcd_io_evidence_manifest.py`、`Makefile` | 新增 `run_board_pcd_io_reader_payload_repeated`，证据写入 `reader_payload_repeat_5`。 |
| target hygiene | done | `bench_pcd_io.cpp`、`Makefile` | 新增 `component` case-filter，并把 component repeated target 从 `all` 收窄到 component，避免未来复跑混入 shaped case。 |
| 验证 | done | 见下方验证表 | correctness、QEMU、asm、board、doctor 和 registry 均闭合。 |

## 验证结果

| 证据层 | 命令 / 路径 | 结果 |
| --- | --- | --- |
| correctness | `make -C test-rvv/io/pcd_io run_test_compare` | Std/RVV 两侧各 5 个 gtest 通过。 |
| QEMU smoke | `make -C test-rvv/io/pcd_io run_qemu_smoke` | reader payload case 可运行；QEMU timing 不作为性能证据。 |
| asm attribution | `make -C test-rvv/io/pcd_io dump_bench_rvv`；`build/asm/riscv/bench_pcd_io_rvv.asm` | 可见 `vle32.v` / `vsse32.v`，归属到 reader payload bench 内联 unpack candidate path。 |
| board repeated | `make -C test-rvv/io/pcd_io run_board_pcd_io_reader_payload_repeated` | 5-run summary 为 weak-positive。 |
| Evidence Doctor | `log/board/reader_payload_repeat_5/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=5。 |
| evidence registry | `evidence_registry.py check ... reader_payload_repeat_5 ... --fail-on any` | fresh。 |

## Board performance

| case | median | min | max | values | decision bucket |
| --- | ---: | ---: | ---: | --- | --- |
| `reader_payload_xyzi_307k` | 1.07x | 1.04x | 1.10x | 1.04x, 1.10x, 1.09x, 1.06x, 1.07x | weak-positive |
| `reader_payload_padded_xyzi_307k` | 1.03x | 1.01x | 1.05x | 1.01x, 1.04x, 1.03x, 1.01x, 1.05x | weak-positive |

LZF decompression 和标量 finite scan 稀释了 unpack component 的收益。连续布局仍有弱正向信号；padding 布局接近
1.0 阈值，不能写成稳定加速。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| actual evidence role | production-shaped diagnostic |
| actual A/B boundary | test helper / reader-shaped payload wrapper |
| timer boundary | LZF 解压、unpack、finite scan；checksum 在计时后执行 |
| row source | PCLPointCloud2 reader payload LZF 到 AoS，再做 dense scan |
| point type / layout | synthetic PCLPointCloud2，3 个 `FLOAT32` finite 字段和 1 个 payload 字段，连续 `xyzi` 和尾部 padding `xyzi` |
| baseline / candidate | scalar unpack plus scalar finite scan vs RVV unpack plus scalar finite scan |
| 能证明什么 | reader shaped boundary 下 4 字节 unpack 仍有可归因弱收益。 |
| 不能证明什么 | 不能证明真实 public reader、mmap / file I/O、header parsing、finite scan RVV 或 production dispatch 值得接入。 |
| weak / negative / neutral / unstable 时 production probe 条件 | 本阶段为 weak-positive，且 padding 接近阈值；不建议进入 reader production probe。 |
| clean adoption 是否需要 production boundary 内 A/B | 需要。当前结果不能 clean adopt，也不能修改 reader production。 |

## Evidence Doctor 和 registry

Evidence Doctor 输入：

- `log/board/reader_payload_repeat_5/evidence_manifest.json`
- `log/board/reader_payload_repeat_5/summary.md`

结果：

- Errors=0
- Warnings=0
- Suggestions=5

Suggestions 是环境 metadata、binary identity 缺失，以及 `reader_payload_padded_xyzi_307k` near-threshold。当前
checksum 一致、没有 Error / Warning，但收益桶只能写成 weak-positive。

registry 状态：

- `python3 test-rvv/script/evidence_registry.py check --registry test-rvv/io/pcd_io/log/evidence_registry.json --scan-glob 'test-rvv/io/pcd_io/log/board/reader_payload_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}' --fail-on any`
- 输出 `evidence registry check: fresh`。

## EvidenceDecision

EvidenceDecision：`attempted / reader-shaped weak-positive`。

production_decision：

- 本阶段不修改 `io/src/pcd_io.cpp`。
- reader 方向不进入 production integration loop（生产接入闭环）。
- writer `std::ostream` overload 的 PI2 production patch 仍是主线生产候选，但需要用户明确确认。

## 阶段反思和后续队列

| candidate / action | status | reason | next action |
| --- | --- | --- | --- |
| reader production probe | rejected for now | shaped boundary 只有 weak-positive，padding 接近阈值。 | 除非后续 profile 指向 reader unpack / scan，否则不进入 PI1。 |
| finite scan RVV mask | deferred / lower priority | reader shaped 结果显示仅替换 unpack 不够强；finite scan 仍可能是独立诊断，但不能从本阶段直接推出 production 价值。 | 若用户要求继续 reader 方向，可另开 finite scan component phase。 |
| writer production patch | turn_stop_deferred with stop_condition_hit | 继续主线会修改 production 源码。 | 用户确认后进入 PI2。 |

continue_stop_decision：

- Phase 040 完成。
- 当前没有板卡 blocker。
- reader 方向不建议继续到 production；主线 writer PI2 仍命中 production patch authorization checkpoint。
- next_phase_default：`PI2 production_patch for writer std::ostream 4-byte field payload`，等待用户确认。
