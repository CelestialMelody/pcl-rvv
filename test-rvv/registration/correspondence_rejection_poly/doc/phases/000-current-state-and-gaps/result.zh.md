# Phase 000 结果：当前状态和诊断 scaffold

## 实际执行范围

本阶段建立了 topic-local test / bench / diagnostic / document scaffold，未修改 production 源码。实际触碰范围：

- `test-rvv/registration/correspondence_rejection_poly/**`
- `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md`
- `tmp/rvv-work-logs/registration/correspondence_rejection_poly/**`

`registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp` 和 `registration/include/pcl/registration/correspondence_rejection_poly.h` 只读复核。

## 计划动作回填

| action | 状态 | 证据路径 | 结论 |
| --- | --- | --- | --- |
| A1 test support 聚合入口和 candidate helper | done | `include/correspondence_rejection_poly.h`、`include/impl/correspondence_rejection_poly_candidates.hpp` | 已建立 reference 和 test-only RVV candidate。 |
| A2 correctness tests | done | `src/test_correspondence_rejection_poly.cpp`、`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | Std / RVV 各 6 tests passed。 |
| A3 bench wrapper | done | `src/bench_correspondence_rejection_poly.cpp` | edge / acceptance / full-entry case-filter 已建立。 |
| A4 Makefile / board.mk | done | `Makefile`、`board.mk` | QEMU smoke、doctor target、board runner 入口已建立；修复 doctor aggregate 和延迟展开路径。 |
| A5 Evidence Doctor manifest wrapper | done | `script/generate_crpoly_evidence_manifest.py` | edge / acceptance manifest 可生成。 |
| A6 evaluation / topic doc / roadmap / matrix / Handoff | done | `doc/`、`doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md`、`tmp/rvv-work-logs/.../current-handoff.zh.md` | 当前结论写入 diagnostic/no-production 边界。 |

## Optimization matrix 更新

| candidate family | 状态 | 证据 | 决策 |
| --- | --- | --- | --- |
| `edge_length_batch` | attempted | correctness pass；QEMU smoke manifest / doctor pass；asm 指令存在 | 保留诊断，不接 production。 |
| `accept_rate_filter` | attempted | correctness pass；QEMU smoke manifest / doctor pass；asm 指令存在 | 保留诊断，不接 production。 |
| `full_entry_diagnostic` | attempted | 固定 seed public-entry-shaped gtest pass | 证明诊断输入对齐当前源码，不证明 RVV dispatch。 |
| `histogram_otsu_scalar` | adopted scalar | histogram / Otsu gtest pass | 继续标量。 |

## Evidence Doctor 结果

| report | Errors | Warnings | Suggestions | 处理 |
| --- | --- | --- | --- | --- |
| `log/qemu/edge_batch/evidence_doctor.md` | 0 | 0 | 0 | 作为 QEMU smoke doctor pass。 |
| `log/qemu/acceptance_filter/evidence_doctor.md` | 0 | 0 | 0 | 作为 QEMU smoke doctor pass。 |

doctor 输入的 evidence role 是 diagnostic。QEMU timing 不进入性能结论。

## Evidence registry 状态

`log/evidence_registry.json` 已登记 9 个当前证据文件：correctness logs、edge / acceptance summary、manifest、doctor report 和 asm summary。后续若覆盖这些文件，应先运行 registry check 并刷新文档引用。

## 验证摘要

| 验证 | 状态 | 命令 |
| --- | --- | --- |
| production diff check | pass | `git diff -- registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp` 无输出 |
| QEMU correctness | pass | `make -C test-rvv/registration/correspondence_rejection_poly run_test_compare` |
| asm summary | partial | `make -C test-rvv/registration/correspondence_rejection_poly dump_bench_rvv` |
| Evidence Doctor | pass | `make -C test-rvv/registration/correspondence_rejection_poly run_evidence_doctor_qemu` |
| board evidence | not_run | 本阶段只建立 board plan，未运行目标硬件。 |

## 继续 / 停止决策

当前 phase 完成。`edge_length_batch` 与 `accept_rate_filter` 的下一步需要目标硬件性能或进入 production integration loop，已经超出本阶段默认诊断闭环。默认下一阶段是 `010-board-and-production-boundary`：补 board smoke / repeated board 和更窄 asm attribution，然后再决定是否写 PI1 生产接入计划。

当前停止条件命中：production 接入需要用户确认和目标硬件证据；本轮用户明确要求 S10 EvidenceDecision 前不修改 production。
