# Phase 000 Plan: current-state-and-label-equivalence

## 阶段意图和边界

本阶段为 `features/include/pcl/features/impl/organized_edge_detection.hpp` 建立首个 RVV topic（主题）证据面。目标是用测试专用 candidate（候选实现）验证 `OrganizedEdgeBase<PointXYZ, Label>::extractEdges()` 中 depth discontinuity（深度突变）主循环能否在有组织点云的内部像素上批量计算标签，并保持 `compute()` 输出的 label bits（标签位）和 `assignLabelIndices()` 顺序等价。

本阶段不修改 production（生产源码），不声明 production direct（真实生产路径证据），不覆盖 `OrganizedEdgeFromRGB` 的 Canny 灰度链路、`OrganizedEdgeFromNormals` 的 normal Canny 链路、`OrganizedEdgeFromRGBNormals` 的组合入口，也不把 `PointXYZ` 结果外推到泛型点类型或 `Scalar=double`。如果本阶段 evidence（证据）正向，只能支持 bounded production probe（有界生产探针）候选，进入 production integration loop（生产接入闭环）前仍需用户确认。

## 当前状态清单

| 项目 | 当前状态 |
| --- | --- |
| 队列来源 | `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md` 已列出 `organized_edge_detection.hpp`，状态为未启动，建议先做 label-equivalence diagnostic（标签等价诊断）。 |
| production 源码 | `OrganizedEdgeBase::extractEdges()` 逐内部像素检查 8 邻域 depth、NaN boundary（非数边界）、occluding / occluded（遮挡前景 / 被遮挡背景）标签；`assignLabelIndices()` 按线性索引顺序收集每类标签。 |
| 上游测试 | `test/features/test_organized_edge_detection.cpp` 已覆盖 occluding / occluded 回归，但只检查一个合成方块场景。 |
| test-rvv topic 资产 | 本阶段开始前不存在 `test-rvv/features/organized_edge_detection/`。 |
| 板卡状态 | 当前会话说明板卡可用；本阶段需要板卡性能证据时应执行有界复跑，不把“需要板卡”写成停止理由。 |
| dirty isolation（脏工作区隔离） | 工作区已有其它 topic 和 `.agents` 未提交改动；本阶段只触碰 `test-rvv/features/organized_edge_detection/**`，必要时更新队列表中的本 topic 状态。 |

## 假设与候选族

| candidate family | idea source | applies to | expected benefit | risk / unknown |
| --- | --- | --- | --- | --- |
| `depth_labels_rvv_same_chain` | 当前源码中内部像素 8 邻域 depth 比较和 label write 是主循环；同模块 `integral_image_normal` 的 organized image 经验只作为结构校准。 | `PointXYZ`、organized dense cloud、depth-only label path、无 invalid neighbor search 或少量 invalid search fallback。 | 批量加载当前像素和 8 邻域 z，减少 per-pixel `std::vector` 分配、`minmax_element` 和分支开销。 | invalid neighbor search（跨 NaN 搜索）有可变步长和控制流，首阶段可能保留标量 fallback；RVV candidate 与 production baseline 的边界必须写清。 |
| `label_indices_scalar_preserve_order` | `assignLabelIndices()` 的 push_back 顺序是公开输出的一部分。 | 所有 labels 输出。 | 暂不优化，保留标量收集以避免改变顺序。 | 如果 labels 生成加速但 index 收集成为主成本，需要后续 phase 单独消融。 |

## 阶段优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `depth_labels_rvv_same_chain` | organized-grid internal pixels（有组织网格内部像素） | `PointXYZ`, `Label`, float z, AoS（结构数组） | test-only candidate 与标量 same-chain 对拍；production 未修改 | `run_test_compare` 覆盖 flat plane、depth step、NaN boundary、mixed invalid neighbor、label_indices 顺序 | `run_bench_compare` 仅板卡；QEMU 禁止作为性能结论 | `board_repeated`，默认 5 run，decision bucket 见下节 | `dump_bench_rvv` 归属到 test-only candidate 符号或内联范围 | board summary 后运行 topic-local manifest + Evidence Doctor | `planned` |
| `label_indices_scalar_preserve_order` | label output scan | `Label` bit mask | 保持标量收集 | correctness 与 `depth_labels_rvv_same_chain` 同 target | 若 labels RVV 正向但总耗时弱，后续 phase 做收集消融 | deferred | not_applicable for Phase 000 | not_applicable | `deferred` |

## 实现和测试动作

| action | 产物 | 命令 / 证据 | 完成判据 |
| --- | --- | --- | --- |
| A1 scaffold | `Makefile`、`board.mk`、`src/test_organized_edge_detection.cpp`、`src/bench_organized_edge_detection.cpp`、`include/organized_edge_detection.h`、`include/impl/*` | `make -C test-rvv/features/organized_edge_detection run_test_compare` | Std / RVV 构建均通过，QEMU correctness（QEMU 正确性）日志可读。 |
| A2 same-chain correctness | 标量 reference 与 RVV candidate label / label_indices 对拍 | QEMU `run_test_compare`、板卡 `run_board_test` | 所有测试 case 的 labels 和 label_indices 完全一致。 |
| A3 bench diagnostic | 同一 harness 下测 scalar reference 与 RVV candidate | 板卡 `board_repeated` + summary | checksum 一致，输出包含 dataset、iterations、case avg 和 total time。 |
| A4 asm attribution | RVV bench 二进制反汇编 | `make ... dump_bench_rvv` | 关键 RVV load / compare / merge / store 指令可归属到 candidate。 |
| A5 Evidence Doctor / registry | `log/board/evidence_manifest.json`、`log/board/evidence_doctor.md`、`log/evidence_registry.json` | `evidence_doctor_repeated` / `record_board_evidence_state` | Errors / Warnings / Suggestions 已解释；registry fresh 或人工列出缺口。 |
| A6 docs | evaluation、README、roadmap、optimization matrix、phase result 和 Handoff | topic-local docs | 诊断证据链、Traceability Map（可追踪性地图）、doc suite inventory 和 continue / stop decision 可恢复。 |

