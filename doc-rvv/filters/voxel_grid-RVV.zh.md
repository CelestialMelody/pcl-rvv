# `voxel_grid`（`impl/voxel_grid.hpp`）：RVV 优化实现说明

本文说明 `filters/include/pcl/filters/impl/voxel_grid.hpp` 中 `PCLPointCloud2 getMinMax3D` dense、float 路径的 `__RVV10__` 扩展。当前覆盖基础非 indices 路径、indices gather 路径，以及非 indices 的 distance field mask 路径。公开 API 不变；不满足范围的路径保持标量实现。

实现文件：`filters/include/pcl/filters/impl/voxel_grid.hpp`。

筛选与测试材料：

- 模块二轮筛选：`doc-rvv/library-screening/filters/filters-function-evaluation-queue.zh.md`
- 函数级评估：`test-rvv/filters/voxel_grid/voxel_grid-evaluation.zh.md`
- 专项测试与 bench：`test-rvv/filters/voxel_grid/`

## 1. 背景与需求

`voxel_grid.hpp` 是 filters 模块首批高优先级文件。首轮只覆盖 `PCLPointCloud2 getMinMax3D` 的最规整路径：

- `T=float`；
- `cloud->is_dense == true`；
- 基础非 indices 路径；
- indices 路径；
- 非 indices 的 distance field 过滤路径；
- `x/y/z` 字段均为 `FLOAT32`；
- `point_step` 与字段 offset 合法；
- 点数达到 RVV 阈值。

暂缓覆盖：

- `double`；
- non-dense NaN/Inf 过滤路径；
- `PointCloud<PointT>` distance field 路径；
- `VoxelGrid::applyFilter` 的前置 index 生成、sort、分组和 centroid 聚合。

暂缓原因：

- `PointCloud<PointT>` distance field 路径依赖泛型点类型布局与 `getArray4fMap()`，无法在当前主题内安全假设 xyz 连续 float 布局；
- `VoxelGrid::applyFilter` 前置 index 生成受 `std::floor`、`emplace_back` 和后续 sort 主导，直接 RVV 化会明显侵入现有结构，收益难以稳定归因。

## 2. 与上游实现的差异

| 条目 | 原路径 | RVV 路径 |
| --- | --- | --- |
| dense `PCLPointCloud2 getMinMax3D` 基础路径 | 按 `point_step` 标量扫描 x/y/z，更新 6 路 min/max | 使用 `vlse32` strided load 读取 x/y/z，并用 `vfredmin` / `vfredmax` 做规约 |
| dense `PCLPointCloud2 getMinMax3D` indices 路径 | 按 indices 间接访问并更新 min/max | 使用 RVV gather 读取 x/y/z，并做向量规约 |
| dense `PCLPointCloud2 getMinMax3D` distance field 路径 | 标量读取 distance 字段，按 `limit_negative` 过滤后更新 min/max | 使用 RVV mask 表达距离过滤，再对有效 lanes 做 min/max 规约 |
| non-dense | 标量读取并跳过 NaN/Inf | 保持标量 |
| 字段类型不匹配 | 报错后返回 | 保持原行为 |
| 小规模输入 | 标量扫描 | RVV helper 返回失败后落到 `getMinMax3DStd` |

当前结构已按 workflow 统一为：

- 基础、indices、distance field 三组 helper 分别贴近对应的 `getMinMax3D` 分发入口，而不是集中堆在文件开头；
- 常驻 `pcl::getMinMax3DStd`、`pcl::getMinMax3DIndicesStd`、`pcl::getMinMax3DDistanceStd`；
- `__RVV10__` 下的 `pcl::getMinMax3DFloatDenseRVV`、`pcl::getMinMax3DFloatDenseIndicesRVV`、`pcl::getMinMax3DFloatDenseDistanceRVV`；
- 主路径 helper 都放在 `pcl` 命名空间下，避免为主路径 helper 额外套 `pcl::detail`；
- 公开入口中用 `#if defined(__RVV10__)` + `if constexpr` 短路尝试 RVV；
- RVV helper 未命中时显式回到 Std。

## 3. 总体设计

`PCLPointCloud2` 是字节布局点云，x/y/z 字段由 `fields[].offset` 和 `point_step` 描述。RVV 路径不改变存储格式，而是按 `point_step` 做 strided load。

处理流程：

