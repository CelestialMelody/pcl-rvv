# marching_cubes_rbf 函数级评估

## S2 函数级评估

`MarchingCubesRBF<PointNT>::voxelizeData()` 先为输入点生成 on-surface / off-surface 两组中心点，
构造 `2N x 2N` RBF（radial basis function，径向基函数）矩阵，再用 Eigen
`fullPivLu().solve()` 求权重，最后对整个体素网格逐点累加 `weight * r^3` 写入 `grid_`。
后续表面生成仍走 `MarchingCubes<PointNT>::performReconstruction()` 和 `createSurface()`。

当前源码的硬边界是 Eigen solve（求解器）和可变三角形输出。已完成的 RVV 阶段只接管
matrix fill（矩阵填充）和 voxel evaluation（体素求值）；Eigen solve、base class active-cell scan
（活动单元扫描）和 surface emission（表面输出）保持既有路径。

## 标量路径拆分

| 阶段 | 源码位置 | 热点形态 | 当前判断 |
| --- | --- | --- | --- |
| center / RHS 准备 | `voxelizeData()` 前半段 | `2N` 点的结构数组读取和法线偏移 | production patch 抽成 SoA centers（数组结构中心点），标量 / RVV 共用同一准备边界。 |
| RBF matrix fill | 双重 `row_i / col_i` 循环 | `4N^2` 次 cubic-distance kernel | 已在 traits-gated normal AoS production path 中用 RVV column chunk 接管。 |
| Eigen solve | `M.fullPivLu().solve(d)` | 动态矩阵分解 | 保持标量库边界；本 topic 不替换 solver。 |
| voxel evaluation | 三重体素循环 + centers 内层 | `res_x * res_y * res_z * 2N` 次 kernel + reduction（规约） | 已在 traits-gated normal AoS production path 中用 RVV reduction 接管；用测试容差和 checksum 约束数值边界。 |
| surface emission | base `createSurface()` | active-cell 判断、edge interpolation、可变输出 | 不属于 RBF `voxelizeData()` 接入范围；本 topic 不重复接入。 |

## 诊断到生产的证据演进

`000-current-state-and-component-ablation` 阶段先在 `test-rvv/surface/marching_cubes_rbf/` 中建立
production-shaped diagnostic（生产形态诊断），验证两个 candidate：

- `fillMatrixCandidate()`：按 Eigen column-major（列主序）写入矩阵，并在每列内用 RVV 处理多个 row center。
- `evaluateGridCandidate()`：每个体素点对 centers 做 RVV cubic-distance 和 vector reduction（向量规约）。

该诊断没有修改 production（生产源码），只证明在同一 Eigen solve 和同一 bench wrapper 下，
matrix fill / voxel evaluation 候选能穿透 full pipeline（完整诊断流水）成本。随后
`010-production-integration-pointnormal` 阶段先把同一实现形态接入 exact `PointNormal` production helper；
`020-generic-normal-point-types` 阶段进一步把 production gate 扩展为
`RVVXYZNormalFloatLayout<PointNT>` traits gate（点类型字段特征门控），并用 `PointNormal`、
`PointXYZINormal`、`PointXYZRGBNormal` 三个代表点型完成真实 production direct 证据。

## 当前 production patch 范围

| 项 | 当前行为 |
| --- | --- |
| production 文件 | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` |
| public API（公开接口） | 不变。 |
| 命中条件 | `__RVV10__`、`pcl::rvv::RVVXYZNormalFloatLayout<PointNT>::value`、`input_->size() >= 16`。 |
| RVV 覆盖段 | RBF matrix fill 和 grid evaluation。 |
| 保持标量段 | center 准备、RHS 构造、Eigen `fullPivLu()`、surface emission。 |
| fallback（回退路径） | 非 RVV 构建、不满足 traits gate 的模板实例、小规模输入均走 `marchingCubesRBFVoxelizeDataStandard()`。 |
| 当前状态 | `adopted_production_behavior`，traits-gated normal AoS production path 已确认保留。 |

当前性能证据覆盖 `PointNormal`、`PointXYZINormal`、`PointXYZRGBNormal` 代表点型。traits gate 允许
字段和布局前提成立的 normal-like AoS（结构数组）类型进入 RVV，但本文不把代表点型性能外推为所有
自定义点型的强性能结论。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `MarchingCubesRBF::voxelizeData()` | production public helper | 真实 RBF voxel pipeline；负责 RVV / fallback 分流 | production boundary（生产边界） | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` |
| `marchingCubesRBFVoxelizeDataStandard()` | production Std helper | 标量 fallback 和非覆盖模板实例语义 | fallback coverage（回退覆盖） | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` |
| `marchingCubesRBFVoxelizeDataRVV<PointNT>()` | production RVV helper | traits-gated normal AoS 的 matrix fill + grid evaluation RVV 路径 | production direct path（真实生产路径） | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` |
| `fillMatrixCandidate()` / `evaluateGridCandidate()` | diagnostic candidate | 早期 matrix / voxel candidate 对拍和组件 bench | diagnostic evidence（诊断证据） | `test-rvv/surface/marching_cubes_rbf/include/impl/marching_cubes_rbf_core.hpp` |
| `ProductionProbeRBF<PointT>` | production direct test helper | 通过真实 `voxelizeData()` 构造测试和 bench 输入 | production direct correctness / bench | `test-rvv/surface/marching_cubes_rbf/include/impl/marching_cubes_rbf_core.hpp` |
| `test_marching_cubes_rbf` | correctness gate（正确性验收） | 标量 / RVV helper 对拍、production direct、fallback 覆盖 | QEMU 和板卡 correctness | `test-rvv/surface/marching_cubes_rbf/src/test_marching_cubes_rbf.cpp` |
| `bench_marching_cubes_rbf` | bench wrapper | component diagnostic、full diagnostic 和 production direct 计时 | board performance（板卡性能） | `test-rvv/surface/marching_cubes_rbf/src/bench_marching_cubes_rbf.cpp` |
| `generate_marching_cubes_rbf_evidence_manifest.py` | analysis script | 生成 Evidence Doctor 可读 manifest | Evidence Doctor input（证据体检输入） | `test-rvv/surface/marching_cubes_rbf/script/generate_marching_cubes_rbf_evidence_manifest.py` |

