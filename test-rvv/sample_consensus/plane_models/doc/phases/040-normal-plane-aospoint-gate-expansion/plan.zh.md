# Normal-plane AoS 点类型 Gate 扩展 Phase Plan

## 阶段意图和边界

本阶段把 normal-plane 三条公开入口的 RVV dispatch（分流逻辑）从字段语义 gate 收紧为 AoS（结构数组）字节偏移 gate，并补代表性点类型测试。目标是让生产入口的判断与 `pcl::rvv_load::byte_offsets_u32m2`、`indexed_load3_fields_f32m2` 和单字段 gather 的实际前提一致。

本阶段只覆盖：

| 维度 | 本阶段范围 |
| --- | --- |
| public entry（公开入口） | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` |
| row source（行来源） | 当前 `indices_` ordered index（按 `indices_` 顺序访问输入点和法线） |
| source 点类型 | `PointXYZ`、`PointXYZI`、`PointXYZINormal` 代表性 `RVVXYZAoSFloatLayout<PointT>` |
| normal 点类型 | `Normal`，即 registered single-float normal + curvature |
| fallback（回退路径） | non-AoS source layout、curvature 非单 float normal layout |
| 不覆盖 | `Scalar=double`、新的 RVV math approximation family（数学近似实现族）、新的 row source、未注册自定义点类型全集 |

## 当前状态清单

| area | 当前状态 | 路径 |
| --- | --- | --- |
| production RVV helper | 已有 `selectWithinDistanceRVV`、`countWithinDistanceRVV`、`getDistancesToModelRVV`，helper 使用 byte offset gather。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` |
| dispatch gate | 当前公开入口使用 `RVVXYZFloatLayout<PointT>` + `RVVNormalFloatLayout<PointNT>` + curvature 单 float gate；source 侧未在 dispatch 层要求 POD / standard-layout / stride alignment。 | 同上 |
| helper contract | `rvv_point_load` 的 byte offset 和 `indexed_load3_fields_f32m2` 对 `PointT` 有 standard-layout / aligned field offset 前提。 | `common/include/pcl/impl/rvv_point_load.hpp` |
| existing evidence | Phase 030 重复板卡证据已证明 `PointXYZ + Normal` protected helper hot path 为 positive-stable。 | `030-normal-plane-repeated-board-summary/result.zh.md` |
| roadmap gap | `stronger AoS layout gate` 仍为 planned；泛型点类型扩展此前被写成需要确认。 | `doc/optimization-roadmap.zh.md` |

## 假设与候选族

候选族是 `source AoS layout dispatch gate`：公开入口改用 `RVVXYZAoSFloatLayout<PointT>`，normal 侧继续要求 normal/curvature registered single-float 字段，并补 `PointXYZI`、`PointXYZINormal` 代表性 public-entry 测试。`PointT` 只被读取 `x/y/z`，不会构造完整 `PointT` 输出，因此 `intensity` 或 source normal 字段不参与本阶段输出语义。

## 优化矩阵

