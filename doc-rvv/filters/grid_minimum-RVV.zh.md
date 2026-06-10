# filters/grid_minimum RVV 诊断说明

## 1. 函数入口作用

`pcl::GridMinimum<PointT>` 是 `filters` 模块的二维网格最小值下采样滤波器。用户配置 `resolution` 后调用 `filter(output)`，公开入口最终进入 `GridMinimum<PointT>::applyFilter(PointCloud&)`，该函数调用 `applyFilterIndices(indices)` 找到每个 `x/y` cell 中 `z` 最小的原始点，再通过 `copyPointCloud` 输出这些点。

该滤波器在地形或地面点粗筛场景中用于保留每个网格单元的最低点。输入是 `PointCloud<PointT>` 和可选 `indices_`，输出是被保留点的索引或对应点云。当前 RVV 工作只做 bench-only 诊断，不改变公开 API，也不修改 `filters/include/pcl/filters/impl/grid_minimum.hpp` 的生产分流。

## 2. 标量路径与诊断边界

上游标量路径的关键公式是：

```text
ix = floor(point.x * inverse_resolution) - min_b0
iy = floor(point.y * inverse_resolution) - min_b1
idx = ix + iy * div_x
```

随后执行：

```text
sort(idx, source_index)
-> 对每段相同 idx 的点扫描 z
-> 输出 z 最小的 source_index
```

当前诊断覆盖 `PointXYZ`、显式 indices、dense / non-dense 两种输入。RVV 只批量化 cell-id 预计算，排序和 min-z 分组扫描保持标量。生产入口没有接入新 RVV helper；bench 中 `production unchanged` case 只是未修改上游源码的整体观察，其中可能包含此前 `getMinMax3D` RVV 的间接受益。

bench-only 主题升级为生产路径时，判断依据必须是 full diagnostic 或生产入口 case，而不是局部片段 speedup。`grid_minimum` 的局部 cell-id 片段在板卡上有 `1.33x` 到 `1.52x`，但包含排序和每 cell 最小 z 扫描后的 full diagnostic 只有 `1.09x` 到 `1.14x`。这说明当前 RVV 覆盖没有覆盖完整入口主成本；接入生产需要承担 staging、分流、fallback 和维护复杂度，但完整收益不足，因此本主题不加入生产路径。

## 3. 覆盖范围与 fallback

| 项目         | 结论                                                                                     |
| ------------ | ---------------------------------------------------------------------------------------- |
| 覆盖点类型   | 诊断覆盖 `pcl::PointXYZ`                                                               |
| 覆盖数据形态 | 显式 `indices` gather；dense 全 lane，non-dense finite mask                            |
| RVV 内容     | `x/y/z` gather、`floor(x/y * inverse_resolution)`、`idx=ix+iy*div_x`、保序 staging |
| 保持标量     | bounds、sort、每 cell 最小 z、`copyPointCloud`、生产入口                               |
| fallback     | 小规模、非 RVV 编译、生产源码路径均保持标量                                              |
| FRM/FCSR     | 使用 `vfcvt.rtz.x.f.v` 加校正，不使用 `_rm` intrinsic，不修改 FRM/FCSR               |

## 4. 详细设计

专项实现位于 `test-rvv/filters/grid_minimum/grid_minimum_diag.hpp`。

### 4.1 类型与布局边界

本主题是 bench-only 诊断，不提供模板化生产 helper。诊断入口固定为 `pcl::PointXYZ`：

```cpp
computeGridCellsRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                    const pcl::Indices& indices,
                    float inverse_resolution,
                    int min_b0,
                    int min_b1,
                    int div_x,
                    std::vector<GridCell>& out)
```

因此本主题没有新增 `HasXYZFloatLayout<PointT>` 这类生产级 SFINAE。类型安全来自函数签名、`PointXYZ` 标准字段，以及公共 `rvv_point_load` gather wrapper 对 `offsetof(PointXYZ, x/y/z)` 的使用。若后续重新纳入生产候选，应改为模板入口，并采用与既有生产主题一致的编译期检查：确认 `PointT` 有 `x/y/z`，布局可用 `offsetof` 计算，三坐标底层类型均为 `float`；不满足时在公开入口短路回 `*_Std`。

### 4.2 特殊实体说明

