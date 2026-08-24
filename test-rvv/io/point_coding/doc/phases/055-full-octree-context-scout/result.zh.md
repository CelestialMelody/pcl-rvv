# Phase 055：full-octree-shaped decode context scout 结果

## 实际范围

本阶段只修改 `test-rvv/io/point_coding/**` 下的测试支撑、bench、manifest metadata 和 topic-local 文档，没有修改 `io/include/pcl/compression/point_coding.h`。新增 `decode_multileaf_*` case 是 production-shaped diagnostic（生产形态诊断）：Std side 使用真实 `pcl::octree::PointCoding<PointXYZ>` 对象保存完整 diff vector、初始化 iterator，并按多 leaf point count sequence（多叶节点点数序列）多次调用 `decodePoints`；RVV side 使用测试专用 helper 按相同 leaf sequence、reference points 和 output range 写回。

它比 Phase 050 的单 leaf context 更接近 `OctreePointCloudCompression::deserializeTreeCallback` 中 point coder 的调用形态，但仍不包含真实 tree traversal（树遍历）、entropy decoding（熵解码）、stream input 或 production dispatch（生产分流）。

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| multi-leaf fixture / helper | done | `include/impl/point_coding_support.hpp` | 新增 `MultiLeafDecodeCase`、真实 `PointCoding` multi-leaf reference 和 candidate-to-cloud helper。 |
| correctness test | done | `make run_test_compare` | Std / RVV 两侧各 5 个 gtest 通过，新增 `DecodeCandidateMatchesMultiLeafObjectState`。 |
| bench case | done | `src/bench_point_coding.cpp` | 新增 `decode_multileaf_256/1024/4096/16384`。 |
| manifest metadata | done | `script/generate_point_coding_evidence_manifest.py` | `decode_multileaf_*` 标为 `production_shaped_diagnostic` / `point_coding_multileaf_decode_context`。 |
| QEMU smoke / asm | done | `make run_bench ... --case-filter decode_multileaf_1024`；`make dump_bench_rvv` | QEMU 可运行 multi-leaf case；asm 中可见 `vlse8.v` / `vsse32.v` / `vfcvt.f.xu.v`。 |
| board repeated + doctor | done | `log/board/repeated_phase055_decode_multileaf/summary.md`、`evidence_doctor.md` | 10-run median 全部大于 1，doctor 为 `Errors=0，Warnings=3，Suggestions=0`。 |

## Board 结果

命令：

```bash
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase055_decode_multileaf \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_multileaf_*'
```

| case | runs | median | min | max | p10 | p90 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `decode_multileaf_256` | 10 | 1.25x | 0.95x | 1.27x | 1.01x | 1.27x | weak-positive / unstable small context，1/10 退化 |
| `decode_multileaf_1024` | 10 | 1.28x | 1.28x | 1.47x | 1.28x | 1.34x | positive diagnostic with high-side tail |
| `decode_multileaf_4096` | 10 | 1.22x | 1.16x | 1.48x | 1.19x | 1.32x | weak-positive with long-tail warning |
| `decode_multileaf_16384` | 10 | 1.18x | 1.14x | 1.25x | 1.14x | 1.25x | weak-positive diagnostic |

## Evidence Doctor 处理

`log/board/repeated_phase055_decode_multileaf/evidence_doctor.md` 报告 `Errors=0，Warnings=3，Suggestions=0`。Warnings 分两类：

- `decode_multileaf_256` 有 1/10 B/A 低于 1，最小值为 `0.95x`。处理方式是保留退化频率，把 256 点多 leaf context 写成 weak-positive / unstable small context，不用 median 正向掩盖。
- `decode_multileaf_256` 和 `decode_multileaf_4096` 有 long-tail / variance（长尾 / 波动）warning。处理方式是保留 min/median/max 和 p10/p90，不剔除异常值。

这些 warning 不说明实现有 bug；它们说明更完整的 point coder shaped context 仍有收益线索，但小规模 leaf sequence 和 run-to-run 波动仍阻止 production probe 或 production-ready 结论。

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；仍是 test-only。 |
| A/B boundary | Std side 是真实 `PointCoding<PointXYZ>` 多 leaf object state；RVV side 是 test helper。 |
| 当前决策问题 | decode RVV 在多 leaf point coder context 中是否仍值得继续保留为生产接入前输入。 |
| diagnostic 是否可外推到 production | no。它不覆盖真实 tree traversal、entropy decoding、stream input 或 production dispatch。 |
| comparison-boundary / baseline mismatch 风险 | yes。RVV helper 直接按 byte offset 和 leaf sequence 解码，不是同一个 production detail helper。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前不允许。`decode_multileaf_256` 的退化和长尾 warning 要求先做 trace / full public context 可行性审计，或由用户明确授权 PI1 只写计划。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。当前没有 production patch，也没有 production direct evidence。 |

## EvidenceDecision

`EvidenceDecision`：`production-shaped diagnostic weak-positive with instability warnings / no-production`。

本阶段证明：

- multi-leaf point coder shaped correctness、QEMU log-shape、asm 和 board repeated 均可执行。
- 加入多 leaf point-count staging、多个 reference point 和多次 leaf decode 调用后，median 仍全部大于 1。
- Phase 055 的 Warning 数量少于 Phase 050，但仍存在小规模退化和长尾。

本阶段不能证明：

- 真实 `OctreePointCloudCompression` public decompression pipeline 已经变快。
- production source 中已有 RVV dispatch。
- entropy decoding、tree traversal 或 stream I/O 不会吞掉收益。
- `decode_multileaf_256` 的退化只是噪声；当前没有 trace / 温度 / 频率 metadata。

## 阶段反思和下一步

`continue_stop_decision`：本阶段关闭 `decode_multileaf_*` scout。当前结论仍不进入 production integration loop，因为证据角色是 production-shaped diagnostic，且 Evidence Doctor 仍有 3 个 warning。

默认下一步有两个分支：

- 当前授权范围内继续：`056-public-octree-roundtrip-feasibility`，只写 test-rvv plan / smoke，评估能否构造不改 production 的 public roundtrip（公开压缩/解压往返）诊断，并明确熵编码和 traversal 是否会吞掉收益。
- 需要用户授权：`060-production-integration-plan`。只有用户明确授权 PI1，且 PI1 先冻结 no-production fallback、warning 处理和不得直接 PI2 的边界，才进入 production 接入计划；不能把 Phase 055 写成 production-ready。
