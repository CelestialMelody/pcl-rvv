# pcd_io Benchmark 与证据说明

本文说明 `src/bench_pcd_io.cpp` 的输出、case-filter、board repeated target、Evidence Doctor（证据体检）、
manifest（证据清单）和 registry（证据登记表）。它是 bench 统计和提交边界的主归属。

## Bench 输出格式

每个 case 输出两行：

```text
<case_label>: <ms_per_iter> ms/iter
  Total Time: <total_ms> ms, checksum: <checksum>, points: <point_count>, point_step: <point_step>
```

checksum 是 FNV-1a 风格 byte checksum。checksum 在计时循环后更新，用于防止结果被优化掉和供 manifest
检查 Std/RVV 是否同口径。它不是性能结论。

## CLI 参数和 case-filter 字典

| 参数 / label | 含义 | 证明范围 | 不能证明 |
| --- | --- | --- | --- |
| `--iterations <n>` | 每个 case 的计时迭代次数 | 控制单次 run 的平均时间。 | 不改变证据角色。 |
| `--warmup-iterations <n>` | 计时前 warm-up（预热）次数 | 减少冷启动影响。 | 不替代 repeated board。 |
| `--case-filter component` | 只运行 pack / unpack component case | Phase 000 component matrix。 | 不含 writer / reader payload shaped case。 |
| `--case-filter all` | 运行所有当前 bench case | QEMU smoke 或人工排查。 | 不作为单一 evidence role repeated 输入。 |
| `--case-filter writer_payload` | 只运行 writer payload family | Phase 010 production-shaped diagnostic。 | 不证明 public overload。 |
| `--case-filter reader_payload` | 只运行 reader payload family | Phase 040 production-shaped diagnostic。 | 不证明 public reader 或 finite scan RVV。 |
| `--case-filter production_writer` | 只运行 public `std::ostream` writer family | Phase 050 production-public evidence。 | 不证明 reader、filename overload 或 templated writer。 |
| `pack_xyzi_307k` | AoS 到 field-major pack，连续 4x u32 | pack component ablation。 | 不含 LZF。 |
| `pack_padded_xyzi_307k` | AoS 到 field-major pack，尾部 padding | padding layout pack component。 | 不含 LZF。 |
| `unpack_xyzi_307k` | field-major 到 AoS unpack，连续 4x u32 | unpack component ablation。 | 不含 reader finite scan。 |
| `unpack_padded_xyzi_307k` | field-major 到 AoS unpack，尾部 padding | padding layout unpack component。 | 不含 reader finite scan。 |
| `writer_payload_xyzi_307k` | pack + LZF + 8-byte payload header | production-shaped writer diagnostic。 | 不含 text header / ostream flush。 |
| `writer_payload_padded_xyzi_307k` | padding layout writer payload | production-shaped writer diagnostic。 | 不含 production dispatch。 |
| `reader_payload_xyzi_307k` | LZF decompress + unpack + scalar finite scan | production-shaped reader diagnostic。 | 不含 public read、mmap / file I/O。 |
| `reader_payload_padded_xyzi_307k` | padding layout reader payload | production-shaped reader diagnostic。 | 不含 finite scan RVV。 |
| `production_writer_xyzi_307k` | public ostream header + pack + LZF + payload write | production-public writer evidence。 | 不覆盖 filename overload / file I/O。 |
| `production_writer_padded_xyzi_307k` | padding layout public ostream writer | production-public writer evidence。 | 不覆盖 non-4-byte fields。 |

## Target 和输出路径

| target | 输出 | evidence role |
| --- | --- | --- |
| `run_qemu_smoke` | `log/qemu/run_test_*.log`、`log/qemu/run_bench_rvv.log` | correctness / log-shape only |
| `dump_bench_rvv` | `build/asm/riscv/bench_pcd_io_rvv.asm` | diagnostic asm attribution |
| `run_board_pcd_io_repeated` | `log/board/component_ablation_repeat_5/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | component ablation |
| `run_board_pcd_io_writer_payload_repeated` | `log/board/writer_payload_repeat_5/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | production-shaped diagnostic |
| `run_board_pcd_io_reader_payload_repeated` | `log/board/reader_payload_repeat_5/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | production-shaped diagnostic |
| `run_board_pcd_io_production_writer_repeated` | `log/board/production_writer_repeat_5/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | production-public |