| 实体                       | 类型                   | 输入 / 输出                                   | 作用与边界                                                                                                                  |
| -------------------------- | ---------------------- | --------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| `GridCell`               | staging 结构体         | `idx/source_index/ix/iy/z`                  | 保存 RVV cell-id 阶段输出。`idx/ix/iy` 来自标量 grid 公式，`source_index` 指回原输入点，`z` 用于后续标量 min-z 扫描。 |
| `computeBoundsStd`       | 标量 helper            | cloud + indices -> min/max                    | 对应上游 `getMinMax3D` 输入边界。诊断中保持标量，避免把已有 common RVV min/max 与本主题 cell-id 收益混在一起。            |
| `computeGridCellsStd`    | 标量 helper            | cloud + indices + grid shape ->`GridCell[]` | 作为 RVV 对拍基准，逐 index 计算 floor 和 grid id。                                                                         |
| `floorF32ToI32NoFrm`     | RVV 数值 helper        | `vfloat32m2_t -> vint32m2_t`                | 用向 0 转换加负小数校正实现 `std::floor`，避免 FRM/FCSR 副作用。                                                          |
| `finiteMask`             | RVV mask helper        | `vfloat32m2_t -> vbool16_t`                 | non-dense 路径排除 NaN 和 Inf，对应标量 `pcl::isXYZFinite`。                                                              |
| `computeGridCellsRVV`    | RVV 诊断 helper        | cloud + indices + grid shape ->`GridCell[]` | 用公共 `rvv_point_load` gather wrapper 读取 `x/y/z`，生成 grid id 并用 `vcompress` 保序写 staging。                   |
| `selectMinimumZIndices`  | 标量 helper            | `GridCell[] -> Indices`                     | 对 staging 排序并按 cell 选择最小 z。保持上游排序后扫描语义。                                                               |
| `gridMinimumPointXYZRVV` | full diagnostic helper | cloud + indices -> Indices                    | RVV cell-id staging + 标量 sort/min-z，用于判断局部收益是否能进入整体入口。                                                 |

### 4.3 数值 helper：`floorF32ToI32NoFrm`

`GridMinimum` 的 cell 坐标使用 `std::floor(point.x * inverse_resolution)`。RVV 的 `vfcvt.rtz.x.f.v` 是向 0 截断，负小数时与 floor 不同，例如 `-1.5` 截断为 `-1`，floor 为 `-2`。本 helper 用 RTZ 转换后比较原值与截断值，对负小数 lane 减 1；它不使用 `_rm` intrinsic，也不修改 FRM/FCSR。

```cpp
inline vint32m2_t
floorF32ToI32NoFrm(vfloat32m2_t values, std::size_t vl)
{
  const vint32m2_t trunc = __riscv_vfcvt_rtz_x_f_v_i32m2(values, vl);
  const vfloat32m2_t trunc_f = __riscv_vfcvt_f_x_v_f32m2(trunc, vl);
  const vbool16_t negative_fraction = __riscv_vmflt_vv_f32m2_b16(values, trunc_f, vl);
  const vint32m2_t zero = __riscv_vmv_v_x_i32m2(0, vl);
  const vint32m2_t adjust = __riscv_vmerge_vxm_i32m2(zero, 1, negative_fraction, vl);
  return __riscv_vsub_vv_i32m2(trunc, adjust, vl);
}
```

### 4.4 mask helper：`finiteMask`

上游 `GridMinimum` 只在 `cloud.is_dense == false` 时跳过 invalid xyz。RVV 诊断保持相同条件：dense 输入直接保留全部 lane；non-dense 输入分别检查 `x/y/z`。NaN 用 `value == value` 排除，Inf 用 `abs(value) < infinity` 排除。

```cpp
inline vbool16_t
finiteMask(vfloat32m2_t values, std::size_t vl)
{
  const vbool16_t eq_self = __riscv_vmfeq_vv_f32m2_b16(values, values, vl);
  const vfloat32m2_t abs_v = __riscv_vfabs_v_f32m2(values, vl);
  const vfloat32m2_t inf_v = __riscv_vfmv_v_f_f32m2(std::numeric_limits<float>::infinity(), vl);
  const vbool16_t not_inf = __riscv_vmflt_vv_f32m2_b16(abs_v, inf_v, vl);
  return __riscv_vmand_mm_b16(eq_self, not_inf, vl);
}
```

### 4.5 cell-id 片段：`computeGridCellsRVV`

