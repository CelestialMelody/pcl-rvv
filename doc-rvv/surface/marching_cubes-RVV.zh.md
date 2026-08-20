# surface/marching_cubes RVV 优化说明

## 当前状态

`pcl::MarchingCubes<PointNT>::performReconstruction()` 已接入 generic RVV production path（泛型生产路径）：在 `__RVV10__` 构建下，`PointNT` 满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointNT>` 时先用 RVV 批量扫描 z 方向 active cell（活跃体素单元），再把命中的 cell 交给原标量 `createSurface()` 输出三角点。不满足该 traits gate（字段布局门禁）的模板实例回退原标量扫描。

当前采用方式是 `production-active-cell-prepass`，不是 edge interpolation RVV。edge interpolation 候选已经验证过 correctness 和反汇编，但板卡收益弱且 Evidence Doctor 有退化 Error，因此不接入生产。

## 函数入口作用

Marching Cubes 把子类生成的 signed distance grid（有符号距离网格）转换成三角网格。公开入口先检查 `iso_level_`，分配 `grid_`，通过输入点云计算 bounding box 和 voxel size，再调用子类 `voxelizeData()` 填充 `grid_`。之后主循环扫描内部 voxel cell：

```text
x/y/z cell index
  -> 读取 8 个 grid 邻点值
  -> 跳过 NaN cell
  -> 计算 cube index
  -> edgeTable 判断 active / inactive
  -> createSurface 标量 edge interpolation + triTable 输出点
  -> polygons 按每 3 个点生成一个 triangle
```

RVV 只接管“读取 8 个相邻 grid 值、计算 cube index、过滤 inactive / NaN cell”这一段。`voxelizeData()`、edge interpolation、triTable 遍历、`PointNT` 构造和 polygon 输出仍是标量语义。

## 覆盖范围与 fallback

| 维度 | 当前状态 | 证据 | 边界 |
| --- | --- | --- | --- |
| `RVVXYZAoSFloatLayout<PointNT>` / `float` / synthetic public grid | adopted | `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 5-run board repeated 均 positive，doctor 无 finding | 覆盖 synthetic voxelized grid 的 public wrapper，不外推真实 Hoppe/RBF 分布 |
| historical `PointNormal` anchor | retained | 5-run `5.330x/6.386x/8.824x`，doctor `Errors=0, Warnings=1, Suggestions=0` | 历史窄 gate 对照，不再是当前唯一 gate |
| 不满足 traits gate 的 `PointNT` | scalar fallback | `NonAoSXYZFallsBackToScalarReference` 通过 | RVV build 下仍走 `reconstructSurfaceStd()` |
| edge interpolation RVV | rejected for production | `0.992x/1.024x/1.049x`，doctor `Errors=1` | 暂停，除非后续 profile 证明成为新瓶颈 |
| active-z finite-collapse single-buffer | attempted / not adopted | RVV-vs-RVV `1.002x/1.007x/1.012x`，doctor `Errors=0, Warnings=1, Suggestions=1` | 收益 near-threshold，当前 production 保持双 staging store 形态 |
| triangle emission RVV | deferred | 未覆盖 | 输出数量由 triTable 可变，push_back 和 polygon 构造保持标量 |
| Hoppe / RBF voxelization | scalar | 未覆盖 | 子类 `voxelizeData()` 的 search / solver 成本独立 |

Fallback 矩阵：

| 条件 | 行为 |
| --- | --- |
| 非 `__RVV10__` 构建 | 只编译并运行 `reconstructSurfaceStd()` |
| `PointNT` 不满足 `RVVXYZAoSFloatLayout<PointNT>` | 在 RVV 构建中仍走 `reconstructSurfaceStd()` |
| active cell emission | 即使命中 RVV prepass，也调用原 `getNeighborList1D()` 和 `createSurface()` |
| NaN cell | RVV prepass 用 `value == value` 组合 finite mask；后续标量 `getNeighborList1D()` 仍保留 NaN skip |
| `iso_level_` 非法 | 保持原入口错误处理，清空输出并返回 |

## 当前采用的优化方式

生产源码新增三个内部 helper：

| helper | 层级 | 作用 |
| --- | --- | --- |
| `reconstructSurfaceStd` | scalar fallback | 保留原三重循环、`getNeighborList1D()` 和 `createSurface()` 路径。 |
| `getActiveVoxelsZRVV` | RVV prepass | 对固定 `(x, y)` 的 z-lane chunk 批量加载 8 个相邻 grid 值，生成 cube index 和 finite flag，再把 active z 写入 `active_z`。 |
| `reconstructSurfaceRVV` | production RVV wrapper | 遍历 `(x, y)`，调用 RVV prepass 得到 active z，然后对每个 active cell 回到标量 surface emission。 |

一个 VL chunk 内部流程：

```text
g000/g100/g110/g010 base pointers
  -> vle32 加载 8 个 corner value
  -> vmflt(value, iso_level_) 生成 cube bits
  -> vmfeq(value, value) 合成 finite mask
  -> vse32 临时写出 cube_indices / finite_flags
  -> 标量 tail 查 edgeTable，压入 active_z
```

公开入口的分流是：

```cpp
#if defined(__RVV10__)
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointNT>::value)
    reconstructSurfaceRVV (intermediate_cloud);
  else
    reconstructSurfaceStd (intermediate_cloud);
#else
  reconstructSurfaceStd (intermediate_cloud);
#endif
```

这个 traits gate 只表达 xyz AoS float 输入/输出布局前提。它不批准真实 Hoppe/RBF 输入分布、`Scalar=double`、indices/correspondences 或 triangle emission RVV。

