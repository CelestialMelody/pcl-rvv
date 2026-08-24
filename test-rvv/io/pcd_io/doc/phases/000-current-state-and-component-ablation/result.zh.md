# Phase 000 结果：current state and component ablation

## 阶段范围回填

本阶段按 `plan.zh.md` 只验证 `io/src/pcd_io.cpp` 中 PCLPointCloud2 binary_compressed（压缩二进制）
pack/unpack 的字段布局转换组件。production（生产源码）没有修改；所有 helper 都位于
`test-rvv/io/pcd_io`。

validated_scope（已验证范围）：

- writer component：4 字节字段的 AoS（结构数组）到 field-major（字段连续）打包。
- reader component：4 字节字段的 field-major 到 AoS 解包。
- layout：`xyzi` 连续布局和带尾部 padding 的 `xyzi` 布局。
- evidence role（证据角色）：component ablation（组件消融）。

unvalidated_scope（未验证范围）：

- LZF compression / decompression（LZF 压缩 / 解压）、header、`std::ostream`、mmap / file I/O。
- `readBodyBinary` 后续 finite scan（有限值扫描）。
- `impl/pcd_io.hpp` templated writer（模板 writer）。
- production dispatch（生产分流）、fallback（回退路径）和所有 datatype / count / offset 泛型组合。

## 计划动作结果

| action | status | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| A1 写 failing correctness test | done | 早期 RED 已通过 `run_test_compare` 捕捉缺失 helper；当前可复跑命令见下方验证表。 | 测试能覆盖 pack、unpack 和 mixed field fallback。 |
| A2 实现 test-only scalar / RVV candidate | done | `include/impl/pcd_io_support.hpp` | RVV build 在 4 字节字段命中 stride load/store；非 RVV 或混合字段回到标量参考链路。 |
| A3 写 component bench | done | `src/bench_pcd_io.cpp` | bench 输出 dataset、iterations、checksum、points 和 point_step。 |
| A4 反汇编归属 | done | `build/asm/riscv/bench_pcd_io_rvv.asm` | 可见 `vlse32.v`、`vle32.v`、`vse32.v`、`vsse32.v`，归属到 bench 内联候选路径。 |
| A5 板卡 repeated bench | done | `log/board/component_ablation_repeat_5/summary.md` | 5-run decision bucket 稳定。 |
| A6 Evidence Doctor 和 registry | done | `log/board/component_ablation_repeat_5/evidence_doctor.md`、`log/evidence_registry.json` | Errors=0，Warnings=0，Suggestions=9；registry check 为 fresh。 |
| A7 文档回填 | done | 本文件、evaluation、roadmap、optimization matrix | Phase 000 关闭为 diagnostic positive，下一阶段进入 production-shaped writer diagnostic。 |

## 证据分层

correctness（正确性）：

- `make -C test-rvv/io/pcd_io run_test_compare`
- Std/RVV 两侧各 3 个 gtest 通过。测试覆盖 pack、padded unpack 和 mixed field scalar fallback。

QEMU path evidence（QEMU 路径证据）：

- `make -C test-rvv/io/pcd_io run_qemu_smoke`
- QEMU 只证明 correctness 和日志形状，不参与性能结论。

asm attribution（反汇编归属）：

- `make -C test-rvv/io/pcd_io dump_bench_rvv`
- `build/asm/riscv/bench_pcd_io_rvv.asm` 中存在 `vlse32.v` / `vse32.v` 的 pack 路径，以及
  `vle32.v` / `vsse32.v` 的 unpack 路径。helper 在 bench 中内联，因此当前归属粒度是
  `packFieldsCandidate/unpackFieldsCandidate inlined bench path`。

board performance（板卡性能）：

| case | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | --- |
| `pack_xyzi_307k` | 1.20x | 1.19x | 1.20x | positive |
| `pack_padded_xyzi_307k` | 1.12x | 1.12x | 1.13x | positive |
| `unpack_xyzi_307k` | 1.11x | 1.07x | 1.15x | positive |
| `unpack_padded_xyzi_307k` | 1.03x | 1.02x | 1.06x | weak-positive |