`computeGridCellsRVV` 对应 bench 中的 `grid minimum cell-id diag ...` case。它只批量化“给定 bounds / grid shape 后，把每个 source index 转成 grid cell id 并写入 staging”的前置纯函数片段。

入口 fallback 条件是：

- `n < 64` 时返回 false，避免小规模进入诊断 RVV；
- `indices.size()` 或 `cloud.size()` 超过 `uint32_t` 可表达范围时返回 false，因为当前 gather offset 和 staging source index 使用 32 位 lane；
- 非 RVV 编译时不存在该 helper，专项测试 / bench 走 Std 构建。

每个 VL chunk 的组织如下：

1. `vle32` 读取 `indices[i:i+vl)`，得到 source index lane；
2. `byte_offsets_u32m2<PointXYZ>` 把 source index 转成 AoS byte offset；
3. `pcl::rvv_load::indexed_load3_f32m2<PointXYZ, offsetof(...x/y/z)>` gather `x/y/z`；
4. 用 `floorF32ToI32NoFrm(x * inverse_resolution)` 和 `floorF32ToI32NoFrm(y * inverse_resolution)` 计算 `ix/iy`；
5. 用 `idx = ix + iy * div_x` 生成与标量相同的二维 grid id；
6. dense 输入用全 true mask，non-dense 输入合并 `finite(x) & finite(y) & finite(z)`；
7. 用 `vcompress` 把有效 lane 的 `idx/source_index/ix/iy/z` 保序压缩到寄存器前部；
8. 用 `vslidedown + vmv_x_s / vfmv_f_s` 逐 lane 写回 `GridCell` staging。

核心片段（覆盖访存、mask、压缩和写回）：

```cpp
const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
const vint32m2_t v_indices = __riscv_vle32_v_i32m2(indices.data() + i, vl);
const vuint32m2_t v_indices_u = __riscv_vreinterpret_v_i32m2_u32m2(v_indices);
// Explicit indices are source point ids; convert them to AoS byte offsets and
// gather x/y/z through the common point-load wrapper.
const vuint32m2_t v_offsets = pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZ>(v_indices_u, vl);

vfloat32m2_t vx;
vfloat32m2_t vy;
vfloat32m2_t vz;
pcl::rvv_load::indexed_load3_f32m2<pcl::PointXYZ,
                                   offsetof(pcl::PointXYZ, x),
                                   offsetof(pcl::PointXYZ, y),
                                   offsetof(pcl::PointXYZ, z)>(base, v_offsets, vl, vx, vy, vz);

const vfloat32m2_t sx = __riscv_vfmul_vf_f32m2(vx, inverse_resolution, vl);
const vfloat32m2_t sy = __riscv_vfmul_vf_f32m2(vy, inverse_resolution, vl);
const vint32m2_t ix = __riscv_vsub_vx_i32m2(floorF32ToI32NoFrm(sx, vl), min_b0, vl);
const vint32m2_t iy = __riscv_vsub_vx_i32m2(floorF32ToI32NoFrm(sy, vl), min_b1, vl);
const vint32m2_t idx_i32 = __riscv_vmacc_vx_i32m2(ix, div_x, iy, vl);

// Dense input keeps every lane.  Non-dense input matches pcl::isXYZFinite by
// combining the three coordinate masks before compression.
vbool16_t keep = __riscv_vmset_m_b16(vl);
if (!cloud.is_dense) {
  keep = __riscv_vmand_mm_b16(__riscv_vmand_mm_b16(finiteMask(vx, vl), finiteMask(vy, vl), vl),
                              finiteMask(vz, vl),
                              vl);
}

// Compress keeps lane order for valid points; source_index and z remain paired
// for the later scalar sort/min-z stage.
const vuint32m2_t idx_kept =
    __riscv_vcompress_vm_u32m2(__riscv_vreinterpret_v_i32m2_u32m2(idx_i32), keep, vl);
const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2(v_indices_u, keep, vl);
const vint32m2_t ix_kept = __riscv_vcompress_vm_i32m2(ix, keep, vl);
const vint32m2_t iy_kept = __riscv_vcompress_vm_i32m2(iy, keep, vl);
const vfloat32m2_t z_kept = __riscv_vcompress_vm_f32m2(vz, keep, vl);
const std::size_t keep_count = __riscv_vcpop_m_b16(keep, vl);

// Final output is one minimum-z point per cell, so this stage writes scalar
// GridCell records instead of emitting final indices directly from lanes.
for (std::size_t lane = 0; lane < keep_count; ++lane) {
  out_cells[kept + lane].idx =
      __riscv_vmv_x_s_u32m2_u32(__riscv_vslidedown_vx_u32m2(idx_kept, lane, keep_count));
  out_cells[kept + lane].source_index =
      __riscv_vmv_x_s_u32m2_u32(__riscv_vslidedown_vx_u32m2(source_kept, lane, keep_count));
  out_cells[kept + lane].ix =
      __riscv_vmv_x_s_i32m2_i32(__riscv_vslidedown_vx_i32m2(ix_kept, lane, keep_count));
  out_cells[kept + lane].iy =
      __riscv_vmv_x_s_i32m2_i32(__riscv_vslidedown_vx_i32m2(iy_kept, lane, keep_count));
  out_cells[kept + lane].z =
      __riscv_vfmv_f_s_f32m2_f32(__riscv_vslidedown_vx_f32m2(z_kept, lane, keep_count));
}
```

