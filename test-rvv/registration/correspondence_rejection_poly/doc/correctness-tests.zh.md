# Correctness 测试说明

## 本文职责

本文解释 `src/test_correspondence_rejection_poly.cpp` 中每个 correctness gate（正确性验收）覆盖的输入、被测路径、断言和证据边界。它的目标是让 reviewer 能区分 deterministic corpus（确定性样本集）和 seeded random stress（固定种子随机压力样本），并确认当前测试覆盖的是回滚后 production public entry（生产公开入口）的标量语义和 test-only RVV candidate（测试专用 RVV 候选）。

## 测试文件分工

| 文件 / helper | 作用 | 证据边界 |
| --- | --- | --- |
| `src/test_correspondence_rejection_poly.cpp` | 组织 8 个 gtest case，运行 guard、edge formula、gather staging、accept rate、histogram / Otsu 和 public-entry smoke | correctness 证据；不提供性能结论 |
| `include/impl/correspondence_rejection_poly_candidates.hpp` | 提供 reference path（参考链路）和 test-only RVV candidate | 复刻当前源码语义，不能证明 production dispatch |
| `support::remaining_correspondences_reference` | 参考 `getRemainingCorrespondences` 的 guard、随机采样、accept rate、histogram / Otsu 和输出构造 | 用于 public entry 对拍；后续 production 改动必须重新复核 |
| production `CorrespondenceRejectorPoly` | 当前真实生产类公开入口 | 回滚后保持标量；测试证明输出语义一致 |

## 共同输入和断言

所有 public-entry smoke（公开入口小型验证）在 reference 和 production 调用前都会重置 `std::srand(seed)`。这是必要条件，因为生产源码使用 `std::rand() % n` 无放回采样。两个路径使用相同 seed 后，采样序列一致，测试才能把输出差异归因到算法语义，而不是随机序列差异。

输出断言使用 exact correspondence output（完全一致的对应关系输出）：输出数量、`index_query` 和 `index_match` 必须逐项一致。这个断言保护 `remaining_correspondences` 按输入顺序 append（追加）的语义。

## gtest 用例

| TEST 名称 | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `MissingInputReturnsAllCorrespondences` | 8 条 identity correspondences，不设置 source / target | production public entry guard | 输出大小和每条 index 等于输入 | 缺 source / target 时返回输入副本 | RVV dispatch 或性能 |
| `CardinalityAndSimilarityGuardsReturnAll` | 16 点 source / target，分别触发 cardinality 和 similarity guard | production public entry guard | 每种配置都返回输入副本 | guard 原因可隔离，不把多个 fallback 混成一类 | 点型 traits 或 production RVV |
| `EdgeSimilarityBatchMatchesReference` | 6 组 source / target squared distance，包括 `0 / 0` | `edge_similarity_batch_candidate` | RVV candidate 输出 mask 等于 reference | `min/max >= threshold^2`、零长度 NaN 拒绝语义 | production 点云 gather |
| `EdgeGatherStagingCandidateMatchesReference` | 64 点 source / target、identity correspondences 和 7 条 edge pairs | `edge_similarity_gather_candidate` | production-shaped gather candidate 输出 mask 等于 reference | correspondence index 读点、squared distance staging 和 edge predicate 一致 | 完整随机采样、histogram / Otsu、输出 append |
| `AcceptanceRateCandidateKeepsZeroSampleSemantics` | `num_samples` 包含 0 和非 0 | `compute_acceptance_rates_candidate` | RVV rates 与 reference 在 `1e-6` 内一致 | `num_samples == 0` 输出 0，非 0 样本正确除法 | histogram scatter 和生产收益 |
| `HistogramOtsuAndFilterSemantics` | 12 个 accept rate 样本 | scalar histogram / Otsu reference + filter candidate | histogram 总数、Otsu cut 和 kept indices 一致 | clamp 到最后一格、Otsu 空类跳过、`accept_rate > cut` | 完整随机采样路径加速 |
| `FixedSeedReferenceMatchesProductionEntry` | 96 条 identity correspondences、512 轮采样、固定 seed | reference path + production class | reference 和 production class 输出完全一致 | 确定性 public-entry smoke，保护当前标量语义 | production direct RVV dispatch |
| `SeededRandomPublicEntryMatchesReference` | 固定 seed 随机点云、乱序 correspondences、多组参数 | reference path + production class | reference 和 production class 输出完全一致 | 可复现随机压力样本覆盖 size、cardinality 和 threshold 组合 | 性能、泛型点型、`Scalar=double` |

## 边界和随机样本策略

测试数据分两层。

确定性样本集负责精确边界。它覆盖缺输入、cardinality guard（多边形点数验收）、similarity threshold guard（相似度阈值验收）、`0 / 0` NaN 拒绝、histogram clamp、Otsu 空类跳过和输出顺序。这些 case 的输入在源码里直接构造，失败后容易定位到具体语义。

固定种子随机压力样本负责扩大组合覆盖。它使用固定 `std::mt19937` seed，生成点云扰动、乱序 query/match correspondence、不同输入规模、`cardinality` 2/3/4 和多个 similarity threshold。它可重复运行，输出失败时能用同一 seed 复现。

## 测试数据策略

当前测试同时保留确定性样本和固定种子随机压力样本。确定性样本用于稳定回归和精确定位；固定种子随机样本用于暴露人工 case 没覆盖的输入组合。两类数据互补，不能互相替代。

## 当前结果

`make -C test-rvv/registration/correspondence_rejection_poly run_test_compare` 已通过：

- `log/qemu/run_test_std.log`：8 tests passed。
- `log/qemu/run_test_rvv.log`：8 tests passed。

`make -C test-rvv/registration/correspondence_rejection_poly run_board_test_smoke` 已通过：

- `log/board/test_smoke/run_test.log`：8 tests passed。

## 保留边界

测试使用合成 `PointXYZ`。它覆盖当前源码语义、局部 candidate 和回滚后的 public entry 标量语义；泛型点类型、`Scalar=double` 和新 production candidate 需要单独证据。
