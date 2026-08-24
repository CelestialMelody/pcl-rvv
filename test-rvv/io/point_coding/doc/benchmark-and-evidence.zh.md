# point_coding Bench 与证据说明

## Bench 入口

```bash
make run_qemu_bench_smoke
make run_board_bench_compare
make collect_board_repeated POINT_CODING_REPEATED_RUNS=5
make run_board_repeated_evidence_doctor
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase040_decode \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_contiguous_*'
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase055_decode_multileaf \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_multileaf_*'
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase060_production_decode \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_production_direct_*'
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase070_traits_decode \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_production_direct_traits_*'
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase080_public_octree \
  BENCH_ARGS='--iterations 10 --warmup-iterations 2 --case-filter octree_*'
```

`src/bench_point_coding.cpp` 输出 `ms/iter` 和 checksum（校验和）。Std build 运行标量参考链路；RVV build 运行测试专用 RVV candidate（候选实现）。checksum 只在计时后混入 sink（防止优化掉输出），不把 checksum 计算计入被测 helper 内部。

## Case 字典

| case | 输入构造 | 计时边界 | 证明点 | 不能证明 |
| --- | --- | --- | --- | --- |
| `encode_indexed_*` | 小 / 中 / 大 leaf sweep。 | 只计 `runEncode` 中的 encode helper 和输出 checksum sink。 | indexed gather 是否仍有局部收益。 | 真实 leaf size 分布、熵编码、vector `push_back` 语义。 |
| `decode_contiguous_*` | synthetic diff。 | 只计 decode helper 和 checksum sink。 | byte-to-float RVV decode。 | 真实 decoder iterator 状态。 |
| `decode_context_*` | synthetic diff 写入真实 `PointCoding<PointXYZ>` object-state shaped output range。 | Std side 计真实 `PointCoding::decodePoints`；RVV side 计测试 helper。 | 更接近 production 对象状态的 decode scout。 | 真实 public entry、entropy context、完整 decompression pipeline、production dispatch。 |
| `decode_multileaf_*` | synthetic diff 写入真实 `PointCoding<PointXYZ>` 多 leaf object-state shaped output range。 | Std side 计真实 `PointCoding` 对象按 leaf sequence 多次 `decodePoints`；RVV side 计测试 helper。 | 多 leaf point-count staging 和多 reference context 下的 decode scout。 | 真实 tree traversal、entropy decoding、stream input、production dispatch。 |
| `decode_production_direct_*` | synthetic diff 写入真实 `PointCoding<PointXYZ>` production object。 | Std/RVV 两侧都计真实 `PointCoding<PointXYZ>::decodePoints`；RVV build 命中 production exact gate。 | Phase 060 exact `PointXYZ` 生产收益。 | 泛型 `PointT`、完整 public octree end-to-end、encode path。 |
| `decode_production_direct_traits_*` | synthetic diff 写入真实 `PointCoding<PointXYZI>` / `PointCoding<PointXYZRGB>` production object。 | Std/RVV 两侧都计真实 `PointCoding<PointT>::decodePoints`；RVV build 命中 traits gate。 | Phase 070 traits-gated production-direct 收益和额外字段保持。 | 所有自定义点型、非标准布局、其它目标硬件、完整 public octree end-to-end。 |
| `octree_decode_public_*` | 先用真实 public encoder 生成压缩流，再计真实 `OctreePointCloudCompression<PointXYZ>::decodePointCloud`。 | Std/RVV 两侧都走 public decode；RVV build 内部可命中 Phase 060 point coder decode dispatch。 | Phase 080 public decode boundary 是否还能看到收益。 | 不能外推到 encode、其它 profile、真实文件流或稳定公开入口正向。 |
| `octree_roundtrip_public_*` | 每次计真实 `OctreePointCloudCompression<PointXYZ>` encode + decode 往返。 | 包含 tree traversal、entropy coding、stream 和 point coder decode。 | Phase 080 完整 public roundtrip 稀释程度。 | 不能单独决定回滚或扩大 production patch。 |

## 当前板卡摘要

当前性能结论来自 `log/board/repeated_phase000/summary.md` 的 12-case 5-run repeated board summary（重复板卡摘要）。这批结果是 Phase 020 拒绝 f64 exact quantize 后，恢复默认 indexed gather + scalar same-chain quantize 的刷新结果：

