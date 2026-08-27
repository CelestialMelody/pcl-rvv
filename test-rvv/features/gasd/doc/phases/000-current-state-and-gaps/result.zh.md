# Phase 000 result: current state and gaps

## 执行范围

本阶段完成 GASD topic 的骨架和 first diagnostic candidate（首个诊断候选）闭环：README、evaluation、roadmap、phase index、phase plan、phase result、Makefile、board.mk、test support aggregator、fixed-grid histogram copy（固定网格直拷贝）test-only helper、correctness（正确性）对拍、QEMU（仿真器）日志形状、反汇编和板卡 repeated benchmark（重复性能测试）。当前生产源码未修改。

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| A1 test-first 红灯 | done | `src/test_gasd.cpp` 的 `GASDHistogramCopy.*` 用例先固定期望 API，再实现 helper | 测试能覆盖 shape / color copy 语义 |
| A2 Std/RVV diagnostic helper | done | `include/impl/gasd_reference.hpp`、`include/impl/gasd_copy_candidate.hpp` | RVV 构建走 `vle32.v` / `vse32.v`，非 RVV 构建回到 scalar reference |
| A3 bench diagnostic | done | `src/bench_gasd.cpp`，case `candidate_shape_copy_rvv` / `candidate_color_copy_rvv` | 输出 checksum 和 ms/iter；QEMU 只作日志形状 smoke |
| A4 反汇编归属 | done | `build/asm/riscv/bench_gasd_rvv.asm` | filtered asm 命中 `vle32.v`、`vse32.v`、`vsetvli`；manifest 统计 RVV 相关行数为 1399 |
| A5 板卡 repeated bench | done | `log/board/repeated_phase000_shape_copy/summary.md` | 5-run 全部同向，median 1.050x，range 1.040x-1.080x，decision bucket 为 `weak_positive` |
| A6 Evidence Doctor 和文档回填 | done | `log/board/repeated_phase000_shape_copy/evidence_doctor.md`、本文件、roadmap、matrix | Errors=0，Warnings=0，Suggestions=2；建议项不阻塞当前 diagnostic 结论 |

## EvidenceDecision

`current_decision`: `diagnostic-weak-positive / continue-phase-loop`。fixed-grid copy 在 test helper boundary（测试 helper 边界）上有稳定弱正向板卡证据，足以说明“连续写回尾段值得保留为候选”，但不能作为 production（生产源码）接入结论。phase loop（阶段循环）继续进入 `010-shape-sample-projection-diagnostic`，因为 shape sample projection（形状样本投影）仍是逐样本热点，且仍在当前 topic 的 test-only diagnostic 授权范围内。

## 证据边界

