# MarchingCubesRBF RVV 生产接入说明

## 当前状态

`MarchingCubesRBF<PointNT>::voxelizeData()` 已采纳一个 traits-gated normal AoS（由点类型字段特征门控的法线结构数组）
RVV（RISC-V Vector，可变向量扩展）生产路径：在 `__RVV10__` 构建、
`pcl::rvv::RVVXYZNormalFloatLayout<PointNT>::value` 和 `input_->size() >= 16` 同时成立时，
RBF matrix fill（矩阵填充）和 voxel grid evaluation（体素网格求值）使用 RVV；非 RVV 构建、
不满足 traits gate 的模板实例和小规模输入保持标量 fallback（回退路径）。公开 API 不变。

当前采用范围来自 `010-production-integration-pointnormal` 和 `020-generic-normal-point-types`
两个 phase result。`PointNormal` 已由用户确认保留；新增 `PointXYZINormal` 和
`PointXYZRGBNormal` 的接入依据是 020 阶段接入 traits gate 后的 production direct（真实生产路径）
板卡数据。长期结论覆盖 traits gate 本身和三个已测代表点型；不把代表点型性能外推成所有自定义
normal-like 点型的强性能结论。

## 函数语义和标量路径

`voxelizeData()` 使用输入点及其法线构造 RBF（radial basis function，径向基函数）隐式场。
对每个输入点，标量路径生成两个 center（中心点）：原点和沿法线偏移 `off_surface_epsilon_`
的 off-surface 点。随后它构造 `2N x 2N` 矩阵，矩阵元素为两 center 间的 cubic-distance kernel：

```text
kernel(c, p) = ||p - c||^3
```

矩阵和右端项交给 Eigen `fullPivLu().solve()` 求 RBF 权重。求得权重后，函数遍历整个体素网格，
对每个网格点累加所有 center 的 `weight * kernel(center, grid_point)`，并把结果写入 `grid_`。
后续 active-cell scan（活动单元扫描）、edge interpolation（边插值）和 mesh 输出仍由
`MarchingCubes<PointNT>` 基类流程处理。

| 阶段 | 当前执行路径 | 说明 |
| --- | --- | --- |
| center / RHS 准备 | 标量 | 生成 on-surface / off-surface center，成本较小。 |
| RBF matrix fill | traits-gated normal AoS RVV；其它 fallback | RVV 按 Eigen column-major（列主序）逐列填矩阵。 |
| Eigen solve | 标量 | 保持 `fullPivLu()`，本 topic 不替换 solver。 |
| voxel grid evaluation | traits-gated normal AoS RVV；其它 fallback | RVV 对 center 做可变向量长度分块和 vector reduction（向量规约）。 |
| surface emission | 既有基类路径 | 不属于本次 RBF RVV 接入范围。 |

## 当前采用的优化方式

production 源码位于 `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp`。实现把原
`voxelizeData()` 主体拆成内部标量 helper 和 RVV helper，公开入口只做短路 dispatch（分流逻辑）：

```cpp
#if defined(__RVV10__)
  if (pcl::detail::marchingCubesRBFVoxelizeDataRVV<PointNT> (...))
    return;
#endif

  pcl::detail::marchingCubesRBFVoxelizeDataStandard (...);
```

RVV helper 的当前 gate（门控）是：

```text
__RVV10__ &&
pcl::rvv::RVVXYZNormalFloatLayout<PointNT>::value &&
input_->size() >= 16
```

