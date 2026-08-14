# Phase 010 结果：board repeated diagnostic 和 Evidence Doctor

## 当前结论

Phase 010 已完成。`fused_ordered_cloud_pair_accum`（历史 alias：`fused_full_cloud_accum`）在 Milkv-Jupiter 板卡上的 test-only diagnostic（测试专用诊断）为 `positive`。Phase 010 当时没有 production（生产源码）RVV dispatch（分流逻辑）或 fallback（回退路径）补丁，因此本阶段 EvidenceDecision（证据决策）是 `partial-production-candidate`：可以进入 PI1 production integration plan（生产接入计划），不能直接写成 production-ready。

2026-08-14 命名刷新后重跑了本 summary，输出 label 统一为 ordered-cloud-pair。该重跑发生在 Phase 020 production patch 之后，因此 public build sanity（公开入口构建一致性）只保留为 diagnostic sanity，不替代 Phase 020 的 production direct 证据。

Phase 010 本身没有修改 `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp`；当前 production patch 记录在 Phase 020。

## 执行范围回填

| action | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| B1 board correctness smoke | done | `make -C test-rvv/registration/transformation_estimation_svd run_board_test_smoke`；`log/board/test_smoke/run_test.log` | 板卡 RVV gtest 8/8 passed。 |
| B2 repeated board collect | done | `make -C test-rvv/registration/transformation_estimation_svd run_board_bench_ordered_cloud_pair_repeated`；兼容别名 `run_board_bench_fused_full_cloud_repeated`；`log/board/fused_full_cloud_repeated/run-01..run-05/` | 5-run，20 iterations，5 warm-up iterations。 |
| B3 summary / manifest | done | `test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/summary.md`；`test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/evidence_manifest.json` | summary 按 same-boundary 和 mixed-boundary 分表，manifest 标为 diagnostic。 |
| B4 Evidence Doctor | done | `test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/evidence_doctor.md` | Errors=0、Warnings=4、Suggestions=0。 |
| B5 registry record / freshness | done | `test-rvv/registration/transformation_estimation_svd/log/evidence_registry.json`；`make evidence_status` | summary / manifest / doctor 已登记；文档刷新后 `evidence_status` 为 fresh。 |

## 板卡结果摘要

same-boundary fused Std/RVV（同边界 fused 标量 / RVV）只比较同一 test-support fused helper 的 Std 与 RVV 构建，用于判断 RVV accumulation 本身是否有收益。

| size | repeated B/A values | median | bucket |
| --- | --- | ---: | --- |
| 4K | `3.063, 3.094, 3.008, 3.109, 3.081` | 3.081 | `positive` |
| 64K | `3.087, 2.992, 3.179, 3.206, 3.186` | 3.179 | `positive` |
| 256K | `3.115, 3.166, 3.145, 3.157, 3.174` | 3.157 | `positive` |

mixed-boundary public baseline vs fused RVV（混合边界 public baseline 与 fused RVV）使用当前 public Umeyama Std 作为 baseline、test-only fused RVV 作为 candidate。它只回答是否值得进入 PI1；不能证明 production dispatch。

| size | repeated B/A values | median | bucket |
| --- | --- | ---: | --- |
| 4K | `15.396, 17.023, 16.174, 16.297, 15.686` | 16.174 | `positive` |
| 64K | `26.059, 25.397, 26.782, 26.839, 26.631` | 26.631 | `positive` |
| 256K | `25.989, 26.460, 26.294, 26.575, 26.362` | 26.362 | `positive` |

public build sanity（公开入口构建一致性）只检查 Std/RVV build drift（构建漂移），不作为候选收益。命名刷新后的重跑发生在 Phase 020 production dispatch 已接入之后，因此 public build sanity 也呈现 `positive`；这不改变 Phase 010 当时的 pre-production 证据角色。production 结论仍以 `production_ordered_cloud_pair_repeated` summary 为准。

## Evidence Doctor 解释