## Evidence Doctor 和 registry 规则

本阶段的 performance（性能）结论只来自 board / target hardware（板卡 / 目标硬件）。QEMU target 只用于 correctness、build 和日志形状。board repeated 计划使用 5 run 预算：

- `positive`：所有主 case checksum 一致，median 和 mean speedup 均 >= 1.20x，且退化 run 频率不高于 1/5。
- `weak-positive`：主 case median 和 mean speedup 均 > 1.05x，但低于 1.20x，或存在少量退化 run。
- `neutral`：speedup 在 0.95x-1.05x 区间。
- `negative`：主 case median 或 mean < 0.95x。
- `unstable`：同一 case 方向在预算内摇摆，无法稳定归桶。

若 Evidence Doctor 报 Error，先修复或降级证据，不用该 run 关闭阶段。Warning 必须写入 `result.zh.md`、evaluation 或 Handoff，说明处理动作和结论边界。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic`，本阶段为 test-only same-chain candidate。 |
| A/B boundary | `test helper`，baseline 和 candidate 都在 `test-rvv/features/organized_edge_detection`。 |
| 当前决策问题 | `RVV-vs-scalar` 诊断，判断是否值得后续 production probe。 |
| diagnostic 是否可外推到 production | `unknown`。candidate 复刻 depth label 逻辑，但 production 仍包含模板点型、真实 `compute()`、`assignLabelIndices()` 和派生 RGB / normal 入口。 |
| comparison-boundary / baseline mismatch 风险 | 有。test helper 会消除 production 中每像素 `std::vector` 分配的部分开销，若 production 直接接入需要确认同边界 baseline。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 仅当 correctness、fallback、asm 和至少 weak-positive board bucket 同时成立时允许；负向或 unstable 时不进入 production，转为 no-production / follow-up 消融。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。若后续存在多个 RVV family，不能只用本阶段 Std/RVV 正向结果 clean-adopt。 |

## Phase scope 与扩展队列

| scope type | 内容 |
| --- | --- |
| `validated_scope` | `OrganizedEdgeBase<PointXYZ, Label>` depth label path，organized width × height 内部像素，float z，label bit 等价。 |
| `unvalidated_scope` | RGB Canny、normal Canny、RGB+normal 组合入口、泛型 `PointT` traits、非 `Label` PointLT、production `compute()` dispatch、上游完整 features test。 |
| `point_type_expansion_queue` | 若 production probe 值得继续，下一 phase 先读取泛型点类型策略，审计 `pcl::traits::has_field<PointT, pcl::fields::z>`、offset / stride、`PointXYZ` 以外的 XYZ-like 类型和 fallback。 |
| `phase_closeout_boundary` | Phase 000 只能关闭 test-only depth label candidate 的诊断 correctness / bench / asm / doctor 条目。 |

## Topic maturity audit

| area | current shape scan | decision | next action |
| --- | --- | --- | --- |
| production boundary | production 未接 RVV；公开入口为 `compute()`，实际热点在 protected `extractEdges()`。 | `not_applicable with evidence` for production patch | Phase 000 不改 production；证据正向后另建 PI1。 |
| RVV test support architecture | 本阶段新建 `src/`、`include/`、`include/impl/`、topic-local `script/`。 | `adopted` | scaffold 后在 result 回填真实文件和职责。 |
| target granularity | 尚无 topic Makefile。 | `phase_deferred + unblocked` | 本阶段补 `run_test_compare`、board smoke、board repeated、doctor / registry target。 |
| topic-local doc suite | 尚无 README、evaluation、roadmap、matrix。 | `phase_deferred + unblocked` | 本阶段建立最小 doc suite；若 EvidenceDecision 强于诊断，再补完整 role 文档。 |
| evidence freshness | 尚无 registry。 | `phase_deferred + unblocked` | 本阶段新增 registry 入口或人工检查边界。 |

## 文档更新清单

本阶段应创建或更新：

- `test-rvv/features/organized_edge_detection/README.zh.md`
- `test-rvv/features/organized_edge_detection/doc/organized_edge_detection-evaluation.zh.md`
- `test-rvv/features/organized_edge_detection/doc/optimization-roadmap.zh.md`
- `test-rvv/features/organized_edge_detection/doc/phases/README.zh.md`
- `test-rvv/features/organized_edge_detection/doc/phases/optimization-matrix.zh.md`
- `test-rvv/features/organized_edge_detection/doc/phases/000-current-state-and-label-equivalence/result.zh.md`

`doc-rvv/features/organized_edge_detection-RVV.zh.md` 本阶段不适用，因为没有 adopted production behavior（已采用生产行为）或用户确认保留的 production patch。

## 继续 / 停止条件

默认继续到 A1-A6 闭环。如果 QEMU correctness 失败，先修测试或 candidate；如果板卡目标不可达、工具链失败或 Evidence Doctor Error 无法修复，写 blocked Handoff。若板卡 evidence 为 positive / weak-positive 且 correctness、asm 和 doctor 边界闭合，下一 phase 默认是 `010-production-probe-plan`，但进入 production integration loop 前需要用户确认。若结果 neutral / negative / unstable，下一 phase 默认是 `010-negative-attribution-or-no-production-closeout`，只在 topic-local evaluation 和 phase result 中收口，不创建 production 长期文档。
