# 优化证据索引

## 本文职责

本文把 candidate family（候选族）映射到代码路径、test target（测试目标）、bench target、board evidence（板卡证据）、asm attribution（反汇编归因）、Evidence Doctor（证据体检）和当前 decision（决策）。它是候选取舍的索引，最终 no-production 结论主归属仍在 `correspondence_rejection_poly-evaluation.zh.md` 和 Phase 030 result。

## 当前结论摘要

当前 EvidenceDecision（证据决策）是 `rollback/no-production`。`edge_length_batch` 和 `edge_gather_staging` 给出了局部或生产形态诊断的弱正向线索；Phase 030 的 `production_edge_batch_rvv` 把该思路接入真实公开入口探针后，在 board repeated summary（板卡重复摘要）中变为 negative。由于 production direct 证据层级高于 production-shaped diagnostic（生产形态诊断），当前生产补丁已回滚。

| candidate family | 当前状态 | 代码路径 | test target | bench / evidence | decision |
| --- | --- | --- | --- | --- | --- |
| `edge_length_batch` | attempted diagnostic; board `weak_positive` | `include/impl/correspondence_rejection_poly_candidates.hpp` / `edge_similarity_batch_candidate` | `run_test_compare` | `run_bench_edge_batch_smoke`、`log/board/edge_batch_repeated/summary.md`、`run_evidence_doctor_edge_batch_qemu`、`dump_bench_rvv` | 已推进到 production-shaped gather 诊断；不直接接 production |
| `edge_gather_staging` | historical production-shaped diagnostic; board `weak_positive` | `include/impl/correspondence_rejection_poly_candidates.hpp` / `edge_similarity_gather_candidate` | `run_test_compare`、`run_board_test_smoke` | `run_bench_edge_gather_staging_smoke`、`log/board/edge_gather_staging_repeated/summary.md`、`log/board/edge_gather_staging_repeated/evidence_doctor.md` | 已被 production-direct 探针校验为不可直接接 production；保留为历史诊断 |
| `production_edge_batch_rvv` | attempted production direct; board `negative` | 回滚前 `getRemainingCorrespondencesRVV` / `getRemainingCorrespondencesStandard`；当前 production diff 为空 | `run_test_compare`、`run_board_test_smoke` | `production-direct` 历史探针、`log/board/production_direct_repeated/summary.md`、`log/board/production_direct_repeated/evidence_doctor.md` | `rollback/no-production`；不再默认接入 |
| `accept_rate_filter` | attempted diagnostic; board `neutral` | `include/impl/correspondence_rejection_poly_candidates.hpp` / `compute_acceptance_rates_candidate`、`filter_by_acceptance_rate_candidate` | `run_test_compare` | `run_bench_acceptance_smoke`、`log/board/acceptance_filter_confirm/summary.md`、`run_evidence_doctor_acceptance_qemu`、`dump_bench_rvv` | 保留诊断；不接 production |
| `histogram_otsu_scalar` | adopted scalar | `compute_histogram_reference`、`find_threshold_otsu_reference` | `run_test_compare` | included in full-entry plan | 暂不 RVV 化 |
| `full_entry_scalar_regression` | adopted scalar regression | `remaining_correspondences_reference` + production class | `run_test_compare` | `run_bench_full_rejection_smoke` 可作为 smoke | 证明固定 seed public entry 与参考链路一致；不证明性能 |

## 标量路径与 RVV 路径差异