这里的 `vcompress` 只移除 invalid lane，不改变保留下来的 lane 相对顺序。写回到 `GridCell` 后，`source_index` 仍指向原始输入点，`z` 仍来自同一 source point，后续标量排序和最小 z 扫描可以复用上游语义。

### 4.6 full diagnostic：RVV staging + 标量排序 / min-z

`gridMinimumPointXYZRVV` 对应 bench 中的 `grid minimum full diag ...` case。它不是生产入口，而是为了检查 cell-id 片段收益是否能穿透完整 `GridMinimum` 主要流程：

```text
computeBoundsStd(cloud, indices)
-> computeGridShape(min/max, inverse_resolution)
-> computeGridCellsRVV(...)          # RVV cell-id staging
-> selectMinimumZIndices(cells)      # 标量 sort + 每 cell 最小 z 扫描
```

`selectMinimumZIndices` 对应上游 `applyFilterIndices` 后半段：

```text
sort(cells by idx)
for each contiguous group with same idx:
  scan z
  emit source_index of the minimum z point
```

排序和每 cell 最小 z 扫描都保持标量，原因有三点：

- 最终输出不是“所有有效点”，而是每个 cell 中 `z` 最小的 source index；
- 同一 cell 的点可能分布在多个 VL chunk 中，直接在 chunk 内输出会破坏全局分组语义；
- 上游使用排序后连续段扫描，保留这部分标量逻辑能避免改变 tie/order 和输出顺序边界。

因此，本主题中的“cell-id 片段”只到 `GridCell` staging 为止；“full diagnostic”是在该 staging 后追加同一排序和每 cell 最小 z 标量扫描；“生产入口”没有接入本诊断 helper。

## 5. 数值算例与 VL chunk 图示

设 `inverse_resolution = 2`、`min_b = (-3, -1)`、`div_x = 6`。一个 VL chunk 中 4 个点：

| lane | `point.x` | `point.y` | `point.z` | `floor(x*2)-min_b0` | `floor(y*2)-min_b1` | `idx=ix+iy*6` |
| ---: | ----------: | ----------: | ----------: | --------------------: | --------------------: | --------------: |
|    0 |       -0.01 |       -0.50 |        0.30 |                     2 |                     0 |               2 |
|    1 |        0.49 |        0.50 |        0.20 |                     3 |                     2 |              15 |
|    2 |        1.01 |        1.49 |        0.10 |                     5 |                     3 |              23 |
|    3 |       -1.01 |        1.01 |        0.40 |                     0 |                     3 |              18 |

图示：

```text
indices lanes:  [p0, p1, p2, p3]
gather xyz:     [x0..x3], [y0..y3], [z0..z3]
floor/grid:     ix=[2,3,5,0], iy=[0,2,3,3], idx=[2,15,23,18]
vcompress:      dense 时全部保留；non-dense 时只保留 finite(x,y,z) lane
scalar tail:    sort idx -> per-cell min z -> output source_index
```

负坐标是必要覆盖点：`floor(-0.01 * 2) = -1`，不能用向 0 截断得到 `0`。本实现用 `vfcvt.rtz.x.f.v` 后对负小数 lane 减 1，匹配 `std::floor`，且不污染 FRM。

## 6. 测试、QEMU、反汇编和板卡证据

