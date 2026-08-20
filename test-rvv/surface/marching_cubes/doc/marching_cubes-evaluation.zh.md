# marching_cubes 函数级评估

## S2 函数级评估

`pcl::MarchingCubes<PointNT>::performReconstruction()` 公开入口先检查 `iso_level_`，创建 `grid_`，计算 bounding box 和 voxel size，再调用子类 `voxelizeData()` 填充 signed distance grid。随后三重循环扫描内部 voxel cell，对每个 cell 读取 8 个邻点值，跳过 NaN，再进入 `createSurface()` 输出三角点和 polygon 索引。

本 topic 的首个 RVV 候选只覆盖 `createSurface()` 中的 active cell edge interpolation（边插值）：根据 cube index 查 `edgeTable`，对最多 12 条 edge 计算 `p1 + mu * (p2 - p1)`，其中 `mu = (iso_level - val_p1) / (val_p2 - val_p1)`。第二个候选 `cube-index-prepass` 则上移到 neighbor gather / active cell 判断：先批量判断 `leaf < iso_level_`、NaN skip 和 `edgeTable[cubeindex] != 0`，再把 active cell 交给标量 surface emission。

## 标量路径和热点边界

| 阶段 | production 行为 | RVV 机会 | 本阶段状态 |
| --- | --- | --- | --- |
| grid allocation / bounding box | 每次 reconstruct 初始化状态 | 不优先优化 | 保持标量 |
| `voxelizeData()` | Hoppe 使用 nearestKSearch，RBF 有 matrix fill / solve | 子类独立 topic 或后续消融 | 不计入 Phase 000 |
| neighbor gather | 每个 cell 读取 8 个 grid 值并跳过 NaN | 可做 cube-index-prepass | Phase 010 strong-positive diagnostic |
| `createSurface()` edge interpolation | active cell 定长 edge 表和 xyz 插值 | Phase 000 test helper RVV 候选 | attempted / not production candidate |
| triangle output | 按 tri table push 3 点和 polygon | 可变长度 append，标量更清晰 | 保持标量 |

## S11 Closeout

当前结论已从 `diagnostic` 推进到 `adopted generic production`。Phase 000 已证明 edge interpolation RVV 候选 correctness 通过且反汇编可见 RVV 指令，但板卡 repeated 只有 `0.992x/1.024x/1.049x`，Evidence Doctor 报告 1 个退化 Error，因此不接入生产。Phase 010 的 `cube-index-prepass` 在 helper 边界上达到 `2.431x/2.681x/3.217x`，Evidence Doctor 无 Error。Phase 030 先把该思路接到 `PointNormal` / `float` synthetic production direct 边界，随后 Phase 050 已把生产 gate 放宽为 `pcl::rvv::RVVXYZAoSFloatLayout<PointNT>`，并用四个代表点型的 5-run board repeated 闭合当前 generic 接入证据。Phase 060 尝试 active-z finite-collapse single-buffer，但 RVV-vs-RVV median 只有 `1.007x`，bucket 为 neutral，因此不改变当前 production truth。

## Traceability Map

| artifact | role | path |
| --- | --- | --- |
| production source | RVV generic production path + scalar fallback | `surface/include/pcl/surface/impl/marching_cubes.hpp` |
| public declarations / tables | edgeTable / triTable / API | `surface/include/pcl/surface/marching_cubes.h` |
| topic Makefile | QEMU / board build harness | `test-rvv/surface/marching_cubes/Makefile` |
| test support aggregator | stable include entry | `test-rvv/surface/marching_cubes/include/marching_cubes.h` |
| correctness test | Std/RVV helper 对拍 | `test-rvv/surface/marching_cubes/src/test_marching_cubes.cpp` |
| bench | helper timing and checksum | `test-rvv/surface/marching_cubes/src/bench_marching_cubes.cpp` |
| phase plan / result | phase loop evidence | `test-rvv/surface/marching_cubes/doc/phases/000-current-state-and-edge-interpolation-diagnostic/`、`test-rvv/surface/marching_cubes/doc/phases/010-cube-index-prepass-diagnostic/`、`test-rvv/surface/marching_cubes/doc/phases/020-structure-parity-doc-suite-and-production-boundary-audit/`、`test-rvv/surface/marching_cubes/doc/phases/030-pi1-production-integration-plan/`、`test-rvv/surface/marching_cubes/doc/phases/040-production-stabilization-and-point-type-expansion-audit/`、`test-rvv/surface/marching_cubes/doc/phases/050-generic-point-type-expansion-audit/`、`test-rvv/surface/marching_cubes/doc/phases/060-active-z-tail-table-lookup-compression-ab/` |
| optimization roadmap | candidate frontier | `test-rvv/surface/marching_cubes/doc/optimization-roadmap.zh.md` |
| optimization matrix | evidence status | `test-rvv/surface/marching_cubes/doc/phases/optimization-matrix.zh.md` |
| production long-term doc | adopted production behavior | `doc-rvv/surface/marching_cubes-RVV.zh.md` |

## 文档归属矩阵

| 内容 | 主归属 | 备注 |
| --- | --- | --- |
| 函数职责、标量路径、初步接入判断 | 本 evaluation | 离开对话后可恢复 S2 状态。 |
| 阶段计划、命令、证据结果 | phase plan/result | result 完成后回填。 |
| benchmark 数值和 doctor finding | output summary / phase result | 不把 raw log 复制进长期文档。 |
| production 长期行为 | `doc-rvv/surface/marching_cubes-RVV.zh.md` | 记录已采纳的 generic `RVVXYZAoSFloatLayout<PointNT>` production 行为和未覆盖范围。 |

## 当前证据状态

Phase 000 EvidenceDecision 是 `attempted / not production candidate`：测试专用 edge interpolation RVV 候选不支持 production probe。Phase 010 已结束，`cube-index-prepass` 在 helper 边界上强正向。Phase 020 已补齐 topic-local doc suite，并冻结 `performReconstruction()` 的生产边界。Phase 030/040 的 narrow `PointNormal` production adoption 现在作为历史锚点保留：`production_direct_repeated` 为 5-run positive，median `5.330x/6.386x/8.824x`。Phase 050 已完成 generic gate 接入验证：QEMU Std/RVV 各 6 tests passed，`PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 5-run board repeated 分别为 `3.873x`、`3.618x`、`3.601x`、`3.639x`，Evidence Doctor 均 `Errors=0`、`Warnings=0`、`Suggestions=0`。Phase 060 作为 RVV-family-selection 负向/中性证据保留：finite-collapse single-buffer correctness 通过且 asm 形态更轻，但 `mc_prod_xyz_64` RVV-vs-RVV 3-run median 只有 `1.007x`，Evidence Doctor `Errors=0, Warnings=1, Suggestions=1`，候选已回退。
