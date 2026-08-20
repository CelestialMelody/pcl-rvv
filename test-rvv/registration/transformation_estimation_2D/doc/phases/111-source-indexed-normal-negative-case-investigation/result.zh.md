# Phase 111 Result: source-indexed-normal-negative-case-investigation

## 当前结论

Phase 111 完成 Phase 106 source-indexed generic public variance（源索引泛型公开入口方差）
的 Normal 类负向样本审计。本阶段没有修改 production gate（生产分流门控），也没有新增 board
performance evidence（板卡性能证据）；性能 authority 仍以 Phase 106 独立 20-run board summary
为准。

结论：当前证据不支持把 `PointNormal` / `PointXYZINormal` 纳入 source-indexed production
dispatch，也不支持恢复 full source-indexed generic widening。Normal 类 case 的 median 多数仍为正，
但 48B stride（步长）点型在 source-indexed gather + target prefix strided load 的组合下出现明显
long-tail / below-1 退化频率；这与 Phase 110 `PointXYZI -> PointXYZI` exact probe 的稳定正向边界不同。

Phase 111 结束时，Phase 110 exact `PointXYZI -> PointXYZI` 仍是 positive bounded production probe /
pending user adoption；随后 Phase 112 已按用户确认采纳该 exact gate。该采纳不能外推到 Normal 类或完整泛型。

## 证据回填

本阶段复核的主证据路径：

```text
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/doc/phases/106-source-indexed-generic-public-variance/result.zh.md
```

Phase 106 20-run 结果中，Normal 类相关风险集中在三类 case：

| case | median | min | max | `B/A<1` | bucket | Phase 111 解释 |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `PointNormal->PointNormal 256K` | `1.574x` | `0.694x` | `2.652x` | `7/20` | negative | 退化频率高，Board Doctor Error；不能用 median 正向覆盖。 |
| `PointXYZINormal->PointNormal 64K` | `2.507x` | `0.812x` | `2.780x` | `1/20` | negative | mixed target/source stride 仍有 below-1 和长尾。 |
| `PointXYZINormal->PointXYZINormal 256K` | `2.648x` | `0.753x` | `2.807x` | `1/20` | negative | 48B same-type 大规模下仍有 below-1。 |

Board Doctor 对 Phase 106 给出 `Errors=1, Warnings=27, Suggestions=0`。唯一 Error 是
`PointNormal->PointNormal 256K` 的 `7/20` below-1；Warnings 主要是 long-tail / variance
和 group outlier。因此 Phase 106 不能作为 full generic production adoption 证据。

## Layout / stride 审计

本阶段用仓库源码和 PCL traits layout 探针核对代表点型：

| point type | sizeof | align | xyz offsets | extra offsets | 当前判断 |
| --- | ---: | ---: | --- | --- | --- |
| `PointXYZ` | 16 | 16 | `0/4/8` | none | Phase 091 source-indexed exact 已 adopted；Phase 106 中 20-run 稳定 positive。 |
| `PointXYZI` | 32 | 16 | `0/4/8` | intensity `16` | Phase 110 exact probe 20-run `B/A<1=0/20`，positive 但待用户采纳。 |
| `PointNormal` | 48 | 16 | `0/4/8` | normal `16/20/24`, curvature `32` | Phase 106 256K `7/20` below-1；不支持 production gate。 |
| `PointXYZINormal` | 48 | 16 | `0/4/8` | normal `16/20/24`, intensity `32`, curvature `36` | Phase 106 大规模 / mixed case 有 below-1；不支持 production gate。 |

`RVVXYZAoSFloatLayout<PointT>` 只保证 registered single-float `x/y/z`、POD size、alignment 和
offset 可用于 AoS 直接读取。它不保证不同 stride 的板卡性能稳定。source-indexed helper 每一遍都要：

- 用 indices 对 source 做 indexed load（离散加载）；
- 对 target prefix 做 strided load（跨步加载）；
- 两遍规约 centroid 和 centered correlation。

当点型 stride 从 `PointXYZI` 的 32B 增至 Normal 类的 48B 时，source gather 和 target stride
访存压力都变重。Phase 106 的 below-1 事件集中在 48B Normal 类组合，说明当前泛型 widening 的
performance risk（性能风险）来自数据布局和 row-source 访存形态，而不是 correctness 或数学公式错误。

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | 本阶段是 evidence audit over prior production-public evidence；不新增 production patch。 |
| A/B boundary | Phase 106 public source-indexed generic guarded probe 的 Std/RVV board summary、manifest 和 Doctor。 |
| 当前决策问题 | Normal 类 exact 或 generic 子范围是否值得进入后续 production probe。 |
| 是否可外推到 production | Phase 106 是 public boundary，但代表 full generic guarded widening；本阶段只能据此拒绝 clean-adopt full widening，不能自动采纳或回滚。 |
| comparison-boundary / baseline mismatch 风险 | Phase 104 single-case 5-run positive 与 Phase 106 16-case 20-run label 不同；当前以 Phase 106 20-run 为主。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前不建议 Normal exact probe。若未来要恢复，必须使用独立 label / evidence dir，先证明 exact Normal 子范围 20-run 无 below-1 和 Doctor Error。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV A/B | 需要。Normal 类若将来出现 positive public Std/RVV probe，还需接入后 correctness、QEMU/asm、board repeated、Doctor、registry 和 PI5 用户确认。 |

## 决策边界

- `source-indexed PointNormal -> PointNormal`：rejected for production gate in current evidence。
- `source-indexed PointXYZINormal -> PointXYZINormal`：rejected / deferred until new exact evidence。
- `source-indexed PointNormal <-> PointXYZINormal` mixed pairs：not production-safe from Phase 106 representative evidence。
- `source-indexed generic widening`：仍为 guarded / not adopted；Phase 103/104 不能覆盖 Phase 106 20-run negative。
- `source-indexed PointXYZI -> PointXYZI`：Phase 111 当时保持 Phase 110 positive probe / pending user adoption；Phase 112 后已 adopted / retained。

本阶段没有发现新的、当前无需用户决策即可推进的 Normal 类 production-safe 子范围。继续扩大 Normal 类
需要先做更细的 profiling 或新的 exact Normal bounded probe；在当前 Phase 106 证据下不建议直接接入。

## Continue / Stop Decision

`continue_stop_decision`：turn-stop at user decision checkpoint。

`stop_condition_hit`：当前未发现可直接继续接入的 Normal 类候选；下一步会触及 Phase 110 probe 的采纳 /
回滚，或需要为 Normal 类开启新的 production probe 授权。Phase 110 仍需用户确认是否采纳 exact
`PointXYZI -> PointXYZI`。

`next_phase_default`：

1. 若用户确认采纳 Phase 110：进入 `112-source-indexed-pointxyzi-adoption-closeout`，把
   `PointXYZI -> PointXYZI` 写成 adopted，重跑接入后检查并准备 topic-only commit。
2. 若用户要求回滚 Phase 110：进入 rollback closeout，退回 pending gate 并重跑 correctness /
   evidence_status。
3. 若用户要求继续探索 Normal：新建独立 exact Normal bounded probe 或 profiling phase，必须使用独立
   board label / evidence dir，不覆盖 Phase 106 历史证据。
