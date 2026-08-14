# ICP transformCloud 正确性测试说明

## 本文职责

本文逐项说明 `src/test_icp.cpp` 中 12 个 gtest 的输入、被测路径、断言和证明范围。测试支撑 helper 的
函数地图见 `doc/test-support-code-map.zh.md`，bench 证据见 `doc/benchmark-and-evidence.zh.md`。

## 测试文件分工

| 文件 | 职责 |
| --- | --- |
| `src/test_icp.cpp` | gtest case、断言和 production direct exposed wrapper。 |
| `include/test_icp.h` | gtest 聚合入口，只 include `icp.h`。 |
| `include/icp.h` | test/bench 共用聚合入口。 |
| `include/impl/icp_transform_cloud.hpp` | test-only 标量 reference、diagnostic RVV candidate、fixtures 和 checksum。 |
| `registration/include/pcl/registration/impl/icp.hpp` | 被测 production `IterativeClosestPoint::transformCloud`。 |

## 共同输入和断言

测试使用 `support::transformCloudStd` 作为 reference。该 reference 按运行期 field offset 用 `memcpy`
读取 XYZ 和 optional normal，先检查 XYZ finite gate，再写回 XYZ；若 source 有 normal，再检查 normal
finite gate 并写回 normal。`expectXYZNear` 和 `expectNormalsNear` 对有限值使用 `2e-5f` 误差预算，对
NaN / Inf 使用保留语义断言。

`ExposedICP` 只把 protected `transformCloud` 暴露给测试，不改变 production 调用路径。

## Diagnostic Candidate 测试

| gtest | 输入 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `PointXYZMatchesScalar` | 4096 个 `PointXYZ` | `support::transformCloudCandidate` | stats 与 reference 一致；RVV build 下 `used_rvv=true`；XYZ 近似一致。 | test-only XYZ RVV candidate 的矩阵、finite mask 和 store 语义。 |
| `PointNormalMatchesScalarAndFiniteBranches` | 4096 个 `PointNormal`，注入 XYZ/normal NaN/Inf | `support::transformCloudCandidate` | XYZ / normal 写回计数一致；normal 非有限不回滚 XYZ。 | test-only XYZ+normal 双 mask 语义。 |
| `InPlaceMatchesOutOfPlace` | 2048 个 `PointNormal` | candidate out-of-place 与 in-place | in-place 输出与 out-of-place 一致。 | 同一对象读写不污染后续 lane。 |
| `SmallInputFallsBack` | 17 个 `PointXYZ` | candidate size gate | `used_rvv=false`、`used_fallback=true`、零误差一致。 | `input.size() < 32` 不进入 RVV。 |
| `MismatchedRuntimeOffsetsFallback` | 256 个 `PointXYZ`，交换 x/y offset | candidate runtime offset gate | `used_rvv=false`、`used_fallback=true`、零误差一致。 | 运行期 offset 不匹配时不会误用硬编码 layout。 |

这些 case 是 Phase 001/002 的 diagnostic evidence。production 已接入后，它们继续保护 test support 与
production-shaped helper 的语义一致性，但 production 采纳主要依赖下面的 direct tests。

## Production Direct 测试

| gtest | 输入 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `ProductionDirectPointXYZMatchesScalar` | 4096 个 `PointXYZ` | `IterativeClosestPoint::transformCloud` | production 输出与 reference XYZ 一致。 | 无 normal production RVV 主分支。 |
| `ProductionDirectPointNormalFiniteBranches` | 4096 个 `PointNormal`，注入 XYZ/normal NaN/Inf | production `transformCloud` | XYZ / normal 与 reference 一致；normal 非有限保留 input normal。 | production 双 mask 和 finite gate。 |
| `ProductionDirectPointXYZIGenericXYZGate` | 4096 个 `PointXYZI` | production generic XYZ layout gate | XYZ 与 reference 一致；intensity 保持 input。 | 非 exact `PointXYZ` 的兼容 AoS 点型可进入 RVV，非 transform 字段不被写。 |
| `ProductionDirectPointXYZINormalGenericNormalGate` | 4096 个 `PointXYZINormal`，注入 XYZ/normal 非有限值 | production generic XYZ+normal layout gate | XYZ / normal 与 reference 一致；intensity / curvature 保持 input。 | 非 exact `PointNormal` 的兼容 AoS 点型可进入 RVV。 |
| `ProductionDirectSmallInputFallback` | 17 个 `PointXYZ` | production size gate | production 与 reference 零误差一致。 | 小规模输入保持标量 fallback。 |
| `ProductionDirectScalarDoubleFallback` | 4096 个 `PointXYZ`，`Scalar=double` | production scalar gate | production 与 float-cast reference 零误差一致。 | `Scalar=double` 不进入 RVV，保持上游 cast 行为。 |
| `ProductionDirectInPlaceMatchesOutOfPlace` | 2048 个 `PointNormal` | production in-place | in-place 输出与 out-of-place 一致。 | `cloud_in == cloud_out` 公开语义。 |

## 验证命令

QEMU correctness：

```bash
make -C test-rvv/registration/icp run_test_compare record_qemu_correctness_state
```

板卡 correctness：

```bash
make -C test-rvv/registration/icp run_board_test fetch_board_logs
```

当前记录的结果：

| backend | 结果 | 证据 |
| --- | --- | --- |
| QEMU Std | 12 tests passed | `test-rvv/registration/icp/log/qemu/run_test_std.log` |
| QEMU RVV | 12 tests passed | `test-rvv/registration/icp/log/qemu/run_test_rvv.log` |
| board RVV | 12 tests passed | `test-rvv/registration/icp/log/board/run_test.log` |

## 正确性边界

- Bench checksum 只是路径指纹；正式数值等价由 gtest 负责。
- `PointXYZI` / `PointXYZINormal` 有 production direct correctness；board repeated performance 只用
  `PointXYZ` / `PointNormal` 代表主布局族。
- `IterativeClosestPointWithNormals` 不走本 `transformCloud`，需要另开函数族评估。
- indices / correspondences 不属于 `transformCloud` row source；这些对象在 ICP 主循环其它阶段处理。
