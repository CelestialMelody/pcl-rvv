# SHOT test-support code map

## 本文职责

本文用于定位测试支撑代码、bench wrapper（性能测试包装层）、script（脚本）和 production 对照关系。它不承担性能结论，也不把 test-only helper 写成 production helper。

## 总调用图

```text
features/include/pcl/features/impl/shot.hpp
  -> public SHOT / SHOTColor production boundary

test-rvv/features/shot/src/test_shot.cpp
  -> include/shot.h
      -> impl/shot_fixtures.hpp
      -> impl/shot_normalize.hpp
      -> impl/shot_shape_bin.hpp
      -> impl/shot_interpolate.hpp
      -> impl/shot_color.hpp

test-rvv/features/shot/src/bench_shot.cpp
  -> include/shot.h
  -> stdout case lines
      -> script/generate_shot_evidence_manifest.py
      -> ../../script/evidence_doctor.py

features/include/pcl/features/impl/shot.hpp
  -> createBinDistanceShape scalar production path after PI3 rollback
  -> test-rvv/features/shot/src/test_shot.cpp historical production-detail tests
  -> test-rvv/features/shot/src/bench_shot.cpp production_shape_bin_direct historical case
```

## 稳定聚合入口

| 入口 | 路径 | 角色 |
| --- | --- | --- |
| test executable | `src/test_shot.cpp` | correctness aggregate。 |
| bench executable | `src/bench_shot.cpp` | diagnostic bench cases。 |
| aggregator header | `include/shot.h` | 对外隐藏内部 helper 文件拆分。 |
| Makefile | `Makefile` | 本地 / QEMU / doctor target 参数。 |
| board config | `board.mk` | 板卡侧 binary 名和 remote dir。 |

## Fixtures 与输入构造

`shot_fixtures.hpp` 生成 SHOT public smoke 所需的点云、normal、reference frame 和 descriptor 检查 helper。Bench source 另有 case-specific batch generator，用于构造 normalization、shape-bin、interpolation 和 color component 的输入规模。

## 标量 Reference

| helper | 关系 | 不能证明 |
| --- | --- | --- |
| `normalizeDescriptorScalar` | test-only same-chain reference。 | production `normalizeHistogram` 已修改或命中 RVV。 |
| `computeShapeBinDistance*Scalar` | 复刻 shape-bin dot / clamp / NaN count 片段。 | production `PCL_WARN` 副作用和完整 public dispatch。 |
| `computeInterpolationGeometryIndexedScalar` | 复刻 geometry projection / distance staging 片段。 | 完整 interpolation scatter。 |
| `computeInterpolationBinSelectionScalar` | 复刻 bucket selection 局部状态。 | `acos` / `atan2`、半径 / 倾角 / 方位角完整邻接写入。 |
| `computeColor*Scalar` | 复刻 LAB arithmetic 或 indexed RGB/LUT staging。 | `std::vector::push_back` 和完整 color interpolation。 |

## Candidate / Diagnostic Helper

| helper | 文件 | 状态 |
| --- | --- | --- |
| `normalizeDescriptorRVV` | `shot_normalize.hpp` | positive component。 |
| `computeShapeBinDistanceRVV` | `shot_shape_bin.hpp` | positive SoA component。 |
| `computeShapeBinDistanceAoSRVV` | `shot_shape_bin.hpp` | positive AoS component。 |
| `computeShapeBinDistanceIndexedRVV` | `shot_shape_bin.hpp` | positive indexed component；PI2 production probe 已执行但 public negative，PI3 已回滚。 |
| `computeInterpolationGeometryIndexedRVV` | `shot_interpolate.hpp` | attempted; not recommended。 |
| `computeInterpolationBinSelectionRVV` | `shot_interpolate.hpp` | attempted / unstable。 |
| `computeColorBinDistanceRVV` | `shot_color.hpp` | arithmetic-only positive。 |
| `computeColorBinDistanceIndexedRGBRVV` | `shot_color.hpp` | neutral-weak staging。 |

## Bench Harness 与 Case Registry

`bench_shot.cpp` 维护 `--case-filter`、case enable、case output 和 checksum 合同。Case label 必须同步 `script/generate_shot_evidence_manifest.py`，否则 Evidence Doctor 无法读取 role metadata。PI2 新增的 `production_shape_bin_direct` 现在保留为 historical production probe / scalar regression case（历史生产探针 / 标量回归 case）；public cases 仍用于说明接入后为什么不值得保留。

## Scripts 与 Evidence Output

| path | 作用 |
| --- | --- |
| `script/generate_shot_evidence_manifest.py` | 把 board summary / raw log 解析为 topic-specific manifest。 |
| `log/board/board-shot-production-shape-bin-direct-pi2/analyze_bench_compare.log` | PI2 production-detail board summary。 |
| `log/board/board-shot-public-shot352-production-pi2/analyze_bench_compare.log` | PI2 SHOT352 production-public board summary。 |
| `log/board/board-shot-public-shot1344-production-pi2/analyze_bench_compare.log` | PI2 SHOT1344 production-public board summary。 |
| `log/board/analyze_bench_compare.log` | 覆盖式 fetched board summary；不作为 current truth 主入口。 |
| `log/board/evidence_manifest.json` | doctor 输入，生成产物。 |
| `log/board/evidence_doctor.md` | doctor 输出，生成产物。 |
| `build/asm/riscv/bench_shot_rvv.asm` | asm dump，生成产物。 |

## Production 与 Test Support 边界

Production 文件 `features/include/pcl/features/impl/shot.hpp` 已在 PI3 按用户确认回滚，当前不包含 SHOT RVV helper 或 dispatch（分流逻辑）。所有 `test-rvv/features/shot/include/impl/shot_*.hpp` helper 仍是 test-only diagnostic，不被 PCL public API 调用。PI2 public evidence 不支持把该 patch 写成 adopted production behavior。

## 拆分审计

当前结构已采用 `src/`、`include/` 和 `include/impl/`，没有旧 `test_support/` 目录；单个源文件均低于 800 行软阈值。Phase 100 已补齐 target alias 和 evidence registry。PI3 已完成用户确认后的 production 回滚；当前没有未阻塞的测试结构动作。
