# Phase 020 结果：matrix-local scale simplification diagnostic

## 结果摘要

本阶段完成 `matrix-local-scale-simplification` 的 correctness（正确性）、QEMU smoke（QEMU 小型验证）、board repeated（板卡重复采集）、Evidence Doctor（证据体检）和 registry（证据登记）闭环。该候选把旧后段公式中的 `R4 * cloud_src_demean` 后再逐列点积，改成 `sum_tt = trace(R * H)`。

结论是 `implementation_shape_weak_positive_deferred`：局部公式数值等价，board B/A（baseline/candidate 对比）整体为 `weak_positive`，可以保留为较小 production patch（生产补丁）备选线索；但它不是 RVV intrinsic（RVV 内建指令）证据，也不是 production direct（真实生产路径）证据，不能改变 Phase 010 的 `production_patch_positive_pending_user_confirmation`。

## 实际执行范围

| 维度 | 实际范围 |
| --- | --- |
| evidence role | `implementation_shape_diagnostic`（实现形态诊断证据）。 |
| A/B boundary | test helper micro path；Std build 使用 legacy formula，RVV build 使用 trace formula。 |
| row source | ordered-cloud-pair（顺序点云对）。 |
| point type / Scalar / layout | `PointXYZ -> PointXYZ` / `float` / dense xyz AoS。 |
| sizes | correctness 使用 8192 点；QEMU 和 board smoke 覆盖 4K、64K、256K。 |
| production scope | 未修改 production 源码；不修改当前 PI5 待确认状态。 |

## 计划动作回填

| action | status | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| helper | done | `include/impl/tesvd_scale_candidates.hpp` 中 `estimateScaleMatrixLocalLegacy` / `estimateScaleMatrixLocalTrace` | 两条局部后段公式可输出 4x4 matrix 并返回可比较 checksum。 |
| correctness | done | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare` | Std/RVV 各 7 个 TEST 全通过；新增 TEST 为 `MatrixLocalScaleSimplificationMatchesLegacyPath`。 |
| QEMU smoke | done | `ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter matrix-local-scale --iterations 3 --warmup-iterations 1"` | 日志可解析，`max_reference_error` 最大 `2.384186e-07`；QEMU timing 不作为性能证据。 |
| QEMU doctor / registry | done | `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_matrix_local_scale_smoke_state` | `log/qemu/evidence_doctor.md` 当前为 matrix-local smoke，`Errors=0`、`Warnings=0`、`Suggestions=0`。 |
| correctness registry | done | `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correctness_state` | registry 已记录 7-test correctness。 |
| board repeated | done with plan deviation | `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_matrix_local_scale_repeated` | 虽然 plan 原先把 board repeated 放在后续条件，本轮已完成 5-run 采集；结果整体 `weak_positive`。 |
| docs | done | 本 result、phase index、optimization matrix、roadmap、testing / benchmark / correctness / code map / evaluation 文档 | 已写清本候选只作为实现形态诊断，不触发 adoption（采纳）。 |

## correctness 与 QEMU 证据

`run_test_compare` 当前聚合为 7 个 TEST。新增 `MatrixLocalScaleSimplificationMatchesLegacyPath` 使用 dense `PointXYZ` ordered pairs，对比 legacy formula 和 trace formula 的 4x4 matrix，误差预算为 `5e-4`。

QEMU smoke 只验证 build、case label、checksum 和 manifest 形状。`log/qemu/evidence_doctor.md` 当前对应 `matrix-local-scale`，不是 Phase 010 的 `public-scale` smoke 裸文件；Phase 010 public-scale smoke 作为历史登记和 Phase 010 result 事实保留。

## board repeated 证据

| case | runs B/A | median | min | max | bucket | max ref error |
| --- | --- | ---: | ---: | ---: | --- | ---: |
| `matrix local scale simplification ordered-cloud-pair 4K` | 2.276, 1.952, 1.719, 1.719, 1.809 | 1.809 | 1.719 | 2.276 | `positive` | 1.192e-07 |
| `matrix local scale simplification ordered-cloud-pair 64K` | 1.225, 1.187, 1.207, 1.175, 1.173 | 1.187 | 1.173 | 1.225 | `weak_positive` | 1.192e-07 |
| `matrix local scale simplification ordered-cloud-pair 256K` | 1.245, 1.154, 1.186, 1.161, 1.130 | 1.161 | 1.130 | 1.245 | `weak_positive` | 1.192e-07 |

summary path：`log/board/matrix_local_scale_repeated/summary.md`。Evidence Doctor path：`log/board/matrix_local_scale_repeated/evidence_doctor.md`。

Evidence Doctor 结果为 `Errors=0`、`Warnings=2`、`Suggestions=0`。两个 warning 都来自 4K：`long_tail_or_variance` 和 `group_outlier`。处理方式是按 size 分开报告，不把 4K 的较高 B/A 外推到 64K / 256K；整体 decision bucket 降为 `weak_positive`。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `implementation_shape_diagnostic`。 |
| A/B boundary | test helper micro path，不是 public overload，也不是 production dispatch。 |
| 当前决策问题 | implementation-shape：局部代数改写是否正确、是否值得保留为较小 patch 备选。 |
| diagnostic 是否可外推到 production | no；它不覆盖父类 centroid / demean、公开入口 dispatch、fallback 或当前生产 RVV helper。 |
| comparison-boundary / baseline mismatch 风险 | yes；Std/RVV build 只是 legacy formula 与 trace formula 的载体，不表示 RVV 指令收益。 |
| weak-positive 时是否允许 bounded production probe | yes, only if 用户不采纳 direct-fused patch 或明确想比较较小 patch；届时必须另写 PI1 plan、public path correctness、fallback、asm 和 board evidence。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | no for this diagnostic；若接 production，它需要自身 production direct 证据，而不是复用本阶段 micro path。 |

## optimization matrix 更新

`matrix-local-scale-simplification` 从 `phase_deferred + unblocked` 更新为 `implementation_shape_weak_positive_deferred`。它的 board 证据是 weak-positive，不能替代 `direct-fused-scale-accum` 的 production direct positive，也不能成为自动采纳理由。

## evidence registry 与 freshness

执行 `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` 后 registry 为 fresh。当前 QEMU 裸路径已经被 Phase 020 smoke 覆盖，因此文档中引用 `log/qemu/evidence_doctor.md` 时必须说明它是可覆盖输出；Phase 010 public-scale smoke 事实以 Phase 010 result / registry 条目为准。

## 阶段反思新增路线

本阶段产生一条较小 patch 备选路线：如果用户不采纳当前 `direct-fused-scale-accum` production patch，或希望降低 production 改动规模，可以另建 `matrix-local-production-probe`，只接 `sum_tt = trace(R * H)` 的局部后段简化。该路线需要重新进入 production integration loop，并用真实 public path 证明收益；本阶段不能直接支撑接入。

## continue / stop decision

本阶段已完成，`stop_condition_hit=PI5_user_confirmation_boundary`。默认下一动作仍是等待用户确认 Phase 010 production patch 是否采纳；确认前不更新 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档，不把当前 production patch 标成 adopted，也不回滚。
