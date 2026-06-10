# filters/approximate_voxel_grid RVV 优化说明

## 1. 函数入口作用

`pcl::ApproximateVoxelGrid<PointT>` 是 `filters` 模块的近似体素下采样滤波器。公开调用路径是用户配置 leaf size、输入点云和 `downsample_all_data_` 后调用 `filter(output)`，最终进入 `ApproximateVoxelGrid<PointT>::applyFilter(PointCloud&)`。

该滤波器不像标准 `VoxelGrid` 那样先完整排序所有 voxel，而是维护一个固定大小的 history hash 表。每个有效输入点被映射到整数 leaf id 和 hash bucket；如果 bucket 中已有不同 leaf，则先 flush 旧 centroid，再累加当前点字段。输出是近似下采样后的 `PointCloud<PointT>`。

本轮 RVV 优化覆盖 `PointT = pcl::PointXYZ` 的生产主路径：

```text
finite(x,y,z) -> floor(xyz * inverse_leaf_size) -> hash bucket
             -> scalar history collision/flush -> PointXYZ centroid output
```

公开 API 不变。非 `PointXYZ`、小规模、非 RVV 编译和非法 `histsize_` 继续走原标量实现。

## 2. 标量路径与覆盖边界

上游标量代码位于 `filters/include/pcl/filters/impl/approximate_voxel_grid.hpp`：

```cpp
if(!pcl::isXYZFinite(point))
  continue;
int ix = static_cast<int> (std::floor (point.x * inverse_leaf_size_[0]));
int iy = static_cast<int> (std::floor (point.y * inverse_leaf_size_[1]));
int iz = static_cast<int> (std::floor (point.z * inverse_leaf_size_[2]));
auto hash = static_cast<unsigned int> ((ix * 7171 + iy * 3079 + iz * 4231) & (histsize_ - 1));
```

原实现随后用 `history_[hash]` 处理冲突、`flush`、`Eigen::VectorXf scratch`、RGB 解包、字段复制和 centroid 累加。本轮只替换 `PointXYZ` centroid-only 形态：RVV 预计算 leaf/hash，history bucket、冲突 flush、末尾 flush 和 `x/y/z/count` 累加保持同序标量状态机。泛型字段聚合、`downsample_all_data_`、动态 scratch 和 RGB/RGBA 打包仍由 `applyFilterStd` 处理。

## 3. 覆盖范围与 fallback

| 项目 | 结论 |
| --- | --- |
| 覆盖点类型 | 生产主路径只覆盖 `PointT = pcl::PointXYZ` |
| 覆盖数据形态 | 全云线性扫描，`histsize_` 为 2 的幂，`input_->size() >= 64` |
| 回退条件 | 非 `PointXYZ`、小规模、非法 `histsize_`、非 RVV 编译 |
| history / flush | 仍为标量状态机，保证冲突输出顺序和 centroid 语义 |
| 泛型字段 / RGB | 不覆盖，保持原 `Eigen::VectorXf scratch` + `FieldList` 标量路径 |
| FRM/FCSR | 使用 `vfcvt.rtz.x.f.v` 加校正，不使用 `_rm` intrinsic，不修改 FRM/FCSR |

## 4. 详细设计

`filters/include/pcl/filters/impl/approximate_voxel_grid.hpp` 新增：

- `applyFilterStd`：原 `applyFilter` 标量实现常驻保留；
- `applyFilterPointXYZRVV`：`__RVV10__` 下的 `PointXYZ` 生产 helper；
- `pcl::approximate_voxel_grid_rvv::computePointXYZLeafHashes`：复用 `pcl/common/rvv_point_load.h` 的 xyz stride-load wrapper；
- `PointXYZHistoryEntry` / `flushPointXYZHistoryEntry`：只服务 `PointXYZ` centroid-only 主路径。

### 4.1 标量流程为什么不需要中间结构

原标量实现是单点流式状态机。每个输入点完成 finite 检查、leaf id、hash 计算后，马上访问 `history_[hash]`：

