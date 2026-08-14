# Phase 020 计划：production-shaped gather 诊断

## 阶段意图和边界

Phase 010 的 board evidence（板卡证据）显示 `edge_length_batch` 在预构造 squared distance（平方距离）数组上是 `weak_positive`，但该证据没有覆盖真实 production（生产源码）中的 correspondence index（对应关系索引）读点成本。本阶段只在 topic-local test support（测试支撑）里建立 production-shaped gather diagnostic（生产形态 gather 诊断）：从 `PointXYZ` source / target 点云和 `pcl::Correspondences` 读取 edge 两端点，计算 source / target squared distance，再复用现有 `edge_similarity_batch_candidate` 做 RVV 公式段。

本阶段不修改 production 源码，不新增 public API（公开接口），不进入 production integration loop（生产接入闭环）。

不可触碰路径：

- `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`
- `registration/include/pcl/registration/correspondence_rejection_poly.h`
- 其它 topic 的 `test-rvv`、`doc-rvv` 或 agent asset（代理资产）

允许触碰路径：

- `test-rvv/registration/correspondence_rejection_poly/**`
- `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md`
- `tmp/rvv-work-logs/registration/correspondence_rejection_poly/**`

## 写文件前 worker quality gate check

| gate | status | evidence | missing_items |
| --- | --- | --- | --- |
| preferences_loaded | pass | S0 已读取 defaults；`.agents/local/user-preferences.yaml` absent；prompt 限定 topic-local、S10 前不改 production。 | none |
| frozen_policies | pass | 测试资产 / diagnostic 详细中文注释；production 注释克制；summary-only evidence；默认不 commit。 | none |
| scalar_path_ready | pass | Phase 000 evaluation 和本轮源码复核记录 `thresholdPolygon` / `thresholdEdgeLength` correspondence 索引路径。 | none |
| production_to_diagnostic_mapping_ready | pass | 本阶段从 `pcl::Correspondences` 的 `index_query` / `index_match` 读 `PointXYZ`，只覆盖 `thresholdEdgeLength` 局部公式，不覆盖随机采样和 histogram。 | production dispatch 不建立。 |
| phase_plan_written_before_edits | pass | 本文件是 Phase 020 首个产物。 | none |
| optimization_roadmap_ready | pass | Phase 010 result 指向 `020-production-shaped-gather-diagnostic`。 | roadmap 待本阶段结束刷新结果。 |
| optimization_matrix_ready | pass | 既有 matrix 有 `edge_length_batch`；本阶段新增 `edge_gather_staging` 行。 | 待本阶段结束更新。 |
| bench_boundary_ready | pass | 计时边界包含 correspondence index gather、PointXYZ squared distance staging、RVV formula 和 checksum。 | 不包含随机采样、acceptance rate、histogram / Otsu。 |
| board_backend_ready | pass | 用户确认板卡可用；沿用 Phase 010 board runner 和 5-run 起步预算。 | none |
| evidence_doctor_ready | planned | 本阶段新增 run label `edge_gather_staging_repeated` 的 summary / manifest / doctor。 | 等待 bench 输出。 |
| evidence_registry_ready | planned | 本阶段结束后登记 QEMU / board summary、manifest、doctor 和新 correctness logs。 | 等待输出。 |
| dirty_isolation_ready | pass | 只触碰当前 topic-local 路径；无关 dirty paths 在 Handoff 中继续隔离。 | none |
| production_decision_ready | pass | 本阶段明确 no-production；EvidenceDecision 最强只到 `diagnostic/no-production` 或 `production-shaped-diagnostic/continue`。 | none |

## 候选定义

| candidate family | row source policy | point type / layout | scope and entry | 计时边界 | 证明点 | 不证明边界 |
| --- | --- | --- | --- | --- | --- | --- |
| `edge_gather_staging` | correspondences（对应关系） | `PointXYZ` / `float` / AoS（结构数组） | test-only helper `edge_similarity_gather_candidate` | edge pair 列表 -> scalar gather + squared distance staging -> RVV edge formula -> checksum | 真实 correspondence index 读点和距离 staging 成本是否吞掉 Phase 010 弱正向 | 不覆盖 random sampling、完整 `thresholdPolygon` 多边形控制流、histogram / Otsu、production dispatch、generic point type |

## 实现动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| B1 test support helper | 在 `include/impl/correspondence_rejection_poly_candidates.hpp` 增加 edge pair 构造、reference 和 candidate。 | candidate 与 reference 输出 mask 和 checksum 一致；fallback 在非 RVV 构建走 reference。 |
| B2 correctness test | 在 `src/test_correspondence_rejection_poly.cpp` 增加 production-shaped gather 测试。 | Std / RVV gtest 都 pass，覆盖真实 correspondence index 和 target scramble。 |
| B3 bench case | 在 `src/bench_correspondence_rejection_poly.cpp` 增加 `edge-gather-staging` case。 | QEMU smoke 可生成日志；case label 可被 manifest wrapper 解析。 |
| B4 manifest / board summary parser | 更新 `generate_crpoly_evidence_manifest.py` 和 `summarize_crpoly_board_repeated.py` 的 CASE_LABELS。 | QEMU / board manifest 中 row_source、timer_boundary、checksum_policy 正确。 |
| B5 Makefile targets | 增加 QEMU smoke、doctor、board repeated target。 | `make -n` 展开正常；实际 board 输出独立 run label。 |
| B6 QEMU correctness / smoke / asm | 运行 gtest、QEMU smoke、doctor、asm dump。 | correctness pass；doctor Error=0；asm 至少定位 bench binary RVV 指令。 |
| B7 board repeated | 跑 5-run board repeated，必要时按同边界最多追加 1 轮确认。 | summary、manifest、doctor 生成；bucket 写入 result、matrix、roadmap、evaluation、Handoff。 |
| B8 registry / docs closeout | 登记证据并刷新文档。 | registry check fresh 或显式写 stale/blocker；production diff 仍为空。 |

## 决策桶和停止条件

沿用 Phase 010 的 board decision bucket：

- `positive` / `weak_positive`：只能支持继续写 PI1 production integration plan，不能直接改 production。
- `neutral` / `negative`：保持 `diagnostic/no-production`，将 `edge_gather_staging` 标为 attempted diagnostic。
- `unstable`：用完复跑预算后降级为 no-production 或 blocked，由 reviewer / 用户判断。

停止条件：

- board bucket 非正向；
- Evidence Doctor Error 未解决；
- 继续需要修改 production、public API、其它 topic 或 agent asset；
- production-shaped gather 正向但下一步需要 PI1 授权。

默认下一动作：

- 若 `edge_gather_staging` 正向：停在 `production-shaped-diagnostic/continue`，下一轮可写 PI1 计划，仍需用户确认 production 范围。
- 若非正向：完成 no-production closeout。
