# Phase 070 PointXYZ-like Traits Expansion Result

## 执行范围

本阶段按 `plan.zh.md` 执行，把 Phase 060 已采纳的 exact `PointXYZ` decode RVV path 扩成
PointXYZ-like traits gate（类似 PointXYZ 的点型特征门控）。生产入口仍是
`pcl::octree::PointCoding<PointT>::decodePoints`；RVV 构建下只有满足
`pcl::rvv::kRVVXYZAoSPointCompatible<PointT>` 的 AoS（结构数组）点型命中 RVV helper，其它点型和非 RVV
构建继续走 `decodePointsStd`。

本阶段实际验证的代表点型是 `PointXYZI` 和 `PointXYZRGB`。它们证明 `x/y/z` 可按 traits offset 写回，且
`intensity`、`r/g/b` 这类额外字段不会被 RVV store 覆盖。本阶段不覆盖 encode path、完整
`OctreePointCloudCompression` public end-to-end（公开入口端到端）性能、所有自定义点型或其它目标硬件。

## 实现结果

| action | 状态 | 产物 / 证据 | 结论 |
| --- | --- | --- | --- |
| production patch | done | `io/include/pcl/compression/point_coding.h` | `decodePoints` 使用 `kRVVXYZAoSPointCompatible<PointT>` 分流；`decodePointsRVV` 使用 `RVVXYZAoSFloatLayout<PointT>::kX/kY/kZ` 和 `strided_store3_f32m2<sizeof(PointT), ...>` 写回。 |
| correctness | done | `make run_test_compare` | Std / RVV 各 11 个 gtest 通过，新增 `PointXYZI` / `PointXYZRGB` traits-hit extra-field preservation（额外字段保持）测试，并补 `PointCodingTestXYZDouble` non-compatible fallback。 |
| QEMU smoke | done | `make run_qemu_bench_smoke ... --case-filter decode_production_direct_traits_xyzi_1024` | 仅证明 RVV bench 可运行和日志形状；不作为性能结论。 |
| Std QEMU smoke | done | `make run_bench_std ... --case-filter decode_production_direct_traits_xyzi_1024` | checksum 与 RVV smoke 对齐，证明两侧 case label 和输出口径一致。 |
| asm attribution | done | `make dump_bench_rvv` | 可见 `vlse8.v`、`vfwcvt.f.xu.v`、`vfncvt.f.f.w` 和 `vssseg3e32.v`；紧凑 xyz layout 由公共 store wrapper 选择 segment store。 |
| board repeated | done | `log/board/repeated_phase070_traits_decode/summary.md` | 10-run production-direct repeated 全部 median/min 正向。 |
| Evidence Doctor | done | `log/board/repeated_phase070_traits_decode/evidence_doctor.md` | `Errors=0，Warnings=2，Suggestions=0`；warning 是 `PointXYZRGB` 两个规模的 long-tail / variance。 |

## 板卡结果

命令：

```bash
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase070_traits_decode \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_production_direct_traits_*'
```

| case | runs | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `decode_production_direct_traits_xyzi_1024` | 10 | `1.27x` | `1.25x` | `1.33x` | positive |
| `decode_production_direct_traits_xyzi_4096` | 10 | `1.20x` | `1.13x` | `1.26x` | weak-positive / positive |
| `decode_production_direct_traits_xyzrgb_1024` | 10 | `1.27x` | `1.18x` | `1.44x` | positive with long-tail warning |
| `decode_production_direct_traits_xyzrgb_4096` | 10 | `1.19x` | `1.11x` | `1.31x` | weak-positive / positive with long-tail warning |

所有 case 的 min 都大于 `1.0x`，median 都大于 `1.05x`。按本阶段计划和用户口径，接入后的板卡测试显示有收益即可采纳，因此 traits-gated decode RVV path 进入 adopted production behavior（已采纳生产行为）。

## Evidence Doctor 处理

Evidence Doctor 报告为 `Errors=0，Warnings=2，Suggestions=0`。

