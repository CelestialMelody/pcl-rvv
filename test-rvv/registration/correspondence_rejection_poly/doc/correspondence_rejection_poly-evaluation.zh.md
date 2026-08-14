# correspondence_rejection_poly 函数级评估

## 范围和目标源码

本评估覆盖 `pcl::registration::CorrespondenceRejectorPoly<SourceT, TargetT>` 的 polygonal correspondence rejection（多边形对应关系剔除）路径。目标源码是：

- `registration/include/pcl/registration/correspondence_rejection_poly.h`
- `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`

当前 production（生产源码）没有本 topic diff。Phase 030 曾按 `Standard` / `RVV` helper 分层做过真实公开入口探针；板卡证据为负向后，生产补丁已回滚。

## 函数级结论

当前 EvidenceDecision（证据决策）是 `rollback/no-production`。

本 topic 的 test-rvv 资产证明了三件事：

- 当前标量语义已被可重复测试覆盖。QEMU Std/RVV 构建各 8 tests passed，板卡 smoke 8 tests passed。
- 局部 `edge_gather_staging` 诊断在板卡上曾为 `weak_positive`，但它只覆盖 correspondence index gather（对应关系索引离散加载）、squared distance staging（平方距离暂存）和 RVV edge formula（RVV 边公式）。
- 回滚前 production direct（真实生产入口直连）探针在板卡上为 `negative`：2048 和 8192 correspondences 两个 case 都是 5/5 degradation。该证据否决当前生产接入。

## Public Entry 和输出语义

`applyRejection` 从 `input_correspondences_` 进入 `getRemainingCorrespondences`。`getRemainingCorrespondences` 先把 `remaining_correspondences` 复制为原输入；任一 guard 失败时直接返回这份副本。guard 通过后，它清空输出并预留输入数量的容量，后续只把 `accept_rate[i] > cut` 的原 correspondence 按原输入顺序追加回输出。

当前源码使用 `std::rand() % n` 无放回抽样。每一轮采样 `cardinality_` 个 correspondence，调用 `thresholdPolygon` 检查多边形边长相似性。通过时同时增加 `num_samples` 和 `num_accepted`；失败时只增加 `num_samples`。accept rate（接受率）阶段保留 `num_samples == 0` 输出 0 的语义。随后 `computeHistogram` 把接受率映射到 `[0,1]` 的 `nr_correspondences / 2` 个 bin，`findThresholdOtsu` 查找 Otsu threshold（Otsu 阈值），最后按 `accept_rate > cut` 过滤。

## 标量流程与 RVV 边界

| 阶段 | 当前源码语义 | RVV / 诊断状态 |
| --- | --- | --- |
| `applyRejection` | 调用 `getRemainingCorrespondences(*input_correspondences_, correspondences)`。 | 当前保持标量。 |
| 输入 guard | `remaining_correspondences` 先复制输入；缺 source、缺 target、`cardinality_ < 2`、`cardinality_ >= nr_correspondences`、similarity threshold 不在 `[0,1]` 时返回输入副本。 | gtest 隔离覆盖 guard。 |
| random sampling | 每轮 `std::rand() % n` 无放回抽样 `cardinality_` 个 correspondence。 | 保持标量；测试用固定 seed 对拍。 |
| `thresholdPolygon` | `cardinality_ == 2` 检查一条边；否则检查相邻边和尾首边，任一边失败则拒绝整组。 | 局部 candidate 覆盖 edge predicate，完整控制流保持标量。 |
| `thresholdEdgeLength` | source / target 两条边分别算 xyz squared distance，比较 `min/max >= similarity_threshold_squared_`。零长度 `0 / 0` 产生 NaN，比较为 false。 | gtest 保护 NaN 拒绝语义；生产探针正确但板卡退化。 |
| accept rate | `num_samples == 0` 时接受率为 0；否则 `num_accepted / num_samples`。 | `accept_rate_filter` 诊断为 neutral，不接 production。 |
| histogram / Otsu | `hist_size = nr_correspondences / 2`；Otsu 最大化类间方差。 | 保持标量。 |
| output | `accept_rate[i] > cut` 时按输入顺序 push 原 correspondence。 | 保持标量，保护输出顺序。 |

## 文档归属矩阵

