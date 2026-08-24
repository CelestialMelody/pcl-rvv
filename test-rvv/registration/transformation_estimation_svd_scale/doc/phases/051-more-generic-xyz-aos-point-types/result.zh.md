# Phase 051 结果：more generic xyz AoS point types

## EvidenceDecision

`evidence-boundary-expanded / more-generic-xyz-aos-point-types / ordered public / qemu-correctness-smoke`

本阶段没有修改 production 源码。它把已采纳 traits-gated xyz AoS production gate（字段布局门控的 xyz 结构数组生产门控）的常见点型 correctness / QEMU smoke 证据从 Phase 041 的 `PointXYZI` / `PointXYZRGB` 代表组合，扩到更多 PCL 点型组合：

- `PointXYZRGBA -> PointXYZRGBA`
- `PointXYZL -> PointXYZ`
- `PointNormal -> PointXYZRGB`
- `PointWithRange -> PointWithRange`
- `PointWithViewpoint -> PointXYZ`

## 实际执行范围

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| A1 point type scout | done | 新增 `MoreGenericXYZAoSPointTypesMatchReference`，编译期 static_assert 证明 source / target 命中 `RVVXYZAoSFloatLayout`。 |
| A2 bench smoke labels | done | 新增 `more-generic-xyz-aos-point-types-public` case-filter，输出 5 个 64K public label。 |
| A3 manifest / registry | done | 新增 `record_qemu_more_generic_public_state`，manifest 能识别 `more generic public scale` label。 |
| A4 证据运行 | done | `run_test_compare`：Std/RVV 各 16 tests passed；QEMU smoke Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；registry fresh。 |
| A5 文档同步 | done | 本 result、matrix、roadmap、README 和 topic-local doc suite 已同步 Phase 051 边界。 |

## 证据摘要

| evidence | result | boundary |
| --- | --- | --- |
| correctness | Std/RVV 16 tests passed | ordered public path、fallback、row-source、sorted-copy、target-sorted guard、matrix-local 和更多 generic xyz AoS correctness。 |
| QEMU smoke | 5 comparisons；Doctor `0/0/0` | 只证明 build、label、manifest 和路径形状；不作为真实性能结论。 |
| registry | `evidence_status` fresh | QEMU correctness 与 Phase 051 smoke 均已登记。 |

QEMU smoke 输出的仿真耗时只作为日志完整性输入，不写性能结论。Phase 051 本阶段没有 board repeated，因此当时只能扩大 correctness / smoke 证据边界，不能替代 Phase 041 的代表点型 board 证据。后续 Phase 052 已为这 5 个新增点型补齐 64K board repeated，结果见 `doc/phases/052-more-generic-xyz-aos-board/result.zh.md`。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-public correctness/smoke`。 |
| A/B boundary | Std public ordered scale vs RVV public ordered scale，新增点型均走真实 public ordered overload。 |
| 当前决策问题 | 已采纳 traits-gated xyz AoS gate 是否有更多常见 PCL 点型的正确性支撑。 |
| diagnostic 是否可外推到 production | 这些具体点型的 correctness 可支撑 public 语义；QEMU timing 不可外推性能。 |
| comparison-boundary / baseline mismatch 风险 | low；同一 public ordered overload，同一 deterministic corpus。 |
| weak / negative / unstable 时是否允许 bounded production probe | 不适用；本阶段不做性能采纳判断。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；没有新增 RVV family。 |

## 矩阵更新

`more-generic-xyz-aos-point-types` 记为 `evidence-boundary-expanded`：它增强当前 adopted generic gate 的 correctness / QEMU smoke 覆盖，但不扩大 production gate，也不把全部自定义点型写成已验证。

后续 Phase 052 已把同一条矩阵项进一步更新为 `positive_more_generic_board_complete`。Phase 051 的本地结论仍保留为当阶段的 correctness / smoke closeout。

## 继续 / 停止判断

当前 Phase 051 已闭合，Phase 052 已接续补齐新增点型 board repeated。矩阵中剩余明确方向是更多自定义点型、row-source 更广泛点型、非法 index / correspondence、`Scalar=double` 或新的 shuffle mitigation family；这些都需要独立 scope、独立 phase 和必要时板卡证据。当前 topic 内没有一个无需扩大范围即可继续接入 production 的高优先级候选。
