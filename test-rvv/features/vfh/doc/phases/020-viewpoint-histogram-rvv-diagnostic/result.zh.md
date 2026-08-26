# Phase 020 Viewpoint Histogram RVV Diagnostic Result

## 当前结论

Phase 020 已闭合。本阶段新增 test-only `computeVFHSignatureSPFHAndViewpointRVV`
（测试专用候选 helper），在 Phase 000 的 centroid-to-point SPFH-like pair math
（从质心到点的简化点特征直方图点对数学）基础上，把 viewpoint histogram
（视点直方图）的 normal dot 与 bin preparation（法线点积和分箱准备）也放进 RVV
（RISC-V Vector，可变长度向量）路径，histogram scatter（直方图离散累加）仍保持标量顺序。

EvidenceDecision（证据决策）：`attempted / positive diagnostic`。它证明当前 test helper
边界内的 combined candidate 比 Phase 000 centroid-only candidate 更快，但仍不能证明 production
dispatch（生产分流）已经成立。

## 实际执行范围

| action | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| RED-1 | done | `src/test_vfh.cpp` 的 `VFHCandidate.SPFHAndViewpointRVVMatchesReference` | helper 缺失时曾出现预期编译失败；测试先行成立。 |
| GREEN-1 | done | `include/impl/vfh_reference.hpp` 的 `accumulateViewpointRVV` 与 `computeVFHSignatureSPFHAndViewpointRVV` | RVV 构建返回 true，非 RVV 构建保持 false fallback。 |
| BENCH-1 | done | `src/bench_vfh.cpp` 的 `candidate_vfh_spfh_viewpoint_rvv` | bench 输出同时包含 Phase 000 和 Phase 020 candidate。 |
| ASM-1 | done | `make -C test-rvv/features/vfh dump_bench_rvv` | 反汇编用于确认 RVV bench binary 中含候选路径；具体 production 归属仍不适用。 |
| BOARD-1 | done | `make -C test-rvv/features/vfh board_repeated BENCH_ARGS='--side 80 --iterations 8 --warmup 2'` | 5-run board repeated 已完成；远端 Make 报 clock skew warning，但不影响日志回收和 bench 完成。 |
| DOCTOR-1 | done | `make -C test-rvv/features/vfh evidence_doctor_repeated` | `Errors=0, Warnings=0, Suggestions=10`。 |

## Board repeated 结果

输入为 dense finite `PointNormal -> VFHSignature308`、`float`、默认 45/128 bin、full-cloud
sequential indices，`side=80`、`points=6400`、`iterations=8`、`warmup=2`。性能结论只来自
Milkv-Jupiter 板卡 repeated summary；QEMU 不用于性能结论。

| case | speedup runs | mean | min / max | 平均 Std / RVV |
| --- | --- | ---: | ---: | --- |
| `candidate_vfh_centroid_spfh_rvv` | `2.64, 2.59, 2.64, 2.61, 2.66` | `2.628x` | `2.59x / 2.66x` | `3.7429 ms / 1.4258 ms` |
| `candidate_vfh_spfh_viewpoint_rvv` | `2.89, 2.86, 2.88, 2.91, 2.88` | `2.884x` | `2.86x / 2.91x` | `3.7446 ms / 1.2984 ms` |
| `component_vfh_reference` | `1.01, 1.00, 1.01, 1.00, 1.01` | `1.006x` | `1.00x / 1.01x` | `3.7487 ms / 3.7180 ms` |
| `public_vfh_compute_baseline` | `1.00, 1.00, 1.02, 1.00, 1.00` | `1.004x` | `1.00x / 1.02x` | `8.8255 ms / 8.7740 ms` |

Phase 020 combined candidate 的平均 RVV 耗时比 Phase 000 centroid-only candidate 约少
`0.1274 ms`，约 `8.9%`。这说明 viewpoint normal-dot preparation 在当前 test helper 边界内值得纳入
后续 bounded production probe（有界生产探针）候选；公开入口 baseline 仍接近 1x，不能写成 production
performance（生产性能）收益。

## Evidence Doctor

当前报告路径：

- `test-rvv/features/vfh/log/board/repeated/evidence_manifest.json`
- `test-rvv/features/vfh/log/board/repeated/evidence_doctor.md`
- `test-rvv/features/vfh/log/board/repeated/evidence_doctor.json`

Evidence Doctor 结果为 `Errors=0, Warnings=0, Suggestions=10`。Suggestion 均不阻塞当前阶段：
环境 metadata（device、taskset、governor、freq、temperature）和 binary identity（二进制身份）缺失，
以及 `component_vfh_reference` / `public_vfh_compute_baseline` 接近 1x 阈值。处理动作是保留诊断结论边界：
candidate 数字可作为 test-only diagnostic 正向证据，不能升级成 clean adopted production behavior
（干净采纳的生产行为）。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic`；本阶段是 test helper 边界，不是 production direct（真实生产路径直连）。 |
| A/B boundary | `test-rvv/features/vfh/src/bench_vfh.cpp` 中 Std build 与 RVV build 的 test helper case。 |
| 当前决策问题 | `RVV-vs-scalar`，并辅助判断后续 production probe 应选择 centroid-only 还是 combined candidate。 |
| diagnostic 是否可外推到 production | 不能直接外推；production 仍要在 `VFHEstimation::compute()` 真实边界重跑。 |
| comparison-boundary / baseline mismatch 风险 | 存在；`public_vfh_compute_baseline` 没有接入新 RVV helper，只是当前公开入口稀释参照。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive diagnostic；若未来 production-public 仍 near-threshold，只能保留 bounded probe 或回退，不能 clean-adopt。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。combined candidate 应在 PI2/PI5 与 centroid-only 或标量 production detail 同边界比较。 |

## 矩阵更新和下一步

`vfh-viewpoint-histogram-rvv` 从 `deferred` 更新为 `attempted / positive diagnostic`。PI1 的生产候选范围应从
Phase 000 centroid-only 扩展为优先考虑 `centroid-to-point SPFH-like pair math + viewpoint normal-dot preparation`
combined helper，但 production patch 仍需要用户明确授权。

`continue_stop_decision`：Phase 020 完成，但 topic 不收口。当前仍有两个未闭合方向：

1. `vfh-production-probe`：`turn_stop_deferred with stop_condition_hit=production_patch_authorization_required`，需要用户确认才能修改 `features/include/pcl/features/impl/vfh.hpp`。
2. `vfh-centroid-normal-reduction-rvv`：`phase_deferred + unblocked`，仍在 topic-local diagnostic 边界内，可作为下一阶段继续评估 centroid / normal centroid reduction（质心 / 法线质心规约）是否进一步降低公开入口稀释。

默认下一 phase：`030-centroid-normal-reduction-rvv-diagnostic`。
