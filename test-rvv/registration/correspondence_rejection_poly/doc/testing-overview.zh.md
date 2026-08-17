# 测试体系总览

## 本文职责

本文是 `correspondence_rejection_poly` topic 的测试和证据入口。读者先用本文建立 public entry（公开入口）、test entry（测试运行入口）、bench entry（性能测试运行入口）和 evidence（证据）的对应关系，再进入更细的文档。

本文说明 test（测试）、bench（性能测试）、QEMU（仿真器）、board（板卡）、production direct（真实生产路径证据）、Evidence Doctor（证据体检）和 EvidenceDecision（证据决策）各自能证明什么。本文不复制每个 gtest（Google Test 单元测试用例）的完整解释，也不复制 bench summary（性能测试摘要）的数值表。细节按下面路径读取。

## 文档阅读路径

| 读者问题 | 首选入口 | 作用 |
| --- | --- | --- |
| 当前为什么不接入 production | `doc/correspondence_rejection_poly-evaluation.zh.md`、current Handoff | 区分历史 `rollback/no-production`、Phase 050 临时 patch replay、用户确认回滚和当前 clean production。 |
| 测试类型和运行入口如何分层 | 本文 | 建立 public entry、test entry、bench entry、QEMU、board 和 historical probe（历史探针）的证据边界。 |
| 每个 gtest 验证什么 | `doc/correctness-tests.zh.md` | 逐项解释输入、被测路径、断言、证明范围和不能证明的范围。 |
| bench case、checksum（校验和）和 Evidence Doctor 如何解释 | `doc/benchmark-and-evidence.zh.md` | 说明 case-filter（用例过滤条件）、计时边界、summary / manifest / doctor、registry（证据登记表）和提交边界。 |
| 每个 candidate（候选实现）为什么采用、拒绝或保留为历史线索 | `doc/optimization-evidence.zh.md`、`doc/phases/optimization-matrix.zh.md` | 把候选族映射到代码、target、board summary、asm attribution（反汇编归因）和 decision。 |
| helper、script 和 output 在哪里 | `doc/test-support-code-map.zh.md` | 连接 `include/`、`include/impl/`、`src/`、script、output 和 production 对照。 |
| production-direct 历史负向证据在哪里 | `doc/phases/030-pi1-production-integration-plan/result.zh.md` | 记录 Phase 030 临时生产探针、board negative、Evidence Doctor Errors 和历史回滚结论。 |
| 当前如何继续验证 | `doc/phases/050-production-patch-replay-user-validation/result.zh.md`、current Handoff | Phase 050 已执行 QEMU smoke 和板卡 5-run；用户已确认回滚。继续时需另开 profile / ablation phase。 |

## 当前生产边界

目标 production（生产源码）文件是：

- `registration/include/pcl/registration/correspondence_rejection_poly.h`
- `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`

当前 production 文件已还原为标量实现，没有本 topic production diff。Phase 050 回滚前曾临时加入 public dispatch（公开入口分流）到 RVV helper，再 fallback（回退）到 `Standard` helper；该 patch 的 Std/RVV 和 board correctness（正确性）通过，但 production-direct board repeated benchmark（板卡重复性能测试）仍为 negative。`doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md` 因 no-production closeout 仍不适用。

## Public / Test / Bench 入口对应关系

