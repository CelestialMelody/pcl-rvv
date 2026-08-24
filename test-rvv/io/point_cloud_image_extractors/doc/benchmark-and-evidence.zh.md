# Benchmark And Evidence

## 本文职责

本文记录 bench label、case-filter、QEMU smoke（QEMU 小型验证）、board repeated（重复板卡测试）、
manifest 和 Evidence Doctor（证据体检）的证据边界。候选取舍索引见
`doc/optimization-evidence.zh.md`。

## Bench 输出格式

`src/bench_pcie.cpp` 每个 case 输出：

- label：case-filter 使用的稳定名称。
- `ms/iter` 和 `Total Time`：只在 board / target hardware 上作为性能证据。
- checksum：FNV-1a 风格 checksum，用于 Std/RVV 输出一致性检查。
- bytes / pixels：输出规模。

## Case-filter 字典

| label | candidate | 点类型 | 证明点 |
| --- | --- | --- | --- |
| `rgb_unpack_pointxyzrgb_640x480` | `rgb_u32_stride_unpack_v0` | `PointXYZRGB` | v0 RGB field load + unpack。 |
| `rgb_unpack_pointxyzrgba_640x480` | `rgb_u32_stride_unpack_v0` | `PointXYZRGBA` | v0 RGBA fallback field lookup。 |
| `rgb_segment_store_pointxyzrgb_640x480` | `rgb_segment_store_v1` | `PointXYZRGB` | `vsseg3e8` RGB store 形态。 |
| `rgb_segment_store_pointxyzrgba_640x480` | `rgb_segment_store_v1` | `PointXYZRGBA` | `vsseg3e8` RGBA store 形态。 |
| `scaling_full_range_intensity_640x480` | `scaling_float_stride_v0` | `PointXYZI` | full-range v0，已负向。 |
| `scaling_full_range_reduction_intensity_640x480` | `scaling_reduction_v1` | `PointXYZI` | full-range RVV min/max 规约。 |
| `scaling_fixed_factor_intensity_640x480` | `scaling_float_stride_v0` | `PointXYZI` | fixed-factor scaling 弱正向线索。 |
| `normal_field_pointnormal_640x480` | `normal_float_stride_v0` | `PointNormal` | normal_x/y/z 三路 float stride load，Phase 060 已负向。 |
| `label_mono16_pointxyzl_640x480` | `label_mono16_stride_v0` | `PointXYZL` | label mono16 的 `uint32_t` 跨步读取和低 16 位写回。 |
| `production_rgb_pointxyzrgb_640x480` | `production_rgb_segment_store_v1` | `PointXYZRGB` | 真实 RGB public extractor 命中窄范围 RVV 分流。 |
| `production_rgb_pointxyzrgba_640x480` | `production_rgb_segment_store_v1` | `PointXYZRGBA` | 真实 RGBA public extractor 命中窄范围 RVV 分流。 |
| `production_scaling_full_range_intensity_640x480` | `production_scaling_reduction_v1` | `PointXYZI` | 真实 intensity public extractor 命中 full-range RVV 规约分流。 |
| `production_label_mono16_pointxyzl_640x480` | `production_label_mono16_stride_v0` | `PointXYZL` | 真实 label public extractor 的 `COLORS_MONO` 命中窄范围 RVV 分流。 |

## 推荐命令

```bash
make run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 1 --case-filter <label>'
make dump_bench_rvv
make collect_board_repeated BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter all' PCIE_REPEATED_RUNS=5 PCIE_REPEATED_DIR=log/board/repeated_phase020
make collect_board_repeated PCIE_REPEATED_DIR=log/board/repeated_phase060 PCIE_REPEATED_RUNS=5 BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter normal_field_pointnormal_640x480'
make collect_board_repeated PCIE_REPEATED_DIR=log/board/repeated_phase070 PCIE_REPEATED_RUNS=5 BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter label_mono16_pointxyzl_640x480'
make collect_board_repeated PCIE_REPEATED_DIR=log/board/repeated_pi4 PCIE_REPEATED_RUNS=5 BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter production_rgb_pointxyzrgb_640x480,production_rgb_pointxyzrgba_640x480,production_scaling_full_range_intensity_640x480'
make collect_board_repeated PCIE_REPEATED_DIR=log/board/repeated_phase090 PCIE_REPEATED_RUNS=5 BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter production_label_mono16_pointxyzl_640x480'
make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_phase020
make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_phase060
make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_phase070
make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_pi4
make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_phase090
```

QEMU timing（QEMU 计时）不进入性能结论；只用于构建、路径和日志形状。

## 当前板卡证据

当前主证据路径：

