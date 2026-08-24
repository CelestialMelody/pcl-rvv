# Phase 080 Public Octree End-to-End Timing Result

## 执行范围

本阶段按 `plan.zh.md` 执行，只修改 `test-rvv/io/point_coding` 的 bench / manifest 支撑和文档，
没有继续修改 production 源码。新增 production-public（公开生产入口）bench case：

- `octree_roundtrip_public_256/1024`：每次计时都调用真实 `OctreePointCloudCompression<PointXYZ>` 的
  `encodePointCloud` + `decodePointCloud`。
- `octree_decode_public_256/1024`：先用真实 public encoder 生成压缩流，计时阶段只调用真实
  `decodePointCloud`。

Std / RVV 两侧 checksum 一致。RVV build 内部会命中 Phase 060 已采纳的
`PointCoding<PointXYZ>::decodePoints` RVV dispatch；完整 public case 同时包含 tree traversal（树遍历）、
entropy coding（熵编码）和 stream（流）成本。

## 动作回填

| action | 状态 | 产物 / 证据 | 结论 |
| --- | --- | --- | --- |
| plan | done | `plan.zh.md` | 计划先于 bench/helper edits 存在。 |
| helper comment refresh | done | `include/impl/point_coding_support.hpp`、`src/test_point_coding.cpp` | public smoke 说明已更新为“仅 correctness，性能看 Phase 080 board”。 |
| bench case | done | `src/bench_point_coding.cpp` | 新增 public roundtrip 和 public decode labels，未改既有 labels。 |
| manifest metadata | done | `script/generate_point_coding_evidence_manifest.py` | public labels 标为 `production_public` / `public_entry`，wrapper 指向真实 public API。 |
| correctness | done | `make run_test_compare` | Std / RVV 各 11 个 gtest 通过。 |
| QEMU smoke | done | `octree_decode_public_256`、`octree_roundtrip_public_256` | 只证明 bench label 可运行和日志形状；不作为性能结论。 |
| asm attribution | done | `make dump_bench_rvv` | bench RVV asm 中可见 `vlse8.v`、`vfwcvt.f.xu.v`、`vfncvt.f.f.w`、`vssseg3e32.v`。 |
| board repeated | done | `log/board/repeated_phase080_public_octree/summary.md` | 10-run public boundary 只有 near-threshold weak signal。 |
| Evidence Doctor | done | `log/board/repeated_phase080_public_octree/evidence_doctor.md` | `Errors=1，Warnings=2，Suggestions=4`；public roundtrip 1024 退化频率触发 Error。 |

## 板卡结果

命令：

```bash
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase080_public_octree \
  BENCH_ARGS='--iterations 10 --warmup-iterations 2 --case-filter octree_*'
```

| case | runs | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `octree_decode_public_1024` | 10 | `1.01x` | `0.99x` | `1.04x` | weak / near-threshold with degradation warning |
| `octree_decode_public_256` | 10 | `1.01x` | `1.00x` | `1.08x` | weak / near-threshold |
| `octree_roundtrip_public_1024` | 10 | `1.01x` | `0.99x` | `1.03x` | neutral / weak with Doctor Error |
| `octree_roundtrip_public_256` | 10 | `1.02x` | `1.00x` | `1.05x` | weak / near-threshold with degradation warning |

这些结果说明：point coder direct 的 `1.16x..1.27x` 收益在完整 public boundary 中被 tree traversal、
entropy coding 和 stream 成本明显稀释。public decode / roundtrip 的 median 仅 `1.01x..1.02x`，不足以写成稳定公开入口加速。

## Evidence Doctor 处理

Evidence Doctor 报告为 `Errors=1，Warnings=2，Suggestions=4`。

- Error：`octree_roundtrip_public_1024` 有 `3/10` 低于 `1.0x`，不能只按 median `1.01x` 写成正向。
- Warning：`octree_decode_public_1024` 有 `2/10` 低于 `1.0x`；`octree_roundtrip_public_256` 有 `2/10` 低于 `1.0x`。
- Suggestions：四个 public case 都是 near-threshold（接近阈值），median 距 `1.0x` 不足 `0.05`。

处理方式：不把 Phase 080 作为新的 stable production-public positive evidence（稳定公开入口正向证据）。
它只作为 post-integration public boundary audit（接入后的公开边界审计）记录：完整公开链路没有呈现值得继续自动优化的稳定收益。

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | production-public。 |
| A/B boundary | Std/RVV 两侧都调用真实 `OctreePointCloudCompression<PointXYZ>` public encode/decode 或 decode-only public entry。 |
| 当前决策问题 | 已采纳 `PointCoding<PointXYZ>::decodePoints` RVV 在完整 public boundary 是否仍有可见收益。 |
| diagnostic 是否外推 | 不外推 diagnostic；本阶段直接测 public production entry。 |
| comparison-boundary / baseline mismatch 风险 | 中等；公开链路包含大量非 point coder decode 成本，实际结果证明 direct helper 收益被稀释。 |
| weak / negative / neutral / unstable 时策略 | 不回滚 Phase 060/070 adopted patch；public boundary 只标为 weak / neutral，不作为继续改 production 的依据。 |
| 是否需要 RVV-vs-RVV detail A/B | 不需要；没有引入新 RVV family。 |

## 优化矩阵更新

| candidate family | evidence | decision | 未覆盖范围 |
| --- | --- | --- | --- |
| public octree decode / roundtrip production-public | Phase 080 10-run median `1.01x / 1.01x / 1.01x / 1.02x`，Doctor `Errors=1，Warnings=2，Suggestions=4`。 | attempted / weak-near-threshold; not adopted as stable public-positive evidence | 真实文件流、其它 compression profile、真实 leaf distribution、其它点型未覆盖。 |
| production direct decode RVV | Phase 060 / 070 direct board min/median 正向。 | remains adopted production behavior | Phase 080 不推翻 direct helper 采纳，只限制 public end-to-end claim。 |

## 继续 / 停止决策

`continue_stop_decision`: pause / no further worthwhile automatic optimization in current point_coding topic.

`stop_condition_hit`: Phase 080 已覆盖当前最自然的外层 public boundary。结果只有 near-threshold weak signal，
并有 Evidence Doctor Error / Warning；继续扩大 run count 只能更精确地描述稀释，不太可能导出新的 production patch。
encode 方向仍因语义 / 性能证据保持 rejected；继续堆同类 traits case也难以改变生产决策。

`next_phase_default`: adopted closeout / review-ready。若未来要继续，应先有新的 profile（性能剖析）或真实 public workload，
再另开 profile / ablation phase；不建议在当前 topic 内自动继续改 production。
