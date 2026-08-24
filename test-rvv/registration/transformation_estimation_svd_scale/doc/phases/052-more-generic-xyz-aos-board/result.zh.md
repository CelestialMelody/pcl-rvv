# Phase 052 结果：more generic xyz AoS board evidence

## EvidenceDecision

`positive_more_generic_board_complete / more-generic-xyz-aos-point-types / ordered public / board-repeated`

本阶段没有修改 production 源码。它把 Phase 051 新增的 5 个常见 PCL xyz AoS 点型组合，从 correctness / QEMU smoke（QEMU 小型验证）扩展到 board repeated（板卡重复采集）性能证据。证据边界仍是 ordered-cloud-pair public overload、`Scalar=float`、dense、traits-gated xyz AoS；不覆盖 source-indexed、dual-indexed、correspondence、全部自定义点型或 `Scalar=double`。

## 实际执行范围

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| A1 board target 接线 | done | `run_board_bench_more_generic_xyz_aos_point_types_public_repeated`、`collect_board_bench_more_generic_xyz_aos_point_types_public_repeated`、`record_board_more_generic_public_state` 已接入 Makefile。 |
| A2 summary wrapper 扩展 | done | `script/generate_tesvd_scale_board_repeated_summary.py` 已识别 `more-generic-xyz-aos-point-types-public` 和 `production_public_more_generic`。 |
| A3 board repeated | done | 5 runs，20 iterations，5 warmup；summary / manifest / Evidence Doctor 已生成。 |
| A4 registry | done | `log/evidence_registry.json` 已登记 summary、manifest 和 doctor，run label 为 `board-tesvd-scale-more-generic-xyz-aos-point-types-public-repeated-phase052`。 |
| A5 文档同步 | done | 本 result、matrix、roadmap、README、evaluation、testing / benchmark / code-map 和 production 长期文档同步 Phase 052 边界。 |

## Board repeated 摘要

证据路径：

- `log/board/more_generic_xyz_aos_point_types_public_repeated/summary.md`
- `log/board/more_generic_xyz_aos_point_types_public_repeated/evidence_manifest.json`
- `log/board/more_generic_xyz_aos_point_types_public_repeated/evidence_doctor.md`

| case | median B/A | min | max | bucket |
| --- | ---: | ---: | ---: | --- |
| `PointXYZRGBA -> PointXYZRGBA 64K` | `24.490x` | `24.104x` | `24.701x` | positive |
| `PointXYZL -> PointXYZ 64K` | `26.934x` | `26.866x` | `27.127x` | positive |
| `PointNormal -> PointXYZRGB 64K` | `20.555x` | `19.879x` | `20.602x` | positive |
| `PointWithRange -> PointWithRange 64K` | `24.635x` | `23.954x` | `24.693x` | positive |
| `PointWithViewpoint -> PointXYZ 64K` | `26.989x` | `26.726x` | `27.166x` | positive |

全部 5 个 case 的 decision bucket 都是 `positive`。B/A = Std more-generic public scale ms / RVV more-generic public scale ms，大于 1 表示当前 public RVV path 快于 public scalar path。`log checksum` 在 summary 中为 mismatch，这是 RVV reduction tree（规约树）与标量规约顺序不同后的日志指纹差异；正确性仍以 Phase 051 `run_test_compare` 16 tests 和当前 bench `max_reference_error <= 2e-3` 合同为准。

## Evidence Doctor 和 Registry

| 项目 | 结果 |
| --- | --- |
| Evidence Doctor | `Errors=0`、`Warnings=0`、`Suggestions=0` |
| comparisons | 5 |
| evidence role | `production_public_more_generic` |
| case-filter | `more-generic-xyz-aos-point-types-public` |
| registry | `record_board_more_generic_public_state` 已登记 summary / manifest / doctor |

本阶段没有 Evidence Doctor warning 需要降级或复跑。QEMU timing 仍不参与性能结论；性能结论只来自上述 board repeated summary。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-public board evidence`，真实 public ordered overload。 |
| A/B boundary | Std public ordered scale vs RVV public ordered scale；同一 case-filter，计时包含 public path、RVV / 标量 scale 路径和 3x3 SVD 后段。 |
| 当前决策问题 | Phase 051 新增常见 PCL xyz AoS 点型是否也有板卡 repeated performance 支撑。 |
| diagnostic 是否可外推到 production | 当前是 production-public 边界，可支撑这 5 个具体组合的 board performance；不能外推到全部自定义点型、row source 或 `Scalar=double`。 |
| comparison-boundary / baseline mismatch 风险 | low；Std/RVV 使用同一 public wrapper、同一 deterministic corpus 和同一 case-filter。 |
| weak / negative / unstable 时是否允许 bounded production probe | 不适用；production gate 已存在，本阶段只升级新增点型的性能证据边界。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；没有新增 RVV family 或 family selection。 |

## 矩阵更新

`more-generic-xyz-aos-point-types` 从 Phase 051 的 `evidence-boundary-expanded / qemu-correctness-smoke` 更新为 `positive_more_generic_board_complete`。它现在同时有 correctness、QEMU smoke、board repeated、Evidence Doctor 和 registry 证据，但仍只覆盖 5 个具体常见 PCL 点型组合。

## 继续 / 停止判断

本阶段完成，`continue_stop_decision=turn_stop_deferred with current_phase_complete`。当前没有必须立刻接入 production 的新实现，因为 Phase 052 不改变生产源码，也没有新增 RVV family。仍可继续的新 scope 是更多自定义 xyz AoS 取样、row-source 更广点型、非法 index / correspondence、`Scalar=double` 或新的 shuffle mitigation family；这些都需要独立 phase plan 和同边界证据。