- `log/board/repeated_phase020/summary.md`
- `log/board/repeated_phase020/evidence_manifest.json`
- `log/board/repeated_phase020/evidence_doctor.md`
- `log/board/repeated_phase060/summary.md`
- `log/board/repeated_phase060/evidence_manifest.json`
- `log/board/repeated_phase060/evidence_doctor.md`
- `log/board/repeated_phase070/summary.md`
- `log/board/repeated_phase070/evidence_manifest.json`
- `log/board/repeated_phase070/evidence_doctor.md`
- `log/board/repeated_pi4/summary.md`
- `log/board/repeated_pi4/evidence_manifest.json`
- `log/board/repeated_pi4/evidence_doctor.md`
- `log/board/repeated_phase090/summary.md`
- `log/board/repeated_phase090/evidence_manifest.json`
- `log/board/repeated_phase090/evidence_doctor.md`

关键 repeated board 结果：

| case | median | decision |
| --- | ---: | --- |
| `rgb_segment_store_pointxyzrgb_640x480` | 1.74x | positive |
| `rgb_segment_store_pointxyzrgba_640x480` | 1.75x | positive |
| `scaling_full_range_reduction_intensity_640x480` | 1.56x | positive |
| `scaling_full_range_intensity_640x480` | 0.92x | rejected for v0 |
| `normal_field_pointnormal_640x480` | 0.61x | rejected within diagnostic boundary |
| `label_mono16_pointxyzl_640x480` | 1.21x | diagnostic positive; promoted by Phase 090 |
| `production_rgb_pointxyzrgb_640x480` | 1.54x | adopted |
| `production_rgb_pointxyzrgba_640x480` | 1.55x | adopted |
| `production_scaling_full_range_intensity_640x480` | 1.52x | adopted |
| `production_label_mono16_pointxyzl_640x480` | 1.08x | weak-positive adopted |

Phase 020 的 Evidence Doctor 为 `Errors=1, Warnings=0, Suggestions=0`。唯一 Error 指向
`scaling_full_range_intensity_640x480`，即旧 v0 full-range scaling，不指向 RGB segment-store
或 scaling reduction v1。

Phase 060 的 Evidence Doctor 为 `Errors=1, Warnings=1, Suggestions=0`。Error 指向
`normal_field_pointnormal_640x480` 5/5 退化，Warning 指向 min 0.46x、median 0.61x、max 0.67x
的长尾。checksum 一致，因此该结果是 normal v0 的负向性能诊断，不是正确性失败，也不能直接替代
production evidence（生产证据）。

Phase 070 的 Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。`label_mono16_pointxyzl_640x480`
的 5-run speedup 为 1.18x、1.21x、1.25x、1.20x、1.27x，checksum 一致。该结果支持
label mono16 的 production-shaped diagnostic（生产形态诊断）正向；Phase 090 已补同范围 production-public
证据。

Phase 080 的 Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`，证据角色为
production-public（公开入口生产证据）。三个 RGB/scaling production case checksum 一致，5-run median 分别为
1.54x、1.55x 和 1.52x，支持当前公开入口 RVV path 相对公开入口标量 path 有收益。

Phase 090 的 Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`，证据角色为
production-public。`production_label_mono16_pointxyzl_640x480` checksum 一致，5-run median 为
1.08x、min 为 1.05x、max 为 1.09x。该结果属于 weak-positive，但实现小、fallback 简单，按本阶段计划
和用户确认可采纳。

## ASM Attribution

| 指令 | 归属 | 证据 |
| --- | --- | --- |
| `vlse32.v` | RGB/RGBA 和 intensity 字段跨步加载 | `build/asm/riscv/bench_pcie_rvv.asm` |
| `vsseg3e8.v` | `rgb_segment_store_v1` 三通道写回 | `build/asm/riscv/bench_pcie_rvv.asm` |
| `vfredmin.vs` / `vfredmax.vs` | `scaling_reduction_v1` min/max 规约 | `build/asm/riscv/bench_pcie_rvv.asm` |
| `vfadd.vf` / `vfmul.vf` | `normal_float_stride_v0` 的 `(normal + 1) * 127` 计算 | `build/asm/riscv/bench_pcie_rvv.asm` |
| narrow / `vse16.v` | `label_mono16_stride_v0` 和 production label mono16 的低 16 位截断与 `mono16` 写回 | `build/asm/riscv/bench_pcie_rvv.full.asm` / `build/asm/riscv/bench_pcie_rvv.asm` |

Phase 080 之后，production labels 的 ASM 归属还覆盖真实公开入口：RGB public entry / helper 中可见
`vlse32.v` 和 `vsseg3e8.v`，scaling helper 中可见 `vlse32.v`、`vfredmin.vs` 和 `vfredmax.vs`。
Phase 090 之后，label mono16 production-public bench 也覆盖真实 label public entry：可见 `vlse32.v`、
narrow 和 `vse16.v`。RGB random / Glasbey label modes 仍是标量路径。

## 提交边界

默认提交候选只包括 topic 源码、文档和 summary-only 证据引用。`build/`、raw board logs、
`log/qemu/*.log`、`log/board/run_*.log`、私有 `config.mk` 和 `__pycache__` 不进入默认提交边界。
