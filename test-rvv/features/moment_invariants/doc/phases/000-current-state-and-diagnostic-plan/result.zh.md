# Phase 000: current-state and moment accumulation diagnostic result

## 执行摘要

本阶段建立了 `features/include/pcl/features/impl/moment_invariants.hpp` 的 test-only diagnostic（仅测试使用的诊断）资产，隔离 centroid（质心）之后的 6 个中心矩累加和 `j1/j2/j3` 公式。production（生产源码）未修改。

结果是 weak-positive diagnostic（弱正向诊断）：indexed helper-only（只计索引 helper）在板卡 5-run repeated（重复采集）中 median speedup 为 1.141x，min 为 1.095x，0/5 run 低于 1.0。这个结果支持继续 Phase 010 的 public-search-shaped dilution check（公开入口形态稀释检查），但不能单独支持 production integration loop（生产接入闭环）。

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| 创建 topic scaffold | done | `Makefile`、`board.mk`、`include/`、`src/`、`doc/` | 形成可独立运行的 test-rvv topic。 |
| TDD 红灯 | done | `make run_test_rvv` 曾因 `computeMomentSummaryRVV` 缺失失败 | 红灯证明测试能捕捉 RVV helper 缺失。 |
| 实现 Std/RVV helper | done | `include/impl/moment_invariants_reductions.hpp` | RVV 构建使用 `vluxei32` gather（离散加载）、`vlse32` stride load（跨步加载）和 `vfredusum` vector reduction（向量规约）；非 RVV 构建回到 Std helper。 |
| 正确性验证 | done | `make run_test_compare` | Std 3/3 pass，RVV 3/3 pass；QEMU（仿真器）只作为正确性和路径证据。 |
| 反汇编检查 | done | `build/asm/riscv/bench_moment_invariants_rvv.asm` | `vluxei32`、`vlse32`、`vfsub`、`vfmul`、`vfredusum`、`vsetvli` 出现，归属为 test-only helper 内联到 bench。 |
| 板卡 repeated bench | done | `log/board/repeated_phase000_moment_accumulation_diagnostic/summary.md` | 5-run median 1.141x，decision bucket 为 `weak_positive`。 |
| Evidence Doctor | done | `log/board/repeated_phase000_moment_accumulation_diagnostic/evidence_doctor.md` | 0 Error / 0 Warning / 2 Suggestion；建议补环境 metadata 和 binary hash。 |
| registry 记录 | done | `log/evidence_registry.json` | Phase 000 summary、manifest、doctor 已登记为 fresh。 |

## 证据分层

| 层级 | 当前证据 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness（正确性） | `make run_test_compare` | Std/RVV helper 与 production helper 的 `j1/j2/j3` 在容差内一致。 | 不证明公开入口 `computeFeature` 已走 RVV。 |
| QEMU path（QEMU 路径） | QEMU gtest 和小规模 bench smoke | 构建、日志形状和 helper 路径可运行。 | 不证明真实性能。 |
| asm（反汇编） | `build/asm/riscv/bench_moment_invariants_rvv.asm` | RVV 指令存在于 candidate helper 所在 bench binary。 | 不证明 production 符号命中。 |
| board performance（板卡性能） | Phase 000 summary | helper-only indexed moment accumulation 在该合成输入上弱正向。 | 不证明 KdTree search 外层后的公开入口收益。 |

## diagnostic-to-production mismatch audit

| 问题 | 回填结果 |
| --- | --- |
| evidence role | `diagnostic`。 |
| A/B boundary | `test helper`，计时边界为 centroid 后中心矩累加 helper。 |
| 当前决策问题 | RVV-vs-scalar helper value（RVV 相对标量 helper 是否有局部收益）。 |
| 是否可外推到 production | 不能直接外推。production 入口还包含 `searchForNeighbors`、centroid 和输出状态维护。 |
| mismatch 风险 | helper-only 没有计入 KdTree search、public fallback、非 dense 输入和真实 `MomentInvariantsEstimation::computeFeature` dispatch。 |
| 弱 / 负 / 中性时 bounded production probe 条件 | weak-positive 只允许继续 public-search-shaped diagnostic；不直接进入 production patch。 |
| clean adoption 是否需要 detail A/B | 需要真实 production direct correctness、fallback、asm、board repeated 和 PI5 用户确认。 |

## Evidence Doctor 解释

Phase 000 没有 Error 或 Warning。两个 Suggestion 不阻塞当前 diagnostic 结论：环境字段缺失会削弱长尾解释能力，binary hash 缺失会削弱旧二进制排除能力；由于 5-run 全部正向且 decision bucket 稳定，本阶段不重跑板卡，把这两项记录为后续 evidence hygiene（证据卫生）改进。

## 继续 / 停止决定

本阶段未命中停止条件。helper-only weak-positive 触发 Phase 010：继续检查真实 KdTree search 外层是否稀释收益。Phase 000 不能作为 production-ready（可接入生产）结论。