helper 先把满足 gate 的输入点中的 `x/y/z` 和 `normal_x/normal_y/normal_z` 转成 SoA
（structure of arrays，数组结构）center 数组。matrix fill 阶段对每个 column 固定当前 center，
在 row 方向用 `vle64` 加载多个 center 坐标，计算 `dx*dx + dy*dy + dz*dz`，再做 `r2 * sqrt(r2)`
并连续写入 Eigen column-major 内存。voxel evaluation 阶段对每个网格点分块加载 center 和 weight，
计算 cubic kernel 后用 `vfredusum` 做当前 VL chunk（可变向量长度分块）的规约，再累加到标量结果。

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| traits-gated normal AoS 点型 | adopted | `PointNormal`、`PointXYZINormal`、`PointXYZRGBNormal` 代表点型 production direct 均为弱正向，fallback 简单，public API 不变 | QEMU 7/7、板卡 gtest 7/7、production direct `1.06x-1.08x` | 不把代表点型性能外推到所有自定义点型 |
| RBF matrix fill | adopted | 组件收益约 `2.43x-2.52x`，且 production direct 仍保留端到端正向 | board compare + asm | 只覆盖当前 RBF center SoA 边界 |
| voxel grid evaluation | adopted | full pipeline 和 production direct 均正向 | board compare + checksum | vector reduction 顺序由 correctness 测试和 checksum 约束 |
| Eigen solve | not_now | 属于库求解器边界，非局部 RVV 优化 | production patch 未修改 | 若后续 profile 证明 solve 主导，应另开非 RVV / solver topic |
| per-iteration repeated summary | deferred | 当前 Evidence Doctor 只有 `low_run_count` warning，不阻塞弱正向采纳 | `Errors=0, Warnings=12` | reviewer 要求强稳定性时扩展 analyzer |

## Fallback 矩阵