## 数值算例

假设某个 cell 的 8 个 corner 值为：

```text
v0=-0.20, v1=0.15, v2=0.30, v3=-0.10,
v4=0.40,  v5=0.35, v6=-0.05, v7=0.25, iso=0.0
```

标量路径逐项比较 `< iso`，得到 bits：

```text
v0 -> bit 1
v3 -> bit 8
v6 -> bit 64
cubeindex = 1 + 8 + 64 = 73
```

RVV prepass 在一个 z-lane chunk 中同时对多组 cell 做相同判断。每个 lane 的 `cubeindex` 与标量一致；如果 8 个值中有 NaN，finite flag 为 0，该 lane 不进入 `active_z`。如果 `edgeTable[73] != 0`，该 z 被压入 active list，后续仍按原标量 `createSurface()` 做 edge interpolation 和 triTable 输出。

## Bench 与证据

当前 generic production direct board summary：

| point type | case | median | values | doctor |
| --- | --- | ---: | --- | --- |
| `PointXYZ` | `mc_prod_xyz_64` | `3.873x` | `3.820x, 3.873x, 3.892x, 3.876x, 3.870x` | `Errors=0, Warnings=0, Suggestions=0` |
| `PointXYZI` | `mc_prod_xyzi_64` | `3.618x` | `3.618x, 3.600x, 3.618x, 3.618x, 3.633x` | `Errors=0, Warnings=0, Suggestions=0` |
| `PointXYZRGB` | `mc_prod_xyzrgb_64` | `3.601x` | `3.597x, 3.566x, 3.623x, 3.601x, 3.626x` | `Errors=0, Warnings=0, Suggestions=0` |
| `PointXYZRGBA` | `mc_prod_xyzrgba_64` | `3.639x` | `3.627x, 3.636x, 3.714x, 3.639x, 3.695x` | `Errors=0, Warnings=0, Suggestions=0` |

证据路径：

| artifact | role | path |
| --- | --- | --- |
| production source | adopted implementation | `surface/include/pcl/surface/impl/marching_cubes.hpp` |
| public declarations | helper declarations and tables | `surface/include/pcl/surface/marching_cubes.h` |
| correctness tests | Std/RVV compare and production direct synthetic test | `test-rvv/surface/marching_cubes/src/test_marching_cubes.cpp` |
| bench wrapper | helper and production-direct bench labels | `test-rvv/surface/marching_cubes/src/bench_marching_cubes.cpp` |
| generic board summaries | production representative performance evidence | `test-rvv/surface/marching_cubes/log/board/generic_*_repeated/summary.md` |
| Phase 060 RVV-vs-RVV summary | active-z tail candidate family-selection evidence | `test-rvv/surface/marching_cubes/log/board/phase060_rvv_ab/summary.md` |
| Evidence Doctor | anomaly and warning reports | `test-rvv/surface/marching_cubes/log/board/generic_*_repeated/evidence_doctor.md` |
| evaluation | function-level decision and closeout | `test-rvv/surface/marching_cubes/doc/marching_cubes-evaluation.zh.md` |
| optimization evidence | adopted / rejected families | `test-rvv/surface/marching_cubes/doc/optimization-evidence.zh.md` |
| roadmap | next phase and expansion queue | `test-rvv/surface/marching_cubes/doc/optimization-roadmap.zh.md` |

QEMU 只用于 correctness、日志形状和路径命中。性能结论只引用板卡 repeated summary。

## 正确性与高效性证据链

| evidence layer | 当前结论 | 边界 |
| --- | --- | --- |
| correctness | Std/RVV `run_test_compare` 通过，Std/RVV 各 6 tests | 覆盖 synthetic grids、generic 代表点型和 fallback |
| asm | RVV bench binary 可见 RVV scan 指令 | 反汇编证明 RVV 指令存在，不单独证明性能 |
| board performance | 四个 generic 代表点型 5-run 均 positive，checksum match | 只支撑 synthetic public boundary |
| boundary | 已采纳范围是 `RVVXYZAoSFloatLayout<PointNT>` + public synthetic `performReconstruction()` | 不证明真实 Hoppe/RBF 分布或 edge interpolation RVV |
| risk | finite-collapse single-buffer 已 neutral；更复杂 table lookup / compress 需要 profile 才恢复；真实输入分布未 profile | roadmap 保留恢复条件 |

## 生产接入评估

接入理由：

- active-cell prepass 位于主扫描路径，能减少 inactive cell 上的标量 `getNeighborList1D()` / `createSurface()` 调用；
- 三角点输出保持标量，维护风险低；
- public production direct 证据明显强于 helper-only edge interpolation；
- traits gate 让未验证布局自然 fallback。

不扩大范围的理由：

- synthetic public path 不能替代真实 Hoppe / RBF voxelization 分布；
- `createSurface()` 构造 `PointNT` 时只写 xyz，额外字段语义按标量路径保持，不额外承诺初始化；
- `Scalar=double`、indices / correspondences、triangle emission 不是本 topic 当前入口。

## 后续方向

当前接入已闭合到 generic representative repeated board。Phase 060 已尝试 active-z finite-collapse single-buffer：正确性通过、asm 形态更轻，但同边界 RVV-vs-RVV 只有 `1.007x` median，属于 neutral，因此不接入。更复杂的 active-z table lookup / `vcompress` 只有在 profile 或 component ablation 证明尾段仍是瓶颈时恢复。edge interpolation RVV 继续暂停；真实 Hoppe/RBF 输入分布应作为单独 phase 或子 topic 处理。
