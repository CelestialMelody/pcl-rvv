# Phase 050：decode production-shaped context scout 结果

## 实际范围

本阶段只修改 `test-rvv/io/point_coding/**` 下的测试支撑、bench 和 topic-local 文档，没有修改 `io/include/pcl/compression/point_coding.h`。新增的 `decode_context_*` case 是 production-shaped diagnostic（生产形态诊断）：Std side 使用真实 `pcl::octree::PointCoding<PointXYZ>` 对象、真实 diff vector 和 iterator reset（迭代器重置）；RVV side 使用测试专用 candidate 写入同形状 output cloud range（输出点云区间）。它比 `decode_contiguous_*` 更接近 production 对象状态，但仍不是 production direct（真实生产入口直连）证据。

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| production-shaped decode helper | done | `include/impl/point_coding_support.hpp` | 新增 `decodePointsProductionObject`、`decodePointsCandidateToCloud` 和 cloud range checksum。 |
| correctness test | done | `make run_test_compare` | Std / RVV 两侧各 4 个 gtest 通过，新增 `DecodeCandidateMatchesPointCodingObjectState`。 |
| bench case | done | `src/bench_point_coding.cpp` | 新增 `decode_context_16/64/256/1024/4096/16384`，输出格式兼容 repeated summary 和 Evidence Doctor。 |
| QEMU smoke / asm | done | `make run_bench ... --case-filter decode_context_1024`；`make dump_bench_rvv` | QEMU 能运行 context case；asm 中可见 `vlse8.v` / `vsse32.v` / `vfcvt.f.xu.v`。 |
| board repeated + doctor | done | `log/board/repeated_phase050_decode_context/summary.md`、`evidence_doctor.md` | 10-run median 全部大于 1，但 doctor 为 `Errors=0，Warnings=7`。 |
| manifest metadata | done | `script/generate_point_coding_evidence_manifest.py` | `decode_context_*` 已标为 `production_shaped_diagnostic` / `production_shaped_helper`。 |

## Board 结果

命令：

```bash
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase050_decode_context \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_context_*'
```

| case | runs | median | min | max | p10 | p90 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `decode_context_16` | 10 | 1.40x | 1.17x | 1.40x | 1.17x | 1.40x | weak-positive with long-tail warning |
| `decode_context_64` | 10 | 1.32x | 1.32x | 1.32x | 1.32x | 1.32x | positive diagnostic |
| `decode_context_256` | 10 | 1.31x | 1.27x | 1.71x | 1.30x | 1.57x | positive diagnostic with long-tail warning |
| `decode_context_1024` | 10 | 1.31x | 0.89x | 1.32x | 1.27x | 1.32x | weak-positive / unstable signal，1/10 退化 |
| `decode_context_4096` | 10 | 1.21x | 0.98x | 1.25x | 1.14x | 1.23x | weak-positive / unstable signal，1/10 退化 |
| `decode_context_16384` | 10 | 1.23x | 1.21x | 1.43x | 1.21x | 1.32x | weak-positive with long-tail warning |

## Evidence Doctor 处理

`log/board/repeated_phase050_decode_context/evidence_doctor.md` 报告 `Errors=0，Warnings=7，Suggestions=0`。Warnings 分两类：

- `decode_context_1024` 和 `decode_context_4096` 各有 1/10 B/A 低于 1。处理方式是保留退化频率，不用 median 正向掩盖退化；这阻止 production probe（生产探针）或 PI1。
- `decode_context_16/256/1024/4096/16384` 有 long-tail / variance（长尾 / 波动）warning。处理方式是保留 min/median/max 和 p10/p90，不剔除异常值。

这些 warning 不说明实现必然有 bug；它们也不足以否定 test-only scout 的价值。它们说明当前 production-shaped diagnostic 只能写成弱正向且有不稳定信号，下一步需要更完整 context、trace 或更大 run budget 才能改变生产判断。

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；仍是 test-only。 |
| A/B boundary | Std side 是真实 `PointCoding<PointXYZ>` 标量对象；RVV side 是 test helper candidate 写同形状 cloud range。 |
| 当前决策问题 | decode RVV 是否值得继续到更完整 test-only octree-shaped context 或请求 PI1 授权。 |
| diagnostic 是否可外推到 production | no。它覆盖对象状态和 output range，但不覆盖 public decompression pipeline、entropy context 或 production dispatch。 |
| comparison-boundary / baseline mismatch 风险 | yes。Std/RVV 两侧不是同一个 production detail helper；positive 只能作为继续侦察信号。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前不允许。1/10 退化和 7 个 warning 要求先做更完整 test-only context 或 trace。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。当前没有 production patch，也没有 production direct evidence。 |

## EvidenceDecision

`EvidenceDecision`：`production-shaped diagnostic weak-positive with instability warnings / no-production`。

本阶段证明：

- `decode_context_*` correctness、QEMU log-shape、asm 和 board repeated 均可执行。
- 更接近 `PointCoding` 对象状态的 decode RVV 仍有 median 正向。

本阶段不能证明：

- 真实 `PointCoding::decodePoints` production path 已接入 RVV 或已经变快。
- 完整 `OctreePointCloudCompression` 解码链路收益。
- `decode_context_1024/4096` 的退化只是噪声；当前没有 trace / 温度 / 频率 metadata。

## 阶段反思和下一步

`continue_stop_decision`：本阶段关闭 `decode_context_*` scout，因 Evidence Doctor warning 和 A/B boundary mismatch，不进入 production integration loop。当前仍可在 test-rvv 范围内继续做 `055-full-octree-context-scout`，但它需要构造更完整 decompression context；若用户希望更快进入生产接入，应先确认是否接受 PI1 只写计划、且不得把 Phase 050 的弱正向写成 production-ready。
