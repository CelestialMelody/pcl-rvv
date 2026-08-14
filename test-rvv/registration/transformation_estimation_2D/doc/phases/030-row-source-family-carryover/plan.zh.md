# Phase 030 Plan: row-source family carry-over

## 阶段意图和边界

本阶段只在 `test-rvv/registration/transformation_estimation_2D` 范围内审计 row source policy（行来源策略）是否能复用 Phase 020 的 two-pass centered fused 2D correlation accumulator（两遍中心化融合 2D 相关项累加器）。本阶段不修改 production（生产源码），也不扩大 Phase 050 已冻结的 ordered-cloud-pair production gate。

要覆盖的 row source：

- source-indexed-cloud-pair（源索引点云对）：`source[indices_src[i]]` 与 `target[i]` 配对。
- dual-indexed-cloud-pair（双索引点云对）：`source[indices_src[i]]` 与 `target[indices_tgt[i]]` 配对。
- correspondence-pair（对应关系点对）：`source[index_query]` 与 `target[index_match]` 配对。

本阶段只做 `PointXYZ -> PointXYZ`、`Scalar=float`、valid-index-only（只包含有效索引）和 dense finite（点云标记稠密且坐标有限）输入。非法 index / correspondence 的 public API（公开接口）语义不在本阶段扩展；production 路径保持标量。

## 当前状态清单

