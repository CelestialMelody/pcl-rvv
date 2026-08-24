# Phase 080 Public Octree End-to-End Timing Plan

## 阶段意图和边界

本阶段在 Phase 060 / 070 已采纳 production patch（生产补丁）之后，补一个更外层的
production-public evidence boundary（公开生产入口证据边界）：Std / RVV 两侧都调用真实
`pcl::io::OctreePointCloudCompression<PointXYZ>::encodePointCloud` 和 `decodePointCloud`，计时边界包含公开压缩、公开解压、tree traversal（树遍历）、entropy coding（熵编码）和当前已接入的
`PointCoding<PointXYZ>::decodePoints` RVV dispatch（RVV 分流逻辑）。

本阶段不修改 production 源码，不扩大到 color coding、其它点型、其它压缩 profile 或 public API（公开接口）
变更。它只回答一个问题：Phase 060/070 的 point coder direct（点编码器直连）收益在完整公开压缩/解压链路中是否仍可观察，或是否被树遍历和熵编码稀释。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| production | `PointCoding<PointT>::decodePoints` 已有 exact `PointXYZ` 和 traits-gated compatible point type RVV path；encode 仍标量。 |
| correctness | `make run_test_compare` 已有 public roundtrip smoke，Phase 070 后 Std / RVV 各 11 个 gtest 通过。 |
| public smoke | Phase 056 证明公开 `OctreePointCloudCompression<PointXYZ>` 往返可构造；当时还没有 production dispatch，因此没有性能结论。 |
| board evidence | Phase 060/070 production-direct board 均正向；尚无完整 public end-to-end timing。 |
| evidence role | 本阶段是 production-public。它不能替代 Phase 060/070 direct evidence，也不能单独决定其它点型或 encode path。 |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-public。 |
| A/B boundary | Std/RVV 两侧都调用真实 `OctreePointCloudCompression<PointXYZ>` public encode/decode；RVV build 内部命中已采纳 `PointCoding<PointXYZ>::decodePoints` dispatch。 |
| 当前决策问题 | 完整公开链路是否仍能看到 point coder decode RVV 收益，或收益被公开链路其它成本稀释。 |
| diagnostic 是否可外推到 production | 不外推 diagnostic；本阶段直接测 public production entry。 |
| comparison-boundary / baseline mismatch 风险 | 中等；公开链路包含 encode、树遍历、熵编码和 stream 成本，可能稀释 decode helper 收益。case 必须写清 stream、profile、点数和计时边界。 |
| weak / negative / neutral / unstable 时策略 | 若 public case 弱正向、负向或不稳定，不回滚 Phase 060/070 adopted patch；只把 public end-to-end 收益标为稀释或不成立，并记录恢复条件。 |
| 是否需要 RVV-vs-RVV detail A/B | 不需要；不是选择新 RVV family，而是验证已采纳 production helper 在更外层 public boundary 的可见收益。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / board target | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| public octree roundtrip production-public | public octree encode/decode stream | `PointXYZ` / float xyz / AoS / no color | `OctreePointCloudCompression<PointXYZ>::encodePointCloud` + `decodePointCloud` | existing public roundtrip gtest; QEMU smoke for bench label | `octree_roundtrip_public_*` with repeated board summary | production bench asm should still include point coder decode RVV instructions | repeated manifest -> Evidence Doctor | planned |
| public octree decode production-public | public octree compressed stream decode | `PointXYZ` / float xyz / AoS / no color | prebuilt public compressed stream + `OctreePointCloudCompression<PointXYZ>::decodePointCloud` | existing public roundtrip gtest; QEMU smoke for bench label | `octree_decode_public_*` with repeated board summary | production bench asm should still include point coder decode RVV instructions | repeated manifest -> Evidence Doctor | planned |

## 实现和测试动作

| action | artifact | command / evidence | done criteria |
| --- | --- | --- | --- |
| plan | 本文件 | manual review | plan exists before bench/helper edits |
| update public helper comment | `include/impl/point_coding_support.hpp` | source review | comment reflects Phase 060/070 production dispatch is now present |
| bench case | `src/bench_point_coding.cpp` | QEMU smoke and board logs | adds narrow public roundtrip and public decode labels without changing existing labels |
| manifest metadata | `script/generate_point_coding_evidence_manifest.py` | Evidence Doctor manifest | labels use `production_public` role and public wrapper metadata |
| QEMU smoke | bench smoke only | `make run_qemu_bench_smoke ... --case-filter octree_roundtrip_public_256` | log shape only; no QEMU performance conclusion |
| asm | bench RVV dump | `make dump_bench_rvv` + grep point coder decode RVV instructions | RVV instructions still attributable in bench binary |
| board repeated | public labels | bounded 10-run board repeated + doctor | bucket recorded as positive / weak / neutral / negative / unstable |
| docs | phase result, matrix, roadmap, evaluation, `doc-rvv`, Handoff | doc refresh | public result is documented separately from direct evidence |

## 板卡复跑预算和决策桶

默认使用 10-run repeated board summary，`--iterations 10 --warmup-iterations 2`，case filter 为
`octree_*_public_*`。公开链路比 direct helper 更重，先采用较低 iteration 避免板卡长时间占用。

决策桶：

- `positive`：所有 case median >= `1.05x` 且 min >= `1.0x`，Doctor 无 Error。
- `weak-positive`：至少一个 case median > `1.0x`，但存在 min < `1.0x`、median < `1.05x` 或 Doctor warning。
- `neutral`：median 在 `0.98x..1.02x` 附近，无法说明公开链路收益。
- `negative`：median < `0.98x` 或多数 run 退化。
- `unstable`：方向随 run 摇摆，Doctor warning 说明不能用当前预算关闭结论。

## 继续 / 停止条件

本阶段完成后，如果 public boundary 为 weak / neutral / negative，不继续自动改 production；Phase 060/070 direct patch 仍按接入后板卡收益保留。如果 public boundary 明显 positive，也只把完整公开链路收益记录为 additional evidence（补充证据），不扩大点型或 encode 范围。

`next_phase_default`：本阶段完成后，若无具体点型、profile 或 public API 新需求，回到 adopted closeout / review-ready。若 public 结果暴露新的稀释瓶颈，后续应另开 profile / ablation phase，不在本阶段直接改 production。
