# Phase 000 Current State And VFH Scaffold Result

## 阶段结论

Phase 000 已闭合，当前 EvidenceDecision（证据决策）是
`partial-production-candidate / PI1-plan-ready`。test-only candidate
`computeVFHSignatureCentroidSPFHRVV` 在 dense finite `PointNormal` + sequential full-cloud
indices 上与标量 reference same-chain（同构链路）对拍通过；board repeated（重复板卡测试）五轮稳定正向；
Evidence Doctor（证据体检）只对 reference / public baseline 报出退化频率和环境元数据缺失提醒，没有否定
candidate 的诊断结论。

当前结论只说明 centroid-to-point pair math（从质心到点的点对数学）在诊断边界内值得继续，不说明
production patch（生产补丁）已经存在，也不说明公开 `VFHEstimation::compute()` 已获得
production direct（真实生产路径直连）证据。下一步默认进入 PI1 production integration plan
（生产接入计划）；在用户确认可修改 production 之前，不改 `features/include/pcl/features/impl/vfh.hpp`。

## 计划动作回填

| action | status | command / evidence | result |
| --- | --- | --- | --- |
| RED-1 reference tests | done | `make -C test-rvv/features/vfh run_test_compare` | public compute reference gate 和 candidate 对拍都通过。 |
| GREEN-1 candidate helper | done | `include/impl/vfh_reference.hpp` | `computeVFHSignatureCentroidSPFHRVV` 仅在 `__RVV10__` 下启用，且对 sequential full-cloud indices 返回成功。 |
| BENCH-1 bench wrapper | done | `src/bench_vfh.cpp` | 输出 `component_vfh_reference`、`candidate_vfh_centroid_spfh_rvv`、`public_vfh_compute_baseline` 三个 case。 |
| ASM-1 asm attribution | done | `dump_bench_rvv` + `build/asm/riscv/bench_vfh_rvv.asm` | 反汇编中可见 RVV load / arithmetic / reduction 片段，candidate 边界清楚。 |
| BOARD-1 repeated board | done | `board_repeated` + `evidence_doctor_repeated` | 5-run repeated board 落盘，candidate 2.61x-2.70x，public baseline 0.97x-1.01x。 |
| DOC-1 phase docs | done | 本文件、matrix、roadmap、evaluation、README、phase index | 当前结论和下一阶段入口已同步。 |

## Correctness（正确性）

`run_test_compare` 通过，Std / RVV 两侧三项测试都为 PASSED：

- `VFHReference.MatchesPublicComputeDefaultDescriptor`
- `VFHReference.MatchesPublicComputeWithSizeComponent`
- `VFHCandidate.CentroidSPFHRVVMatchesReference`

这说明当前 reference 复刻了 `VFHEstimation::compute()` 的默认描述子语义，也说明 test-only RVV helper
在 Phase 000 的边界内没有破坏 same-chain 对拍。

## Bench / Board（性能证据）

board repeated 使用 `Milkv-Jupiter`，输入是 synthetic VFH dense PointNormal cloud，
`side=80`、`points=6400`、`iterations=8`、`warmup=2`。

| case | 5-run B/A 区间 | 结论 |
| --- | --- | --- |
| `component_vfh_reference` | `0.99x` - `1.00x` | reference 本身基本持平，只能做对照。 |
| `candidate_vfh_centroid_spfh_rvv` | `2.61x` - `2.70x` | centroid-to-point pair math 在诊断边界内稳定正向。 |
| `public_vfh_compute_baseline` | `0.97x` - `1.01x` | 公开入口尚未获得稳定稀释收益，只能作为 production gap 提醒。 |

## Evidence Doctor

`evidence_doctor_repeated` 结果为 `Errors=1, Warnings=1, Suggestions=7`。异常都集中在
`component_vfh_reference` 和 `public_vfh_compute_baseline` 的退化频率 / 元数据缺失上：

- `component_vfh_reference` 有 3/5 低于 1 的 B/A 频率，说明如果只看均值或中位数会掩盖退化频次。
- `public_vfh_compute_baseline` 有 1/5 低于 1，且 median 只在 1.0 附近，不能当成稳定 production 收益。
- candidate case 没有被 doctor 否定，但环境元数据和 binary identity 还没写全，下一轮 board 记录应补齐。

结论是：candidate 正向可信，但 public baseline 仍不足以证明生产接入已经成立。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic` / `production_public_baseline`。candidate 是 test-helper 边界，public baseline 是公开入口稀释检查。 |
| A/B boundary | test helper / public overload。 |
| 当前决策问题 | 先判断 `VFHEstimation::compute()` 的 public dilution 是否值得进入 PI1，而不是直接写 production patch。 |
| diagnostic 是否可外推到 production | 不能直接外推。candidate 只覆盖 helper 边界；public baseline 还没有 production direct path。 |
| comparison-boundary / baseline mismatch 风险 | 有。candidate 和 public baseline 的计时边界不同，不能把 helper 正向写成 production 证据。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许继续到 PI1 plan，但生产补丁仍需用户确认；若后续 production-public 仍是 near-threshold，则应继续保留诊断边界。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。若未来出现多个 RVV family，必须补同边界 RVV-vs-RVV。 |

## 矩阵更新

| candidate family | decision | reason | next action |
| --- | --- | --- | --- |
| `vfh-scalar-reference-scaffold` | adopted | public compute reference gate 已通过，能作为后续对拍基线。 | 仅保留为测试基线。 |
| `vfh-centroid-spfh-rvv` | attempted / positive diagnostic | 5-run repeated board 稳定正向，且 asm、correctness、doctor 均闭合。 | 进入 PI1 生产接入计划。 |
| `vfh-public-dilution-baseline` | attempted / near-threshold | public baseline 只在 1x 附近波动，且有一次退化。 | 仅作 production gap 参照。 |
| `vfh-production-probe` | blocked_pending_user_confirmation | 进入 PI2 会修改 `features/include/pcl/features/impl/vfh.hpp`，需要用户明确确认。 | 先完成 PI1 计划并等待确认。 |

## 继续 / 停止判断

本阶段不再继续 RED/GREEN，因为当前已收束到 PI1 入口。下一步默认恢复点是
`010-production-integration-plan`。如果用户确认可以修改 production source，再进入 PI2；
如果暂不授权，就保持当前 diagnostic 资产和 PI1 计划，不把 `doc-rvv/features/vfh-RVV.zh.md`
提前写成 adopted production 文档。
