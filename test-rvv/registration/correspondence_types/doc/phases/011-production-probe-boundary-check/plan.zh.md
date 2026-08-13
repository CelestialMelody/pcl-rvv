# Phase 011 计划：production-probe-boundary-check

## 阶段意图和边界

本阶段新增一个有边界的 production probe（生产路径探针），回答 Phase 010 不能回答的问题：`test-rvv`
diagnostic（诊断）里的 `strided-index-extract` 负向，是否也会出现在真实 production helper（生产源码
helper）边界下。

本阶段不是把 Phase 010 的 `rollback/no-production` 直接改成 production-ready。当前默认结论仍保持
no-production，直到真实 `getQueryIndices` / `getMatchIndices` 在 production direct（真实生产路径证据）
中同时闭合 correctness（正确性）、fallback（回退路径）、asm attribution（反汇编归属）和 board repeated
benchmark（重复板卡性能测试）。

范围只包含：

- `registration/include/pcl/registration/impl/correspondence_types.hpp` 中 `getQueryIndices`。
- 同文件中的 `getMatchIndices`。
- `pcl::Correspondence` 12-byte AoS（结构数组）布局，`pcl::index_t` 为 32-bit。
- `Scalar` 不适用；本 helper 只处理 index 字段。

明确不包含：

- `getCorDistMeanStd`。它涉及 float-square same-chain（同构链路）和 double accumulation（双精度累加）边界，仍保持标量。
- 改 public API（公开接口）。
- 扩大到其它 correspondence estimation / rejection topic。
- 发布 `doc-rvv` 长期主题文档。只有 production probe 通过 PI5 并最终保留生产行为后才适用。

## 为什么现在新增本阶段

历史记录 `tmp/test-update/8.production_source_indices_block_fused_abcd_ilp.md` 说明过：diagnostic negative
（诊断负向）不能自动禁止 production probe。weighted topic 中 Phase 030 diagnostic helper 负向，
但 Phase 031 public Std/RVV（公开入口标量 / RVV）production direct 正向；后续 Phase 032 又证明二者
核心差异是 comparison boundary / baseline mismatch（比较边界 / 基线不一致）。

当前 topic 与该历史经验的相同点：

- Phase 010 的负向来自 test-only candidate wrapper（仅测试使用候选包装）。
- 真实 `getQueryIndices` / `getMatchIndices` production helper 尚未接 RVV，缺少 production direct 证据。
- 当前 bench 的 Std 侧不是原 production helper，而是 test support 中的 fallback candidate，因此仍可能存在边界差异。

当前 topic 与该历史经验的不同点：

- 本 topic 的 helper 极小，主要是内存搬运；production direct 也可能仍然负向。
- 没有已有 adopted RVV family（已采纳 RVV 实现族）可做 RVV-vs-RVV detail A/B；本阶段只做 public Std/RVV probe。
- 若 production direct 仍负向，应回退 production patch，结论保留在 topic-local phase result。

## 当前状态清单

| 项目 | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| production 源码 | 标量，无 RVV 分支 | `registration/include/pcl/registration/impl/correspondence_types.hpp` |
| diagnostic candidate | `query_indices_candidate` / `match_indices_candidate` 使用 `vlse32` + `vse32` | `test-rvv/registration/correspondence_types/include/impl/correspondence_types_candidates.hpp` |
| correctness | QEMU pass；board 6/6 pass | `doc/phases/010-board-diagnostic-and-production-decision/result.zh.md` |
| diagnostic board | 5-run 全部 negative bucket | `log/board/010-board-diagnostic/index-extract/summary.md` |
| Evidence Doctor | board Errors=3 / Warnings=1 / Suggestions=0 | `log/board/010-board-diagnostic/index-extract/evidence_doctor.md` |
| production direct | not_started | 本阶段新增 |
| evidence registry | not_available | 仍做人工 freshness 检查 |

## 候选和优化矩阵

| candidate family | row source policy | point type / layout | scope and entry | correctness / fallback target | bench target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production-strided-index-extract-probe | correspondences 顺序扫描 | `pcl::Correspondence` 12-byte AoS，`index_t` 32-bit | 真实 `getQueryIndices` / `getMatchIndices` | 新增 production direct gtest；非 RVV 构建自然走标量；布局 gate 不满足时 fallback | 新增 `production-index-extract` case-filter | 5-run，20 iterations，5 warm-up | RVV bench 二进制中归属到 production helper 或内联调用边界 | 新增 production direct manifest / doctor | planned |
| diagnostic-strided-index-extract | correspondences 顺序扫描 | 同上 | test support candidate wrapper | Phase 010 已通过 | 既有 `index-extract` | Phase 010 negative | bench/candidate asm smoke | Phase 010 doctor negative | historical baseline / risk signal |
| distance-stats-reduction | correspondences 顺序扫描 | `distance` float 字段 | `getCorDistMeanStd` | 不变 | 不运行 | 不运行 | 不运行 | 不运行 | out_of_scope |