两个 warning 都是 `PointXYZRGB` 代表点型上的 long_tail_or_variance（长尾 / 方差）：

- `decode_production_direct_traits_xyzrgb_1024`：min `1.18x`，median `1.27x`，max `1.44x`。
- `decode_production_direct_traits_xyzrgb_4096`：min `1.11x`，median `1.19x`，max `1.31x`。

处理方式：保留 min / median / max，不剔除高侧异常，不把单一均值写成稳定性结论。因为两个 warning 的 min 仍为正向，且没有 checksum mismatch、metadata 缺失或 comparison boundary mismatch（比较边界不一致），本阶段不因 warning 降级为 unstable。

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | production_direct。 |
| A/B boundary | Std/RVV 两侧都调用真实 `PointCoding<PointT>::decodePoints` production entry。 |
| 当前决策问题 | traits-gated PointXYZ-like decode RVV 是否保持语义并在代表点型上快于标量 production decode。 |
| diagnostic 是否外推 | 不外推旧 diagnostic；采纳数据来自本阶段接入后的 production-direct board repeated。 |
| comparison-boundary / baseline mismatch 风险 | 低；case 两侧只差 RVV 构建下的 production dispatch。仍不覆盖完整 public octree pipeline。 |
| weak / negative / neutral / unstable 时策略 | 若代表点型出现非正向或 Doctor Error，应回退到 Phase 060 exact `PointXYZ` adopted scope；本轮未触发。 |
| 是否需要 RVV-vs-RVV detail A/B | 不需要；本阶段不是新 RVV family 选择，而是同一已采纳 RVV family 的 traits gate 扩围。 |

## 优化矩阵更新

| candidate family | point type / layout | evidence | decision | 未覆盖范围 |
| --- | --- | --- | --- | --- |
| production decode RVV v0 | exact `PointXYZ` AoS | Phase 060 10-run median `1.22x / 1.23x / 1.18x / 1.16x`，Doctor `Errors=0，Warnings=1`。 | adopted | Phase 080 后续已测 public end-to-end，结果为 weak / near-threshold；不推翻 direct adoption。 |
| traits-gated production decode RVV | `PointXYZI`、`PointXYZRGB` 代表的 xyz AoS traits gate | Phase 070 10-run median `1.27x / 1.20x / 1.27x / 1.19x`，Doctor `Errors=0，Warnings=2`。 | adopted | 所有自定义点型、非 xyz AoS、非标准布局、其它硬件未覆盖。 |
| encode production RVV | source-indexed leaf encode | Phase 020/030 证明 f32 语义风险和 f64 exact 性能成本。 | rejected / scalar-only | 需要新的输入域合同或误差合同才能重开。 |
| full public octree end-to-end | `OctreePointCloudCompression<PointXYZ>` public entry | Phase 080 后续完成 public decode / roundtrip 10-run：median `1.01x / 1.01x / 1.01x / 1.02x`，Doctor `Errors=1，Warnings=2，Suggestions=4`。 | attempted / weak-near-threshold | 不作为稳定 public-positive evidence；需要真实 workload / profile 才值得重开。 |

## 继续 / 停止决策

`continue_stop_decision`: pause / no further high-priority unblocked optimization inside current authorized point_coding production patch.

`stop_condition_hit`: 当前 topic 已完成最有价值的未阻塞生产扩展：exact `PointXYZ` 和 traits-gated PointXYZ-like decode 都有接入后 production-direct 板卡收益；encode 方向已有语义 / 性能拒绝证据。完整 public octree end-to-end timing 后续已由 Phase 080 测量，结果为 weak / near-threshold，不支持继续自动扩大 production patch。

`next_phase_default`: Phase 080 后续已完成 public boundary audit。当前不建议继续做更多同类 synthetic traits case 或 public synthetic runs；它们只会扩大代表点型数量或更精细描述稀释，难以改变生产决策。重新推进需要真实 workload / profile 或具体点型需求。