| evidence | result | proves | does not prove |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/features/gasd run_test_compare`，Std/RVV 均 4/4 通过 | 公开入口合成输入可运行；shape / color copy helper 与标量参考同构 | 不证明完整 production dispatch 命中 RVV |
| QEMU bench smoke | `make -C test-rvv/features/gasd run_bench_rvv BENCH_ARGS='--case-filter candidate_shape_copy_rvv --shape-half-grid 6 --hists-size 16 --repeat 256 --iterations 3 --warmup 1'` | RVV bench 二进制可运行，输出 checksum 和日志形状 | QEMU timing 不用于性能结论 |
| asm | `make -C test-rvv/features/gasd dump_bench_rvv`，`build/asm/riscv/bench_gasd_rvv.asm` | RVV build 中存在 copy helper 需要的 `vle32.v` / `vse32.v` / `vsetvli` | filtered asm 未单独证明完整 public compute 热点占比 |
| board repeated | `make -C test-rvv/features/gasd board_repeated`，`log/board/repeated_phase000_shape_copy/summary.md` | 目标硬件上 copy-only diagnostic 的稳定弱正向收益 | 不包含 alignment、sample projection、interpolation、color hue 或完整 public compute |
| Evidence Doctor | `make -C test-rvv/features/gasd evidence_doctor_repeated`，`log/board/repeated_phase000_shape_copy/evidence_doctor.md` | 当前 manifest 可解释，Errors=0，Warnings=0 | environment metadata 和 binary identity 仍是建议项 |

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：Std copy vs RVV copy |
| 当前决策问题 | RVV-vs-scalar diagnostic；是否值得继续到 projection / interpolation |
| diagnostic 是否可外推到 production | no；当前只是固定网格 copy 的局部边界，且没有接入真实 `GASDEstimation::computeFeature` |
| comparison-boundary / baseline mismatch 风险 | yes；生产链路仍有投影、插值、颜色 hue 计算和输出 descriptor 清零 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes；weak-positive 只支持继续做更接近热点的 bounded diagnostic，不支持直接改 production |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | yes |

## Optimization matrix 更新

`fixed-grid histogram copy` 更新为 `attempted / diagnostic-weak-positive`。`shape sample projection` 被提升为当前默认下一 phase，因为它位于逐样本循环内，能更好回答 GASD 主成本是否值得进入 production-shaped probe（生产形态探针）。

## Evidence Doctor 结果

| severity | count | 处理 |
| --- | ---: | --- |
| Errors | 0 | 无需修复。 |
| Warnings | 0 | 原先 `warmup_iterations` 解析缺失由 `script/generate_gasd_evidence_manifest.py` 的正则修复并重跑。 |
| Suggestions | 2 | `environment_metadata_missing` 和 `binary_identity_missing` 已记录为下一批 board evidence 的增强项；不阻塞当前 diagnostic weak-positive。 |

## 文档归属和 Traceability Map

| 事实 | 主归属 | 说明 |
| --- | --- | --- |
| 阶段计划 / 结果 | `doc/phases/000-current-state-and-gaps/` | 保存本阶段执行事实和 EvidenceDecision。 |
| 候选搜索空间 | `doc/optimization-roadmap.zh.md` | 记录下一 phase 和仍未闭合候选。 |
| 跨 phase 状态 | `doc/phases/optimization-matrix.zh.md` | 保存 candidate family 的证据状态。 |
| 函数级评估与 Traceability Map | `doc/gasd-evaluation.zh.md` | 说明 production 入口、diagnostic helper、bench wrapper 和证据路径如何互相定位。 |
| board / Doctor summary | `log/board/repeated_phase000_shape_copy/summary.md`、`evidence_doctor.md` | 默认 local-only，作为当前阶段引用证据。 |

## doc_suite_role_inventory

| role | status | evidence | next action |
| --- | --- | --- | --- |
| topic_navigation | standalone:`README.zh.md` | 已有阅读路径、命令和证据提交边界 | Phase 010 后继续刷新当前状态 |
| testing_overview | merged:`README.zh.md#常用命令` + `doc/gasd-evaluation.zh.md#Traceability Map` | 当前 target 数量有限，已能定位 correctness / bench / board / doctor | 若 Phase 010 增加 case，拆出独立 testing overview |
| correctness_tests | merged:`src/test_gasd.cpp` 文件头 + `doc/gasd-evaluation.zh.md#Traceability Map` | 4 个 gtest 覆盖公开入口 smoke 和 copy helper 对拍 | Phase 010 增加 projection 用例后刷新 |
| benchmark_and_evidence | merged:`README.zh.md#常用命令` + board summary | 已说明 QEMU 不支撑性能，board repeated 为性能证据 | Phase 010 若新增 repeated target，再补 case 字典 |
| optimization_evidence | standalone:`doc/phases/optimization-matrix.zh.md` | matrix 已记录 copy / projection / hue / interpolation | Phase 010 更新 projection 行 |
| optimization_roadmap | standalone:`doc/optimization-roadmap.zh.md` | roadmap 已维护搜索空间 | Phase 010 后回填新增候选或拒绝理由 |
| test_support_code_map | merged:`doc/gasd-evaluation.zh.md#Traceability Map` | 当前 include / include/impl / src / script 已可定位 | 后续 helper 超过职责阈值时拆独立文档 |
| phase_index | standalone:`doc/phases/README.zh.md` | 阶段索引已存在 | 加入 Phase 010 |
| evaluation_diagnostic | standalone:`doc/gasd-evaluation.zh.md` | 承担 no-production diagnostic 评估 | Phase 010 后刷新证据 |
| production_topic_doc | not_applicable with evidence | 未修改 production，未通过 PI5 | 不创建 `doc-rvv/features/gasd-RVV.zh.md` |

## continue / stop decision

`stop_condition_hit`: none。`next_phase_default`: `010-shape-sample-projection-diagnostic`。继续条件成立：下一候选仍在当前 topic 的 test-only assets 内，不需要 production 授权，板卡可用，当前 Evidence Doctor 无 Error。