| 信息类型 | 主归属 | 当前状态 |
| --- | --- | --- |
| 标量流程、public entry、guard、random sampling、histogram / Otsu 和输出语义 | 本 evaluation 的 `Public Entry 和输出语义`、`标量流程与 RVV 边界` | updated |
| 每个 gtest 的输入、断言和证明范围 | `doc/correctness-tests.zh.md` | updated |
| 测试类型、运行入口和证据白名单 | `doc/testing-overview.zh.md` | updated |
| bench case、case-filter、计时边界、checksum、Evidence Doctor 和 registry | `doc/benchmark-and-evidence.zh.md` | updated |
| candidate family 的 adopted / attempted / rejected / deferred 状态 | `doc/optimization-evidence.zh.md`、`doc/phases/optimization-matrix.zh.md` | updated |
| phase 计划、结果、早停检查和 doc-suite parity 审计 | `doc/phases/**` | updated by Phase 040 |
| 跨阶段恢复队列和继续条件 | `doc/optimization-roadmap.zh.md` | updated |
| Handoff、dirty isolation、artifact publication 和下一步 | `tmp/rvv-work-logs/registration/correspondence_rejection_poly/current-handoff/` | local-only handoff |
| production 长期主题文档 | `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md` | not_applicable；当前不存在 |

## 测试数据构成

当前测试不是只靠随机数据。它同时包含：

- deterministic corpus（确定性样本集）：固定合成点云、固定 edge pairs、固定 accept-rate 样本、identity correspondences 和固定 `std::srand` public-entry smoke。它们用于保护已知边界。
- seeded random stress（固定种子随机压力样本）：`SeededRandomPublicEntryMatchesReference` 使用固定 `std::mt19937` seed 生成点云扰动和乱序 correspondence，并覆盖不同 size、cardinality 和 threshold。每个 case 在 reference 和 production public entry 前都重置 `std::srand`，所以结果可复现。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 上游 / 消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- |
| `CorrespondenceRejectorPoly::applyRejection` | production public entry | 从 base class 输入 correspondences 进入目标 helper | 上游 registration rejector pipeline | production boundary（生产边界） | `registration/include/pcl/registration/correspondence_rejection_poly.h` |
| `getRemainingCorrespondences` | production scalar helper | guard、随机采样、accept rate、histogram、Otsu 和输出构造 | `applyRejection` / 直接调用 | scalar truth（标量事实） | `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp` |
| `thresholdPolygon` / `thresholdEdgeLength` | production scalar helper | 多边形边长比阈值判断 | `getRemainingCorrespondences` | formula source（公式来源） | `registration/include/pcl/registration/correspondence_rejection_poly.h` |
| `include/correspondence_rejection_poly.h` | test support aggregator（测试支撑聚合入口） | 给 test / bench 提供稳定 include 入口 | `src/test_*.cpp`、`src/bench_*.cpp` | reviewer navigation（审查定位） | `test-rvv/registration/correspondence_rejection_poly/include/correspondence_rejection_poly.h` |
| `include/impl/correspondence_rejection_poly_candidates.hpp` | reference / candidate helper | 标量参考和 test-only RVV 局部候选 | gtest、bench wrapper | correctness / asm smoke | `test-rvv/registration/correspondence_rejection_poly/include/impl/correspondence_rejection_poly_candidates.hpp` |
| `src/test_correspondence_rejection_poly.cpp` | correctness test | 8 个 gtest 覆盖 guard、公式、诊断候选、输出和固定种子随机压力 | QEMU Std/RVV build、board smoke | correctness gate（正确性验收） | `test-rvv/registration/correspondence_rejection_poly/src/test_correspondence_rejection_poly.cpp` |
| `src/bench_correspondence_rejection_poly.cpp` | bench wrapper | edge batch、edge gather staging、acceptance filter、full-entry 和 production-direct 探针输出合同 | QEMU / board runner | log shape / board bench | `test-rvv/registration/correspondence_rejection_poly/src/bench_correspondence_rejection_poly.cpp` |
| `script/generate_crpoly_evidence_manifest.py` | analysis script | 将 topic-bound logs 转成 Evidence Doctor manifest | `run_evidence_doctor_*` | doctor input（证据体检输入） | `test-rvv/registration/correspondence_rejection_poly/script/generate_crpoly_evidence_manifest.py` |
| `script/summarize_crpoly_board_repeated.py` | analysis script | 生成 repeated board summary / manifest | board repeated targets | board summary | `test-rvv/registration/correspondence_rejection_poly/script/summarize_crpoly_board_repeated.py` |
| `log/evidence_registry.json` | evidence registry | 记录当前证据文件 hash、run label 和 doc refs | Handoff / freshness check | freshness（证据新鲜度） | `test-rvv/registration/correspondence_rejection_poly/log/evidence_registry.json` |
| `doc/phases/030-pi1-production-integration-plan/result.zh.md` | phase closeout | production probe 负向证据和回滚判断主归属 | Handoff / reviewer | rollback evidence | `test-rvv/registration/correspondence_rejection_poly/doc/phases/030-pi1-production-integration-plan/result.zh.md` |