## 实现和测试动作

| action | 产物 | 命令 / 验收 | 完成判据 |
| --- | --- | --- | --- |
| P1 写阶段计划 | 本文件 | 文件存在且边界清楚 | done before production edits |
| P2 production probe patch | `registration/include/pcl/registration/impl/correspondence_types.hpp` | `__RVV10__` 下新增小型 RVV helper；非 RVV 构建保持标量 | public API 不变；`getCorDistMeanStd` 不变 |
| P3 production direct correctness | `src/test_correspondence_types.cpp` | `make run_test_compare`；board `make run_board_test fetch_board_logs` | Std/RVV 均 pass；生产 helper 输出与参考一致 |
| P4 production direct bench case | `src/bench_correspondence_types.cpp`、Makefile / script | case-filter `production-index-extract` 可运行；checksum 一致 | bench label 区分 production direct 和 diagnostic |
| P5 asm attribution | `make dump_bench_rvv` | 检查 `vlse32.v` / `vse32.v`，记录内联或符号归属 | 不能只写“二进制有 RVV 指令” |
| P6 board repeated production bench | `log/board/011-production-probe-boundary-check/production-index-extract/**` | 5 runs，`--case-filter production-index-extract --iterations 20 --warmup-iterations 5` | 形成 summary、manifest、doctor |
| P7 决策和文档回填 | phase result、roadmap、matrix、evaluation | 按 Evidence Doctor 更新 | positive 才考虑保留 production；negative/neutral 则回退 production patch |

## Evidence Doctor 和 registry 规则

- 新增 production direct summary / manifest，`evidence_role=production_direct_probe`。
- 若 checksum mismatch（校验和不一致）、strict A/B 元数据缺失或 production boundary 缺失，Evidence Doctor Error 必须先修正或降级。
- 若 asm attribution 不闭合，只能把性能证据降级为 `production_probe_boundary_incomplete`。
- `log/evidence_registry.json` 仍未接入；本阶段 result 必须列出人工 freshness 检查路径。

## 板卡复跑预算和决策桶

- 预算：默认 5 个 run-labelled 目录；若 Evidence Doctor 暴露边界错误，先修正后重跑当前 5-run。
- 每个 run：20 iterations，5 warm-up。
- `B/A = Std production helper ms / RVV production helper ms`，大于 1 表示 RVV 更快。
- `positive`：所有主 case > 1.15。
- `weak_positive`：median >= 1.03 且 min >= 0.97。
- `neutral`：所有值在 0.97 到 1.03。
- `negative`：median < 0.97 或 min < 0.97。
- 若不同 case 跨桶摇摆，写 `unstable`，不自动扩大复跑预算。

## 继续 / 停止条件

继续到 production 保留的条件：

- production direct correctness pass。
- 非 RVV build 自然走标量，RVV build 命中 RVV 指令。
- board repeated 至少达到 `weak_positive`，且 Evidence Doctor 没有未处理 Error。
- production helper 维护成本仍低，不引入 public API 或复杂 traits gate。

回退 / 停止条件：

- correctness fail。
- asm attribution 不能证明 production helper 或内联调用边界命中目标 RVV 指令。
- board repeated 为 `negative` / `neutral` / `unstable`。
- Evidence Doctor Error 不能修正。

`next_phase_default`：执行本 production probe 并在 result 中二次决策。若 production direct 负向或中性，
默认回退本阶段 production patch，topic 回到 `ready_for_review_no_production`，不发布 `doc-rvv`。

## 文档更新清单

- 本阶段完成后新增 `result.zh.md`。
- 更新 `doc/phases/README.zh.md` 的当前阶段和默认恢复动作。
- 更新 `doc/phases/optimization-matrix.zh.md`。
- 更新 `doc/optimization-roadmap.zh.md`。
- 更新 `doc/correspondence_types-evaluation.zh.md` 的诊断证据链 / production probe 结论。
- 只有 production probe 被最终采用后才创建 `doc-rvv/registration/correspondence_types-RVV.zh.md`。
