# Phase 010 Result: scaffold-and-ordered-cloud-pair-correlation-diagnostic

## 执行范围

本阶段实际范围与 `plan.zh.md` 一致：只修改 `test-rvv/registration/transformation_estimation_2D/**`，不修改 production 源码，不创建长期 `doc-rvv` production 主题文档。

## 计划动作完成矩阵

| id | 动作 | 状态 | 证据路径 / 命令 | 结论 |
| --- | --- | --- | --- | --- |
| A1 | 写 topic scaffold | done | `Makefile`、`board.mk`、`include/te2d.h`、`include/impl/te2d_candidates.hpp`、`src/test_te2d.cpp`、`src/bench_te2d.cpp` | 已采用 `src/`、`include/`、`include/impl/` 布局，topic token 为 `te2d`。 |
| A2 | public semantics tests | done | `make -C test-rvv/registration/transformation_estimation_2D run_test_compare` | Std / RVV 各 10 个 gtest 全通过；覆盖 size mismatch、非有限 x/y/z 和基础 ordered-cloud-pair。 |
| A3 | scalar reference 与 fused candidate | done | `include/impl/te2d_candidates.hpp`、`src/test_te2d.cpp` | 初版 raw sums 在近抵消样本上失败，已改为两遍中心化 correlation；当前 correctness 通过。 |
| A4 | bench smoke 和 asm dump | done | `run_bench_ordered_cloud_pair_smoke`、`dump_bench_rvv` | QEMU smoke 可运行并输出 checksum；asm dump 有 vector 指令，但热点归属未完全闭合。 |
| A5 | topic-local doc suite | done | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` | 文档能定位 target、helper、证据边界和未覆盖范围。 |
| A6 | 更新索引和矩阵 | done | `README.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` | 旧 `full-cloud` 恢复入口同步为 `ordered-cloud-pair`；下一阶段为 `020-board-and-asm-evidence`。 |

## 关键实现结论

候选从 raw sums（原始和）公式调整为两遍中心化结构。原因是 RVV reduction tree（规约树）改变 raw sums 的抵消顺序，在 near-cancellation（近抵消）样本上会放大误差。当前结构先求 source/target 的 x/y 质心，再直接累加中心化后的 `H00/H01/H10/H11`，仍避免两份 4xN dynamic demean matrix（动态去中心化矩阵）和 Eigen correlation multiply。

非有限输入没有被 candidate 重新定义：x/y 非有限的 public path 会产生非有限矩阵；z 非有限会影响 centroid 的 accepted rows。test-only candidate 对非 dense finite 输入退回 public path。

## Correctness / QEMU / ASM / Board 分层结论

| 证据层 | 状态 | 事实 | 不能证明 |
| --- | --- | --- | --- |
| correctness | pass | `run_test_compare`：Std 10/10 pass，RVV 10/10 pass。 | 不能证明目标硬件性能。 |
| QEMU smoke | pass | `run_bench_ordered_cloud_pair_smoke` 生成 RVV bench 输出和 checksum。 | QEMU timing 不进入性能结论。 |
| asm input | partial | `dump_bench_rvv` 生成 `build/asm/riscv/bench_transformation_estimation_2D_rvv.asm`，可见 `vlsseg3e32`、`vfsub`、`vfmacc`、`vfredosum`。 | helper inline 后热点符号归属未完全闭合。 |
| board performance | missing | 未运行 board / target hardware repeated benchmark。 | 当前没有 speedup / slowdown 结论。 |
| production boundary | no production | production 源码未修改。 | 不能写 production-ready。 |

## Evidence Doctor 和 Evidence Registry

本阶段没有生成正式 JSON manifest 和 Evidence Doctor 报告；原因是当前只形成 QEMU correctness、QEMU smoke 和 asm input。人工轻量检查：

| severity | count | 说明 | 处理 |
| --- | --- | --- | --- |
| Errors | 0 | correctness 全通过；bench smoke checksum 非空。 | 无需降级。 |
| Warnings | 2 | QEMU smoke metadata 不完整；asm 只有指令存在和局部 grep，缺正式热点归属。 | 降级为 smoke / asm input。 |
| Suggestions | 2 | Phase 020 补 manifest + Evidence Doctor；Phase 020 补 board repeated summary。 | 已写入 roadmap 和 matrix。 |

`log/evidence_registry.json` 已本地生成并登记：

- `qemu-te2d-correctness-phase010`：`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`。
- `qemu-te2d-ordered-cloud-pair-smoke-phase010`：`log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log`。

这些路径被 `test-rvv/.gitignore` 排除，默认不提交。文档引用它们作为本地证据指针。

## 板卡复跑预算和决策桶

本阶段未运行 board performance。Phase 020 默认预算仍为 5 次 repeated run，warm-up 5 次、measurement 20 次；若 Evidence Doctor 或 summary 显示方向接近阈值，最多一次同边界确认复跑。当前 decision bucket 为 `not_available`。

## Doc Suite Parity 审计

| area | current shape scan | quality bar / optional calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已有并更新 | 需要当前结论、先读路径、命令、提交边界和 `doc-rvv` 适用性。 | adopted | `README.zh.md` | 随 Phase 020 证据刷新。 |
| testing-overview | 已创建 | 需要运行入口分类、覆盖矩阵、QEMU / board 边界和证据白名单。 | adopted | `doc/testing-overview.zh.md` | none |
| correctness-tests | 已创建 | 需要逐测试说明输入、被测路径、断言和证明范围。 | adopted | `doc/correctness-tests.zh.md` | none |
| benchmark-and-evidence | 已创建 | 需要 case-filter、计时边界、QEMU smoke、board、asm、registry 和提交边界。 | adopted | `doc/benchmark-and-evidence.zh.md` | Phase 020 补正式 manifest / doctor。 |
| optimization-evidence | 已创建 | 需要候选到代码、target、证据和决策的映射。 | adopted | `doc/optimization-evidence.zh.md` | 随 Phase 020 决策刷新。 |
| test-support-code-map | 已创建 | 需要聚合入口、internal helper、src、script 和 output code map。 | adopted | `doc/test-support-code-map.zh.md` | Phase 020 若新增 script 再更新。 |
| evaluation | 已更新 | 需要 EvidenceDecision、Traceability Map 和 production boundary。 | adopted | `doc/transformation_estimation_2D-evaluation.zh.md` | Phase 020 刷新证据链。 |
| long-term `doc-rvv` | 不存在 | 只有 adopted production behavior 后适用。 | not_applicable with evidence | 无 production patch / PI5。 | PI5 后再创建。 |
| phase index / result | 已更新 | 需要 phase 恢复入口、计划和结果。 | adopted | `doc/phases/README.zh.md`、本文件 | 下一阶段创建 `020-board-and-asm-evidence`。 |
| artifact tracking | topic files untracked / to-be-staged；logs ignored-local | README / evaluation / roadmap 引用文件必须存在并进入提交候选或明确 local-only。 | adopted | `git status --short --untracked-files=all -- test-rvv/registration/transformation_estimation_2D`；`git check-ignore` 确认 logs ignored。 | 提交时只 stage topic docs/source，不 stage raw logs。 |

## Optimization Matrix 更新

| candidate family | row source policy | decision | 理由 |
| --- | --- | --- | --- |
| scalar baseline characterization | all policies | partial | 基础 public semantics 已覆盖；correspondence-pair 和有效 indexed row order 仍 deferred。 |
| fused 2D correlation accumulator | ordered-cloud-pair | attempted / continue | correctness 和 QEMU smoke 通过；board、正式 asm attribution 和 Evidence Doctor 未闭合。 |
| raw sums fused formula | ordered-cloud-pair | rejected for this phase | 近抵消样本暴露 reduction tree 风险；已改为两遍中心化结构。 |
| indexed / correspondence carry-over | non-ordered policies | deferred | 需要 ordered-cloud-pair 证据后另开 row-source phase。 |
| production dispatch | all policies | not_applicable | 本阶段未授权 production 修改。 |
| topic-local doc suite parity | documentation | adopted | 当前 doc suite 已补齐并登记提交边界。 |

## Validation

| 检查 | 命令 | 结果 |
| --- | --- | --- |
| correctness | `make -C test-rvv/registration/transformation_estimation_2D run_test_compare` | pass：Std 10/10，RVV 10/10。 |
| QEMU bench smoke | `make -C test-rvv/registration/transformation_estimation_2D run_bench_ordered_cloud_pair_smoke` | pass：RVV bench 输出 4K / 64K / 256K label 和 checksum；不作为性能结论。 |
| asm dump | `make -C test-rvv/registration/transformation_estimation_2D dump_bench_rvv` | pass：生成本地 ignored asm 文件；正式归属 pending。 |
| registry record | `python3 ../../script/evidence_registry.py record ...` | pass：本地 `log/evidence_registry.json` 已登记 3 个 QEMU log。 |
| registry freshness | `make -C test-rvv/registration/transformation_estimation_2D evidence_status` | pass：topic-local `log/evidence_registry.json` check fresh。 |

## Continue / Stop Decision

`continue_stop_decision`：Phase 010 完成，但 topic 不能写 `ready_for_review`。仍有当前 topic 内的下一阶段动作：正式 asm attribution、board repeated benchmark、Evidence Doctor 和 production integration 判断。

`stop_condition_hit`：本阶段已完成计划闭环；继续到 production 需要后续证据和可能的生产接入授权。板卡可用性尚未在本阶段确认。

`next_phase_default`：

```text
020-board-and-asm-evidence
```

Phase 020 应先创建 plan，再补 asm attribution / manifest / board repeated summary / Evidence Doctor；只有证据支持时才进入 PI1 production integration plan。
