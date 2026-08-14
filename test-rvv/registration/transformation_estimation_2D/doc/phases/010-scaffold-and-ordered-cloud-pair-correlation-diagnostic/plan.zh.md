# Phase 010 Plan: scaffold-and-ordered-cloud-pair-correlation-diagnostic

## 阶段意图和边界

本阶段把 `transformation_estimation_2D` 从只有评估文档推进到可执行的
test-rvv scaffold（测试支撑脚手架）。首个候选只覆盖顺序点云对
（ordered-cloud-pair，source/target 按相同下标一一对应）的
`PointXYZ -> PointXYZ`、`Scalar=float`、dense finite（稠密且有限）输入。

本阶段要证明：

- 当前 production public entry（公开入口）的基础语义可以被测试稳定刻画。
- fused 2D correlation accumulator（融合 2D 相关项累加器）在同构链路
  （same-chain，标量和候选按同一数学边界对拍）下能重建当前 2D transform。
- Makefile、test、bench smoke（小规模可运行性检查）、文档套件和 evidence registry
  入口已经具备后续 Phase 020 生成反汇编和板卡证据的结构。

本阶段不证明：

- 不修改 `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp`
  或其它 production 源码。
- 不批准 production dispatch（生产分流），不创建长期 `doc-rvv` production 主题文档。
- 不把顺序点云对结果外推到 source-indexed-cloud-pair（源索引点云对）、
  dual-indexed-cloud-pair（双索引点云对）或 correspondence-pair（对应关系点对）。
- 不用 QEMU timing（QEMU 计时）做性能结论；QEMU bench 只允许作为非 compare 的
  log-shape smoke（日志形状检查）。

允许路径：

```text
test-rvv/registration/transformation_estimation_2D/**
tmp/rvv-work-logs/registration/transformation_estimation_2D/**
```

## 当前状态清单

