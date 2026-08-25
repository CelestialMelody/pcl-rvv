# Phase 000 result: current state and reduction diagnostic

## 执行范围

本阶段按计划完成 test-only（仅测试使用）reduction summary diagnostic（规约摘要诊断）：`PointXYZ`、`float`、AoS（结构数组）布局、ordered indexed cloud（按 `indices` 顺序遍历的点云），覆盖 mean/AABB、covariance、single-axis moment of inertia（单轴惯性矩）和 OBB extrema（有向包围盒极值）。Production（生产源码）未修改。

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| A1 test-first 红灯 | done | `make run_test_rvv` 初次失败：`computeReductionSummaryRVV` 不是 `moi` 成员 | 测试能抓到缺失 candidate |
| A2 Std/RVV diagnostic helper | done | `include/impl/moi_reductions.hpp`；`make run_test_compare` | Std/RVV QEMU correctness（正确性）通过 |
| A3 bench diagnostic | done | `src/bench_moi.cpp`；QEMU smoke `make run_bench_rvv BENCH_ARGS='--case-filter moi_reductions --points 4096 --iterations 2 --warmup-iterations 1'` | QEMU 只证明可运行和日志形状，不作为性能结论 |
| A4 反汇编归属 | done | `build/asm/riscv/bench_moi_rvv.asm` | 命中 `vluxei32`、`vfredusum/vfredmin/vfredmax`、`vfmacc` |
| A5 板卡 repeated bench | done | `log/board/repeated_phase000_reduction_diagnostic/summary.md` | 5-run median speedup 2.082x，decision bucket `positive` |
| A6 Evidence Doctor / registry | done | `log/board/repeated_phase000_reduction_diagnostic/evidence_doctor.md`；`log/evidence_registry.json` | Errors=0，Warnings=0，Suggestions=1；registry fresh |

## EvidenceDecision

`current_decision`: diagnostic-positive。该结果支持继续做 projected covariance fusion（投影协方差融合）diagnostic，并允许后续讨论 bounded production probe（有界生产探针）。它不能直接证明真实 `MomentOfInertiaEstimation<PointT>::compute()` production direct（真实生产路径）收益，因为 phase 000 不包含 angle scan（角度扫描）、projected cloud allocation（投影点云分配）、Eigen solver（特征求解器）和 production dispatch（生产分流）。

## 证据边界

| evidence | result | proves | does not prove |
| --- | --- | --- | --- |
| QEMU correctness | `run_test_compare` 通过 | RVV helper 与 Std helper 在容差内等价 | 目标硬件性能、production dispatch |
| asm attribution | filtered asm 命中 gather / reduction / FMA 指令 | bench RVV binary 确实含 RVV 指令 | 指令全部归属于 production helper |
| board performance | median 2.082x，min 2.048x，max 2.133x | test helper boundary 下有稳定正向性能信号 | 完整 `compute()` 端到端收益 |
| Evidence Doctor | Errors=0，Warnings=0，Suggestions=1 | 当前 manifest 没有阻塞性证据异常 | binary identity 仍可补强 |

Raw checksum（原始校验值）因为浮点规约顺序不同而不相等；本阶段不把 raw checksum 当 strict equality gate（严格相等验收），语义等价性由 QEMU gtest 的 numerical tolerance gate（数值容差验收）证明。

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：Std reduction summary vs RVV reduction summary |
| 当前决策问题 | RVV-vs-scalar diagnostic；是否值得继续生产形态或投影协方差诊断 |
| diagnostic 是否可外推到 production | no，当前只能说明同边界 helper 有收益；完整 production 还包含角度扫描、投影点云、Eigen 求解和对象状态 |
| comparison-boundary / baseline mismatch 风险 | yes，diagnostic helper 融合了多个 production 循环的摘要，和真实 `compute()` 的内存分配 / 角度循环边界不同 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive；若后续 phase 010 仍 positive，可进入 PI1 计划。若 phase 010 negative，应先做 no-production 或更窄 probe 判断 |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | yes；任何 production adoption 都必须经 PI1-PI5 和用户确认 |

## Optimization matrix 更新

`fused xyz reductions` 在当前 diagnostic 边界下为 `attempted_positive`，未升级为 production-adopted。`projected cloud covariance fusion` 仍是 `phase_deferred + unblocked`，下一 phase 默认入口为 `010-projected-covariance-fusion-diagnostic`。

## Evidence Doctor 和 registry

- summary: `log/board/repeated_phase000_reduction_diagnostic/summary.md`
- manifest: `log/board/repeated_phase000_reduction_diagnostic/evidence_manifest.json`
- doctor: `log/board/repeated_phase000_reduction_diagnostic/evidence_doctor.md`
- registry: `log/evidence_registry.json`
- `make evidence_status`: fresh

Evidence Doctor suggestion `binary_identity_missing` 不阻塞当前 diagnostic conclusion；若后续出现方向反转或进入 production direct，再补 binary hash / build label。

## continue / stop decision

`stop_condition_hit`: none。当前 topic 内仍有未阻塞的 high-priority candidate：projected covariance fusion。按 phase loop 继续到 phase 010。

`next_phase_default`: `010-projected-covariance-fusion-diagnostic`
