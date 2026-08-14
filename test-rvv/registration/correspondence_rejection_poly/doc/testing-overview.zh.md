# 测试总览

## 本文职责

本文说明 `correspondence_rejection_poly` topic 的测试体系。它解释 test（测试）、bench（性能测试）、QEMU（仿真器）、board（板卡）和 production direct（真实生产路径证据）各自能证明什么。具体 gtest case（单元测试用例）细节见 `correctness-tests.zh.md`；bench label（性能测试标签）、checksum（校验和）和 Evidence Doctor（证据体检）见 `benchmark-and-evidence.zh.md`。

## 测试类型定义

| 类型 | 当前形态 | 作用 | 边界 |
| --- | --- | --- | --- |
| deterministic corpus（确定性样本集） | 固定点云、固定 correspondence、固定 edge pairs、固定 accept-rate 输入 | 保护 guard、NaN 拒绝、histogram / Otsu 和输出保序等明确边界 | 覆盖面由人工列出的 case 决定 |
| seeded random stress（固定种子随机压力样本） | `std::mt19937` 固定 seed 生成点云扰动和乱序 correspondence | 扩大输入组合，降低漏掉边界的风险 | 不是性能证据；seed 固定后可复现 |
| production-shaped diagnostic（生产形态诊断） | `edge_gather_staging` 等 test-only helper | 模拟真实 row source（行来源）和 AoS point load（结构数组点加载） | 不能替代真实 production dispatch（生产分流） |
| production direct（真实生产路径证据） | Phase 030 临时生产探针历史证据 | 证明公开入口在临时生产补丁下的真实正确性和性能 | 当前生产补丁已回滚；再次采集需要重新应用补丁 |
| QEMU smoke（QEMU 小型验证） | `run_test_compare` 和窄 bench smoke | 证明构建、correctness、日志形状和 manifest 可解析 | QEMU timing 不进入性能结论 |
| board repeated summary（板卡重复摘要） | `log/board/*_repeated/summary.md` | 支撑目标硬件性能 decision bucket（决策桶） | 需要 Evidence Doctor 解释异常 |

## 运行入口分类

| 入口 | 默认 target | 后端 | 证明点 |
| --- | --- | --- | --- |
| correctness compare | `make -C test-rvv/registration/correspondence_rejection_poly run_test_compare` | QEMU Std / RVV | 同一份 gtest 在禁用和启用 `__RVV10__` 的构建中通过 |
| board correctness smoke | `make -C test-rvv/registration/correspondence_rejection_poly run_board_test_smoke` | board | 目标硬件上 correctness binary 可运行并通过 8 个 test |
| QEMU bench smoke | `make -C test-rvv/registration/correspondence_rejection_poly run_bench_qemu_smoke` | QEMU | bench 输出格式、case label 和 checksum 形状可解析 |
| board diagnostic repeated | `make -C test-rvv/registration/correspondence_rejection_poly run_board_bench_repeated` | board | edge / acceptance 诊断候选的重复性能摘要 |
| historical production-direct repeated | `CRPOLY_ENABLE_PRODUCTION_DIRECT_PROBE=1 make -C test-rvv/registration/correspondence_rejection_poly run_board_bench_production_direct_repeated` | board | 仅在临时生产探针补丁存在时支撑 production direct；当前回滚状态下不形成生产 RVV 证据 |

## 输入数据总览

