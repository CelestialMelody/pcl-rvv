# pcd_io_templated_writer Bench 与证据说明

## 本文职责

本文说明 `bench_pcdtw` 的 case label（用例标签）、计时边界、summary（摘要）、
manifest（证据清单）、Evidence Doctor（证据体检）和 evidence registry（证据登记表）。
性能结论只来自板卡或目标硬件；QEMU timing 不作为性能证据。

## Bench 输出格式

每个 case 输出两行：

```text
<case>: <avg-ms> ms/iter
  Total Time: <total-ms> ms, checksum: <checksum>, points: <n>, point_step: <step>
```

checksum 是计时结束后对最终输出 buffer 做 FNV-1a 风格摘要，避免 checksum 计入 hot timing boundary
（热点计时边界）。

## CLI 和 case-filter 字典

| 参数 / case | 含义 | 计时边界 |
| --- | --- | --- |
| `--iterations N` | timed iterations（计时迭代次数）。 | 只影响 bench loop。 |
| `--warmup-iterations N` | warmup iterations（预热迭代次数）。 | 不计入 Total Time。 |
| `--case-filter all` | 运行全部 pack-only 和 compressed case。 | 适合 Phase 000 component ablation。 |
| `--case-filter compressed_*` | 只运行 pack+LZF case。 | 适合 Phase 010 production-shaped diagnostic。 |
| `--case-filter binary_*` | 只运行 binary writer packed output case。 | 适合 Phase 040 binary component ablation。 |
| `--case-filter production_compressed_*` | 只运行真实 `PCDWriter::writeBinaryCompressed<PointT>` public overload case。 | 适合 Phase 060 production-public evidence。 |
| `--case-filter production_binary_tuple_*` | 只运行真实 `PCDWriter::writeBinary<PointT>` public overload 的 tuple / segment production case。 | 适合 Phase 100 production-public evidence。 |
| `--case-filter production_binary_*` | historical only：Phase 080 回滚前曾运行真实 `PCDWriter::writeBinary<PointT>` field-outer public overload case；旧 wrapper 已移除。 | 只作为历史证据路径，不是当前默认入口。 |
| `pointxyzrgb_4f_262k` | 262144 点，4 个 4 字节字段，`point_step=16`。 | pack-only。 |
| `pointxyzrgb_4f_padding_262k` | 同规模，`point_step=20`。 | pack-only with tail padding。 |
| `pointxyzrgb_4f_small_512` | 512 点小规模。 | pack-only smoke。 |
| `compressed_pointxyzrgb_4f_262k` | 同布局，计入 LZF。 | pack + LZF。 |
| `compressed_pointxyzrgb_4f_padding_262k` | padding 布局，计入 LZF。 | pack + LZF。 |
| `compressed_pointxyzrgb_4f_small_512` | 512 点，计入 LZF。 | pack + LZF smoke。 |
| `binary_pointxyzrgb_4f_262k` | 同布局，输出为 point-major packed binary。 | binary pack-only。 |
| `binary_pointxyzrgb_4f_padding_262k` | padding 布局，输出为 point-major packed binary。 | binary pack-only with tail padding。 |
| `binary_pointxyzrgb_4f_small_512` | 512 点，输出为 point-major packed binary。 | binary pack-only smoke。 |
| `production_binary_pointxyzrgb_4f_compact_262k` | historical only：回滚前真实 binary writer public overload，compact 16B registered point type。 | Phase 080 historical production-public binary writer。 |
| `production_binary_pointxyzrgb_4f_padding_262k` | historical only：回滚前真实 binary writer public overload，tail padding 20B registered point type。 | Phase 080 historical production-public binary writer with tail padding。 |
| `production_binary_pointxyzrgb_4f_compact_small_512` | historical only：回滚前真实 binary writer public overload，512 点。 | Phase 080 historical production-public binary writer smoke。 |
| `production_binary_tuple_pointxyzrgba_4f_compact_262k` | 当前真实 binary writer public overload，compact 16B registered point type。 | Phase 100 production-public binary tuple writer。 |
| `production_binary_tuple_pointxyzrgba_4f_padding_262k` | 当前真实 binary writer public overload，tail padding 20B registered point type。 | Phase 100 production-public binary tuple writer with segment store。 |
| `production_binary_tuple_pointxyzrgba_4f_compact_small_512` | 当前真实 binary writer public overload，512 点。 | Phase 100 production-public binary tuple writer smoke。 |

## 当前板卡证据

Phase 000 component ablation：

- target：`make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_repeated`
- summary：`log/board/component_ablation_repeat_5/summary.md`
- Doctor：`log/board/component_ablation_repeat_5/evidence_doctor.md`
- result：`pointxyzrgb_4f_262k` mean `1.4892x`，padding mean `1.4466x`，small mean `1.5739x`。
- Doctor：Errors=0，Warnings=0，Suggestions=0。

Phase 010 production-shaped diagnostic：

- target：`make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_shaped_repeated`
- summary：`log/board/production_shaped_repeat_5/summary.md`
- Doctor：`log/board/production_shaped_repeat_5/evidence_doctor.md`

Phase 060 production-public compressed writer：