| case | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | --- |
| `encode_indexed_16` | 1.17x | 0.42x | 1.25x | weak-positive / unstable tiny leaf，1/5 退化 |
| `encode_indexed_64` | 1.78x | 1.78x | 1.85x | positive |
| `encode_indexed_256` | 2.14x | 2.12x | 2.15x | positive，group outlier warning |
| `encode_indexed_1024` | 2.07x | 1.97x | 2.08x | positive，group outlier warning |
| `encode_indexed_4096` | 1.63x | 1.54x | 1.76x | positive |
| `encode_indexed_16384` | 1.31x | 1.25x | 1.32x | positive，但大 leaf 收益低于中等规模 |
| `decode_contiguous_16` | 1.29x | 1.14x | 1.29x | weak-positive |
| `decode_contiguous_64` | 1.29x | 1.23x | 1.29x | weak-positive |
| `decode_contiguous_256` | 1.27x | 1.25x | 1.27x | weak-positive |
| `decode_contiguous_1024` | 1.27x | 1.27x | 1.27x | weak-positive |
| `decode_contiguous_4096` | 1.18x | 1.15x | 1.21x | weak-positive |
| `decode_contiguous_16384` | 1.18x | 1.15x | 1.20x | weak-positive |

## Phase 060 production-direct 结果

`log/board/repeated_phase060_production_decode/summary.md` 的 10-run repeated board 是 exact `PointXYZ` production direct 证据。板卡结果：

| case | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | --- |
| `decode_production_direct_256` | 1.22x | 1.22x | 1.24x | positive |
| `decode_production_direct_1024` | 1.23x | 1.14x | 1.49x | positive with long-tail warning |
| `decode_production_direct_4096` | 1.18x | 1.09x | 1.25x | weak-positive / positive |
| `decode_production_direct_16384` | 1.16x | 1.10x | 1.19x | weak-positive / positive |

Evidence Doctor 报告为 `Errors=0，Warnings=1，Suggestions=0`。

## Phase 070 traits-gated production-direct 结果

`log/board/repeated_phase070_traits_decode/summary.md` 的 10-run repeated board 是 PointXYZ-like traits gate 证据。板卡结果：

| case | runs | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `decode_production_direct_traits_xyzi_1024` | 10 | `1.27x` | `1.25x` | `1.33x` | positive |
| `decode_production_direct_traits_xyzi_4096` | 10 | `1.20x` | `1.13x` | `1.26x` | weak-positive / positive |
| `decode_production_direct_traits_xyzrgb_1024` | 10 | `1.27x` | `1.18x` | `1.44x` | positive with long-tail warning |
| `decode_production_direct_traits_xyzrgb_4096` | 10 | `1.19x` | `1.11x` | `1.31x` | weak-positive / positive with long-tail warning |

Evidence Doctor 报告为 `Errors=0，Warnings=2，Suggestions=0`。两个 warning 都是 `PointXYZRGB` 代表点型上的 long-tail / variance；保留 min / median / max 后，它们不阻止 traits gate 采纳。

## Phase 080 public octree boundary 结果

`log/board/repeated_phase080_public_octree/summary.md` 的 10-run repeated board 是接入后 public boundary 审计。板卡结果：

| case | runs | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `octree_decode_public_1024` | 10 | `1.01x` | `0.99x` | `1.04x` | weak / near-threshold with degradation warning |
| `octree_decode_public_256` | 10 | `1.01x` | `1.00x` | `1.08x` | weak / near-threshold |
| `octree_roundtrip_public_1024` | 10 | `1.01x` | `0.99x` | `1.03x` | neutral / weak with Doctor Error |
| `octree_roundtrip_public_256` | 10 | `1.02x` | `1.00x` | `1.05x` | weak / near-threshold with degradation warning |

Evidence Doctor 报告为 `Errors=1，Warnings=2，Suggestions=4`。这批 public 数据不推翻 Phase 060 / 070
production-direct 采纳，但说明完整 public octree 链路中的 tree traversal、entropy coding 和 stream 成本明显稀释收益，
不能写成稳定 public entry 加速。

## Evidence Doctor 处理

`log/board/repeated_phase000/evidence_doctor.md` 结果是 `Errors=0，Warnings=6，Suggestions=0`。

Warnings 处理：

- `encode_indexed_16` 有 1/5 退化到 0.42x，tiny leaf 只能写 weak-positive / unstable，不能作为 production gate 的正向证据。
- `encode_indexed_16` 同时有 long-tail；`encode_indexed_16/256/1024/16384` 存在 group outlier，说明 encode 必须按 leaf size 分开解释。
- decode 当前 6 个规模均为 median 正向，但仍只覆盖 synthetic contiguous output segment（合成连续输出片段），不能直接外推到完整 decoder iterator（解码迭代器）状态。

这些 warning 不阻塞默认 candidate 的 diagnostic-positive（诊断正向）结论，因为当前问题只是“局部组件是否值得继续”。它们阻止 clean production adoption（干净生产采纳）。

## Decode-only 稳定性复核

Phase 040 使用独立目录 `log/board/repeated_phase040_decode/` 采集 10-run decode-only repeated board。Evidence Doctor 报告路径是 `log/board/repeated_phase040_decode/evidence_doctor.md`，结果为 `Errors=0，Warnings=1，Suggestions=0`。