1. 确认 cloud 存在且 dense；
2. 确认 x/y/z 字段都是 `FLOAT32`；
3. 确认点数不低于 `kVoxelGridMinMaxRvvMinPoints`；
4. 确认字段 offset 不越过 `point_step`；
5. 基础路径按 VL chunk strided load x/y/z；
6. indices 路径按 indices gather 读取 x/y/z；
7. distance field 路径读取 distance 字段并生成过滤 mask；
8. 每个 chunk 对有效 x/y/z 分别做 min/max 规约；
9. 输出 `min_pt` / `max_pt`。

该路径只负责 `getMinMax3D` 的 min/max 扫描，不把整体 `VoxelGrid::applyFilter` 的耗时归因到该 helper。

## 4. 详细设计

### 4.1 入口分发与 fallback

`voxel_grid.hpp` 当前没有新增公开 API。三个公开入口都保持同一模式：先保留上游类型检查，再在 `__RVV10__` 下用 `if constexpr` 限定 `float` 覆盖范围；RVV helper 返回 `false` 时自然落回对应 Std helper。

基础路径入口骨架如下：

```cpp
// filters/include/pcl/filters/impl/voxel_grid.hpp: getMinMax3D(PCLPointCloud2, x/y/z)
#if defined(__RVV10__)
  if constexpr (std::is_same_v<T, float>)
  {
    Eigen::Vector4f min_f;
    Eigen::Vector4f max_f;
    if (pcl::getMinMax3DFloatDenseRVV(cloud, x_idx, y_idx, z_idx, min_f, max_f))
    {
      min_pt = min_f.template cast<T>();
      max_pt = max_f.template cast<T>();
      return;
    }
  }
#endif

  pcl::getMinMax3DStd(cloud, x_idx, y_idx, z_idx, min_pt, max_pt);
```

该结构的边界是：公开入口只表达分流，不承载 RVV 主体；小规模、non-dense、非 `FLOAT32`、字段 offset 不合法等运行期条件都由 RVV helper 返回 `false`，统一回到 Std。

### 4.2 基础 dense 路径：byte-strided xyz 规约

`PCLPointCloud2` 不是编译期点类型，而是由 `fields[].offset` 与 `point_step` 描述的字节布局。基础 RVV helper 不假设 xyz 紧邻，只按字段 offset 和 point stride 做 `vlse32`。

核心条带如下：

```cpp
// filters/include/pcl/filters/impl/voxel_grid.hpp: getMinMax3DFloatDenseRVV(...)
  // RVV only uses byte-strided access described by PCLPointCloud2 metadata; paths
  // that need NaN filtering, non-float fields, or invalid offsets stay on Std.
  while (i < nr_points)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
    const auto* chunk = base + i * pt_step;
    const auto* x_ptr = reinterpret_cast<const float*>(chunk + x_off);
    const auto* y_ptr = reinterpret_cast<const float*>(chunk + y_off);
    const auto* z_ptr = reinterpret_cast<const float*>(chunk + z_off);
    const auto stride = static_cast<ptrdiff_t>(pt_step);

    const vfloat32m2_t vx = __riscv_vlse32_v_f32m2(x_ptr, stride, vl);
    const vfloat32m2_t vy = __riscv_vlse32_v_f32m2(y_ptr, stride, vl);
    const vfloat32m2_t vz = __riscv_vlse32_v_f32m2(z_ptr, stride, vl);

    min_x = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmin_vs_f32m2_f32m1(vx, __riscv_vfmv_s_f_f32m1(min_x, 1), vl));
    max_x = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmax_vs_f32m2_f32m1(vx, __riscv_vfmv_s_f_f32m1(max_x, 1), vl));
    i += vl;
  }
```

这里每个轴分别维护一个标量 min/max。每个 VL chunk 内用 RVV reduction 规约到标量，再进入下一 chunk。min/max 对遍历顺序不敏感，不引入求和类浮点结合律风险。

### 4.3 indices 路径：按 byte offset gather

indices 版本保留上游的“按 index 访问点”语义。RVV helper 先把 `indices` 转为点内 byte offset，再叠加 xyz 字段 offset，用 `vluxei32` gather 三个字段。

核心条带如下：