- target：`make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_production_compressed_repeated`
- summary：`log/board/production_compressed_repeat_5/summary.md`
- manifest：`log/board/production_compressed_repeat_5/evidence_manifest.json`
- Doctor：`log/board/production_compressed_repeat_5/evidence_doctor.md`
- Evidence Doctor：Errors=0，Warnings=4，Suggestions=0。大规模 compact / padding 的 long-tail warning
  不改变 positive bucket，因为 min 仍分别为 `1.2192x` / `1.1937x`；small case 有 1/5 退化，降级为 smoke-only。
- result：production-public compact mean `1.3122x`，padding mean `1.2495x`，small mean `1.1239x`。
- decision：compressed writer adopted；正式文档为 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。

Phase 040 binary component ablation：

- target：`make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_binary_repeated`
- summary：`log/board/binary_component_repeat_5/summary.md`
- Doctor：`log/board/binary_component_repeat_5/evidence_doctor.md`
- result：binary large median `1.2738x`，padding median `1.1842x`，small median `6.8125x`。
- Doctor：Errors=0，Warnings=1，Suggestions=0；small case 是 group outlier，单独作为 smoke-shaped positive 报告。

Phase 080 production-public binary writer（historical / removed after rollback）：

- historical target：`make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_production_binary_repeated`
- summary：`log/board/production_binary_repeat_5/summary.md`
- manifest：`log/board/production_binary_repeat_5/evidence_manifest.json`
- Doctor：`log/board/production_binary_repeat_5/evidence_doctor.md`
- result：compact mean `0.9842x`，padding mean `0.9727x`，small mean `0.9810x`。
- Evidence Doctor：Errors=3，Warnings=0，Suggestions=0；三个 Error 均为 `ba_degradation_frequency`。
- decision：binary writer patch 不建议采纳；用户确认负收益可回滚后，当前 production patch 和
  `production_binary_*` bench / board wrapper 已移除。

Phase 090 binary tuple / segment diagnostic：

- target：`make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_binary_tuple_segment_repeated`
- summary：`log/board/binary_tuple_segment_repeat_5/summary.md`
- manifest：`log/board/binary_tuple_segment_repeat_5/evidence_manifest.json`
- Doctor：`log/board/binary_tuple_segment_repeat_5/evidence_doctor.md`
- result：diagnostic compact mean `9.8247x`，padding mean `4.0905x`，small smoke mean `18.0526x`。
- Evidence Doctor：Errors=0，Warnings=2，Suggestions=0。
- decision：diagnostic-positive，只作为 Phase 100 production probe 的前置信号。

Phase 100 production-public binary tuple writer：

- target：`make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_production_binary_tuple_repeated`
- summary：`log/board/production_binary_tuple_repeat_5/summary.md`
- manifest：`log/board/production_binary_tuple_repeat_5/evidence_manifest.json`
- Doctor：`log/board/production_binary_tuple_repeat_5/evidence_doctor.md`
- result：compact mean `1.3046x`，padding mean `1.3240x`，small mean `1.1082x`。
- Evidence Doctor：Errors=0，Warnings=1，Suggestions=0；small long-tail 降级为 smoke-only。
- decision：binary tuple / segment production path adopted。

## ASM Attribution 口径

`make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv` 生成
`build/asm/riscv/bench_pcdtw_rvv.asm`。当前可见 `vlse32.v` / `vse32.v`，归属到 diagnostic bench
中内联的 `packFieldsCandidate`。该 asm 不证明 production symbol attribution（生产符号归属）；
Phase 080 回滚前曾补 binary production-public attribution：`writeBinary<PCDTWPointXYZRGBField>` 和
`writeBinary<PCDTWPointXYZRGBFieldPadding>` 符号内可见 `vlse32.v` / `vsse32.v`。回滚后这些生产符号不再存在；
当前 asm 归属要求 compressed writer production path 仍可见 `vlse32.v` / `vse32.v`，并要求 Phase 100
binary tuple padding production path 可见 `vlse32.v` / `vsseg4e32.v`。

## Manifest / Doctor / Registry

Topic-local manifest 由 `script/generate_pcdtw_evidence_manifest.py` 生成。脚本按 case 名区分：

- `pointxyzrgb_*`：`component_ablation`。
- `compressed_*`：`production_shaped_diagnostic`，计时边界为 `pack_and_lzf_compress_checksum_after_timing`。
- `binary_*`：`binary_component_ablation`，计时边界为 `binary_pack_only_checksum_after_timing`。
- `production_compressed_*`：`production_public`，计时边界为
  `public_writer_header_pack_lzf_mmap_write_checksum_after_timing`。
- `production_binary_*`：historical Phase 080 `production_public`，计时边界为
  `public_writer_header_pack_mmap_write_checksum_after_timing`；旧 wrapper 已移除。
- `production_binary_tuple_*`：Phase 100 `production_public`，计时边界为
  `public_writer_header_pack_mmap_write_checksum_after_timing`；当前采用路径。

`log/evidence_registry.json` 已登记 Phase 000、Phase 010、Phase 040、Phase 060、Phase 080、Phase 090
和 Phase 100 的
summary、manifest、Doctor，freshness_state 为 `fresh`。

## 提交边界

默认只考虑 summary / manifest / Doctor / registry。raw board logs、QEMU logs、build binaries、
full asm dump 和远端板卡路径不默认提交；如需提交 logs，应先运行 sanitize / check target。