## 生产接入判断

当前状态是 `adopted_production_behavior`。生产证据支持保留当前 traits-gated normal AoS patch；
正式长期文档是 `doc-rvv/surface/marching_cubes_rbf-RVV.zh.md`。

证据摘要：

- QEMU correctness：`make -C test-rvv/surface/marching_cubes_rbf run_test_compare`，Std/RVV 各 7/7 通过。
- QEMU bench smoke：`mcrbf_prod_pointnormal_n24_r18` 可运行并输出 checksum；该计时只证明日志形状，不用于性能结论。
- asm attribution（反汇编归属）：RVV bench binary 中存在 `PointNormal`、`PointXYZINormal`、
  `PointXYZRGBNormal` 三个 `marchingCubesRBFVoxelizeDataRVV` 实例，并可见 `vle64/vse64/vfmacc/vfsqrt/vfredusum`
  等 double RVV 指令。
- board correctness（板卡正确性）：`make -C test-rvv/surface/marching_cubes_rbf run_board_test fetch_board_logs`，
  RVV gtest 7/7 通过。
- board production bench（板卡生产路径性能）：`PointNormal` 三个 regression case 为 `1.06x-1.08x`；
  `PointXYZINormal` 两个新增 case 为 `1.06x-1.08x`；`PointXYZRGBNormal` 两个新增 case 为 `1.07x-1.08x`。
- Evidence Doctor（证据体检）：`test-rvv/surface/marching_cubes_rbf/log/board/evidence_doctor.md`
  为 `Errors=0, Warnings=12, Suggestions=0`；warning 均为 `low_run_count`。

`low_run_count` 的处理：bench 内部使用 `iterations=5`、`warmup=2`，但 manifest 只有每个 case 的
summary speedup，没有逐次 B/A values（候选相对基线收益序列），因此当前生产证据是 weak-positive（弱正向）
而不是强稳定结论。该弱收益满足本阶段接入门槛：实现小、public API 不变、fallback 简单、代表点型
production direct case 同向为正。

## 板卡性能结果

| case | Std ms | RVV ms | speedup | 证据角色 |
| --- | ---: | ---: | ---: | --- |
| `mcrbf_matrix_fill_n24` | 0.0703 | 0.0279 | 2.52x | component diagnostic |
| `mcrbf_matrix_fill_n40` | 0.2001 | 0.0824 | 2.43x | component diagnostic |
| `mcrbf_full_pipeline_n24_r18` | 5.4230 | 4.3331 | 1.25x | production-shaped diagnostic |
| `mcrbf_full_pipeline_n40_r20` | 13.0415 | 10.5637 | 1.23x | production-shaped diagnostic |
| `mcrbf_full_pipeline_n56_r18` | 16.8196 | 14.2030 | 1.18x | production-shaped diagnostic |
| `mcrbf_prod_pointnormal_n24_r18` | 4.7078 | 4.3612 | 1.08x | production direct |
| `mcrbf_prod_pointnormal_n40_r20` | 11.4994 | 10.7491 | 1.07x | production direct |
| `mcrbf_prod_pointnormal_n56_r18` | 15.1841 | 14.3598 | 1.06x | production direct |
| `mcrbf_prod_pointxyzinormal_n24_r18` | 4.7060 | 4.3499 | 1.08x | production direct |
| `mcrbf_prod_pointxyzinormal_n40_r20` | 11.4824 | 10.7908 | 1.06x | production direct |
| `mcrbf_prod_pointxyzrgbnormal_n24_r18` | 4.7032 | 4.3558 | 1.08x | production direct |
| `mcrbf_prod_pointxyzrgbnormal_n40_r20` | 11.5031 | 10.7386 | 1.07x | production direct |

## 正确性与高效性证据链

当前 production direct evidence（真实生产路径证据）证明：traits-gated normal AoS 点型、RVV 构建、
`input_->size() >= 16` 的真实 `voxelizeData()` 路径可以命中 RVV matrix fill / grid evaluation。
在 Milkv-Jupiter 板卡上，三个代表点型得到 `1.06x-1.08x` 的弱正向收益。QEMU 和板卡 gtest
证明当前测试输入下输出与标量参考一致；反汇编证明 RVV 指令存在并能定位到 production helper 符号附近。

该证据不能证明：

- 所有自定义 normal-like traits 点型都有相同性能收益。
- `input_->size() < 16` 值得命中 RVV；当前明确 fallback。
- Eigen solve 或 surface emission 已被 RVV 优化。
- 没有逐次 B/A values 时的强稳定性；当前只写 weak-positive。

## 后续计划

当前没有新的、同一 topic 内值得直接继续接入源码的 RVV 实现方向。`center` 准备成本较小，Eigen
`fullPivLu()` 属于 solver 边界，surface emission 由基类流程处理，继续改动会扩大到非本阶段授权的
算法或非 RVV topic。

若 reviewer 要求更强稳定性，再做 `030-production-repeated-values-summary`，让 analyzer 输出逐次 B/A values
和环境 metadata，消除当前 `low_run_count` 警告。若有具体自定义 normal-like 点型需要覆盖，应另开
follow-up，以该点型补 compile smoke、production direct correctness、board bench 和 Evidence Doctor。