| public entry / 数据流 | correctness test entry | bench entry | 当前证据角色 | 边界 |
| --- | --- | --- | --- | --- |
| `applyRejection` -> `getRemainingCorrespondences` 的 guard 和输出语义 | `MissingInputReturnsAllCorrespondences`、`CardinalityAndSimilarityGuardsReturnAll`、public-entry smoke | `full-entry` QEMU smoke | 证明当前标量公开入口，以及 Phase 050 回滚前临时 Standard / RVV patch 的语义均与 reference 对齐 | 不证明性能收益或已采纳生产。 |
| `thresholdEdgeLength` 的 `min/max` 边长比 | `EdgeSimilarityBatchMatchesReference` | `edge-batch` | 证明局部 edge predicate（边判断）公式和 NaN 拒绝语义 | 不覆盖 point cloud gather（点云离散读取）。 |
| correspondence index gather（对应关系索引离散加载）和 squared distance staging（平方距离暂存） | `EdgeGatherStagingCandidateMatchesReference` | `edge-gather-staging` | 证明 production-shaped diagnostic（生产形态诊断）在 gather + staging 后仍正确 | 不覆盖完整 random sampling、histogram / Otsu 或输出 append。 |
| accept rate（接受率）和 filter（筛选） | `AcceptanceRateCandidateKeepsZeroSampleSemantics` | `acceptance-filter` | 证明 `num_samples == 0` 和非零样本除法语义 | 不覆盖 histogram scatter（直方图散写）和 production 收益。 |
| histogram / Otsu / output append | `HistogramOtsuAndFilterSemantics`、public-entry smoke | `full-entry` QEMU smoke | 证明 clamp、Otsu 空类跳过和输出顺序 | 不提供目标硬件性能结论。 |
| Phase 050 production direct replay | `run_test_compare`、`run_board_test_smoke` 已覆盖回滚前临时 patch 正确性 | `production-direct` target | 回滚前 patch 的公开入口和板卡性能证据为 negative | 用户已确认回滚；当前不接 production。 |

`run_test_compare` 同时运行 Std 和 RVV 构建。当前 production 已回到标量实现；Phase 050 回滚前的临时 patch 中，Std 构建禁用 `__RVV10__`，走 `Standard` 标量主体，RVV 构建启用 `__RVV10__` 并先尝试 production RVV helper。该证据只证明临时 patch 的正确性，不证明板卡性能或最终采纳。

## 测试类型定义

| 类型 | 当前形态 | 作用 | 边界 |
| --- | --- | --- | --- |
| deterministic corpus（确定性样本集） | 固定点云、固定 correspondence、固定 edge pairs、固定 accept-rate 输入 | 保护 guard、NaN 拒绝、histogram / Otsu、输出保序等明确边界 | 覆盖面由人工列出的 case 决定。 |
| seeded random stress（固定种子随机压力样本） | `std::mt19937` 固定 seed 生成点云扰动和乱序 correspondence | 扩大输入组合，降低漏掉 size / cardinality / threshold 组合的风险 | seed 固定后可复现；不提供性能证据。 |
| local correctness（局部正确性） | edge batch、accept rate、histogram / Otsu helper 对拍 | 证明 test-only candidate 与 reference path（参考链路）一致 | 不证明真实 production dispatch。 |
| production-shaped diagnostic（生产形态诊断） | `edge_gather_staging` 等 test-only helper | 模拟真实 row source（行来源）和 AoS point load（结构数组点加载） | 不能替代 production direct。 |
| production direct（真实生产路径证据） | Phase 030 历史证据 + Phase 050 replay | 在临时 production dispatch 存在时证明公开入口的真实正确性和性能 | Phase 050 replay 为 negative；不能支持生产采纳。 |
| QEMU correctness（QEMU 正确性） | `run_test_compare` | 证明构建、gtest 正确性和 RVV helper 形状 | QEMU timing（QEMU 计时）不进入性能结论。 |
| QEMU log-shape（QEMU 日志形状） | `run_bench_qemu_smoke` 和 Evidence Doctor manifest | 证明 bench 输出、case label、checksum 和 manifest 可解析 | 只支持日志合同，不支持目标硬件性能。 |
| board correctness（板卡正确性） | `run_board_test_smoke` | 证明目标硬件上 correctness binary 可运行并通过 8 个 test | 不是 repeated performance evidence（重复性能证据）。 |
| board repeated evidence（板卡重复证据） | `log/board/*_repeated/summary.md` | 支撑 decision bucket（决策桶）和 no-production 判断 | 需要 Evidence Doctor 解释异常；raw logs 默认不提交。 |

## 运行入口分类