| candidate family | row source | point type / layout | correctness / fallback target | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| AoS source dispatch gate | ordered `indices_` | `PointXYZ + Normal` | 复用现有 public tests | 复用 Phase 030 repeated summary | 复用 helper asm | 复用 Phase 030 doctor | should remain adopted |
| AoS source dispatch gate | ordered `indices_` | `PointXYZI + Normal` | 新增 public-vs-direct RVV test | board public test；必要时板卡 full test | compile/path evidence | 不新增性能 doctor | planned |
| AoS source dispatch gate | ordered `indices_` | `PointXYZINormal + Normal` | 新增 public-vs-direct RVV test | board public test；必要时板卡 full test | compile/path evidence | 不新增性能 doctor | planned |
| non-AoS source fallback | ordered `indices_` | registered xyz single-float but non-standard-layout source + `Normal` | 新增 compile/public fallback test，证明 RVV 构建不实例化 helper static_assert | not_applicable | not_applicable | not_applicable | planned |
| double curvature fallback | ordered `indices_` | `PointXYZ + NormalWithDoubleCurvature` | 复用并保留现有 fallback test | not_applicable | not_applicable | not_applicable | should remain adopted |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 RED 测试 | 在 `src/test_sample_consensus_plane_models.cpp` 增加 non-AoS source fallback 和代表点型 public-vs-direct 测试。 | RVV 构建下现有 production dispatch 触发 non-AoS helper static_assert 或代表点型覆盖缺口。 |
| A2 production gate | 把三条公开入口和 RVV helper 内 `PointLayout` 切到 `RVVXYZAoSFloatLayout<PointT>`，并保留 normal / curvature gate。 | RED 测试转绿，`PointXYZ` 既有测试不退化。 |
| A3 运行验证 | `run_normal_plane_public_tests`、`run_test_compare`；板卡可用时跑 `run_board_normal_plane_public_tests`。 | 本地/QEMU correctness 通过，板卡 public/fallback alias 通过。 |
| A4 文档同步 | phase result、roadmap、matrix、evaluation、optimization evidence、长期 `doc-rvv` 和 queue。 | 文档不再把当前范围写成只覆盖 `PointXYZ + Normal`，同时不外推到自定义点型全集或 `Scalar=double`。 |

## Evidence Doctor 与 registry

本阶段的新增证据以 correctness / fallback 为主，不新增性能 summary。Phase 030 repeated board summary 继续作为 `PointXYZ + Normal` helper hot path 性能证据；若本阶段重跑或覆盖 board performance summary，必须重新执行 `record_repeated_board_evidence_state` 和 `repeated_evidence_status`。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public correctness / fallback；Phase 030 仍是 production-shaped diagnostic performance。 |
| A/B boundary | public overload vs direct RVV helper for supported AoS types；public overload vs Standard helper for fallback type。 |
| 当前决策问题 | fallback correctness and implementation-shape。 |
| diagnostic 是否可外推到 production | correctness 直接来自 public entry；性能只沿用 `PointXYZ + Normal` protected helper hot path，不外推到新增点型性能。 |
| comparison-boundary / baseline mismatch 风险 | 新增测试不作性能比较；只证明 public dispatch、direct RVV 输出一致和 fallback 可编译可运行。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | not_applicable；本阶段不做新性能 probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；没有选择新 RVV family。 |

## 完成条件和停止条件

本阶段完成需要同时满足：

- RED/GREEN 记录能说明测试先于 production gate 修改。
- `PointXYZ` 既有公开入口测试继续通过。
- `PointXYZI` 和 `PointXYZINormal` 代表性 public entry 与 direct RVV helper 输出一致。
- non-AoS source layout 在 RVV build 下不实例化 RVV helper，并保持 public Standard fallback。
- 文档和 matrix 把新增范围写成 representative AoS source expansion（代表性 AoS source 扩展），不写成完整泛型点类型全集。

真实停止条件：

- non-AoS 注册点类型无法可靠构造可编译 fallback test；
- production gate 修改导致现有 `PointXYZ + Normal` QEMU / board correctness 退化；
- 板卡不可达或工具链失败，且本地/QEMU 证据已完成；
- 继续需要扩大到 `Scalar=double`、新 normal layout、未注册点型全集或新的性能实现族。

## roadmap 同步动作

本阶段结束后，将 `stronger AoS layout gate` 从 planned 改成 adopted / attempted，并新增或刷新 `point_type_expansion_queue`：

- adopted：`PointXYZ`、`PointXYZI`、`PointXYZINormal` source + `Normal`。
- deferred：更多 registered xyz AoS source 点型、自定义 AoS layout 采样、`PointNT` 的其它 normal-like layout、`Scalar=double`。
- not_applicable：本阶段不改变 normal-plane 数学近似实现族。
