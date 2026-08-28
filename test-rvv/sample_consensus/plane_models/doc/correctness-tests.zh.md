# plane_models normal-plane correctness 测试说明

本文解释 correctness（正确性）测试的输入、断言和证明范围。bench 统计、board summary（板卡摘要）和 Evidence Doctor（证据体检）归属 `benchmark-and-evidence.zh.md`。

## 测试文件分工

| 文件 | 作用 |
| --- | --- |
| `src/test_sample_consensus_plane_models.cpp` | 历史 plane_models GTest 源码，包含 plane、normal-plane、normal-parallel-plane 回归，以及 normal-plane RVV helper 对拍和公开入口测试。 |
| `pcd/sac_plane_test.pcd` | 文件点云 fixture（测试夹具），供 RANSAC、LMedS、MSAC 等回归和板卡 test 使用。 |
| `SampleConsensusModelNormalPlaneTest` proxy | 测试专用 wrapper，暴露 protected `*_Standard` 和 `*_RVV` helper，用于 helper-level 对拍。 |

## 共同输入和断言

normal-plane RVV 相关测试主要使用 `PointXYZ` 点坐标、`Normal` 法线和 `indices_` 顺序索引。Phase 040 额外使用 `PointXYZI`、`PointXYZINormal` 代表性 source 点型，以及注册 xyz 但非 standard-layout 的 source fallback fixture。Phase 060 额外使用 `PointNormal`、`PointXYZINormal` 作为 normal cloud，并增加注册 normal/curvature 单 float 但非 standard-layout 的 normal fallback fixture。Phase 070 把 `PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 交叉组合，防止两条轴单独通过后仍隐藏模板实例化或字段偏移风险。随机测试允许少量阈值边界差异，因为 RVV 路径使用 float 向量算术和 `getAcuteAngle3DRVV_f32m2`，标量路径使用 double / `getAngle3D` 组合；这些容差只用于阈值附近，不允许大范围输出偏移。

## TEST 字典

| TEST | 默认 target | 被测路径 | 断言 | 证明范围 | 不能证明什么 |
| --- | --- | --- | --- | --- | --- |
| `SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForSupportedLayout` | `run_normal_plane_public_tests`、`run_test_compare` | 公开 `select/count/getDistances` 与直接 RVV helper。 | inliers、errors、count、distances 与直接 helper 一致。 | 支持布局下公开入口真实命中 RVV 分流。 | 不测法线角度近似误差；该测试把 normal weight 设为 0。 |
| `SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZISource` | `run_normal_plane_public_tests`、`run_test_compare` | `PointXYZI + Normal` 公开入口与 direct RVV helper。 | inliers、errors、count、distances 与 direct helper 一致。 | source 额外 intensity 字段不参与 normal-plane 输出语义时，AoS source gate 可以命中 RVV。 | 不证明 `PointXYZI` dedicated board performance，也不覆盖其它 source 点型。 |
| `SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZINormalSource` | `run_normal_plane_public_tests`、`run_test_compare` | `PointXYZINormal + Normal` 公开入口与 direct RVV helper。 | inliers、errors、count、distances 与 direct helper 一致。 | source 只读 `x/y/z` 时，source 自带 normal 字段不会改变本 RVV stage 输出语义。 | 不证明以 `PointXYZINormal` 作为 `PointNT` normal cloud 的布局。 |
| `SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointNormalNormalLayout` | `run_normal_plane_public_tests`、`run_test_compare` | `PointXYZ + PointNormal` 公开入口与 direct RVV helper。 | inliers、errors、count、distances 与 direct helper 一致。 | normal cloud 带 `x/y/z` 额外字段时，只要 normal/curvature AoS gate 成立，公开入口会命中 RVV。 | 不证明其它 source 点型与 `PointNormal` 的交叉组合，也不证明性能。 |
| `SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZINormalNormalLayout` | `run_normal_plane_public_tests`、`run_test_compare` | `PointXYZ + PointXYZINormal` normal cloud 公开入口与 direct RVV helper。 | inliers、errors、count、distances 与 direct helper 一致。 | normal cloud 额外 xyz/intensity 字段不参与当前距离输出语义时，normal/curvature gate 可命中 RVV。 | 不证明 `PointXYZINormal` 同时作为 source 和 normal 的组合性能。 |
| `SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZIAndPointNormal` | `run_normal_plane_public_tests`、`run_test_compare` | `PointXYZI + PointNormal` 公开入口与 direct RVV helper。 | inliers、errors、count、distances 与 direct helper 一致。 | source stride 和 normal stride 同时变化时，公开入口仍使用各自 byte offset 命中 RVV。 | 不证明公开入口性能，也不覆盖其它 source / normal 点型全集。 |
| `SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZIAndPointXYZINormal` | `run_normal_plane_public_tests`、`run_test_compare` | `PointXYZI + PointXYZINormal` 公开入口与 direct RVV helper。 | inliers、errors、count、distances 与 direct helper 一致。 | source 额外 intensity 与 normal 额外 xyz/intensity 同时存在时，RVV helper 字段偏移仍正确。 | 不证明 `PointXYZRGBNormal` 等更多 normal-like 类型。 |
| `SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZINormalAndPointNormal` | `run_normal_plane_public_tests`、`run_test_compare` | `PointXYZINormal + PointNormal` 公开入口与 direct RVV helper。 | inliers、errors、count、distances 与 direct helper 一致。 | source 自带 normal 字段但只读取 xyz，normal cloud 独立读取 normal/curvature。 | 不证明 source 自带 normal 字段参与本模型输出。 |
| `SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZINormalAndPointXYZINormal` | `run_normal_plane_public_tests`、`run_test_compare` | `PointXYZINormal + PointXYZINormal` 公开入口与 direct RVV helper。 | inliers、errors、count、distances 与 direct helper 一致。 | source 和 normal cloud 同为代表性复杂 AoS 点型时，两个模板参数的字段偏移不会互相混用。 | 不证明完整泛型点型采纳或新的 RVV 实现族选择。 |
| `SampleConsensusModelNormalPlane.PublicEntriesFallbackForNonAoSRegisteredXYZSource` | `run_normal_plane_public_tests`、`run_test_compare` | 注册 xyz 但非 standard-layout source 的公开入口 fallback。 | 公开 `select/count/getDistances` 与 Standard helper 输出一致。 | 弱 `RVVXYZFloatLayout` 满足但 `RVVXYZAoSFloatLayout` 不满足时不会误入 RVV helper。 | 不覆盖所有用户自定义点型；只隔离 non-AoS fallback 原因。 |
| `SampleConsensusModelNormalPlane.PublicEntriesFallbackForNonAoSRegisteredNormalLayout` | `run_normal_plane_public_tests`、`run_test_compare` | 注册 normal/curvature 单 float 但非 standard-layout normal 的公开入口 fallback。 | 公开 `select/count/getDistances` 与 Standard helper 输出一致。 | normal field semantics（字段语义）满足但 AoS layout 前提不满足时不会误入 RVV helper。 | 不覆盖所有自定义 normal 类型；只隔离 standard-layout gate。 |
| `SampleConsensusModelNormalPlane.PublicEntriesFallbackForUnregisteredNormalLayout` | `run_normal_plane_public_tests`、`run_test_compare` | RVV build 下公开入口 fallback。 | 公开入口与 Standard helper 输出一致。 | curvature 非 float 注册 normal 类型不会误入 RVV。 | 不覆盖所有未注册或自定义字段布局。 |
| `SampleConsensusModelNormalPlane.SelectHelperResizesEmptyOutputBuffers` | `run_normal_plane_public_tests`、`run_test_compare` | `selectWithinDistanceStandard` 和 `selectWithinDistanceRVV` protected helper。 | 空 `inliers` / `error_sqr_dists_` 会被 helper resize，返回数量和容器大小一致。 | 直接 helper 调用不会写越界。 | 不改变公开 API；不证明其它 helper 的缓冲区合同。 |
| `SampleConsensusModelNormalPlane.SIMD_selectWithinDistance` | `run_test_compare` | Standard helper 与 RVV helper。 | inlier 序列和距离在容差内一致，阈值附近允许最多 2 个差异。 | 随机输入下 select helper 数值语义可接受。 | 测试内 timing 只作为辅助输出。 |
| `SampleConsensusModelNormalPlane.SIMD_countWithinDistance_normal_plane_smoke` | `run_test_compare` | Standard/SSE/AVX/RVV count helper。 | 各 ISA count 与 Standard 差异不超过 2。 | 大规模随机 normal-plane count correctness。 | 不证明 select 写回或 getDistances dense store。 |
| `SampleConsensusModelNormalPlane.SIMD_countWithinDistance` | `run_test_compare` | Standard/SSE/AVX/RVV count helper。 | count 容差内一致，并打印性能块。 | weighted distance count 主路径 correctness。 | QEMU timing 不作为性能证据。 |
| `SampleConsensusModelNormalPlane.SIMD_getDistancesToModel` | `run_test_compare` | Standard helper 与 RVV helper。 | 每个距离 `EXPECT_NEAR(..., 5e-3)`。 | dense distance write 的数值一致性。 | 不覆盖 select 的 `vcompress` 写回。 |
| `SampleConsensusModelNormalPlane.RANSAC` | `run_test_compare` | SAC 公开流程。 | 模型求解、inliers、coefficients 和 projectPoints 回归。 | normal-plane 公开模型流程仍可用。 | 不隔离 RVV dispatch；只作为上游回归。 |

plane 和 normal-parallel-plane 的历史 GTest 保留在同一文件中，作为 `plane_models` topic 的回归背景。它们不参与 normal-plane RVV EvidenceDecision。

## 验证命令

```bash
make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests
make -C test-rvv/sample_consensus/plane_models run_test_compare
make -C test-rvv/sample_consensus/plane_models run_board_normal_plane_public_tests
make -C test-rvv/sample_consensus/plane_models run_board_test fetch_board_logs
```

`run_board_test` 在 phase 010 后默认传入板卡侧 `pcd/sac_plane_test.pcd` 路径，不再要求命令行覆盖 `REMOTE_TEST_ARGS`。