```text
point[i]
  -> finite?
  -> ix/iy/iz/hash
  -> history_[hash] 冲突则 flush
  -> history_[hash] 累加当前点
```

这个流程一次只处理一个点，当前点的 leaf/hash 只在当前迭代中使用，因此不需要把 `ix/iy/iz/hash/source_index` 写入独立数组。泛型标量路径还需要 `he::centroid`、`Eigen::VectorXf scratch`、RGB/RGBA 临时值和 `FieldList` 字段遍历；这些状态都和 `history_`、`flush` 绑定在同一个逐点循环里。

### 4.2 RVV 流程为什么需要 leaf/hash staging

RVV 路径不能简单把标量循环体逐行向量化，因为 `history_[hash]` 是带冲突和 flush 的有序状态机：

- 同一个 VL chunk 中不同 lane 的 hash 可能相同；
- 不同 lane 可能命中同一 bucket 但 leaf id 不同，必须按原输入顺序触发 flush；
- NaN/Inf lane 需要跳过，剩余 lane 必须保持原输入顺序；
- 末尾 flush 仍按 history bucket 顺序执行。

因此本主题采用“两阶段组织”：

```text
阶段 1：RVV leaf/hash staging
  AoS x/y/z -> finite mask -> floor -> hash -> vcompress
  输出保序的 PointXYZLeafHash[]

阶段 2：scalar history state machine
  按 PointXYZLeafHash[] 顺序访问原 cloud[source_index]
  保持 bucket 冲突、flush 顺序和 centroid 累加语义
```

`PointXYZLeafHash` 是阶段 1 和阶段 2 的边界数据：

```cpp
struct PointXYZLeafHash
{
  int ix;
  int iy;
  int iz;
  unsigned int hash;
  std::uint32_t source_index;
};
```

其中 `source_index` 不是新语义，而是压缩后 lane 对原输入点的引用。阶段 2 仍从原 `cloud[source_index]` 读取 `x/y/z` 累加 centroid，避免在 staging 数组中复制坐标，也保证 checksum 对拍的是同一份输入数据。

### 4.3 为什么不复用泛型 `he`

原 `he` 服务泛型 `PointT`：

- `centroid` 是动态 `Eigen::VectorXf`；
- `flush` 需要处理 `FieldList`、字段计数、RGB/RGBA packing；
- `downsample_all_data_` 会决定是否遍历所有字段。

本轮生产覆盖限定为 `PointT = pcl::PointXYZ`。`PointXYZ` 输出只需要 `x/y/z/count` centroid，使用 `PointXYZHistoryEntry` 可以把生产 RVV helper 的语义收窄到固定字段：

```cpp
struct PointXYZHistoryEntry
{
  int ix;
  int iy;
  int iz;
  int count;
  float sx;
  float sy;
  float sz;
};
```

这样做避免把泛型字段聚合、RGB/RGBA 和动态 scratch 的维护边界误带入 RVV 主路径。非 `PointXYZ` 或需要泛型字段聚合的情况仍由 `applyFilterStd` 使用原 `he` 和 `flush` 处理。对 `PointXYZ` 而言，`downsample_all_data_` 没有额外字段可聚合，因此这个窄状态机与标量输出语义一致。

### 4.4 与已有 RVV 组织模式的差异

此前 filters 主题中常见模式是“predicate + compress”直接输出，例如几何筛选把 `point -> mask -> indices/cloud` 作为一个 VL chunk 完成；或者是规约 / 图像式 stencil，把结果直接写到目标位置。本主题不同：RVV 只批量化前置纯函数 `finite + floor + hash`，而有顺序依赖的 bucket/flush 状态机继续标量执行。

这个模式适合满足以下条件的函数：

- 前置计算是逐点纯函数，算术密度或访存组织适合 VL chunk；
- 后续状态机有顺序依赖，直接向量化会改变 flush、冲突处理或输出顺序；
- 可以定义一个小型 staging 结构，完整保存后续状态机需要的标量决策信息；
- full diagnostic 和生产 bench 能证明 staging 的额外写读成本没有抵消 RVV 收益。

