# Phase 073 Result: correspondence sorted-copy `Scalar=double` detail A/B

## 当前结论

本阶段完成同一 production detail boundary（生产细节边界）内的 RVV-vs-RVV A/B。结果为 `detail_ab_negative`：correspondence sorted-copy `Scalar=double` 在 64K / 256K shuffled correspondence 上都慢于既有 D64 gather RVV family。

这不推翻 Phase 072 的 public Std/RVV positive（公开入口标量 / RVV 正向）事实；Phase 072 仍证明当前 sorted-copy double public probe 快于 public scalar fallback。但 Phase 073 回答的是另一个问题：sorted-copy double 是否优于当前已存在的 D64 gather RVV family。板卡证据显示不是，因此不能把 sorted-copy double clean-adopt 为新的实现族。当前 production patch 不自动回滚；是否接受 bounded candidate 风险或回滚该分支，需要用户明确确认。

## 执行范围

已验证范围：

- evidence role：`production_detail_rvv_vs_rvv`。
- A/B boundary：production detail helper。
- row source：correspondence。
- 点型 / Scalar / layout：`PointXYZ -> PointXYZ`、`Scalar=double`、dense xyz AoS。
- 规模：shuffled correspondence 64K / 256K。

未验证 / 不外推范围：

- Phase 069 / 070 / 071 的 candidates；本阶段没有采纳或拒绝它们，后续 Phase 074 已按用户确认收口为 `adopted-by-user`。
- public Std/RVV 性能；Phase 072 已独立覆盖。
- custom layout double、generic point type double、dual-indexed sorted-copy、小规模 sorted-copy、非法 correspondence。
- production rollback；本阶段只形成用户决策输入，不自动撤回生产补丁。

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| RED target 缺失 | done | 先前运行 `make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_correspondence_sorted_copy_scalar_double_detail_ab_smoke`，失败为 target 缺失。 | RED 有效，说明 Phase 073 target 是新增证据链。 |
| bench case-filter | done | `src/bench_tesvd_scale.cpp` 新增 `correspondence-sorted-copy-scalar-double-detail-ab`。 | baseline 调 D64 gather detail + solve，candidate 调 sorted-copy double detail，输出 paired labels。 |
| QEMU smoke + manifest + Doctor + registry | done | `record_qemu_correspondence_sorted_copy_scalar_double_detail_ab_state`；`log/qemu/correspondence_sorted_copy_scalar_double_detail_ab/summary.md`；`evidence_doctor.md`。 | QEMU 只证明 build / path / label / checksum / manifest shape；Doctor 为 `Errors=2`、`Warnings=0`，原因是 QEMU 单次 B/A 均小于 1，不能作为性能结论。 |
| board repeated + manifest + Doctor + registry | done | `run_board_bench_correspondence_sorted_copy_scalar_double_detail_ab_repeated`；`log/board/correspondence_sorted_copy_scalar_double_detail_ab_repeated/summary.md`；`evidence_doctor.md`。 | 板卡 5-run 显示 64K / 256K 全部 negative；Doctor 为 `Errors=2`、`Warnings=1`、`Suggestions=0`。 |
| docs / matrix / roadmap / handoff | done | 本 result、phase README、optimization matrix、roadmap、topic docs、长期 `doc-rvv` 和 current handoff。 | Phase 072 已回填为 `bounded_public_positive_detail_ab_negative`，不写 adopted。 |

## QEMU 证据

命令：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correspondence_sorted_copy_scalar_double_detail_ab_state
```

结果：

- `64K`：B/A `0.736x`，checksum match，max reference error `8.038e-14`。
- `256K`：B/A `0.809x`，checksum match，max reference error `2.474e-13`。
- Evidence Doctor：`Errors=2`、`Warnings=0`、`Suggestions=0`。

QEMU timing（QEMU 计时）不作为性能证据。这里的 Error 只说明单次 smoke 的 B/A 小于 1，不能用它写性能结论；真正的性能判断以板卡 repeated 为准。

## Board 证据

命令：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_correspondence_sorted_copy_scalar_double_detail_ab_repeated
```

结果：

| case | B/A values | median | min | max | bucket | checksum | max reference error |
| --- | --- | ---: | ---: | ---: | --- | --- | ---: |
| `64K` | `0.295, 0.316, 0.282, 0.274, 0.283` | `0.283x` | `0.274x` | `0.316x` | `negative` | match | `8.038e-14` |
| `256K` | `0.431, 0.427, 0.426, 0.441, 0.464` | `0.431x` | `0.426x` | `0.464x` | `negative` | match | `2.474e-13` |

Evidence Doctor：

- Errors：2 个 `ba_degradation_frequency`，64K 和 256K 都是 5/5 run 低于 1。
- Warnings：1 个 `long_tail_or_variance`，64K max/min 约 `1.15`。
- Suggestions：0。

处理方式：这不是 checksum 或 correctness 问题，而是同边界 family-selection 证据明确显示 sorted-copy double 额外 copy + sort 成本大于 query-side locality 收益。保留 min / median / max，不删除异常值；64K warning 不改变 negative bucket。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_detail_rvv_vs_rvv`。 |
| A/B boundary | production detail helper；baseline 为 D64 gather RVV，candidate 为 sorted-copy double RVV。 |
| 当前决策问题 | RVV-family-selection（RVV 实现族选择）。 |
| 是否可外推到 production | 只能外推到 Phase 072 的 public correspondence / `PointXYZ -> PointXYZ` / `Scalar=double` / dense shuffled 64K + 256K boundary。 |
| comparison-boundary / baseline mismatch 风险 | 两侧同在 RVV binary 内；candidate 额外包含 correspondence copy + sort，且 reduction order 不同，manifest 已声明 allowed mismatch。checksum match 和 max reference error 约束数值语义。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | Phase 072 public probe 可继续作为 bounded public positive 事实保留，但本阶段负向 detail A/B 不支持 clean adoption。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 已完成，结果为 negative；因此默认不能 clean-adopt sorted-copy double family。 |

## Optimization Matrix 更新

新增 / 更新矩阵条目：

- `correspondence-sorted-copy-scalar-double-detail-ab`：`rejected_for_clean_adoption_with_detail_ab_negative`。
- `correspondence-sorted-copy-scalar-double-production-probe`：从 detail A/B 未完成的 bounded candidate 更新为 `bounded_public_positive_detail_ab_negative`。

Phase 074 已根据用户确认把 Phase 069 / 070 / 071 收口为 `adopted-by-user`。本阶段的 sorted-copy double detail A/B 负向结论不改变这些已采纳边界。

## Continue / Stop Decision

`continue_stop_decision`：`turn_stop_deferred_with_user_decision_required`。

停止原因：

- sorted-copy double clean adoption 的必要 detail A/B 已完成且为 negative。
- 当前 production patch 不能由 worker 自动回滚；回滚或接受 bounded public positive 风险都需要用户明确授权。
- Phase 074 后 Phase 069 / 070 / 071 已完成用户确认和采纳收口。

`next_phase_default`：Phase 069 / 070 / 071 的采纳收口已由 Phase 074 完成。若用户确认回滚或拒绝 Phase 072 sorted-copy double branch，再开 rollback / no-production closeout。若用户明确接受 Phase 072 bounded candidate 风险，也需要单独 closeout 写明风险接受边界。
