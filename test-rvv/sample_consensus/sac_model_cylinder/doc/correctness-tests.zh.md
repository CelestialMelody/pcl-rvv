# sac_model_cylinder correctness 测试说明

## 本文职责

本文解释 `src/test_sac_model_cylinder.cpp` 中 gtest（Google Test 单元测试）如何保护 cylinder 三个公开距离入口、
diagnostic candidate（诊断候选）、representative point types（代表点型）和 production fallback（生产回退路径）
的语义。它不承担 bench 统计或采纳结论。

## 测试文件分工

| 文件 / 符号 | 职责 |
| --- | --- |
| `src/test_sac_model_cylinder.cpp` | 构造合成圆柱点云、法线、乱序 `indices_`，并对拍 public entry、Standard helper 和 test-only candidate。 |
| `include/sac_model_cylinder.h` | topic 聚合头，测试和 bench 只 include 这一层。 |
| `include/impl/sac_model_cylinder_diagnostic.hpp` | Phase 000 测试专用 diagnostic helper，继续作为历史交叉检查。 |
| `pcl::detail::*StandardCylinder` | 生产标量 helper，是 public-vs-Standard correctness 的基线。 |
| `pcl::detail::*RVVCylinder` | 生产 RVV helper，在 RVV 构建下由公开入口短路分流调用。 |

## 共同输入和断言

小型 gtest 使用 `PointXYZ + pcl::Normal`，模型系数为轴线点 `(0.20, -0.30, 0.10)`、方向 `(0, 0, 1)`、
半径 `1.0`。输入包含明显内点、半径误差超阈值点和 normal angle（法线夹角）超阈值点；`indices_` 使用
`{6, 1, 7, 0, 5, 2, 4, 3}`，保护 indexed gather（按索引离散加载）和 select 输出顺序。

bench-shaped correctness（按性能样本形态构造的正确性测试）使用 4096 点 shuffled adjacent pairs，保护
`getDistancesToModel` dense vector（连续数组）输出的逐项 `1e-5` 近似一致性。
Phase 040 另外复用同一几何输入生成 `PointXYZI + Normal`、`PointXYZRGB + Normal` 和
`PointXYZ + PointNormal`，证明 traits-gated（字段特征门控）source stride（源点步长）和 normal field offset
（法线字段偏移）不会改变三条 public entry 的标量语义。

## TEST 字典

| TEST | 被测路径 | 断言 | 证明范围 | 不能证明什么 |
| --- | --- | --- | --- | --- |
| `CountCandidateMatchesPublicEntryWithEarlyEuclidGate` | public `countWithinDistance` 对比 `countWithinDistanceCandidate`。 | count 完全相等。 | Phase 000 candidate 保持 weighted euclid early gate（带权欧氏距离早停）和 normal angle 权重顺序。 | 不证明 production dispatch 或板卡性能。 |
| `CountPublicEntryMatchesProductionStandardHelper` | public `countWithinDistance` 对比 Standard helper。 | count 完全相等。 | RVV 构建下 public entry 与标量 helper 同边界等价。 | 不覆盖未验证点型性能。 |
| `SelectCandidatePreservesOrderAndErrorDistances` | public `selectWithinDistance` 对比 `selectWithinDistanceCandidate`。 | `inliers` 顺序相等，`error_sqr_dists_` 逐项 `1e-5` 近似。 | Phase 000 candidate 保持 `indices_` 扫描顺序。 | 不证明 production asm。 |
| `SelectPublicEntryMatchesProductionStandardHelper` | public `selectWithinDistance` 对比 Standard helper。 | `inliers` 顺序相等，误差逐项 `1e-5` 近似。 | RVV `vcompress` 保序写回与 Standard helper 等价。 | 不覆盖极端稀疏命中分布性能。 |
| `SelectCandidateClearsStaleStateWhenNoInliers` | diagnostic select candidate。 | 空命中时 `inliers` 和 `error_sqr_dists_` 都清空。 | 保护 select 副作用语义。 | 不证明 public production fallback。 |
| `GetDistancesPublicEntryMatchesProductionStandardHelper` | public `getDistancesToModel` 对比 Standard helper。 | dense distance 逐项 `1e-5` 近似。 | Phase 030 production dense store 与标量等价。 | 不证明更多规模性能。 |
| `GetDistancesBenchShapedPublicEntryMatchesStandardHelper` | 4096 点 bench-shaped public `getDistancesToModel`。 | dense distance 逐项 `1e-5` 近似。 | bench checksum 只保护规模时，逐项数值由此 case 承担。 | 不作为性能测试。 |
| `PublicEntryFallsBackWhenNormalsDoNotCoverInput` | public count/select fallback。 | normal 数不足但欧氏早停能保护标量路径时，public entry 正常回退并输出空结果。 | 保护 RVV normal 预取不能破坏标量懒读取边界。 | 不覆盖 getDistances 的 normal 缺失，因为标量路径也必须读取 normal。 |
| `AdditionalSourcePointTypesMatchStandardPath` | `PointXYZI + Normal` 和 `PointXYZRGB + Normal` public entries 对 Standard helper。 | count、select、getDistances 均与 Standard helper 对齐。 | source xyz 字段不在 `PointXYZ` 精确布局时，traits offset 和 stride 仍保持语义。 | 不证明所有自定义点型性能。 |
| `AdditionalNormalPointTypesMatchStandardPath` | `PointXYZ + PointNormal` public entries 对 Standard helper。 | count、select、getDistances 均与 Standard helper 对齐。 | normal cloud 额外包含 xyz 字段时，normal offset 仍正确。 | 不证明所有 normal-like 点型性能。 |
| `AdditionalPointTypesBenchShapedMatchStandardPath` | 4096 点 bench-shaped typed public entries。 | 三组代表点型的 count、select、getDistances 均与 Standard helper 对齐。 | 板卡 performance manifest 的 typed checksum 有对应逐项 correctness 保护。 | 不作为性能测试，也不覆盖 custom layout。 |

## 验证命令

```bash
make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare
make -C test-rvv/sample_consensus/sac_model_cylinder run_cylinder_count_select_tests
```

`run_test_compare` 是当前 correctness aggregate（正确性汇总入口）。生产性能采纳必须再结合 production asm、
board repeated manifest 和 Evidence Doctor。