不能直接套用的边界：

- staging 数据过大，导致额外内存流量超过前置计算收益；
- 后续状态机需要泛型字段、动态 scratch 或复杂对象生命周期；
- 压缩后的 lane 顺序不足以恢复标量顺序语义；
- full diagnostic 或生产板卡 case 没有证明整体收益。

### 4.5 `approximate_voxel_grid_rvv` 实体说明

`approximate_voxel_grid_rvv` 是本文件内的实现命名空间，不改变公开 API。它集中放置只服务 `ApproximateVoxelGrid<PointXYZ>` RVV 主路径的 staging 数据、数值 helper 和轻量 flush helper。公开分流仍发生在类方法 `applyFilter` / `applyFilterPointXYZRVV` 中。

| 实体 | 类型 | 输入 / 输出 | 作用与边界 |
| --- | --- | --- | --- |
| `PointXYZLeafHash` | staging 结构体 | `ix/iy/iz/hash/source_index` | 保存 RVV 阶段压缩后的有效点决策信息。`ix/iy/iz/hash` 来自原标量 leaf/hash 公式，`source_index` 指回原输入点，保证后续 centroid 累加读取同一份 cloud 数据。 |
| `PointXYZHistoryEntry` | 轻量 history entry | `ix/iy/iz/count/sx/sy/sz` | 替代泛型 `he` 的 `PointXYZ` 专用状态。只保存 centroid-only 需要的 x/y/z 累加和计数，不承载 `FieldList`、RGB/RGBA 或动态 `Eigen::VectorXf scratch`。 |
| `floorF32ToI32NoFrm` | RVV 数值 helper | `vfloat32m2_t values, vl -> vint32m2_t` | 用 `vfcvt.rtz.x.f.v` 得到向 0 截断结果，再对负数非整数 lane 减 1，匹配 `std::floor`。不使用 `_rm` intrinsic，不读写 FRM/FCSR。 |
| `finiteMask` | RVV mask helper | `vfloat32m2_t values, vl -> vbool16_t` | 用 `value == value` 排除 NaN，用 `abs(value) < inf` 排除正负 Inf。x/y/z 三路 mask 继续用 `vmand` 合并，对应标量 `pcl::isXYZFinite`。 |
| `computePointXYZLeafHashes` | RVV staging helper | `PointCloud<PointXYZ> + inverse_leaf_size + histsize -> vector<PointXYZLeafHash>` | 每个 VL chunk 读取 AoS `x/y/z`，计算 finite mask、floor、hash 和 source index，再用 `vcompress` 保序写出有效 lane。仅在 `n >= 64`、`histsize` 为 2 的幂且点数可用 `uint32_t` 表达时返回 true。 |
| `flushPointXYZHistoryEntry` | 标量输出 helper | `PointXYZHistoryEntry -> output.push_back(PointXYZ)` | 将 `sx/sy/sz/count` 转成 centroid 点。只服务 `PointXYZ`，不处理泛型字段和 RGB。 |
| `applyFilterPointXYZRVV` | 类内生产 helper | `PointCloud& output -> bool` | 调用 `computePointXYZLeafHashes` 后，按 staging 顺序执行标量 bucket 冲突、flush 和 centroid 累加；覆盖条件不满足时返回 false，让公开入口回退 `applyFilterStd`。 |

`computePointXYZLeafHashes` 的输出数组是这个特殊实现的核心边界。它没有复制 `x/y/z`，只保存后续状态机做同序判断所需的最小信息：

```cpp
out_leaf[kept + lane].ix = ...;
out_leaf[kept + lane].iy = ...;
out_leaf[kept + lane].iz = ...;
out_leaf[kept + lane].hash = ...;
out_leaf[kept + lane].source_index = ...;
```

这样组织的收益来自两点：

- leaf/hash 的固定公式被批量化为 VL chunk 上的 stride load、FMA / integer hash、mask 和 `vcompress`；
- bucket/flush 仍按压缩后的 `source_index` 顺序访问原 cloud，避免把顺序相关状态机改写成更复杂且风险更高的向量冲突处理。