| 入口 | 默认 target | 后端 | 证明点 |
| --- | --- | --- | --- |
| correctness compare | `make -C test-rvv/registration/correspondence_rejection_poly run_test_compare` | QEMU Std / RVV | 同一份 gtest 在禁用和启用 `__RVV10__` 的构建中通过。 |
| board correctness smoke | `make -C test-rvv/registration/correspondence_rejection_poly run_board_test_smoke` | board | 目标硬件上 correctness binary 可运行并通过 8 个 test。 |
| QEMU bench smoke | `make -C test-rvv/registration/correspondence_rejection_poly run_bench_qemu_smoke` | QEMU | `edge-batch`、`edge-gather-staging` 和 `acceptance-filter` 的日志形状、case label 和 checksum 形状可解析。 |
| board diagnostic repeated | `make -C test-rvv/registration/correspondence_rejection_poly run_board_bench_repeated` | board | edge / acceptance 诊断候选的重复性能摘要；aggregate target 不包含 `production-direct`。 |
| production-direct repeated | `make -C test-rvv/registration/correspondence_rejection_poly run_board_bench_production_direct_repeated` | board | Phase 050 回滚前临时 patch 的 5-run production-direct 证据；结果 negative。当前直接运行只测标量 public entry。 |

## 输入数据策略

| 输入族 | 构造方式 | 覆盖对象 | 证据位置 |
| --- | --- | --- | --- |
| guard corpus | identity correspondences、缺 source / target、非法 cardinality、非法 similarity threshold | public entry guard fallback（公开入口回退验收） | `MissingInputReturnsAllCorrespondences`、`CardinalityAndSimilarityGuardsReturnAll` |
| edge corpus | 预构造 squared distance（平方距离）和 `0 / 0` 零长度边 | `thresholdEdgeLength` 的 `min/max` 比值和 NaN 拒绝语义 | `EdgeSimilarityBatchMatchesReference` |
| gather corpus | `PointXYZ` source / target clouds、identity correspondences、edge pairs | correspondence index gather 和 staging | `EdgeGatherStagingCandidateMatchesReference` |
| accept-rate corpus | `num_samples` / `num_accepted` 包含 0 和非 0 | 除 0 mask（掩码）和 `accept_rate > cut` 输出语义 | `AcceptanceRateCandidateKeepsZeroSampleSemantics` |
| histogram corpus | 12 个固定 accept rate 样本 | histogram clamp（直方图夹取）、Otsu 空类跳过和输出顺序 | `HistogramOtsuAndFilterSemantics` |
| deterministic public entry | 固定 `std::srand`、固定 identity correspondences、固定 rejection rounds | test-only reference 与当前 production public entry 完全一致 | `FixedSeedReferenceMatchesProductionEntry` |
| seeded random public entry | 固定 seed 的点云扰动、乱序 correspondence、不同 size / cardinality / threshold | reference path 与当前 production public entry 完全一致 | `SeededRandomPublicEntryMatchesReference` |

确定性样本用于稳定回归和精确定位。固定种子随机压力样本用于扩大输入组合；它可重复运行，失败后能用同一 seed 复现。两类输入互补，任何一类都不能单独代表完整覆盖。

## 8 个 gtest 的测试层级

| 层级 | gtest | 被测路径 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- |
| public guard | `MissingInputReturnsAllCorrespondences` | 缺 source / target 的 public entry guard | 返回原 correspondences | RVV dispatch 或性能。 |
| public guard | `CardinalityAndSimilarityGuardsReturnAll` | cardinality 和 similarity threshold guard | 每个 guard 原因可隔离 | 点型 traits 或 production RVV。 |
| local formula | `EdgeSimilarityBatchMatchesReference` | `edge_similarity_batch_candidate` | `min/max >= threshold^2` 和 `0 / 0` NaN 拒绝 | production gather。 |
| production-shaped diagnostic | `EdgeGatherStagingCandidateMatchesReference` | `edge_similarity_gather_candidate` | correspondence index 读点、squared distance staging 和 edge predicate 一致 | 完整采样、histogram / Otsu 和输出 append。 |
| local accept-rate | `AcceptanceRateCandidateKeepsZeroSampleSemantics` | `compute_acceptance_rates_candidate` | `num_samples == 0` 输出 0，非零样本正确除法 | histogram scatter 和生产收益。 |
| scalar post-filter | `HistogramOtsuAndFilterSemantics` | scalar histogram / Otsu reference + filter candidate | histogram clamp、Otsu 空类跳过、`accept_rate > cut` 输出语义 | 完整随机采样路径加速。 |
| deterministic public entry | `FixedSeedReferenceMatchesProductionEntry` | reference path + production class | 固定 `std::srand` 后输出完全一致 | public-entry-shaped smoke 不能替代 production direct。 |
| seeded random public entry | `SeededRandomPublicEntryMatchesReference` | reference path + production class | 固定 `std::mt19937` seed 的乱序 correspondence、多组 size / cardinality / threshold | fuzz 全覆盖、性能和泛型点型。 |

