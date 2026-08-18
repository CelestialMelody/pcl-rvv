# 正确性测试说明

## 本文职责

本文解释 `src/test_tedq.cpp` 中主要 gtest 的语义、输入、被测路径和失败含义。

## 共同输入和断言

测试使用 deterministic corpus（确定性样本集）构造 source cloud，再用固定 4x4 刚体矩阵生成
target cloud。矩阵断言逐元素比较；RVV reduction tree（规约树）与标量顺序不同，因此使用误差预算而不是逐位相同。

## TEST / 测试族字典

| TEST / 测试族 | 被测路径 | 证明范围 | 不能证明 |
| --- | --- | --- | --- |
| `ScalarReferenceMatchesPublicOrderedCloudPair` | public ordered entry 与 test-only scalar reference | reference 复刻 production C1/C2 + Eigen solve 语义 | RVV 或性能 |
| `CandidateMatchesScalarOrderedCloudPair` / `SmallInputFallsBack` | test-only ordered RVV candidate / fallback | ordered candidate 数值一致，小输入不强行走 RVV | production dispatch |
| `PointXYZILayoutMatchesScalar` / public layout gate tests | extra-field xyz AoS layout | `PointXYZI`、`PointXYZRGB` 不污染 x/y/z 语义，layout gate 不静默阻断 | 完整泛型点型性能 |
| public identity / non-identity row-source tests | source-indexed、dual-indexed、correspondence public scalar semantics | row pairing 与 staged scalar reference 对齐 | RVV path-hit 或性能 |
| staged / direct gather diagnostic tests | source-indexed、dual-indexed、correspondence test-only candidates | diagnostic candidate 与 scalar reference 一致 | production direct |
| correspondence direct index stream / segment / locality / point-type tests | test-only correspondence variants | correspondence 诊断候选的语义边界 | 当前 production dispatch |
| `ProductionRVVOrderedCloudPairPathHitMatchesScalar` | production detail ordered RVV helper | ordered retained RVV helper 命中且与 scalar reference 一致 | source / dual / correspondence |
| `ProductionRVVSourceIndexedCloudPairPathHitMatchesScalar` | production detail source-indexed RVV helper | source-indexed retained helper 命中且与 scalar reference 一致 | ordered / dual / correspondence |
| `ProductionRVVDualIndexedCloudPairPathHitMatchesScalar` | production detail dual-indexed RVV helper | dual-indexed retained helper 命中且与 scalar reference 一致 | correspondence |
| `ProductionRVVFallbackGatesRejectOutOfScope` | production fallback gates | 小规模、非 dense、`Scalar=double` 不走 retained RVV | 每一种 unsupported layout 的穷举 |

## 验证命令

```bash
make run_test_compare
```

本轮结果：

- `log/qemu/run_test_std.log`：28 tests passed。
- `log/qemu/run_test_rvv.log`：32 tests passed。

RVV 构建多出的 4 个测试来自三类 retained production path-hit 和 fallback gate。
correspondence production path-hit 已删除；correspondence 仍有 public scalar compatibility 和 test-rvv diagnostic tests。