```cpp
// filters/include/pcl/filters/impl/voxel_grid.hpp: getMinMax3DFloatDenseIndicesRVV(...)
  while (i < indices.size())
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - i);
    const vint32m2_t vidx = __riscv_vle32_v_i32m2(indices_ptr + i, vl);
    // Gather offsets are byte offsets from cloud->data.data(); keeping this in
    // the RVV helper preserves the public entry's Std fallback for all other layouts.
    const vuint32m2_t base_offsets = __riscv_vreinterpret_v_i32m2_u32m2(
        __riscv_vmul_vx_i32m2(vidx, static_cast<std::int32_t>(pt_step), vl));
    const vuint32m2_t x_offsets = __riscv_vadd_vx_u32m2(base_offsets, x_off, vl);
    const vuint32m2_t y_offsets = __riscv_vadd_vx_u32m2(base_offsets, y_off, vl);
    const vuint32m2_t z_offsets = __riscv_vadd_vx_u32m2(base_offsets, z_off, vl);

    const vfloat32m2_t vx = __riscv_vluxei32_v_f32m2(base, x_offsets, vl);
    const vfloat32m2_t vy = __riscv_vluxei32_v_f32m2(base, y_offsets, vl);
    const vfloat32m2_t vz = __riscv_vluxei32_v_f32m2(base, z_offsets, vl);
```

该路径的收益依赖 indices 局部性和硬件 gather 成本；因此只覆盖 dense、float、大规模 indices。非 dense 的 NaN/Inf 过滤仍交给 Std，避免在 gather 路径上叠加额外 mask 语义。

### 4.4 distance field 路径：mask 后规约

distance field 版本只覆盖 `T=float` 且 `D=float`。过滤语义必须与标量路径一致：

```cpp
if (limit_negative == (distance_value < max_distance && distance_value > min_distance))
  continue;
```

等价地：`limit_negative=false` 时只纳入 `(min_distance, max_distance)` 内的点；`limit_negative=true` 时纳入区间外的点。

核心 mask 片段如下：

```cpp
// filters/include/pcl/filters/impl/voxel_grid.hpp: getMinMax3DFloatDenseDistanceRVV(...)
    const vfloat32m2_t vd = __riscv_vlse32_v_f32m2(distance_ptr, stride, vl);

    // Match the scalar condition exactly: limit_negative excludes points inside
    // (min_distance, max_distance), otherwise only inside points participate.
    const vbool16_t gt_min = __riscv_vmfgt_vf_f32m2_b16(vd, min_distance, vl);
    const vbool16_t lt_max = __riscv_vmflt_vf_f32m2_b16(vd, max_distance, vl);
    const vbool16_t inside = __riscv_vmand_mm_b16(gt_min, lt_max, vl);
    const vbool16_t include = limit_negative ? __riscv_vmnot_m_b16(inside, vl) : inside;

    min_x = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmin_vs_f32m2_f32m1_m(
        include, vx, __riscv_vfmv_s_f_f32m1(min_x, 1), vl));
    max_x = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmax_vs_f32m2_f32m1_m(
        include, vx, __riscv_vfmv_s_f_f32m1(max_x, 1), vl));
```

masked reduction 的初始值沿用当前累计的 min/max。若某个 chunk 没有有效 lane，累计值保持不变；这与标量路径“没有命中点就不更新”的行为一致。

### 4.5 暂缓路径的设计原因

`PointCloud<PointT> + distance field getMinMax3D` 暂缓，不是因为中优先级默认跳过，而是因为泛型点类型的 xyz 访问绑定 `getArray4fMap()`，不能像 `PCLPointCloud2` 一样由运行期字段表证明 byte offset 与字段类型。强行 RVV 化会把 ABI / 布局假设扩散到模板点类型。

`VoxelGrid<PointT>::applyFilter` 前置 voxel index 生成也已评估后暂缓。该段链路包含 `std::floor`、整数索引计算、`emplace_back` 和后续 sort。直接插入 RVV 会改变当前数据组织方式，且难以把收益稳定归因到 index 生成本身；当前保持标量更可维护。

## 5. 测试与验证

### 4.1 QEMU 功能验证

命令：

```bash
make -C test-rvv/filters/voxel_grid run_test_compare
```

结果：std 与 RVV 两套 `test_voxel_grid` 二进制均通过专项用例，覆盖：

- dense packed xyz；
- dense padded xyz；
- 小规模 fallback；
- non-dense finite filtering 保持标量语义；
- indices 路径 std/RVV 对拍；
- distance field 路径 `limit_negative=false/true` 对拍。

### 4.2 QEMU bench 与指令路径

命令：

```bash
make -C test-rvv/filters/voxel_grid run_bench_compare dump_bench_rvv
```

QEMU bench 只作为运行、正确性补充和日志格式证据，不作为性能结论。当前 `output/qemu/analyze_bench_compare.log` 已能解析：