| 条件 | 行为 | 证据 |
| --- | --- | --- |
| 非 RVV 构建 | 不编译 RVV helper，公开入口直接走标量 helper | `make -C test-rvv/surface/marching_cubes_rbf run_test_compare` 中 Std 7/7 |
| traits gate 不成立 | RVV helper 编译期返回 false，走标量 helper | production source gate；非 RVV Std target 覆盖标量 fallback |
| `input_->size() < 16` | 走标量 helper，避免小规模 RVV overhead | `SmallPointNormalInputUsesStableFallback` |
| Eigen solve | 始终标量 | production helper 未改 `fullPivLu()` |
| surface emission / mesh 输出 | 既有路径 | 本 patch 只修改 `voxelizeData()` |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `MarchingCubesRBF<PointNT>::voxelizeData()` | production public helper | RVV / fallback 分流入口 | `performReconstruction()` | Std 或 RVV voxelize helper | production boundary | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` |
| `marchingCubesRBFVoxelizeDataStandard()` | production Std helper | 保持标量语义和非覆盖 fallback | `voxelizeData()` | matrix fill、Eigen solve、grid evaluation | fallback coverage | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` |
| `marchingCubesRBFVoxelizeDataRVV()` | production RVV helper | traits-gated normal AoS RVV 主路径 | `voxelizeData()` | RVV matrix fill、Eigen solve、RVV grid evaluation | production direct path | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` |
| `ProductionProbeRBF<PointT>` | test helper | 用真实 protected `voxelizeData()` 构造 production direct 测试 | gtest / bench | production helper | production direct correctness / bench | `test-rvv/surface/marching_cubes_rbf/include/impl/marching_cubes_rbf_core.hpp` |
| `test_marching_cubes_rbf` | correctness target | 对拍 diagnostic、production direct 和 fallback | Makefile | gtest assertions | correctness gate | `test-rvv/surface/marching_cubes_rbf/src/test_marching_cubes_rbf.cpp` |
| `bench_marching_cubes_rbf` | bench wrapper | component、full diagnostic 和 production direct 计时 | Makefile / board.mk | board logs / analyzer | board performance | `test-rvv/surface/marching_cubes_rbf/src/bench_marching_cubes_rbf.cpp` |
| `generate_marching_cubes_rbf_evidence_manifest.py` | analysis script | 生成 Evidence Doctor manifest | `run_board_evidence_doctor` | `evidence_doctor.py` | evidence validation input | `test-rvv/surface/marching_cubes_rbf/script/generate_marching_cubes_rbf_evidence_manifest.py` |
| phase 010 result | phase closeout | 保存 exact `PointNormal` PI1-PI5 计划回填、证据和采用边界 | worker / reviewer | 本长期文档 | adoption audit | `test-rvv/surface/marching_cubes_rbf/doc/phases/010-production-integration-pointnormal/result.zh.md` |
| phase 020 result | phase closeout | 保存 traits-gated normal AoS 扩展的证据和采用边界 | worker / reviewer | 本长期文档 | adoption audit | `test-rvv/surface/marching_cubes_rbf/doc/phases/020-generic-normal-point-types/result.zh.md` |

## 数值算例

若某个 center 为 `(1, 2, 3)`，当前 grid point 为 `(2, 2, 5)`，则一个 lane（向量通道）的 cubic kernel 为：

```text
dx = 2 - 1 = 1
dy = 2 - 2 = 0
dz = 5 - 3 = 2
r2 = 1*1 + 0*0 + 2*2 = 5
kernel = r2 * sqrt(r2) = 5 * sqrt(5)
contribution = weight * kernel
```

matrix fill 的一个 VL chunk 会同时对多个 row center 计算上述 `r2 * sqrt(r2)`，并写入同一 Eigen
column 的连续 row 区间。voxel evaluation 的一个 VL chunk 会同时计算多个 center 对同一 grid point 的
contribution，再用 RVV reduction 得到该 chunk 的部分和；chunk 之间仍按标量 `double f` 累加。

## Bench 与证据

QEMU（仿真器）只用于 correctness（正确性）、路径命中和日志形状，不作为性能结论。性能数据来自
Milkv-Jupiter 板卡。

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

复现入口：

```bash
make -C test-rvv/surface/marching_cubes_rbf run_test_compare
make -C test-rvv/surface/marching_cubes_rbf run_board_test fetch_board_logs
make -C test-rvv/surface/marching_cubes_rbf run_board_production_smoke fetch_board_logs run_board_evidence_doctor
```

Evidence Doctor（证据体检）结果为 `Errors=0, Warnings=12, Suggestions=0`，12 个 warning 都是
`low_run_count`。bench 内部使用 `iterations=5`、`warmup=2`，但 manifest 只保留每个 case 的 summary
speedup，没有逐次 B/A values（候选相对基线收益序列）。因此 production direct 结论写成 weak-positive
（弱正向）：足以采纳当前小而清晰的 traits-gated normal AoS patch，但不写成强稳定 repeated-board 结论。

## 正确性与高效性证据链

| 维度 | 当前证据 | 结论边界 |
| --- | --- | --- |
| correctness | QEMU Std/RVV 各 7/7；板卡 RVV 7/7 | 覆盖 production direct、small fallback 和新增代表点型测试输入。 |
| path / asm | RVV binary 中存在 production helper 符号；可见 `vle64/vse64/vfmacc/vfsqrt/vfredusum` | 证明 RVV 指令存在并接近 production helper；不声称完整热点 profile 已闭合。 |
| performance | production direct 七个 case 为 `1.06x-1.08x` | 只覆盖 Milkv-Jupiter、当前 case 和三个代表点型。 |
| fallback | 小规模 fallback 测试通过；traits 不成立时编译期返回 false | 证明小规模不误命中 RVV；非覆盖点型保持标量语义。 |
| Evidence Doctor | 无 Error，只有 low-run warning | 支持 weak-positive adoption，不支持强稳定外推。 |

## 生产接入后的 closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production patch | adopted for traits-gated normal AoS point types | 用户确认保留 `PointNormal`，020 板卡证据支持 traits gate 扩展 |
| public API | unchanged | production diff 只修改 impl header 内部 helper / dispatch |
| RVV gate | `__RVV10__ && RVVXYZNormalFloatLayout<PointNT>::value && input_->size() >= 16` | production source + fallback gtests |
| target hardware | Milkv-Jupiter | board compare summary |
| 不覆盖范围 | 未指定自定义点型强性能结论、traits 不成立点型、小规模输入、Eigen solve、surface emission | fallback 矩阵和 optimization roadmap |
| 后续 phase | optional `030-production-repeated-values-summary` | 仅当 reviewer 需要强稳定证据时执行 |

## 后续方向

当前没有新的、同一 topic 内值得直接继续接入源码的 RVV 实现方向。`center` 准备成本较小，Eigen solve
属于求解器边界，surface emission 由基类流程处理；继续优化会扩大到非本阶段授权的算法或非 RVV topic。

如果 reviewer 需要更强稳定性，可追加 analyzer 改造，让 board manifest 记录每次 iteration 的 B/A values、
binary hash、taskset / governor / freq / temperature 等环境信息，用于消除当前 `low_run_count` 警告。
如果用户指定某个自定义 normal-like 点型，再另开 follow-up 补 compile smoke、production direct correctness、
board bench 和 Evidence Doctor。
