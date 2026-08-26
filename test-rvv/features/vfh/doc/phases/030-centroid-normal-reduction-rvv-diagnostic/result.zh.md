# Phase 030 Centroid Normal Reduction RVV Diagnostic Result

## 当前结论

Phase 030 已闭合。本阶段新增 test-only `computeVFHSignatureCentroidsSPFHAndViewpointRVV`
（测试专用候选 helper），在 Phase 020 combined helper 基础上把 normal centroid（法线质心）规约也放进
RVV（RISC-V Vector，可变长度向量）路径。xyz centroid（坐标质心）在 RVV 构建下已由 common
`compute3DCentroidRVV` 覆盖，因此本阶段新增证据主要回答 normal centroid reduction 是否值得纳入
production probe（生产探针）。

EvidenceDecision（证据决策）：`attempted / positive diagnostic`。Phase 030 candidate 在板卡 5-run 中稳定
快于 Phase 020 candidate，但仍是 test helper 边界，不能替代 production direct（真实生产路径直连）证据。

## 实际执行范围

| action | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| RED-1 | done | `src/test_vfh.cpp` 的 `VFHCandidate.CentroidsSPFHAndViewpointRVVMatchesReference` | helper 缺失时 `run_test_rvv` 编译失败，测试先行成立。 |
| GREEN-1 | done | `include/impl/vfh_reference.hpp` 的 `computeNormalCentroidRVV` 与 `computeVFHSignatureCentroidsSPFHAndViewpointRVV` | RVV 构建返回 true，非 RVV 构建保持 false fallback；直方图与 reference 在 `2e-3` 误差预算内。 |
| BENCH-1 | done | `src/bench_vfh.cpp` 的 `candidate_vfh_centroids_spfh_viewpoint_rvv` | bench 输出同时包含 Phase 000、020、030 candidate。 |
| ASM-1 | done | `make -C test-rvv/features/vfh dump_bench_rvv` | `bench_vfh_rvv.asm` 可见 `vfadd.vv` 与 `vfredosum.vs` 等规约相关 RVV 指令。 |
| BOARD-1 | done | `make -C test-rvv/features/vfh board_smoke BENCH_ARGS='--side 80 --iterations 3 --warmup 1'` | 板卡 smoke 通过，新增 case 出现且 checksum 与 reference 一致。 |
| BOARD-2 | done | `make -C test-rvv/features/vfh board_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-phase030 BENCH_ARGS='--side 80 --iterations 8 --warmup 2'` | 5-run repeated 已完成；远端 Make 报 clock skew warning，但不影响日志回收和 bench 完成。 |
| DOCTOR-1 | done | `make -C test-rvv/features/vfh evidence_doctor_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-phase030` | `Errors=0, Warnings=0, Suggestions=12`。 |

## Board repeated 结果

输入为 dense finite `PointNormal -> VFHSignature308`、`float`、默认 45/128 bin、full-cloud
sequential indices，`side=80`、`points=6400`、`iterations=8`、`warmup=2`。Phase 030 repeated 使用独立目录
`test-rvv/features/vfh/log/board/repeated-phase030`，避免覆盖 Phase 020 的 `log/board/repeated` 证据。

| case | speedup runs | mean | min / max | 平均 Std / RVV |
| --- | --- | ---: | ---: | --- |
| `candidate_vfh_centroid_spfh_rvv` | `2.66, 2.67, 2.60, 2.66, 2.72` | `2.662x` | `2.60x / 2.72x` | `3.7802 ms / 1.4211 ms` |
| `candidate_vfh_spfh_viewpoint_rvv` | `2.92, 2.73, 2.83, 2.87, 2.96` | `2.862x` | `2.73x / 2.96x` | `3.7707 ms / 1.3184 ms` |
| `candidate_vfh_centroids_spfh_viewpoint_rvv` | `2.98, 3.03, 3.01, 3.07, 3.00` | `3.018x` | `2.98x / 3.07x` | `3.7858 ms / 1.2542 ms` |
| `component_vfh_reference` | `1.00, 1.02, 1.02, 1.01, 1.01` | `1.012x` | `1.00x / 1.02x` | `3.7728 ms / 3.7326 ms` |
| `public_vfh_compute_baseline` | `1.01, 1.00, 1.01, 1.00, 1.01` | `1.006x` | `1.00x / 1.01x` | `8.8293 ms / 8.7666 ms` |

Phase 030 candidate 的平均 RVV 耗时比 Phase 020 candidate 少约 `0.0642 ms`，约 `4.9%`；满足本阶段
“相对 Phase 020 缩短超过 3%”的 positive bucket。相对 Phase 000 centroid-only candidate，平均 RVV
耗时少约 `0.1669 ms`，约 `11.7%`。public baseline 仍接近 1x，继续说明 test-only diagnostic 不能替代
production performance（生产性能）证据。

## Evidence Doctor

当前报告路径：

- `test-rvv/features/vfh/log/board/repeated-phase030/evidence_manifest.json`
- `test-rvv/features/vfh/log/board/repeated-phase030/evidence_doctor.md`
- `test-rvv/features/vfh/log/board/repeated-phase030/evidence_doctor.json`

Evidence Doctor 结果为 `Errors=0, Warnings=0, Suggestions=12`。Suggestion 包括环境 metadata
（device、taskset、governor、freq、temperature）和 binary identity（二进制身份）缺失，以及
`component_vfh_reference` / `public_vfh_compute_baseline` near-threshold（接近阈值）。这些不阻塞当前
diagnostic decision，但要求后续 production direct 证据补齐环境和二进制身份。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic`；本阶段不是 production direct。 |
| A/B boundary | `test-rvv/features/vfh/src/bench_vfh.cpp` 中 Std build 与 RVV build 的 test helper case。 |
| 当前决策问题 | `RVV-vs-scalar`，并辅助判断 production probe 是否应包含 normal centroid reduction。 |
| diagnostic 是否可外推到 production | 不能直接外推；production 仍需 `VFHEstimation::compute()` 真实边界重跑。 |
| comparison-boundary / baseline mismatch 风险 | 存在；`public_vfh_compute_baseline` 没有接入新 RVV helper。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive diagnostic；若 production-public 后续 near-threshold，只能保留 bounded candidate 或回退。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。Phase 030 family 应与 Phase 020 family 在同一 production boundary 内比较。 |

## 矩阵更新和下一步

`vfh-centroid-normal-reduction-rvv` 从 `phase_deferred + unblocked` 更新为
`attempted / positive diagnostic`。如果后续用户授权进入 PI2，优先 production probe 的候选形态应包含：

1. common `compute3DCentroidRVV` 覆盖的 xyz centroid；
2. 本阶段验证的 normal centroid RVV reduction；
3. Phase 000/020 验证的 centroid-to-point pair math 与 viewpoint normal-dot preparation；
4. 仍保持标量顺序的 histogram scatter 和 production fallback gate。

`continue_stop_decision`：当前 topic-local 高优先级 O(N) 诊断候选已完成；继续推进 production integration loop
需要用户明确确认修改 `features/include/pcl/features/impl/vfh.hpp`。在没有该确认前，合法停止条件是
`turn_stop_deferred with stop_condition_hit=production_patch_authorization_required`。

默认下一恢复入口：用户确认可修改 production 后进入 PI2；否则保持当前 diagnostic 资产和 phase 文档，不把
`doc-rvv/features/vfh-RVV.zh.md` 创建为 production 长期主题文档。