完整输入、断言和日志结果见 `doc/correctness-tests.zh.md`。当前登记结果是 QEMU Std / RVV 各 8 tests passed，board smoke 8 tests passed。

## 证据边界

| 证据层 | 当前路径 | 支撑的结论 | 边界 |
| --- | --- | --- | --- |
| QEMU correctness | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | 当前源码语义、reference path 和 test-only candidate 输出一致 | 不提供目标硬件性能。 |
| QEMU log-shape | `log/qemu/analyze_bench_compare_*.log`、`log/qemu/*/evidence_doctor.md` | bench 输出合同、case label、checksum 和 manifest 可解析 | QEMU timing 不进入 EvidenceDecision 的性能部分。 |
| board correctness | `log/board/test_smoke/run_test.log` | 目标硬件上当前 correctness binary 通过 8 个 test | 只证明可运行和正确性。 |
| board diagnostic repeated | `log/board/edge_batch_repeated/summary.md`、`log/board/edge_gather_staging_repeated/summary.md`、`log/board/acceptance_filter_confirm/summary.md` | 局部或 production-shaped diagnostic 的 weak-positive / neutral 线索 | 不能替代 production direct。 |
| board production direct replay | `log/board/production_direct_repeated/summary.md`、`log/board/production_direct_repeated/evidence_doctor.md` | Phase 050 临时生产 replay 在目标硬件退化，用户已确认回滚 | 不说明唯一退化根因。 |
| asm Phase 050 refresh | `build/asm/riscv/bench_correspondence_rejection_poly_rvv.full.asm` | 回滚前 asm 可见 `getRemainingCorrespondencesRVV` / `Standard` 和 RVV 指令 | 不能抵消 board negative。 |

## Production-direct 运行边界

`production-direct` target 不需要额外环境变量。它不会修改生产 header、应用补丁或自动让当前 public entry 命中 RVV；是否形成 production RVV evidence（生产 RVV 证据）取决于运行前是否存在当前 production dispatch patch。

`production-direct` bench 的 C++ 入口是生产类公开调用：

```text
rejector.getRemainingCorrespondences(correspondences, remaining)
```

该 target 要形成真实 production RVV evidence，需要两个条件同时成立：

| 条件 | 当前状态 | 影响 |
| --- | --- | --- |
| 临时 production probe patch 存在 | 当前不存在；用户已确认回滚 | 当前直接运行只形成标量 public-entry timing；Phase 050 证据来自回滚前 replay。 |
| 直接运行 `production-direct` target | 当前可用 | 只运行当前源码；不会修改 production patch。 |

如果 production patch 不存在，target 仍会测到标量公开入口，不能登记为 production RVV evidence。当前 `production_direct_repeated` summary 是 Phase 050 replay 证据；它的 board decision bucket 为 negative，Evidence Doctor 报告 Errors=2。

## 当前可提交证据和默认排除项