| finding | 处理动作 | 对结论的影响 |
| --- | --- | --- |
| `production_name_without_role` x3 | 文档和 manifest 均把 public build sanity 写成 diagnostic sanity，不写 production evidence。 | 不阻塞 PI1，但禁止把 public sanity 表当作 production direct。 |
| `group_outlier`：4K mixed-boundary 低于 64K / 256K | 按 size 分开报告；PI1 必须保留 small-input fallback 或单独 size gate 审计。 | 不触发追加复跑；same-boundary 4K 仍为 positive。 |

本阶段没有 Evidence Doctor Error。decision bucket 在计划内 5-run 中稳定，不使用最多 1 次的追加同边界复跑预算。

## Optimization Matrix 更新

| candidate family | row source policy | point type / Scalar / layout | board evidence | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- |
| `fused_ordered_cloud_pair_accum` | ordered-cloud-pair | `PointXYZ` / `float` / dense xyz AoS | same-boundary fused Std/RVV median `3.081x` / `3.179x` / `3.157x` | Errors=0、Warnings=4、Suggestions=0 | `partial-production-candidate` | PI1 plan |
| `public_umeyama_baseline` | ordered-cloud-pair | `PointXYZ` / `float` | public build sanity：4K negative、64K/256K neutral | warning 已解释 | baseline only | PI1 中作为 production direct baseline |
| `source_indexed_fused_accum` | source-indexed-cloud-pair | `PointXYZ` / `float` | missing | missing | planned | ordered-cloud-pair PI1 边界明确后做 row source audit |
| `dual_indices_or_correspondences` | dual-indices / correspondences | `PointXYZ` / `float` | missing | missing | planned | 单独 phase |
| `production_direct_dispatch` | ordered-cloud-pair first | generic / `float` / layout gated | missing | missing | phase_deferred + unblocked | 先写 PI1，不直接 production patch |

## 诊断证据链

1. correctness：QEMU Std/RVV 各 8 个 gtest 通过；板卡 RVV smoke 8 个 gtest 通过。
2. QEMU path evidence：`ordered-cloud-pair` QEMU bench smoke 只证明 bench binary、case label 和 checksum 形状可解析；QEMU timing 不作为性能结论。
3. asm attribution：bench RVV 反汇编中可见 `vlsseg3e32.v`、`vfadd.vv`、`vfmacc.vv` 和 `vfredosum.vs`，归属到 test-support candidate / bench helper，不是 production symbol。
4. board performance：Phase 010 repeated board 来自目标硬件，支撑 pre-production diagnostic positive。
5. production boundary：当前没有 production patch、production direct tests、fallback tests 或 production bench；diagnostic evidence 不能替代 production evidence。

## 文档和 freshness

本结果刷新以下当前事实归属：

- `README.zh.md`
- `doc/testing-overview.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/transformation_estimation_svd-evaluation.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc-rvv/library-screening/registration/registration-module-second-pass.zh.md`

summary-only 证据路径为：

- `test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/evidence_doctor.md`

raw `run-01..run-05` 日志和远端路径默认 local-only，不进入默认提交边界。

## Continue / Stop Decision

`continue_stop_decision`：Phase 010 已闭合，当前没有板卡或 Evidence Doctor blocker。继续到 production patch 会扩大到 production 源码，因此下一步先进入 PI1 production integration plan，冻结范围、fallback 和暂停条件。

`stop_condition_hit`：当前阶段可以收口，因为 Phase 010 计划动作均已完成，下一动作属于生产接入计划边界；不应在没有 PI1 plan 的情况下直接修改 production。

`next_phase_default`：`doc/phases/020-pi1-production-integration-plan/plan.zh.md` 已创建并在后续恢复中修订。下一步若获得 production patch 授权，按 ordered-cloud-pair、`Scalar=float`、source/target 分别满足 `RVVXYZAoSFloatLayout`、`use_umeyama_ == true` 的范围进入 PI2；source-indexed、dual-indices、correspondences、`Scalar=double` 和泛型异常布局默认保持标量。