pack 两条路径正向稳定。普通 unpack 正向但波动略大。padded unpack 的 median 是 1.03x，按本阶段
阈值属于 weak-positive（弱正向），只能作为继续诊断的信号。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| actual evidence role | component ablation |
| actual A/B boundary | test helper / bench wrapper |
| timer boundary | 只计 layout conversion；checksum 在计时后执行 |
| row source | PCLPointCloud2 AoS 到 field-major，以及 field-major 到 AoS |
| point type / layout | synthetic PCLPointCloud2，4 字节字段，连续 `xyzi` 和尾部 padding `xyzi` |
| baseline / candidate | scalar reference vs RVV stride u32 helper |
| 能证明什么 | 4 字节字段 pack/unpack 组件在板卡上有可归因收益。 |
| 不能证明什么 | 不能证明 LZF、header、ostream、mmap / file I/O、finite scan 或 production dispatch 后仍有收益。 |
| weak / negative / neutral / unstable 时 production probe 条件 | 本轮有 weak-positive 的 padded unpack，但 writer pack 为 positive。允许下一阶段做 writer production-shaped diagnostic；reader 先暂缓到有限扫描或 read shaped phase。 |
| clean adoption 是否需要 production boundary 内 A/B | 需要。当前结果不能 clean adopt，也不能修改 `io/src/pcd_io.cpp`。 |

## Evidence Doctor 和 registry

Evidence Doctor 输入：

- `log/board/component_ablation_repeat_5/evidence_manifest.json`
- `log/board/component_ablation_repeat_5/summary.md`

结果：

- Errors=0
- Warnings=0
- Suggestions=9

Suggestions 主要是缺少 taskset / governor / freq / temperature 环境字段、缺少 binary hash，以及
`unpack_padded_xyzi_307k` near-threshold（接近阈值）。这些不阻塞 component decision，但会限制
后续 production-shaped 结论。010 阶段应继续记录 run count、warmup 和 device；若出现方向反转，再补
二进制身份或环境字段。

registry 状态：

- `python3 test-rvv/script/evidence_registry.py check --registry test-rvv/io/pcd_io/log/evidence_registry.json --scan-glob 'test-rvv/io/pcd_io/log/board/component_ablation_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}' --fail-on any`
- 输出 `evidence registry check: fresh`。

## Doc-suite 与测试支撑形态审计

| area | current shape scan | decision | evidence | next action |
| --- | --- | --- | --- | --- |
| test/bench source layout | 已使用 `src/test_pcd_io.cpp` 与 `src/bench_pcd_io.cpp`。 | adopted | `Makefile`、`src/` | 继续保持。 |
| aggregator and internal helpers | 已使用 `include/pcd_io.h` 与 `include/impl/pcd_io_support.hpp`；无旧 `test_support/`。 | adopted | `include/`、`include/impl/` | 当前 helper 约 205 行，暂不拆分。 |
| script and bench registry | 已有 topic-local manifest wrapper 和 registry。 | adopted | `script/generate_pcd_io_evidence_manifest.py`、`log/evidence_registry.json` | 010 阶段扩展 case 字典。 |
| target granularity | 已有 correctness aggregate、QEMU smoke、board repeated、doctor / registry target。 | adopted | `Makefile`、`board.mk` | 010 阶段新增 writer-shaped repeated target。 |
| topic-local docs | README、evaluation、roadmap、phase index、matrix 已存在；测试总览 / bench 证据 / code map 尚合并在 README 和 evaluation。 | phase_deferred + unblocked | 本 phase 结果 | 若 010 后仍继续，应补 `testing-overview`、`benchmark-and-evidence` 和 `test-support-code-map`。 |
| production topic doc | 当前无 adopted production behavior。 | not_applicable with evidence | 未修改 production | 不创建 `doc-rvv/io/pcd_io-RVV.zh.md`。 |
| legacy compatibility | 无旧路径 pointer 或兼容别名。 | not_applicable with evidence | topic 为新建目录 | none |

## 阶段反思和下一阶段

pack component 的收益高于 reader padded unpack，因此下一阶段默认只推进 writer 方向的
production-shaped diagnostic（生产形态诊断）：把 pack 与 LZF compression 放进同一 payload 边界，模拟
`writeBinaryCompressed(std::ostream&, ...)` 中 header 后的 compressed payload 生成。reader unpack 和 finite scan
仍在 roadmap 中保留，但本轮不把弱正向 padded unpack 扩大成 production 结论。

continue_stop_decision：

- Phase 000 完成。
- 没有命中停止条件。
- next_phase_default：`010-production-shaped-compressed-writer-diagnostic`。