需要注意的是，`PointXYZHistoryEntry` 是局部 history 表，不复用类成员 `history_`。这不改变公开输出：每次 `applyFilterPointXYZRVV` 都重新建立局部 history，末尾完整 flush，输出 `width/height/is_dense` 与标量路径一致。未覆盖路径调用 `applyFilterStd` 时仍使用原 `history_` 和 `he` 结构，泛型状态不会被 RVV 窄路径污染。

公开入口按既有 RVV 主题风格短路分流：

```cpp
#if defined(__RVV10__)
  if constexpr (std::is_same_v<PointT, pcl::PointXYZ>)
  {
    if (applyFilterPointXYZRVV (output))
      return;
  }
#endif
  applyFilterStd (output);
```

每个 VL chunk 执行：

1. 用 xyz wrapper 读取 AoS `PointXYZ::x/y/z`；
2. 对每个坐标生成 finite mask，三路 mask 合并；
3. 计算 `scaled = coord * inverse_leaf_size_`；
4. 用向 0 转换加负数校正得到 `floor(scaled)`；
5. 计算 `hash = (ix*7171 + iy*3079 + iz*4231) & (histsize_-1)`；
6. 对有效 lane 用 `vcompress` 保序保留 `ix/iy/iz/hash/source_index`。

核心 floor 等价式：

```cpp
vint32m2_t trunc = __riscv_vfcvt_rtz_x_f_v_i32m2 (values, vl);
vfloat32m2_t trunc_f = __riscv_vfcvt_f_x_v_f32m2 (trunc, vl);
vbool16_t negative_fraction = __riscv_vmflt_vv_f32m2_b16 (values, trunc_f, vl);
vint32m2_t adjust = __riscv_vmerge_vxm_i32m2 (zero, 1, negative_fraction, vl);
floor_i32 = __riscv_vsub_vv_i32m2 (trunc, adjust, vl);
```

这避免 `_rm` intrinsic 和 FRM 污染，同时匹配负坐标 `std::floor`。

RVV leaf/hash 数组进入同一个标量 history 语义：

```cpp
if (entry.count && ((leaf.ix != entry.ix) || (leaf.iy != entry.iy) || (leaf.iz != entry.iz)))
{
  flushPointXYZHistoryEntry (output, entry);
  entry = PointXYZHistoryEntry {};
}
entry.ix = leaf.ix;
entry.iy = leaf.iy;
entry.iz = leaf.iz;
entry.count++;
entry.sx += point.x;
entry.sy += point.y;
entry.sz += point.z;
```

同一 bucket 不同 leaf 时按输入顺序 flush 旧 centroid，末尾再按 history 顺序 flush 剩余 entry。该组织只把固定公式的 leaf/hash 批量化，避免改写状态相关的 bucket/flush 顺序。

## 5. 数值算例与 VL chunk 图示

设 `inverse_leaf_size = (2, 2, 2)`，`histsize = 512`，一个 VL chunk 中 4 个点如下：

| lane | `point.x` | `point.y` | `point.z` | `floor(x*2)` | `floor(y*2)` | `floor(z*2)` |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 0.49 | 0.50 | 1.01 | 0 | 1 | 2 |
| 1 | -0.01 | -0.50 | -1.01 | -1 | -1 | -3 |
| 2 | -1.00 | 1.99 | -0.001 | -2 | 3 | -1 |
| 3 | NaN | 2.00 | 0.00 | 跳过 | 跳过 | 跳过 |

```text
AoS PointXYZ chunk:
  lane:     0        1        2        3
  x/y/z ->  load xyz vectors
             |
             v
          finite mask = [1, 1, 1, 0]
             |
             v
          floor xyz -> hash
             |
             v
          vcompress keeps lanes 0,1,2 in source order
             |
             v
          scalar history bucket / flush / centroid
```

lane 1 的 `-0.01 * 2 = -0.02`，向 0 截断是 `0`，校正后为 `-1`，与标量 `std::floor` 一致。

## 6. 验证证据

专项测试：