| 输入族 | 构造方式 | 覆盖对象 | 证据位置 |
| --- | --- | --- | --- |
| guard corpus | identity correspondences、缺 source / target、非法 cardinality、非法 similarity threshold | public entry guard fallback（公开入口回退验收） | `MissingInputReturnsAllCorrespondences`、`CardinalityAndSimilarityGuardsReturnAll` |
| edge corpus | 预构造 squared distance（平方距离）和 `0 / 0` 零长度边 | `thresholdEdgeLength` 的 `min/max` 比值和 NaN 拒绝语义 | `EdgeSimilarityBatchMatchesReference` |
| gather corpus | `PointXYZ` source / target clouds、identity correspondences、edge pairs | correspondence index gather（对应关系索引离散加载）和 staging（暂存） | `EdgeGatherStagingCandidateMatchesReference` |
| accept-rate corpus | `num_samples` / `num_accepted` 包含 0 和非 0 | 除 0 mask（掩码）和 `accept_rate > cut` 输出语义 | `AcceptanceRateCandidateKeepsZeroSampleSemantics` |
| histogram corpus | 12 个固定 accept rate 样本 | histogram clamp（直方图夹取）、Otsu 空类跳过和输出顺序 | `HistogramOtsuAndFilterSemantics` |
| seeded random public entry | 固定 seed 的点云扰动、乱序 correspondence、不同 size / cardinality / threshold | reference path（参考链路）与当前 production public entry 完全一致 | `SeededRandomPublicEntryMatchesReference` |

## 覆盖矩阵

| 路径 | 测试 / target | 覆盖内容 | 不能证明什么 |
| --- | --- | --- | --- |
| guard fallback（回退验收） | `MissingInputReturnsAllCorrespondences`、`CardinalityAndSimilarityGuardsReturnAll` | 缺输入、cardinality 和 similarity threshold 返回原 correspondences。 | 不证明 RVV production dispatch。 |
| edge similarity | `EdgeSimilarityBatchMatchesReference` | `min/max` 边长比、NaN 拒绝和 RVV 局部公式一致性。 | 不覆盖 production 点云 gather。 |
| edge gather staging | `EdgeGatherStagingCandidateMatchesReference` | correspondence index 读 `PointXYZ`、squared distance staging 和 RVV edge formula 输出一致。 | 不覆盖完整 public entry 的采样、histogram / Otsu 或输出 append。 |
| accept rate | `AcceptanceRateCandidateKeepsZeroSampleSemantics` | `num_samples == 0` 输出 0，非零样本计算比值。 | 不覆盖 histogram scatter。 |
| histogram / Otsu / filter | `HistogramOtsuAndFilterSemantics` | histogram clamp、Otsu 空类跳过、`accept_rate > cut` 输出语义。 | 不证明完整随机采样路径加速。 |
| deterministic public entry | `FixedSeedReferenceMatchesProductionEntry` | 固定 `std::srand` 后 test-only reference 与 production class 输出一致。 | public-entry-shaped smoke 不等于 production direct。 |
| seeded random public entry | `SeededRandomPublicEntryMatchesReference` | 固定 `std::mt19937` seed 的点云扰动、乱序 correspondence、多组 size / cardinality / threshold。 | 不是不可重复 fuzz；不证明性能。 |

## 运行入口

`run_test_compare` 同时运行 Std 和 RVV 构建。Std 构建禁用 `__RVV10__`，RVV 构建启用 `__RVV10__`。同一份测试源码通过 candidate stats（候选统计）确认 RVV build 命中 test-only RVV helper。当前生产源码已回滚，因此 RVV 构建中的真实 `CorrespondenceRejectorPoly` public entry 仍保持标量行为。

## 当前可提交证据

| 路径 | 角色 |
| --- | --- |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/run_test_std.log` | QEMU Std correctness。 |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/run_test_rvv.log` | QEMU RVV correctness。 |
| `test-rvv/registration/correspondence_rejection_poly/log/board/test_smoke/run_test.log` | board correctness smoke。 |
| `test-rvv/registration/correspondence_rejection_poly/log/board/production_direct_repeated/summary.md` | historical production direct negative summary。 |
| `test-rvv/registration/correspondence_rejection_poly/log/board/production_direct_repeated/evidence_doctor.md` | production adoption 失败的 Evidence Doctor 报告。 |

## 证据边界

这些测试是 correctness gate（正确性验收）。它们证明诊断 helper 和当前 production public entry 保持当前源码语义；它们不单独提供目标硬件性能结论。目标硬件性能结论只来自 `log/board/*/summary.md` 这类 repeated board summary（重复板卡摘要）。当前 production-direct repeated summary 为 negative，因此本 topic 不接 production。