| case | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | --- |
| `decode_contiguous_16` | 1.29x | 1.14x | 1.29x | weak-positive diagnostic |
| `decode_contiguous_64` | 1.23x | 1.23x | 4.86x | weak-positive diagnostic with long-tail warning |
| `decode_contiguous_256` | 1.26x | 1.26x | 1.26x | weak-positive diagnostic |
| `decode_contiguous_1024` | 1.26x | 1.13x | 1.27x | weak-positive diagnostic |
| `decode_contiguous_4096` | 1.19x | 1.14x | 1.20x | weak-positive diagnostic |
| `decode_contiguous_16384` | 1.15x | 1.12x | 1.22x | weak-positive diagnostic |

`decode_contiguous_64` 的 4.86x 是长尾 warning，不作为稳定收益写入决策；当前结论只采用 median 与 min/max 边界。该复核支持下一阶段 test-only production-shaped context scout，但仍不能证明真实 decoder iterator 或完整 octree decompression pipeline（解压流水线）已经变快。

## Decode context 生产形态侦察

Phase 050 使用独立目录 `log/board/repeated_phase050_decode_context/` 采集 10-run context-only repeated board。Evidence Doctor 报告路径是 `log/board/repeated_phase050_decode_context/evidence_doctor.md`，结果为 `Errors=0，Warnings=7，Suggestions=0`。

| case | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | --- |
| `decode_context_16` | 1.40x | 1.17x | 1.40x | weak-positive with long-tail warning |
| `decode_context_64` | 1.32x | 1.32x | 1.32x | positive diagnostic |
| `decode_context_256` | 1.31x | 1.27x | 1.71x | positive diagnostic with long-tail warning |
| `decode_context_1024` | 1.31x | 0.89x | 1.32x | weak-positive / unstable signal，1/10 退化 |
| `decode_context_4096` | 1.21x | 0.98x | 1.25x | weak-positive / unstable signal，1/10 退化 |
| `decode_context_16384` | 1.23x | 1.21x | 1.43x | weak-positive with long-tail warning |

这批证据的 role 是 production-shaped diagnostic，不是 production evidence。`decode_context_1024/4096` 的退化频率和多项 long-tail warning 阻止 production probe；下一步若继续，应先做更完整 test-only context 或 trace。

## Decode multi-leaf 生产形态侦察

Phase 055 使用独立目录 `log/board/repeated_phase055_decode_multileaf/` 采集 10-run multi-leaf repeated board。Evidence Doctor 报告路径是 `log/board/repeated_phase055_decode_multileaf/evidence_doctor.md`，结果为 `Errors=0，Warnings=3，Suggestions=0`。

| case | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | --- |
| `decode_multileaf_256` | 1.25x | 0.95x | 1.27x | weak-positive / unstable small context，1/10 退化 |
| `decode_multileaf_1024` | 1.28x | 1.28x | 1.47x | positive diagnostic with high-side tail |
| `decode_multileaf_4096` | 1.22x | 1.16x | 1.48x | weak-positive with long-tail warning |
| `decode_multileaf_16384` | 1.18x | 1.14x | 1.25x | weak-positive diagnostic |

这批证据的 role 仍是 production-shaped diagnostic，不是 production evidence。它说明多 leaf point coder context 没有吞掉所有 decode 收益，但 `decode_multileaf_256` 退化和 long-tail warning 仍阻止 production-ready 结论。

## 反汇编证据

`make dump_bench_rvv` 生成 `build/asm/riscv/bench_point_coding_rvv.asm`。关键指令包括：

- encode gather：`vluxseg3ei32.v`。默认路径的量化仍落到 scalar same-chain helper；f64 exact quantize 只在 `POINT_CODING_ENABLE_F64_QUANTIZE=1` probe build 中启用。
- decode byte load：`vlse8.v`。
- decode AoS field write：`vsse32.v`。
- decode byte-to-float conversion：diagnostic f32 path 使用 `vfcvt.f.xu.v`；Phase 060 production path 使用 `vfwcvt.f.xu.v` 和 `vfncvt.f.f.w` 保持 double reference rounding。
- traits-gated production-direct bench 的紧凑 xyz layout 会选择 `vssseg3e32.v`，用于 `PointXYZI` / `PointXYZRGB` 这类 compatible AoS 结构。

Phase 060 前的反汇编归属只到 test bench inlined candidate（测试性能入口内联候选）。Phase 060 和 Phase 070 的 `decode_production_direct_*` / `decode_production_direct_traits_*` bench 调用真实 production `decodePoints`，因此可作为 production-direct asm attribution（生产直连反汇编归属）。Phase 080 的 public bench binary 也可见同一 decode RVV 指令族，但 board summary 显示完整 public boundary 收益接近阈值。

## 证据登记状态

当前 topic 尚未接入 `log/evidence_registry.json`。恢复或提交前用路径限定扫描人工检查：

```bash
git status --short --untracked-files=all -- test-rvv/io/point_coding
git ls-files --others --exclude-standard -- test-rvv/io/point_coding
```

生成日志默认 ignored-local（本地忽略）。如果后续要提交 summary evidence，应补登记或明确 `git add -f` 的文件清单。
