# sac_model_plane correctness tests

## 本文职责

本文解释 `src/test_sac_model_plane.cpp` 中每个 gtest 的输入、断言和证据边界。测试使用真实
`SampleConsensusModelPlane` 公开入口和 protected helper（受保护辅助函数），不把 helper-level
pass（辅助函数级通过）外推成其它模型的 production（生产源码）结论。

## 共同输入

测试使用 z=0 平面系数 `(0, 0, 1, 0)`，构造带正负 z 偏移的小点云。乱序 case 覆盖
indexed gather（按索引离散加载）、阈值 mask（掩码）和 `selectWithinDistance` 的保序写回；
identity case 覆盖默认 cloud-only 构造生成的 `indices[i] == i` 路径。

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不证明范围 |
| --- | --- | --- | --- | --- | --- |
| `PublicEntriesMatchDirectRVVForSupportedLayout` | `PointXYZ`，乱序 `indices_`，阈值 `0.05` | 三条公开入口、Standard helper 和 RVV helper | inliers、count、distances 和 error buffer 与公开入口一致。 | `PointXYZ` direct indexed public entry correctness。 | 不证明其它点型或板卡性能。 |
| `PointXYZILayoutMatchesStandardPath` | `PointXYZI`，乱序 `indices_` | public entry dispatch + Standard 对拍 | `PointXYZI` 输出与 Standard path 一致。 | traits gate（字段特征门控）不是 exact `PointXYZ`。 | 不证明 dedicated board performance。 |
| `AdditionalAoSPointTypesMatchStandardPath` | `PointXYZRGB`、`PointXYZRGBA`、`PointXYZINormal`，乱序 `indices_` | public entry dispatch + Standard 对拍 | 三个代表点型输出与 Standard path 一致。 | 更多 registered xyz AoS 点型的 shuffled correctness。 | 不证明所有自定义点型或这些点型的性能。 |
| `CloudOnlyIdentityIndicesMatchStandardPath` | `PointXYZ`，不显式设置 indices | public entry dispatch + Standard 对拍 | 默认整云路径与 Standard path 一致。 | identity indices 下 select/count fast path 的 correctness。 | 不证明 identity fast path 对所有入口都有收益。 |
| `ExplicitEmptyIndicesMatchStandardPath` | `PointXYZ`，显式空 `indices_` | public entry dispatch + Standard 对拍 | inliers、count、distances 和 error buffer 都为空，且不会保留旧输出。 | 显式空子集的 public entry correctness。 | 不证明默认整云 identity，也不证明性能。 |
| `AdditionalAoSPointTypesIdentityMatchStandardPath` | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZINormal`，不显式设置 indices | public entry dispatch + Standard 对拍 | 代表点型 identity 输出与 Standard path 一致。 | select/count identity strided load 在更多 AoS 点型上正确。 | 不证明这些点型的板卡性能。 |
| `SelectHelperResizesEmptyOutputBuffers` | `PointXYZ`，空输出 buffer | `selectWithinDistanceStandard` 和 `selectWithinDistanceRVV` | helper 直接调用时能 resize 后写回。 | protected helper buffer 合同。 | 不证明公开入口以外的模型。 |

## 验证命令

```bash
make -C test-rvv/sample_consensus/sac_model_plane run_test_compare
```

该命令在 Std 和 RVV 构建中各运行 7 个测试。QEMU 只提供 correctness（正确性）和日志形状证据。

板卡 RVV correctness smoke：

```bash
SSH_AUTH_SOCK=<agent-socket> make -C test-rvv/sample_consensus/sac_model_plane run_board_base_plane_public_tests REMOTE_USER=<board-user> REMOTE_IP=<board-ip>
```

该命令运行 7 个 RVV gtest 并全部通过。板卡输出中的 Makefile clock skew（时钟偏差）是环境
warning，不是 correctness failure（正确性失败）。