| area | 当前状态 | 路径 |
| --- | --- | --- |
| production source | Phase 050 narrow patch 保留；本阶段不修改。 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` |
| ordered-cloud-pair diagnostic | correctness、QEMU smoke、asm 和 board diagnostic 已有证据；最新 production-public board 为 `positive`。 | `doc/phases/050-pi2-production-patch-and-direct-evidence/result.zh.md` |
| source-indexed public / candidate | public scalar boundary 和 materialize-to-ordered candidate correctness 已通过。 | `src/test_te2d.cpp`、`include/impl/te2d_candidates.hpp` |
| dual-indexed public / candidate | public scalar boundary 和 materialize-to-ordered candidate correctness 已通过。 | `src/test_te2d.cpp`、`include/impl/te2d_candidates.hpp` |
| correspondence public / candidate | public scalar boundary 和 materialize-to-ordered candidate correctness 已通过。 | `src/test_te2d.cpp`、`include/impl/te2d_candidates.hpp` |
| bench | 已增加 `row-source-fused`，覆盖三种 policy × 4K/64K/256K。 | `src/bench_te2d.cpp`、`Makefile` |
| QEMU evidence | row-source manifest / ASM / Doctor 已生成并登记，Doctor 为 0/0/0。 | `log/qemu/row_source/`、`log/evidence_registry.json` |
| board evidence | done；使用 `test-rvv/config.mk`、`REMOTE_USER`、`REMOTE_IP`、`BOARD_LABEL=Milkv-Jupiter` 完成 5-run repeated。 | `log/board/row_source_fused_repeated/` |

## 假设与候选族

| candidate family | row source | 假设 | 风险 |
| --- | --- | --- | --- |
| materialize-to-ordered fused candidate | source-indexed | 把 indexed row materialize 成顺序点云对后复用 fused accumulator，可以得到正确性证据和真实展开成本。 | materialize 成本可能吞掉 ordered-cloud-pair 的计算收益。 |
| materialize-to-ordered fused candidate | dual-indexed | 双侧 materialize 后数学链路仍与 public scalar overload 对齐。 | 双 gather 和两份临时点云增加内存流量。 |
| materialize-to-ordered fused candidate | correspondence-pair | correspondence 的 query / match 可以转换成 paired ordered clouds，再复用 fused accumulator。 | correspondence 访问和临时点云成本可能让 production 方向没有价值。 |

本阶段不尝试手写 RVV gather kernel。materialize-to-ordered 是 bounded diagnostic（有边界诊断）：它把 row-source 展开成本放进 bench 计时边界，再判断是否值得设计更复杂的 gather / staging（离散加载 / 暂存）候选。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness target | bench target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| materialize-to-ordered fused candidate | source-indexed-cloud-pair | valid source indices, `PointXYZ`, `float`, dense finite | pass: Std/RVV 16/16；candidate 与 public scalar overload 对拍通过 | QEMU 9-case smoke；board median 1.073x / 1.023x / 1.038x | `Milkv-Jupiter` repeated；64K 接近阈值 | row-source wrapper lambda 已归属；共享数学 RVV 在 test-support fixture | QEMU 0/0/0；board 1/2/6 | attempted；diagnostic-only |
| materialize-to-ordered fused candidate | dual-indexed-cloud-pair | valid source / target indices, `PointXYZ`, `float`, dense finite | pass: Std/RVV 16/16；candidate 与 public scalar overload 对拍通过 | QEMU 9-case smoke；board median 1.081x / 1.014x / 1.038x | `Milkv-Jupiter` repeated；64K 有 0.956x 长尾 | row-source wrapper lambda 已归属；共享数学 RVV 在 test-support fixture | QEMU 0/0/0；board 1/2/6 | attempted；diagnostic-only |
| materialize-to-ordered fused candidate | correspondence-pair | valid query / match correspondences, `PointXYZ`, `float`, dense finite | pass: Std/RVV 16/16；candidate 与 public scalar overload 对拍通过 | QEMU 9-case smoke；board median 1.067x / 1.013x / 1.035x | `Milkv-Jupiter` repeated；64K 2/5 退化 | row-source wrapper lambda 已归属；共享数学 RVV 在 test-support fixture | QEMU 0/0/0；board 1/2/6 | attempted；diagnostic-only |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 row-source materialization helpers | `include/impl/te2d_candidates.hpp` | done：materialize 成本明确纳入 candidate bench 计时；不修改 production。 |
| A2 correctness tests | `src/test_te2d.cpp` | done：Std/RVV 各 16/16，通过三类 candidate 对拍。 |
| A3 bench case-filter | `src/bench_te2d.cpp`、`Makefile` | done：`row-source-fused` 覆盖 9 个 label，banner 和 ASM 边界可区分。 |
| A4 QEMU correctness / smoke | `log/qemu/*` | done：correctness 16/16；row-source smoke 生成 9-case 日志。 |
| A5 Evidence Doctor / registry | `log/evidence_registry.json` 和 summary 指针 | done：QEMU row-source manifest 登记为 `qemu_smoke_only`，Doctor 0/0/0。 |
| A6 result / matrix / roadmap / Handoff | phase result、matrix、roadmap、handoff | done：已回填真实 board summary、manifest、Doctor 和当前决策。 |

## Evidence Doctor 和 registry 规则

本阶段如果只跑 QEMU smoke，Evidence Doctor（证据体检）只能支撑 log-shape（日志形状）和路径边界，不能支撑性能结论。若新增 board repeated summary，必须使用 5-run budget、decision bucket 和 `record_*_state` 登记。所有新增 evidence doc-ref 必须指向 topic-local docs 和本 phase result，不指向 `doc-rvv`。

## 板卡复跑预算和决策桶

默认 board budget 为 5-run、20 iterations、5 warmup iterations，沿用当前 `TE2D_BOARD_REPEATED_RUNS` / `TE2D_BOARD_BENCH_ITERATIONS` / `TE2D_BOARD_BENCH_WARMUP_ITERATIONS`。若 QEMU correctness 或 smoke 未通过，不进入 board。若 board 可用且 row-source smoke 输出稳定，本阶段可继续跑 board repeated；若工具或远端不可用，result 必须列出解除阻塞命令。

决策桶：

- `positive`：median 明显大于 1，且 `B/A < 1` 频率低。
- `weak_positive`：median 大于 1 但接近阈值或规模间差异小。
- `neutral`：median 接近 1 或不同规模方向不一致。
- `negative`：median 小于 1 或退化频率高。
- `unstable`：预算内方向摇摆，不能形成稳定桶。

## 继续 / 停止条件

本阶段先闭合 test-rvv correctness 和 QEMU smoke；板卡配置存在且可达时继续跑 row-source board repeated 和 Evidence Doctor。当前 board 已完成，但 64K 的退化 / 长尾让三类 row-source 继续保持 diagnostic-only。即使某个 case 的 median 正向，也只能进入新的 PI1 计划，不能直接扩大当前 production patch。

`next_phase_default`：`060-production-candidate-review-and-row-source-boundaries`。先审阅当前 ordered-cloud-pair production patch，再决定是否为 indexed / correspondence 设计新的 gather/staging candidate。

## 文档更新清单

- `doc/phases/030-row-source-family-carryover/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- `doc/transformation_estimation_2D-evaluation.zh.md`
- `tmp/rvv-work-logs/registration/transformation_estimation_2D/current-handoff/`

`doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` 仍为 not_applicable。
