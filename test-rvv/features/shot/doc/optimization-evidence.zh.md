# SHOT optimization evidence

## 本文职责

本文索引已经尝试的 RVV candidate family（候选族）、对应代码路径、证据和取舍。Roadmap（路线图）负责下一步搜索空间；本文只记录已经发生的证据事实。

## 当前结论摘要

| status | candidate |
| --- | --- |
| partial-production-candidate | normalization component；color LAB arithmetic 仅限 arithmetic-only；shape-bin indexed gather 的 component 证据仍正向但已被 PI2 production-public 负向证据降级。 |
| attempted / unstable | interpolation bin-selection scalar-tail staging。 |
| attempted / neutral / not recommended | interpolation geometry staging arrays；indexed RGB/LUT scalar staging。 |
| rollback/no-production | PI2 shape-bin indexed production patch 已按用户确认回滚；production-detail 1.07x 但 public 0.98x / 0.99x。 |
| not_applicable | production topic doc，当前无 adopted production behavior。 |

## 优化方式总表

| candidate family | code path | test / bench | board evidence | asm evidence | decision |
| --- | --- | --- | --- | --- | --- |
| public fixed-LRF scaffold | `src/test_shot.cpp` public tests；`src/bench_shot.cpp` public cases | `run_test_compare`、`public_shot352_fixed_lrf`、`public_shot1344_fixed_lrf` | about 1x | binary-level only | attempted scaffold |
| normalization component | `include/impl/shot_normalize.hpp` | `normalize_352_component`、`normalize_1344_component` | 1.49x-1.63x | reduction / load / store RVV visible | partial-production-candidate component |
| shape-bin SoA | `include/impl/shot_shape_bin.hpp` | `shape_bin_component` | 2.38x-2.47x | dot / finite mask RVV visible | partial-production-candidate component |
| shape-bin AoS | `include/impl/shot_shape_bin.hpp` | `shape_bin_aos_component` | 1.76x-1.90x | stride load RVV visible | partial-production-candidate component |
| shape-bin indexed gather | `include/impl/shot_shape_bin.hpp` | `shape_bin_indexed_component` | 1.65x-1.86x | indexed gather RVV visible | component positive; superseded by PI2 production probe |
| shape-bin indexed production probe | `features/include/pcl/features/impl/shot.hpp` | `production_shape_bin_direct`、`public_shot352_fixed_lrf`、`public_shot1344_fixed_lrf` | detail 1.07x, public 0.98x / 0.99x | PI2 historical production helper had `vluxei32.v` / `vcpop.m` | rollback/no-production |
| interpolation geometry staging | `include/impl/shot_interpolate.hpp` | `interpolation_geometry_component` | 0.97x after fix | projection / sqrt RVV visible | attempted, not recommended |
| interpolation bin-selection scalar-tail | `include/impl/shot_interpolate.hpp` | `interpolation_bin_selection_component` | 0.84x、1.12x、1.17x | residual / center-weight RVV arithmetic visible | attempted / unstable |
| color LAB distance | `include/impl/shot_color.hpp` | `color_lab_distance_component` | 1.10x-1.23x | abs / clamp / convert RVV visible | arithmetic-only partial candidate |
| color RGB/LUT indexed staging | `include/impl/shot_color.hpp` | `color_rgb_lut_indexed_component` | 1.02x | LAB arithmetic visible; LUT scalar | attempted / neutral-weak |

## 标量路径与 RVV 路径差异

Normalization 和 color LAB arithmetic 是顺序数组批处理；shape-bin 从 SoA 扩展到 AoS / indexed gather 后仍保留收益。Interpolation 两个尝试都依赖 staging arrays（暂存数组）或 scalar-tail staging（标量尾段暂存），它们没有覆盖 histogram scatter，且收益不足或不稳定。

## 代码级证据索引

| evidence object | path |
| --- | --- |
| production boundary | `features/include/pcl/features/impl/shot.hpp` |
| test aggregator | `test-rvv/features/shot/include/shot.h` |
| fixtures | `test-rvv/features/shot/include/impl/shot_fixtures.hpp` |
| normalization helper | `test-rvv/features/shot/include/impl/shot_normalize.hpp` |
| shape-bin helper | `test-rvv/features/shot/include/impl/shot_shape_bin.hpp` |
| interpolation helper | `test-rvv/features/shot/include/impl/shot_interpolate.hpp` |
| color helper | `test-rvv/features/shot/include/impl/shot_color.hpp` |
| correctness source | `test-rvv/features/shot/src/test_shot.cpp` |
| bench source | `test-rvv/features/shot/src/bench_shot.cpp` |
| manifest wrapper | `test-rvv/features/shot/script/generate_shot_evidence_manifest.py` |

## 结论边界

Component positive（组件正向）只能作为 production probe（生产探针）输入，不等于 production adopted（生产已采用）。PI2 曾把 shape-bin indexed gather 接入 `shot.hpp`，但 production-public 证据不支持采纳；PI3 已按用户确认回滚。当前 production path 是标量 / 既有 PCL 实现，本 topic 收口为 diagnostic / no-production。