| 阶段 | 标量源码行为 | 当前 RVV / candidate 行为 | 结论 |
| --- | --- | --- | --- |
| guard 和输入复制 | `remaining_correspondences` 先复制输入，guard 失败直接返回 | 未接 RVV | adopted scalar |
| random sampling（随机采样） | `std::rand() % n` 无放回抽样 | 未接 RVV；测试用固定 seed 对拍 | adopted scalar |
| `thresholdPolygon` | 按 `cardinality` 检查边，任一边失败则拒绝 polygon | 局部 edge predicate 和 gather staging candidate 已尝试 | 诊断弱正向，production direct 负向 |
| accept rate | `num_samples == 0` 输出 0，其它样本做除法 | `accept_rate_filter` 尝试 RVV rate 和 mask，保留 scalar append | neutral，不接 production |
| histogram / Otsu | `hist_size = nr_correspondences / 2`，Otsu 跳过空类 | 保持标量 reference | adopted scalar |
| output | `accept_rate[i] > cut` 时按输入顺序 push | 输出 append 仍保持标量 | adopted scalar |

## 代码级证据索引

| 代码 / 输出 | 证据角色 | 能证明 | 不能证明 |
| --- | --- | --- | --- |
| `include/impl/correspondence_rejection_poly_candidates.hpp` | test support candidate | reference、edge、gather、acceptance 和 checksum helper 可审查 | production dispatch 已采用 |
| `src/test_correspondence_rejection_poly.cpp` | correctness gate | 当前源码语义和 candidate 输出一致 | 性能收益 |
| `src/bench_correspondence_rejection_poly.cpp` | bench wrapper | case-filter 和 checksum 输出合同 | 生产补丁存在 |
| `log/board/edge_gather_staging_repeated/summary.md` | production-shaped diagnostic | gather + staging 后局部公式弱正向 | 完整 public entry 加速 |
| `log/board/production_direct_repeated/summary.md` | historical production direct | 回滚前真实公开入口探针在目标硬件退化 | 当前 production 仍含 RVV dispatch |
| `log/board/production_direct_repeated/evidence_doctor.md` | Evidence Doctor | Errors=2 阻止 production adoption | 退化的单一根因 |

## 取舍说明

`edge_length_batch` 使用预构造 squared distance 数组。Phase 010 board 结果为 `weak_positive`，因此 Phase 020 补了 `edge_gather_staging`，把 correspondence index 读取 source / target 点、squared distance staging（平方距离暂存）和 RVV edge formula 放进同一计时边界。

`edge_gather_staging` 仍是 test support diagnostic（测试支撑诊断）。它证明真实 row source（行来源）和 PointXYZ AoS 读点成本没有吞掉局部公式收益；它不证明 production dispatch、random sampling、完整 `thresholdPolygon` 控制流、histogram / Otsu 或 generic point type。Phase 030 已用 production-direct 探针复核该路线，结果显示完整公开入口里收益被抵消，因此该诊断只能作为历史线索。

`production_edge_batch_rvv` 是回滚前的真实公开入口探针。实现形态符合 public entry（公开入口）短路分流到 `getRemainingCorrespondencesRVV`，失败时回到 `getRemainingCorrespondencesStandard` 的要求；QEMU 和板卡 correctness 都通过。但板卡 production-direct repeated summary 为 `negative`，两个规模均 5/5 退化，Evidence Doctor 报告 2 个 Error。当前生产补丁已回滚，目标生产文件无本 topic diff。

`accept_rate_filter` 使用连续数组，RVV 形态较直接，但输出容器必须按输入顺序 append，当前保留 scalar tail（标量尾段）。Phase 010 确认复跑仍为 `neutral`，且 256K 有 Evidence Doctor warning，因此不作为 production 候选。

`histogram_otsu_scalar` 保持标量。histogram 是 data-dependent scatter（数据相关写入），Otsu 每次只处理 `nr_correspondences / 2` 个 bin；当前证据只要求保护语义。

## 下一阶段恢复条件

默认恢复动作为 `ready_for_review`。如果用户希望继续当前 topic，不应重新应用 Phase 030 的生产补丁；应另开窄范围 profile / component ablation（组件消融）阶段，先解释完整 public entry 中 random sampling（随机采样）、edge staging（边暂存）、histogram / Otsu 和输出 append 的成本占比，再提出新的候选族。