- `make -C test-rvv/filters/approximate_voxel_grid run_test_compare` 通过；
- std/RVV 两套构建均通过 6 个测试，覆盖负数 floor、finite mask、RVV/Std leaf-hash 对拍、full `PointXYZ` 诊断对拍和生产 `ApproximateVoxelGrid<PointXYZ>` 输出对拍。

QEMU：

- `make -C test-rvv/filters/approximate_voxel_grid run_bench_compare` 通过；
- `output/qemu/analyze_bench_compare.log` 可解析，无 `未解析`、`n/a`、`Total Time 不计算`；
- QEMU 不作为性能结论。

反汇编：

- `make -C test-rvv/filters/approximate_voxel_grid dump_bench_rvv` 生成反汇编；
- `output/qemu/rvv_asm_check.log` 确认 `vlsseg3e32.v`、`vfcvt.rtz.x.f.v`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`；
- 完整二进制中的 `frrm/fsrm` 来自 bench checksum 的 `std::lround`，不属于 leaf-hash RVV helper。

板卡：

- `make -C test-rvv/filters/approximate_voxel_grid run_board_test run_board_bench_compare fetch_board_logs` 通过；
- 日志位于 `test-rvv/filters/approximate_voxel_grid/output/board/`。

## 7. 板卡结果与 case 解释

speedup 计算方式为 `Std avg ms / RVV avg ms`，设备为 Milkv-Jupiter，iterations 为 30。

| case | 入口 / 数据 | 路径含义 | speedup | 证明点 |
| --- | --- | --- | ---: | --- |
| `approx voxel leaf-hash diag 64K` | 64K `PointXYZ`，无 invalid | bench-diagnosis leaf-id/hash RVV | 2.00x | 前置 `finite + floor + hash` 片段在小中规模上有板卡收益 |
| `approx voxel leaf-hash diag 1M` | 1M `PointXYZ`，无 invalid | bench-diagnosis leaf-id/hash RVV | 1.99x | 大规模前置片段收益稳定 |
| `approx voxel finite leaf-hash diag 1M` | 1M `PointXYZ`，含 NaN / Inf | bench-diagnosis finite mask + leaf-id/hash RVV | 1.98x | finite mask 与压缩输出没有抵消片段收益 |
| `approx voxel full diag 64K` | 64K `PointXYZ`，无 invalid，`histsize=512` | RVV leaf/hash + 标量 bucket/flush/centroid | 1.67x | bucket、flush 和 centroid 累加纳入后仍有整体收益 |
| `approx voxel full diag 1M` | 1M `PointXYZ`，无 invalid，`histsize=512` | RVV leaf/hash + 标量 bucket/flush/centroid | 1.68x | 大规模 full `PointXYZ` 形态下收益没有被下游标量工作完全抵消 |
| `approx voxel finite full diag 1M` | 1M `PointXYZ`，含 NaN / Inf，`histsize=512` | RVV finite leaf/hash + 标量 bucket/flush/centroid | 1.67x | invalid 跳过与 full diagnostic 同时存在时仍保持收益 |
| `approx voxel production pointxyz 64K` | 64K `PointXYZ`，调用生产 `ApproximateVoxelGrid<PointXYZ>::filter(output)` | 生产 RVV 主路径 | 1.92x | 证明接入生产入口后收益成立；泛型字段路径不参与该结论 |

## 8. 结论

`ApproximateVoxelGrid<PointXYZ>::filter(output)` 已接入 RVV 生产主路径。Milkv-Jupiter 上生产 case 为 `1.92x`，full `PointXYZ` 诊断为 `1.67x` 到 `1.68x`，明显不同于 `shadowpoints` 的生产回退场景，也强于此前已接入的弱收益 `fast_bilateral`。

生产覆盖仍限定为 `PointT = pcl::PointXYZ`、点数 `>=64`、`histsize_` 为 2 的幂。非 `PointXYZ`、小规模、非 RVV 编译、泛型字段、`downsample_all_data_` 和 RGB/RGBA 特例保持原 `applyFilterStd` 标量语义。