Raw per-run logs under `log/board/*/run*/` and build outputs under `build/` are local-only by default.

## 计时边界

| case family | timer boundary（计时边界） | checksum timing | setup excluded |
| --- | --- | --- | --- |
| pack component | field layout conversion only | checksum after timing | input construction |
| unpack component | field layout conversion only | checksum after timing | scalar pack input construction |
| writer payload | pack to field-major plus LZF compression | checksum after timing | input construction |
| reader payload | LZF decompression plus unpack plus scalar finite scan | checksum after timing | compressed payload input construction |
| production writer | public ostream header plus pack plus LZF plus payload write | checksum after timing | synthetic cloud construction |

QEMU timing 不进入性能排序。性能结论只来自 board repeated summary。

## 当前 board 证据

| run label | summary | result | Evidence Doctor |
| --- | --- | --- | --- |
| `board-pcd-io-component-ablation-repeat-phase000` | `log/board/component_ablation_repeat_5/summary.md` | pack positive；unpack positive / weak-positive | Errors=0，Warnings=0，Suggestions=9 |
| `board-pcd-io-writer-payload-repeat-phase010` | `log/board/writer_payload_repeat_5/summary.md` | writer payload median 1.18x / 1.12x | Errors=0，Warnings=0，Suggestions=4 |
| `board-pcd-io-reader-payload-repeat-phase040` | `log/board/reader_payload_repeat_5/summary.md` | reader payload median 1.07x / 1.03x | Errors=0，Warnings=0，Suggestions=5 |
| `board-pcd-io-production-writer-repeat-phase050` | `log/board/production_writer_repeat_5/summary.md` | production writer median 1.33x / 1.25x | Errors=0，Warnings=0，Suggestions=4 |

Suggestions 主要来自环境 metadata 和 binary identity 缺失。Phase 050 的 production writer 结论使用
production-public evidence；其它历史 summary 仍按各自 evidence role 使用。

## Manifest、Doctor 和 Registry

| file / script | role |
| --- | --- |
| `script/generate_pcd_io_evidence_manifest.py` | 把 repeated board raw logs 转成 topic-local manifest。 |
| `test-rvv/script/evidence_doctor.py` | 检查 comparison boundary、metadata、B/A 分布和异常信号。 |
| `test-rvv/script/evidence_registry.py` | 记录 summary / manifest / doctor 的 hash 和 freshness。 |
| `log/evidence_registry.json` | topic-local registry。 |

Freshness check（新鲜度检查）命令：

```bash
python3 test-rvv/script/evidence_registry.py check --registry test-rvv/io/pcd_io/log/evidence_registry.json --scan-glob 'test-rvv/io/pcd_io/log/board/component_ablation_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}' --fail-on any
python3 test-rvv/script/evidence_registry.py check --registry test-rvv/io/pcd_io/log/evidence_registry.json --scan-glob 'test-rvv/io/pcd_io/log/board/writer_payload_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}' --fail-on any
python3 test-rvv/script/evidence_registry.py check --registry test-rvv/io/pcd_io/log/evidence_registry.json --scan-glob 'test-rvv/io/pcd_io/log/board/reader_payload_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}' --fail-on any
python3 test-rvv/script/evidence_registry.py check --registry test-rvv/io/pcd_io/log/evidence_registry.json --scan-glob 'test-rvv/io/pcd_io/log/board/production_writer_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}' --fail-on any
```

## 反汇编口径

`dump_bench_rvv` 当前证明 test helper / bench inlined path 中存在 `vlse32.v`、`vse32.v`、`vle32.v`
和 `vsse32.v`。Phase 050 还在 public writer path 中归属到 `vlse32.v` / `vse32.v`，随后进入
`pcl::lzfCompress`。

## 提交边界

| artifact | 默认策略 |
| --- | --- |
| phase docs、evaluation、README、role docs | review-required，可作为 topic 产物。 |
| summary / manifest / doctor | summary-only，只有被文档引用时进入提交候选。 |
| raw logs、build binaries、private board config | local-only，不默认提交。 |
| `doc-rvv/io/pcd_io-RVV.zh.md` | Phase 050 后适用，记录 adopted production behavior。 |