| area | 当前状态 | 证据路径 | Phase 010 动作 |
| --- | --- | --- | --- |
| production source | 标量路径保持原状；四个公开入口统一进入 `ConstCloudIterator` helper。 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` | 只读取，不修改。 |
| evaluation | `diagnostic-plan-ready`，已记录 2D centroid / demean / correlation 流程。 | `doc/transformation_estimation_2D-evaluation.zh.md` | 阶段末同步新 scaffold、测试和证据边界。 |
| roadmap | 默认恢复动作仍有旧 `full-cloud` 名称。 | `doc/optimization-roadmap.zh.md` | 改成 `ordered-cloud-pair`，并回填 Phase 010 结果。 |
| optimization matrix | 候选仍是 `planned` / `deferred`。 | `doc/phases/optimization-matrix.zh.md` | 增加 scaffold、correctness、QEMU smoke、registry 状态。 |
| executable targets | 不存在。 | 当前 topic 目录只有文档。 | 新增 `Makefile`、`board.mk`、`src/`、`include/`、`include/impl/`。 |
| evidence registry | not_available。 | 未见 `log/evidence_registry.json`。 | 提供 `evidence_status` / record target；阶段内只在有真实输出时登记。 |
| board evidence | missing。 | 无板卡配置确认。 | 只写板卡 target / 预算；不运行远端命令。 |

## 假设与候选族

| 候选族 | 假设 | 风险 | 本阶段验证 |
| --- | --- | --- | --- |
| scalar baseline characterization | 当前公开入口的 size mismatch、空输入和非有限输入行为可用 gtest 刻画。 | 非有限输入可能产生 NaN 矩阵；非法 index / correspondence 不是安全测试合同。 | public semantics tests。 |
| fused 2D correlation accumulator | 2D angle 只需要 `H00`、`H01`、`H10`、`H11`，可以先求 x/y 质心，再直接累加中心化后的 2x2 correlation，避免动态 demean 矩阵。 | reduction tree 改变数值；centroid finite 语义和 demean 全量写出语义容易被误改。 | scalar same-chain candidate + RVV guarded candidate + near-cancellation 样本。 |
| dense finite gate | 首个 RVV diagnostic 可以限制为 dense finite ordered-cloud-pair。 | dense-only 只能说明候选价值，不能覆盖非有限 public semantics。 | gate / fallback stats 和文档边界。 |
| common RVV reuse audit | common 普通 cloud overload 有 RVV，但 2D helper 使用 iterator overload，可能没有命中。 | 需要反汇编或 vectorization report 才能闭合。 | 本阶段仅记录 planned，Phase 020 处理 asm。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / smoke target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| scalar baseline characterization | all policies | `PointXYZ`, `float`, current public entry | `run_test_public_semantics` | not_applicable | not_applicable | not_applicable | not_required | planned in this phase |
| fused 2D correlation accumulator | ordered-cloud-pair | dense finite `PointXYZ -> PointXYZ`, `float` | `run_test_candidates` and `run_test_compare` | `run_bench_ordered_cloud_pair_smoke` | missing / Phase 020 | `dump_bench_rvv` build target only | manual if smoke generated | planned in this phase |
| fused 2D correlation accumulator | source-indexed-cloud-pair | valid indices, `PointXYZ`, `float` | not_yet_covered | not_yet_covered | missing | not_run | not_run | deferred |
| fused 2D correlation accumulator | dual-indexed-cloud-pair | valid source / target indices, `PointXYZ`, `float` | not_yet_covered | not_yet_covered | missing | not_run | not_run | deferred |
| fused 2D correlation accumulator | correspondence-pair | valid query / match correspondences, `PointXYZ`, `float` | not_yet_covered | not_yet_covered | missing | not_run | not_run | deferred |
| production dispatch | ordered-cloud-pair | production public overload | not_yet_covered | not_yet_covered | missing | not_yet_covered | not_run | not_applicable |

## 实现和测试动作

| id | 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- | --- |
| A1 | 写 topic scaffold。 | `Makefile`、`board.mk`、`include/te2d.h`、`include/impl/*.hpp`、`src/test_te2d.cpp`、`src/bench_te2d.cpp` | `make -n` 能解析 target；源码职责拆分符合 `include/impl` 布局。 |
| A2 | 实现 public semantics tests。 | `run_test_public_semantics` 或 `run_test_compare TEST_ARGS=...` | 覆盖 size mismatch、empty input、non-finite x/y/z、索引入口的安全边界说明。 |
| A3 | 实现 scalar reference 与 fused candidate。 | `include/impl/te2d_candidates.hpp` | dense finite ordered-cloud-pair 下矩阵与当前 public path 在预算内一致。 |
| A4 | 实现 bench smoke。 | `run_bench_ordered_cloud_pair_smoke`、`dump_bench_rvv` | 输出 label、iterations、warmup、checksum、build 标记；不默认运行 QEMU compare。 |
| A5 | 补 topic-local doc suite。 | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` | 文档能定位 target、helper、证据边界和未覆盖范围。 |
| A6 | 更新 phase result、roadmap、matrix、README 和 phase index。 | 当前 phase `result.zh.md` 与现有索引文档 | 所有新增路径、旧术语 rename、继续 / 停止条件可恢复。 |

## Evidence Doctor 和 registry 规则

本阶段若只运行 correctness，不需要 Evidence Doctor（证据体检）。若运行 bench smoke 或
`dump_bench_rvv` 生成可引用输出，则按人工轻量 doctor 记录：

- Errors：checksum mismatch、缺少 std / RVV 构建产物、输出 label 不可解析。
- Warnings：QEMU smoke metadata 不完整、asm 只证明指令存在但不能归属热点。
- Suggestions：Phase 020 生成 JSON manifest、板卡 repeated summary 和正式 doctor。

`log/evidence_registry.json` 只登记被文档引用的 summary / manifest / doctor / correctness log。
raw logs（原始日志）默认不提交。

## 阶段完成条件

- `Makefile` 和源码 scaffold 存在，并且至少能完成 std / RVV correctness 构建或给出工具链阻塞原因。
- 顺序点云对 candidate 有 same-chain correctness 结果；非有限语义只刻画，不被 candidate 扩大。
- bench smoke target 可以构建或清楚记录阻塞；没有板卡结果时不写性能结论。
- doc suite、roadmap、matrix、phase README 和 evaluation 均同步当前状态。
- `git diff --check` 对本 topic 通过；artifact tracking scan 包含 untracked 文件。

## 板卡复跑预算和决策桶

本阶段不要求板卡性能证据。Phase 020 如果板卡可用，默认预算为 5 次 repeated run，
warm-up 5 次、measurement 20 次；允许一次同边界确认复跑。决策桶：

- `positive`：所有主要规模 B/A 明显大于 1，且长尾不改变方向。
- `weak_positive`：多数规模正向但收益较小或有少量异常。
- `neutral`：接近 1 或不同规模方向不一致。
- `negative`：多数规模退化。
- `unstable`：预算耗尽后仍跨桶摇摆。

## 继续 / 停止条件

如果本阶段 correctness 和 scaffold 完成但缺少 asm / board evidence，默认下一阶段为：

```text
020-board-and-asm-evidence
```

只有继续会触碰 production 源码、其它 topic、需要不可用板卡 / 工具，或发现证据矛盾时，本轮才允许停止并写成 `turn_stop_deferred with stop_condition_hit`。

## 文档更新清单

- `README.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- `doc/transformation_estimation_2D-evaluation.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/phases/010-scaffold-and-ordered-cloud-pair-correlation-diagnostic/result.zh.md`

## Roadmap 同步动作

阶段结束后把 `full-cloud` 旧名统一为 `ordered-cloud-pair`；新增或更新候选：

- `ordered-cloud-pair fused 2D correlation accumulator`：本阶段尝试。
- `non-finite semantics gate`：根据 public semantics 结果选择 dense-only gate 或完整语义复刻。
- `asm / board evidence`：若 correctness 通过，作为 Phase 020 默认入口。
- `row-source family carry-over`：仍 deferred，等待 ordered-cloud-pair 证据。
