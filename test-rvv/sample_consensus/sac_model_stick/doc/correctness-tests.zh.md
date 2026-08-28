# sac_model_stick correctness tests

## 本文职责

本文解释 `src/test_sac_model_stick.cpp` 中 11 个 gtest（GoogleTest 测试用例）的输入、断言和证明边界。性能统计、checksum（校验和）和 Evidence Doctor（证据体检）不归本文负责。

## 测试文件分工

| 文件 / 符号 | 职责 |
| --- | --- |
| `src/test_sac_model_stick.cpp` | 构造 synthetic stick-distance cloud（合成 stick 距离点云），同时保留 test-only candidate（测试专用候选）回归和 public entry（公开入口）production direct（真实生产入口直连）测试。 |
| `include/impl/sac_model_stick_diagnostic.hpp` | 提供 `countWithinDistanceCandidate`、`selectWithinDistanceCandidate`、`getDistancesToModelCandidate`，用于诊断回归和与生产路径对照。 |
| `makeStickDistanceCloud` | 生成围绕 stick 轴线的点，允许用 radial offset（径向偏移）精确控制内圈、外圈和远外圈。 |
| `makeStickDistanceCloudAs<PointT>` | 用同一几何输入构造 `PointXYZI`、RGB/RGBA 和 RGBNormal 等代表点型。 |
| `expectPublicMatchesStandard` | 在同一模型状态下比较 public entry 和 Standard helper 的 count/select/getDistances 输出。 |
| `expectSelectCandidateMatchesPublic` | 对拍 select 输出的 inliers（内点索引）和 `error_sqr_dists_`。 |
| `expectDistancesCandidateMatchesPublic` | 对拍 public/candidate 的 dense distances（稠密距离输出）。 |

## 共同输入和断言

多数 case 使用 `PointXYZ`、`Eigen::VectorXf` 七维系数和打乱顺序的 `indices_`。candidate 测试证明 test-only 诊断 helper 仍复刻公开入口语义；public production tests 只调用公开入口，Std/RVV 两个构建都运行，用来保护生产分流接入后的可见行为。Phase 090 新增的点型扩展 case 使用 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal`，证明 traits gate（字段特征门控）不是 exact `PointXYZ`。

误差阈值为 `1e-6` 或 `1e-5`，用于比较 public 标量路径、candidate 输出或固定 public 输出中的 `double` 距离 / 平方距离。QEMU correctness（QEMU 正确性验证）只说明路径可构建并通过断言，不说明目标硬件性能。

## TEST 字典

| TEST | 输入重点 | 被测路径 | 断言 | 证明范围 | 不能证明什么 |
| --- | --- | --- | --- | --- | --- |
| `CountCandidateMatchesPublicEntryWithInnerOuterPenalty` | 同时包含内圈、外圈、远外圈点，`indices_` 乱序。 | public `countWithinDistance` vs `countWithinDistanceCandidate`。 | public count 等于 candidate count。 | candidate 复刻 stick 的内外阈值双计数。 | 不证明 production helper 的反汇编归属。 |
| `CountCandidateReturnsZeroWhenOuterBandDominates` | 外圈数量不少于内圈。 | public count vs candidate count。 | 两者都返回 0。 | 保护 `nr_i <= nr_o ? 0 : nr_i - nr_o`。 | 不证明普通 line count 语义可迁移。 |
| `CountPublicEntryPreservesInnerOuterPenalty` | 与第一组 count 输入相同。 | public `countWithinDistance`。 | 返回 1。 | 证明接入后公开入口保持内外圈扣减语义。 | 不证明其它点型性能。 |
| `SelectCandidateMatchesPublicEntryAndErrorDistances` | 命中 / 未命中混合，`indices_` 乱序。 | public `selectWithinDistance` vs candidate。 | `inliers` 和 `error_sqr_dists_` 完全对齐。 | `vcompress` candidate 保持扫描顺序和平方距离写回。 | 不证明 production helper 的指令归属。 |
| `SelectCandidateClearsStaleErrorDistancesWhenNoInliers` | 先制造旧 `error_sqr_dists_`，再用零阈值无命中。 | `selectWithinDistanceCandidate`。 | `inliers` 与 `error_sqr_dists_` 都为空。 | 证明 candidate 清理旧输出状态。 | 不证明 public entry 的性能。 |
| `SelectPublicEntryPreservesOrderAndErrorDistances` | 打乱 `indices_` 且混合命中 / 未命中点。 | public `selectWithinDistance`。 | inliers 为 `{1, 3, 0, 2}`，前两个平方距离对齐。 | 证明 production public entry 保持保序输出和 `error_sqr_dists_` 语义。 | 不证明其它 layout。 |
| `SelectPublicEntryClearsStaleStateWhenNoInliers` | 先用常规阈值填充旧状态，再用零阈值。 | public `selectWithinDistance`。 | `inliers` 与 `error_sqr_dists_` 都为空。 | 证明接入后公开入口仍清理 stale state（旧状态）。 | 不证明 board 性能。 |
| `GetDistancesCandidateMatchesPublicDirectionCoefficientSemantics` | 系数 3-5 按方向向量解释。 | public `getDistancesToModel` vs candidate。 | 每个 dense distance 近似相等。 | 防止误用 count/select 的第二端点语义。 | 不证明 penalty 分支。 |
| `GetDistancesCandidatePreservesPenaltyAndDenseIndexedOrder` | 设置 `radius_max_ = 0.10`，`indices_` 乱序。 | public getDistances vs candidate。 | 输出大小等于 indices 数量，存在 penalty 距离，并逐项对齐。 | 证明 dense 输出顺序和 `2 * sqrt(sqr_distance)` penalty。 | 不证明真实 RANSAC 上游。 |
| `GetDistancesPublicEntryPreservesDirectionPenaltyAndOrder` | 同时覆盖方向系数、penalty 和 dense 输出顺序。 | public `getDistancesToModel`。 | 前两个输出约为 `2.2016206`、`1.4142270`，且存在 penalty 距离。 | 证明 production public entry 保持 getDistances 的特殊系数语义和 penalty。 | 不证明其它点型或上游总耗时。 |
| `AdditionalAoSPointTypesMatchStandardPath` | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZRGBNormal`，打乱 `indices_`。 | public count/select/getDistances vs Standard helper。 | 四个点型的三条公开入口输出都与 Standard helper 对齐。 | 证明代表性 xyz AoS 点型在当前 traits gate 下保持输出语义。 | 不证明这些点型的 dedicated board performance。 |

## 验证命令

```bash
make -C test-rvv/sample_consensus/sac_model_stick run_test_compare
make -C test-rvv/sample_consensus/sac_model_stick run_stick_count_tests
make -C test-rvv/sample_consensus/sac_model_stick run_stick_select_tests
make -C test-rvv/sample_consensus/sac_model_stick run_stick_getdistances_tests
make -C test-rvv/sample_consensus/sac_model_stick run_stick_point_type_tests
```

`run_test_compare` 是当前 correctness aggregate（正确性汇总入口）。`run_stick_count_tests`、`run_stick_select_tests` 和 `run_stick_getdistances_tests` 只跑 RVV build 的细分 filter，适合快速复核对应入口；完整 Std/RVV 对拍仍以 `run_test_compare` 为准。

## 当前边界

correctness tests 覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZRGBNormal`、direct indexed `indices_`、float xyz AoS 布局和七维 `Eigen::VectorXf` 系数。其它点型的独立性能、非 AoS fallback fixture 和更宽调用形态仍需要单独 phase。
