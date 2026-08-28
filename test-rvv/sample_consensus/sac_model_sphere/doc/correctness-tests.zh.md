# sac_model_sphere correctness tests

## 本文职责

本文解释 `src/test_sac_model_sphere.cpp` 中的 correctness（正确性）测试。它不承担性能结论；Phase 020
后只有 `ProductionSelectWithinDistanceMatchesStandardHelper` 证明 `selectWithinDistance` 的 production
dispatch（生产分流），测试专用 candidate（候选实现）仍只作为诊断证据。

## 测试文件分工

| 文件 / 符号 | 职责 |
| --- | --- |
| `src/test_sac_model_sphere.cpp` | gtest 入口和 main；Phase 055 后不再承载共享 helper。 |
| `include/test_sac_model_sphere.h` | fixture（测试夹具）、系数构造和断言 helper 聚合入口。 |
| `include/impl/sac_model_sphere_access.hpp` / `SampleConsensusModelSphereAccess` | 测试专用派生类，暴露 Standard / RVV helper，并提供 select/getDistances candidate。 |
| `run_test_compare` | 分别运行 Std/RVV 构建，证明 `__RVV10__` 开关和 production select 分流不改变公开语义。 |

## 共同输入和断言

测试构造球心、半径、threshold 和 shuffled `indices_`，覆盖球壳内边界、外边界、乱序索引和非 exact `PointXYZ` layout。断言关注：

- inliers 顺序与公开入口一致；
- `error_sqr_dists_` 和 `distances` 与 reference 近似一致；
- count 和 production select 结果与 public entry / RVV helper 一致；
- `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` 的 x/y/z offset 不应被错误写成 exact `PointXYZ`。

## TEST 字典

| TEST | 默认 target | 输入 | 被测路径 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `PublicEntriesMatchReferenceOnShellBoundaries` | `run_test_compare` | `PointXYZ`，覆盖 shell 内外边界和乱序 indices | public select/count/getDistances、reference helper、RVV count helper | shell 双边界 `<=` / `>=`、有序输出和 count 语义一致。 | 不证明未测试点型或其它规模的性能。 |
| `PointXYZILayoutMatchesReference` | `run_test_compare` | `PointXYZI`，带 intensity 但算法只读 x/y/z | public entries 和测试专用 candidate | 当前 traits/offset 读取不只适用于 exact `PointXYZ` correctness。 | 不证明所有 PointXYZ-like 点型，也不证明 dedicated board performance。 |
| `PointXYZRGBAndRGBALayoutsMatchReference` | `run_test_compare` | `PointXYZRGB` / `PointXYZRGBA`，颜色字段不参与算法 | public entries 和测试专用 candidate | Phase 060 证明内建 RGB/RGBA registered x/y/z float layout correctness。 | 不证明自定义 registered xyz 点型或所有规模的性能。 |
| `DiagnosticCandidateMatchesPublicEntries` | `run_test_compare` | `PointXYZ` direct indexed 输入 | 测试专用 select/getDistances candidate vs public entry | candidate 的公式、gather 和输出合同与 public entry 对齐。 | Phase 020 后不再作为 select 生产采纳主证据；仍不证明 getDistances production。 |
| `ProductionSelectWithinDistanceMatchesStandardHelper` | `run_test_compare` | `PointXYZ` direct indexed 输入 | public `selectWithinDistance`、`selectWithinDistanceStandard`、`selectWithinDistanceRVV` | RVV 构建下 public entry 命中 production RVV helper，并与 Standard helper 保持 inliers 和 `error_sqr_dists_` 一致；Std 构建覆盖自然 fallback。 | 不证明 `getDistancesToModel`，也不证明更多点型的板卡性能。 |
| `VCompressSelectCandidateMatchesStandardHelper` | `run_test_compare` | `PointXYZ` direct indexed 输入 | test-only `selectWithinDistanceVCompressCandidate` vs Standard helper | 保留 `vcompress` 候选的直接对拍，防止测试支撑迁移破坏候选输出顺序。 | Phase 045/046 production 采纳仍以 production direct board evidence 为准。 |

## 缺口和下一步

当前 production correctness 仍未覆盖自定义 registered xyz 点型、非 AoS layout、`Scalar=double`
或超大 cloud 的 32-bit byte offset fallback。若继续扩大范围，需要先定义代表点型、字段 offset、
规模和 fallback 断言，再进入新的 point type / fallback expansion phase。