| 类别 | 路径 / 模式 | 提交边界 |
| --- | --- | --- |
| QEMU correctness summary | `test-rvv/registration/correspondence_rejection_poly/log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | 被 README、evaluation、Handoff 引用，可作为 summary-only 审查候选。 |
| QEMU manifest / doctor | `test-rvv/registration/correspondence_rejection_poly/log/qemu/*/evidence_manifest.json`、`evidence_doctor.md` | 支撑 log-shape 和 Evidence Doctor；只按摘要证据审查。 |
| board correctness summary | `test-rvv/registration/correspondence_rejection_poly/log/board/test_smoke/run_test.log` | board correctness smoke 证据。 |
| board repeated summary / doctor | `test-rvv/registration/correspondence_rejection_poly/log/board/*_repeated/summary.md`、`evidence_doctor.md` | 被文档引用时可进入 summary-only 审查候选。 |
| evidence registry | `test-rvv/registration/correspondence_rejection_poly/log/evidence_registry.json` | 记录证据 hash、run label 和 doc refs；提交前运行 freshness check。 |
| raw logs | `test-rvv/registration/correspondence_rejection_poly/log/board/*/run-*/*`、完整 `run_bench_*.log` | local-only；提交需要用户明确要求和脱敏检查。 |
| build output | `test-rvv/registration/correspondence_rejection_poly/build/**` | 默认排除；asm summary 只作为已登记证据路径引用。 |
| current Handoff | `tmp/rvv-work-logs/registration/correspondence_rejection_poly/current-handoff/` | local-only，默认不提交。 |
| production `doc-rvv` | `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md` | 当前 `not_applicable`，no-production closeout 不创建或保留。 |

## 文档归属关系

| 信息类型 | 主归属 | 本文如何引用 |
| --- | --- | --- |
| 标量 public entry、guard、random sampling、histogram / Otsu 和输出语义 | `doc/correspondence_rejection_poly-evaluation.zh.md` | 本文只保留测试入口和证据边界。 |
| 8 个 gtest 的输入、断言和证明范围 | `doc/correctness-tests.zh.md` | 本文给测试层级摘要。 |
| bench label、case-filter、checksum、Evidence Doctor、registry | `doc/benchmark-and-evidence.zh.md` | 本文给入口分类和提交边界。 |
| candidate family 取舍 | `doc/optimization-evidence.zh.md`、`doc/phases/optimization-matrix.zh.md` | 本文只说明哪些测试和 bench 支撑这些取舍。 |
| Phase 030 negative production direct | `doc/phases/030-pi1-production-integration-plan/result.zh.md` | 本文引用其历史探针和 rollback 结论。 |
| Phase 040 doc-suite parity | `doc/phases/040-structure-parity-doc-suite/result.zh.md` | 本文作为 doc-suite area 的 `testing-overview` 主入口。 |
| 跨阶段恢复和下一动作 | `doc/optimization-roadmap.zh.md`、Phase 050、current Handoff | 本文记录当前 production-direct 验证和 PI5 用户确认点。 |
| helper、script 和 output 位置 | `doc/test-support-code-map.zh.md` | 本文只列关键路径，详细调用关系看代码地图。 |

## Reviewer 追踪路径

| 要复核的结论 | 追踪路径 |
| --- | --- |
| 当前 production patch 是否存在 | `git diff -- registration/include/pcl/registration/correspondence_rejection_poly.h registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`；再读 Phase 050 plan 和 current Handoff。 |
| public entry 标量语义 | production `applyRejection` / `getRemainingCorrespondences` -> evaluation `Public Entry 和输出语义` -> `src/test_correspondence_rejection_poly.cpp` 的 public-entry smoke。 |
| test-only RVV candidate 正确性 | `include/impl/correspondence_rejection_poly_candidates.hpp` -> 8 个 gtest -> QEMU Std/RVV logs。 |
| bench target 和 case-filter 含义 | `src/bench_correspondence_rejection_poly.cpp` -> `doc/benchmark-and-evidence.zh.md` -> QEMU / board summary。 |
| Evidence Doctor 和 registry 状态 | `log/**/evidence_doctor.md` -> `log/evidence_registry.json` -> `python3 test-rvv/script/evidence_registry.py check --registry test-rvv/registration/correspondence_rejection_poly/log/evidence_registry.json`。 |
| EvidenceDecision | Phase 030 historical board summary + current Phase 050 production-direct replay -> PI5 user confirmation -> current Handoff。 |

## 当前结论边界

这些测试和当前证据证明诊断 helper、Phase 050 临时 production patch 的正确性边界，以及 replay 在目标硬件上退化。不能把 QEMU timing 写成性能结论；用户已确认回滚，当前 EvidenceDecision 是 `rollback/no-production`。