- `Dataset: synthetic dense PCLPointCloud2 float xyz; cases: 64K packed, 1M packed, 1M padded, 1M indexed, 1M distance`；
- `Iterations: 30`；
- `Total Time = Avg × 30`，不再出现 `n/a`。

已确认 RVV bench 二进制中存在目标指令路径：

- `vlse32`；
- `vluxei32`；
- `vfredmin`；
- `vfredmax`；
- `vmflt` / `vmfgt` / `vmand` / `vmnot`；
- `vsetvli ... e32`。

命名空间格式修正后已复跑：

```bash
make -C test-rvv/common/transforms run_test_compare
make -C test-rvv/filters/voxel_grid run_test_compare
make -C test-rvv/filters/voxel_grid run_bench_compare dump_bench_rvv
```

结果：`transforms` 6 个专项用例通过，`voxel_grid` 专项用例通过，覆盖基础路径、indices 路径和 distance field 路径；`analyze_bench_compare.log` 无 `未解析` / `n/a`；反汇编仍包含 `vlse32`、`vluxei32`、`vfredmin`、`vfredmax`、`vmflt` / `vmfgt` / `vmand` / `vmnot`、`vsetvli ... e32`。

完整反汇编产物：`test-rvv/filters/voxel_grid/build/asm/riscv/bench_voxel_grid_rvv.full.asm`。

### 4.3 板卡闭环

`test-rvv/filters/voxel_grid/Makefile` 已补齐板卡部署与运行入口：

```text
deploy_files
deploy_bench_std
deploy_bench_rvv
deploy_test
deploy_board
run_board_test
run_board_bench_compare
fetch_board_logs
```

板卡侧入口：`test-rvv/filters/voxel_grid/board.mk`。

板卡日志已拉回并保存在：`test-rvv/filters/voxel_grid/output/board/bench_compare.log`。

该日志来自 `Milkv-Jupiter`，可作为当前 `voxel_grid` 性能结论来源。结果摘要：

| Benchmark Item | Std Avg | RVV Avg | Speedup |
| --- | ---: | ---: | ---: |
| dense packed xyz 64K | 2.7675 ms | 0.7057 ms | 3.92x |
| dense packed xyz 1M | 43.9617 ms | 12.0485 ms | 3.65x |
| dense padded xyz 1M | 44.6097 ms | 9.7348 ms | 4.58x |
| dense indexed xyz 1M | 25.1025 ms | 6.9791 ms | 3.60x |
| dense distance xyz 1M | 31.4692 ms | 12.4194 ms | 2.53x |
| dense distance negative xyz 1M | 32.4347 ms | 12.0233 ms | 2.70x |

### 4.4 上游测试

`Makefile` 已提供：

```text
run_upstream_test_std
run_upstream_test_rvv
run_upstream_test_compare
run_test_all
```

当前上游测试对拍已尝试运行，但开发机 RISC-V 依赖链缺少 `flann/util/params.h`，编译阶段受阻。该阻塞来自上游 `test/filters/test_filters.cpp` 引入的 search/kdtree/flann 依赖链，不是 `PCLPointCloud2 getMinMax3D` 首轮 RVV 实现新增的错误。

## 5. 当前结论

`voxel_grid` 当前已完成：

- `PCLPointCloud2 getMinMax3D` dense、float、非 indices 基础路径 RVV 分流；
- `PCLPointCloud2 getMinMax3D` dense、float、indices RVV gather 分流；
- `PCLPointCloud2 getMinMax3D` dense、float、非 indices、distance field RVV mask 分流；
- Std/RVV 专项测试；
- QEMU 对拍；
- QEMU bench 运行；
- RVV 指令路径证据；
- 板卡部署入口；
- 板卡 bench 日志与性能结论：`test-rvv/filters/voxel_grid/output/board/bench_compare.log`，在 `Milkv-Jupiter` 上基础路径约 `3.65x`~`4.58x`，indices 路径约 `3.60x`，distance field 路径约 `2.53x`~`2.70x`。

已评估后暂缓：

- `PointCloud<PointT> + distance field getMinMax3D`：泛型点类型布局与 `getArray4fMap()` 绑定，当前无法安全假设 xyz 连续 float 布局；
- `VoxelGrid<PointT>::applyFilter` 前置 voxel index 生成：`std::floor`、`emplace_back` 与后续 sort 主导数据流，直接 RVV 化结构侵入和验证成本偏高。

剩余外部闭环项：

- 上游 VoxelGrid 测试对拍需等待 RISC-V 依赖链补齐 `flann/util/params.h` 后再运行。
