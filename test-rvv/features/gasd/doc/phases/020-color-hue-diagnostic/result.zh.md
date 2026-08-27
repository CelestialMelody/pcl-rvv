# Phase 020 result: color hue diagnostic

## 执行范围

本阶段完成 `GASDColorEstimation::computeFeature` 中 color hue（颜色色相）/ `hbin`
staging（暂存）片段的 test-only diagnostic（测试专用诊断）闭环。候选把每个
`PointXYZRGBA` 样本的 RGB 值映射到 `hue/hbin` buffers（缓冲区），覆盖 production
标量路径里的 `max/min/diff_inv/std::isfinite` 分支、R/G/B 最大通道选择和负 hue 修正。

本阶段不写真实 color histogram，不覆盖 `addSampleToHistograms` 的 interpolation（插值）
写回，不修改 production（生产源码），也不证明 public `compute()` 已经命中 RVV。

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| C1 test-first 红灯 | done | `make -C test-rvv/features/gasd run_test_rvv` 首次失败于 `ColorHueBuffers` / `projectColorHue*ToBuffers` 不存在 | 测试能捕捉 Phase 020 helper 缺失 |
| C2 scalar reference | done | `include/impl/gasd_reference.hpp` 的 `projectColorHueStdToBuffers` | 标量参考复刻 production 的 hue 公式，覆盖 grayscale（灰度）和三种 max-channel 分支 |
| C3 RVV candidate | done | `include/impl/gasd_copy_candidate.hpp` 的 `projectColorHueRVVToBuffers` | RVV 路径使用 AoS byte stride load（结构数组字节跨步加载）、整数 max/min、float conversion（浮点转换）、mask merge（掩码合并）和 staging store；非 RVV 构建 fallback 到 scalar |
| C4 bench case | done | `src/bench_gasd.cpp`，case `candidate_color_hue_rvv` | 输出 checksum 和 timing；QEMU smoke 两侧 checksum 均为 `3.26169e+19` |
| C5 asm | done | `make -C test-rvv/features/gasd dump_bench_rvv`，`build/asm/riscv/bench_gasd_rvv.asm` | filtered asm 命中 `vlse8.v`、`vzext.vf2`、`vmaxu.vv`、`vminu.vv`、`vfcvt.f.xu.v`、`vfrdiv.vf`、`vmerge.vvm`、`vse32.v` |
| C6 board repeated + Doctor | done | `log/board/repeated_phase020_color_hue/summary.md`、`evidence_doctor.md` | 5-run median 1.920x，range 1.910x-1.980x，bucket=`positive`；Doctor Errors=0、Warnings=0、Suggestions=2 |
| C7 文档回填 | done | 本文件、phase index、matrix、roadmap、evaluation、README | Phase 020 诊断正向，但仍不接 production；默认继续 Phase 030 |

## EvidenceDecision

`current_decision`: `diagnostic-positive / continue-phase-loop`。color hue / hbin staging
在 test helper boundary（测试 helper 边界）上有稳定正向板卡证据，说明 GASD color
侧逐样本 RGB hue 分支存在值得继续利用的 RVV 热点。它仍只是 component diagnostic
（组件诊断），不能替代 production direct（真实生产路径）证据，也不能直接写成
production-ready（可接入生产）。

## 证据边界

| evidence | result | proves | does not prove |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/features/gasd run_test_compare`，Std/RVV 均 7/7 通过 | grayscale、R/G/B max-channel 和负 hue 修正与标量参考在容差内一致 | 不证明 color histogram interpolation 或 public compute dispatch |
| QEMU bench smoke | Std/RVV smoke 的 checksum 均为 `3.26169e+19` | bench wrapper、case-filter、checksum policy 和 fallback 路径可运行 | QEMU timing 不用于性能结论 |
| asm | `build/asm/riscv/bench_gasd_rvv.asm` | hue RVV 指令形态存在，包含 byte stride load、扩展、mask、除法、合并和 store | filtered asm 仍不是完整 public compute 热点占比证据 |
| board repeated | `log/board/repeated_phase020_color_hue/summary.md` | 目标硬件上 hue / hbin staging diagnostic 稳定正向，5-run median 1.920x | 不包含 shape compute、真实 histogram 写回、descriptor 输出或 public dispatch |
| Evidence Doctor | `log/board/repeated_phase020_color_hue/evidence_doctor.md` | Errors=0，Warnings=0，checksum 两侧一致，decision bucket=`positive` | environment metadata 和 binary identity 仍是建议项 |

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：scalar hue / hbin staging vs RVV hue / hbin staging |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 color hue 是否值得继续到 color histogram probe |
| diagnostic 是否可外推到 production | no；本阶段只计算 hue / hbin staging，不写真实 color histogram，也没有 public dispatch |
| comparison-boundary / baseline mismatch 风险 | yes；真实 production 还包含 shape compute、color interpolation 写回、descriptor copy 和对象状态 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes；但本阶段已经 positive，仍需先做更接近 production 的 interpolation / histogram probe |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 异常和修正记录

| issue | status | resolution |
| --- | --- | --- |
| 初始红灯缺少 `ColorHueBuffers` 和 `projectColorHue*ToBuffers` | fixed | 按 TDD 补 scalar reference、RVV candidate 和 fallback |
| RVV `delta / diff` 与 production 标量 `delta * diff_inv` 操作顺序不同，出现约 `3.05e-5` hue 差异 | fixed / budgeted | RVV 改为先算 `diff_inv` 再乘；保留 `1e-4` hue/hbin 容差作为 float lane 预算 |
| 板卡 repeated 输出 `Clock skew detected` | environment note | 5 个 run 的 benchmark 和日志拉取均完成，Doctor 未升为 Warning |

## Evidence Doctor 结果

| severity | count | 处理 |
| --- | ---: | --- |
| Errors | 0 | 无需修复。 |
| Warnings | 0 | 无需降级。 |
| Suggestions | 2 | `environment_metadata_missing` 和 `binary_identity_missing` 已记录为后续 evidence 增强项；不阻塞当前 diagnostic-positive。 |

## Optimization matrix 更新

`color hue projection` 更新为 `attempted / diagnostic-positive`。Phase 030 默认进入
`interpolation-ablation`，因为 shape projection 和 color hue 都只证明了 staging 层；真实
`addSampleToHistograms` 的 trilinear / quadrilinear write boundary（写回边界）仍未消融。

## continue / stop decision

`stop_condition_hit`: none。`next_phase_default`: `030-interpolation-ablation`。继续条件成立：
下一候选仍在当前 topic 的 test-only assets 内，不需要 production 授权，板卡可用，当前
Evidence Doctor 无 Error。