专项测试：

- `make -C test-rvv/filters/grid_minimum run_test_compare` 通过，std/RVV 两套构建均通过 6 个测试；
- `make -C test-rvv/filters/grid_minimum run_upstream_test_compare` 通过，上游 `Grid.Minimum` std/RVV 均通过；
- 测试覆盖负坐标 floor、subset gather、non-dense invalid 跳过、full diagnostic 对拍、先 RVV 后 fallback 的 FRM 污染检查，以及未改生产入口可运行。

QEMU bench：

- `make -C test-rvv/filters/grid_minimum run_bench_compare` 通过；
- `output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`；
- QEMU 只作为构建、checksum、格式和指令路径证据，不作为性能结论。

反汇编：

- `make -C test-rvv/filters/grid_minimum dump_bench_rvv` 生成 `build/asm/riscv/bench_grid_minimum_rvv.full.asm`；
- `output/qemu/rvv_asm_check.log` 确认 `vluxei32.v`、`vfcvt.rtz.x.f.v`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。

板卡验证：

- `make -C test-rvv/filters/grid_minimum run_board_test run_board_bench_compare fetch_board_logs` 通过；
- 日志位于 `test-rvv/filters/grid_minimum/output/board/`；
- 设备：Milkv-Jupiter；iterations：30；数据集：synthetic `PointXYZ`。

## 7. 板卡结果与 case 解释

speedup 计算方式为 `Std avg ms/iter / RVV avg ms/iter`。每个 case 的 std/RVV checksum 一致。

| case                                      | 入口与参数                                                  | 路径含义                                                        | speedup | 证明点                                               |
| ----------------------------------------- | ----------------------------------------------------------- | --------------------------------------------------------------- | ------: | ---------------------------------------------------- |
| `grid minimum cell-id diag 64K`         | 64K `PointXYZ`，全量 indices，dense，resolution `0.125` | 只命中 RVV cell-id staging                                      |   1.36x | 小规模片段有收益                                     |
| `grid minimum cell-id diag 1M`          | 1M `PointXYZ`，全量 indices，dense                        | 只命中 RVV cell-id staging                                      |   1.36x | 片段收益随规模稳定                                   |
| `grid minimum subset cell-id diag 1M`   | 1M `PointXYZ`，奇数 subset indices                        | RVV gather + cell-id staging                                    |   1.33x | 显式 indices gather 仍有收益                         |
| `grid minimum finite cell-id diag 1M`   | 1M `PointXYZ`，non-dense，含 NaN/Inf                      | RVV finite mask + cell-id staging                               |   1.52x | invalid lane 跳过语义成立且片段收益更强              |
| `grid minimum full diag 64K`            | 64K full diagnostic                                         | RVV cell-id + 标量 sort/min-z                                   |   1.11x | 排序和 min-z 已明显稀释收益                          |
| `grid minimum full diag 1M`             | 1M full diagnostic                                          | RVV cell-id + 标量 sort/min-z                                   |   1.09x | 大规模整体收益偏弱                                   |
| `grid minimum finite full diag 1M`      | 1M non-dense full diagnostic                                | RVV finite cell-id + 标量 sort/min-z                            |   1.14x | non-dense full 仍只小幅收益                          |
| `grid minimum production unchanged 64K` | 未修改的 `GridMinimum<PointXYZ>::filter(output)`          | 不代表本主题新增 RVV；可能包含已有 `getMinMax3D` RVV 间接受益 |   1.20x | 生产入口当前已有外部路径影响，不能据此接入本主题分流 |

## 8. 结论

`GridMinimum` 的 2D cell-id 预计算 RVV 片段正确、命中指令，并在 Milkv-Jupiter 上有 `1.33x` 到 `1.52x` 局部收益。但 full diagnostic 只有 `1.09x` 到 `1.14x`，排序和每 cell 最小 z 扫描稀释了局部收益。当前主题保留为 bench-only / 诊断，不接入生产主路径。

若后续重新评估生产接入，应先证明完整 `GridMinimum::filter` 入口稳定明显收益，而不是只证明 cell-id 片段收益；同时需要保持 indices 顺序、相同 cell 内最小 z 选择、non-dense invalid 跳过和公开 API 不变。重新纳入生产候选还需要确认 fallback 边界清晰，且新增 helper、staging 结构、诊断宏和文档维护成本与 full / production 收益匹配。