## 当前验证结果

| 证据 | 状态 | 路径 | 说明 |
| --- | --- | --- | --- |
| QEMU Std correctness | pass | `test-rvv/registration/correspondence_rejection_poly/log/qemu/run_test_std.log` | 8 tests passed。 |
| QEMU RVV correctness | pass | `test-rvv/registration/correspondence_rejection_poly/log/qemu/run_test_rvv.log` | 8 tests passed。 |
| board correctness | pass | `test-rvv/registration/correspondence_rejection_poly/log/board/test_smoke/run_test.log` | 8 tests passed。 |
| QEMU production-direct smoke | pass as log-shape | `test-rvv/registration/correspondence_rejection_poly/log/qemu/analyze_bench_compare_production_direct.log`、`log/qemu/production_direct/evidence_doctor.md` | QEMU timing 不作为性能结论。 |
| asm historical probe | pass for rollback evidence | `test-rvv/registration/correspondence_rejection_poly/build/asm/riscv/bench_correspondence_rejection_poly_rvv.full.asm` | 回滚前 `getRemainingCorrespondencesRVV` / `Standard` 符号可见；当前源码已回滚。 |
| board production direct | negative | `test-rvv/registration/correspondence_rejection_poly/log/board/production_direct_repeated/summary.md` | 2048 median 约 0.901x，8192 median 约 0.956x，两个 case 均 5/5 degradation。 |
| Evidence Doctor board production direct | fail for adoption | `test-rvv/registration/correspondence_rejection_poly/log/board/production_direct_repeated/evidence_doctor.md` | Errors=2，Warnings=0，Suggestions=0。 |

## 诊断证据链

| 证据层 | 当前结果 | 支撑的结论 | 不能支撑 |
| --- | --- | --- | --- |
| correctness | QEMU Std / RVV 各 8 tests passed；board smoke 8 tests passed | 当前源码语义、reference path 和 test-only candidate 输出一致 | 目标硬件性能 |
| deterministic corpus | guard、edge formula、accept-rate、histogram / Otsu、输出顺序均有固定样本 | 已知边界可稳定回归 | 未列出的输入分布 |
| seeded random stress | 固定 seed 随机点云和乱序 correspondence 与 production public entry 完全一致 | 扩大输入覆盖，降低遗漏风险 | 不代表 fuzz 全覆盖或性能收益 |
| production-shaped diagnostic | `edge_gather_staging` board repeated 为 historical `weak_positive` | 局部 gather + staging 后 edge formula 有弱正向线索 | 完整 production public entry 加速 |
| production direct | Phase 030 historical probe 的 board repeated 为 `negative`，2048 和 8192 均 5/5 degradation | 当前候选不能接入 production | 退化的单一根因 |
| Evidence Doctor | production-direct board Errors=2，Warnings=0，Suggestions=0 | 生产采用失败，需回滚或降级 | 功能 bug 结论 |

这条诊断证据链说明：局部 RVV 候选可正确运行，部分诊断 case 在板卡上有弱正向，但真实公开入口生产探针在目标硬件退化。当前结论因此停在 `rollback/no-production`，不会把 diagnostic evidence（诊断证据）写成 production evidence（生产证据）。

## 生产接入判断

不接入 production。当前生产补丁已回滚，`doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md` 按 no-production 发布边界删除。诊断证据链和 rollback 结论主归属是本 evaluation、Phase 030 result、optimization matrix、roadmap 和 current Handoff。

如果后续继续当前 topic，应先建立新的 profile / component ablation phase。该阶段需要解释完整 public entry 中 random sampling、edge staging、histogram / Otsu 和输出 append 的成本占比，再提出新候选。不能重新应用 Phase 030 的生产补丁作为默认下一步。
