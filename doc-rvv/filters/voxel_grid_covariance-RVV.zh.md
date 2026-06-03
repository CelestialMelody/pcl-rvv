# filters/voxel_grid_covariance RVV 优化说明

## 背景与范围

`pcl::VoxelGridCovariance<PointT>` 是 NDT 等算法常用的 covariance voxel 结构。它继承 `VoxelGrid<PointT>`，`filter(output, searchable)` 会先把输入点分到 voxel leaf，再为每个满足 `min_points_per_voxel_` 的 leaf 计算：

- centroid / mean；
- covariance；
- eigen values / eigen vectors；
- inverse covariance；
- 可选 searchable centroid cloud 和 kd-tree。

本轮优化的直接入口是：

```text
VoxelGridCovariance<PointT>::applyFilter(PointCloud &output)
```

它在 PCL 管线中承担“按 voxel 分桶、规约点、生成 covariance voxel 状态”的职责。公开 `filter(output, searchable)` 调用该入口；若 `searchable=true`，入口完成后再用输出 centroid 构建搜索结构。

本轮 RVV 只覆盖 first pass 中的 voxel leaf index 计算：

```text
ijk = floor(point.xyz * inverse_leaf_size)
idx = (ijk - min_b).dot(divb_mul)
```

per-leaf `std::map` 插入、mean / centroid 累加、covariance、eigen solver、inverse covariance、`leaf_layout_` 和 kd-tree 仍保持标量路径。这样做的原因是后半段状态依赖重、数值顺序敏感，直接 RVV 化会明显扩大语义风险。

## 与上游差异

公开 API 不变。`voxel_grid_covariance.h` 未新增公开接口。

`impl/voxel_grid_covariance.hpp` 新增内部实体：

| 实体 | 作用 |
| --- | --- |
| `computeVoxelGridCovarianceLeafIndexStd` | 常驻标量 helper，保留原 `Eigen::floor` leaf id 公式 |
| `kVoxelGridCovarianceIndexMinPoints` | 小规模 fallback 阈值 |
| `kRoundDownMode` | RVV float-to-int round-down 模式，语义对应 `floor` |
| `getVoxelGridCovarianceRoundingMode` / `setVoxelGridCovarianceRoundingMode` | 保存 / 恢复 FRM，避免显式 round-down 转换影响后续标量 fallback |
| `VoxelGridCovarianceScalar` | 去除 cv/ref 后判断字段类型 |
| `VoxelGridCovarianceXYZCompatible` / `kVoxelGridCovarianceXYZCompatible` | 判断 `PointT` 是否标准布局且 `x/y/z` 为 `float` |
| `computeVoxelGridCovarianceLeafIndicesRVV` | `__RVV10__` 下批量预计算 dense AoS leaf id |

主路径 helper 位于 `pcl` 命名空间，不额外放入 `pcl::detail`。traits 和常量使用语义命名，不使用额外 RVV 后缀。

## 分流条件

RVV helper 只在以下条件全部满足时参与：

- `__RVV10__`；
- `PointT` 标准布局，且 `x/y/z` 为 `float`；
- `input_->is_dense == true`；
- 点数不少于 `64`；
- 点数不超过 `int` 范围；
- `filter_field_name_` 为空；
- `downsample_all_data_ == false`；
- 点类型无 `rgb` / `rgba` 字段。

以下路径全部保持标量：

- non-dense；
- distance field 过滤；
- RGB / RGBA 特化；
- `downsample_all_data_`；
- 小规模输入；
- covariance / eigen / inverse covariance / kd-tree。

## 实现设计

标量 helper 保留原公式：

```cpp
template<typename PointT> int
computeVoxelGridCovarianceLeafIndexStd (const PointT& point,
                                        const Eigen::Array4f& inverse_leaf_size,
                                        const Eigen::Vector4i& min_b,
                                        const Eigen::Vector4i& divb_mul)
{
  const Eigen::Vector4i ijk =
      Eigen::floor(point.getArray4fMap() * inverse_leaf_size)
          .template cast<int>();
  return (ijk - min_b).dot(divb_mul);
}
```

RVV helper 只生成 `leaf_indices`，后续标量 loop 仍按原顺序读取每个 point 并写 `leaves_[idx]`：

```cpp
const unsigned int saved_rounding_mode = pcl::getVoxelGridCovarianceRoundingMode ();

while (i < n)
{
  const vfloat32m2_t vx = __riscv_vlse32_v_f32m2 (... offsetof (PointT, x) ..., stride, vl);
  const vfloat32m2_t vy = __riscv_vlse32_v_f32m2 (... offsetof (PointT, y) ..., stride, vl);
  const vfloat32m2_t vz = __riscv_vlse32_v_f32m2 (... offsetof (PointT, z) ..., stride, vl);

  const vfloat32m2_t sx = __riscv_vfmul_vf_f32m2 (vx, inverse_leaf_size[0], vl);
  const vfloat32m2_t sy = __riscv_vfmul_vf_f32m2 (vy, inverse_leaf_size[1], vl);
  const vfloat32m2_t sz = __riscv_vfmul_vf_f32m2 (vz, inverse_leaf_size[2], vl);

  vint32m2_t ix = __riscv_vfcvt_x_f_v_i32m2_rm (sx, pcl::kRoundDownMode, vl);
  vint32m2_t iy = __riscv_vfcvt_x_f_v_i32m2_rm (sy, pcl::kRoundDownMode, vl);
  vint32m2_t iz = __riscv_vfcvt_x_f_v_i32m2_rm (sz, pcl::kRoundDownMode, vl);

  ix = __riscv_vsub_vx_i32m2 (ix, min_b[0], vl);
  iy = __riscv_vsub_vx_i32m2 (iy, min_b[1], vl);
  iz = __riscv_vsub_vx_i32m2 (iz, min_b[2], vl);

  vint32m2_t idx = __riscv_vmul_vx_i32m2 (ix, divb_mul[0], vl);
  idx = __riscv_vmacc_vx_i32m2 (idx, divb_mul[1], iy, vl);
  idx = __riscv_vmacc_vx_i32m2 (idx, divb_mul[2], iz, vl);
  __riscv_vse32_v_i32m2 (out + i, idx, vl);
}

pcl::setVoxelGridCovarianceRoundingMode (saved_rounding_mode);
```

这段代码的维护重点：

- AoS 只能用 stride load 读取 xyz；
- `vfcvt_x_f_v_i32m2_rm(..., kRoundDownMode, ...)` 对齐 `Eigen::floor`，避免负坐标被截断到 0 方向；
- 显式 round-down 转换可能修改 FRM；helper 进入时保存、退出前恢复 FRM，保证同一进程后续 distance / non-dense 等标量 fallback 不继承 RDN 舍入模式；
- `idx` 只作为后续标量 `leaves_[idx]` 的输入，不改变 leaf 插入和累加顺序；
- `divb_mul[3] == 0`，因此只计算 xyz 三维。

### floor、RDN 与 FRM 恢复

本主题的 leaf id 公式必须使用 `floor`，不能用向 0 截断替代：

```text
floor( 1.7) =  1
floor(-0.2) = -1
truncate(-0.2) = 0
```

正坐标上 `floor` 与截断经常相同，但负坐标会直接影响 voxel 编号。若 lane 中 `x=-0.26`、`inverse_leaf_size=2`，则 `x * inverse_leaf_size = -0.52`。标量 `Eigen::floor` 得到 `-1`；若 RVV 使用默认 float-to-int 截断，会得到 `0`，该点会落入错误 leaf。

因此 RVV helper 使用 explicit rounding mode：

```cpp
vint32m2_t ix = __riscv_vfcvt_x_f_v_i32m2_rm (sx, pcl::kRoundDownMode, vl);
```

`kRoundDownMode = 2` 对应 RDN，即向负无穷舍入，与 `floor` 对齐。

这里有一个容易遗漏的副作用：RISC-V 的浮点舍入模式 FRM 属于当前浮点环境状态，不是 C++ 局部变量。部分工具链 / QEMU 会为 explicit-RM intrinsic 生成 `fsrmi` / `fsrm` / `frrm` 指令。如果 helper 结束时没有恢复原 FRM，同一进程后续标量路径可能继续在 RDN 下运行。

本主题初版 bench 曾暴露这个问题：

| 运行顺序 | `non-dense fallback 256K` 结果 |
| --- | --- |
| 单独运行 non-dense fallback | Std / RVV checksum 一致 |
| 先运行 dense RVV 主路径，再运行 non-dense fallback | RVV checksum 曾与 Std 不一致 |

当时的旧 RVV 日志中，Std checksum 为 `5717660375792973828`，RVV 曾为 `16415332133840058372`。进一步诊断发现，non-dense 本身没有进入 `computeVoxelGridCovarianceLeafIndicesRVV`，`common::getMinMax3D` 在 non-dense 输入下也仍走 Standard 分支；差异来自前序 dense RVV helper 留下的 RDN 舍入模式，使后续标量 min/max、centroid/covariance 等浮点计算产生 1 ulp 级漂移。

最终实现采用“覆盖条件检查后保存 FRM，所有 VL chunk 完成后恢复 FRM”的结构：

```cpp
const unsigned int saved_rounding_mode = pcl::getVoxelGridCovarianceRoundingMode ();

while (i < n)
{
  // vlse32 x/y/z
  // scale by inverse_leaf_size
  // vfcvt_x_f_v_i32m2_rm(..., kRoundDownMode, vl)
  // linear leaf id
  // vse32 leaf_indices
}

pcl::setVoxelGridCovarianceRoundingMode (saved_rounding_mode);
```

修复后，同一进程中先跑 dense RVV 主路径再跑 non-dense fallback，min/max、输出 size 和 checksum 均与 Std 一致；当前 bench 中 `non-dense fallback 256K` Std / RVV checksum 均为 `5717660375792973828`。

后续 RVV 优化中，凡是使用 `_rm` 版本 `vfcvt` 或直接修改 FCSR/FRM，都应参考通用说明：`doc-rvv/rvv/RVV Float-to-Int Rounding and FRM.zh.md`。

公开入口附近的使用方式：

```cpp
#if defined(__RVV10__)
std::vector<int> leaf_indices;
bool use_leaf_indices_rvv = false;
if constexpr (pcl::kVoxelGridCovarianceXYZCompatible<PointT>)
{
  use_leaf_indices_rvv =
      !downsample_all_data_ &&
      rgba_index < 0 &&
      pcl::computeVoxelGridCovarianceLeafIndicesRVV (*input_, inverse_leaf_size_, min_b_, divb_mul_, leaf_indices);
}
#endif
```

后续循环只在 `use_leaf_indices_rvv` 为 true 时读取预计算 id，否则调用 Std helper。

## VL chunk 数值算例

设 leaf size 为：

```text
leaf_size = (0.5, 0.5, 0.5)
inverse_leaf_size = (2, 2, 2)
min_b = (-2, -1, 0)
divb_mul = (1, 5, 20, 0)
```

某个 VL chunk 从点 `c` 开始，VL=4：

| lane | `cloud[c+lane].x` | `y` | `z` | `floor(x*2)` | `floor(y*2)` | `floor(z*2)` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | -0.75 | -0.10 | 0.20 | -2 | -1 | 0 |
| 1 | -0.26 | 0.49 | 0.51 | -1 | 0 | 1 |
| 2 | 0.10 | -0.60 | 0.99 | 0 | -2 | 1 |
| 3 | 0.90 | 0.20 | 1.20 | 1 | 0 | 2 |

标量公式：

```text
idx = (ix - min_b[0]) * 1 +
      (iy - min_b[1]) * 5 +
      (iz - min_b[2]) * 20
```

对应结果：

| lane | `(ix,iy,iz)` | `(ix,iy,iz) - min_b` | `idx` |
| --- | --- | --- | ---: |
| 0 | `(-2,-1,0)` | `(0,0,0)` | 0 |
| 1 | `(-1,0,1)` | `(1,1,1)` | 26 |
| 2 | `(0,-2,1)` | `(2,-1,1)` | 17 |
| 3 | `(1,0,2)` | `(3,1,2)` | 48 |

RVV chunk 中 `vfcvt.x.f` 使用 round-down 模式，所以 lane 1 的 `-0.26 * 2 = -0.52` 得到 `-1`，与 `floor` 一致；若误用截断，会得到 `0`，leaf id 会错。

```text
AoS memory: p[c]      p[c+1]    p[c+2]    p[c+3]
            x y z ... x y z ... x y z ... x y z ...

RVV:
  vlse32(x/y/z) -> scale -> floor/RDN -> linear idx -> vse32 leaf_indices

Scalar continuation:
  for each c+lane:
    idx = leaf_indices[c+lane]
    Leaf& leaf = leaves_[idx]
    accumulate mean / cov / centroid
```

## 测试与验证

专项测试：

```text
make -C test-rvv/filters/voxel_grid_covariance run_test_compare
```

结果：std 与 RVV 二进制均通过 6 个用例，覆盖 dense 主路径、80 点负坐标手算 centroid、`save_leaf_layout_`、`searchable=true`、distance field fallback、non-dense fallback 和小规模 fallback。

上游定向测试：

```text
make -C test-rvv/filters/voxel_grid_covariance run_upstream_test_compare
```

`UPSTREAM_TEST_ARGS` 指向仓库已有数据，并用 `--gtest_filter=VoxelGridCovariance.Filters` 聚焦当前主题。结果：std 与 RVV 均通过。

QEMU bench：

```text
make -C test-rvv/filters/voxel_grid_covariance run_bench_compare
```

`output/qemu/analyze_bench_compare.log` 可解析 `Dataset:`、`Iterations:` 和 `Total Time`，无 `未解析`、`n/a` 或 `Total Time 不计算`。QEMU 只作为构建、运行、日志格式和指令路径证据，不作为性能结论。

板卡 bench：

```text
make -C test-rvv/filters/voxel_grid_covariance run_board_bench_compare fetch_board_logs
```

本地日志：

- `test-rvv/filters/voxel_grid_covariance/output/board/run_bench_std.log`
- `test-rvv/filters/voxel_grid_covariance/output/board/run_bench_rvv.log`
- `test-rvv/filters/voxel_grid_covariance/output/board/analyze_bench_compare.log`

板卡环境：

- Device: `Milkv-Jupiter`
- Dataset: synthetic PointXYZ clouds; dense leaf-index RVV cases plus distance/non-dense fallback cases
- Iterations: 8

反汇编：

```text
make -C test-rvv/filters/voxel_grid_covariance clean_bench_rvv dump_bench_rvv
```

`build/asm/riscv/bench_voxel_grid_covariance_rvv.full.asm` 中确认：

- `vlse32.v`
- `vfcvt.x.f.v`
- `vmul.vx`
- `vmacc.vx`
- `vse32.v`
- `vsetvli`

## bench case 含义

speedup 计算方式为 `Std Avg / RVV Avg`。QEMU 数值只作为构建、运行、格式和指令路径补充证据；真实性能结论使用板卡 `Milkv-Jupiter` 日志。

| case | 函数入口与数据 | 是否命中 RVV 主路径 | 板卡结果 | 说明 |
| --- | --- | --- | ---: | --- |
| `vgcov dense leaf-index 64K` | `VoxelGridCovariance<PointXYZ>::filter(output)`，64K dense 点，`downsample_all_data_=false`，无 distance field | 是 | 1.50x | 验证标准 dense first pass leaf id 预计算路径；Std/RVV checksum 均为 `16873979133168490664` |
| `vgcov dense leaf-index 256K` | 同上，256K 点 | 是 | 1.46x | 扩大输入规模后主路径仍稳定受益；Std/RVV checksum 均为 `11578983304961162696` |
| `vgcov dense save-layout 256K` | `save_leaf_layout_=true`，256K dense 点 | 是，后续 leaf layout 标量 | 1.51x | 说明 `leaf_layout_` 维护仍由标量后续逻辑完成，但前置 leaf-id RVV 可间接受益 |
| `vgcov dense searchable 64K` | `filter(output, true)`，64K dense 点 | 是，kd-tree 构建标量 | 1.52x | 说明 searchable 入口可运行，RVV 只优化 kd-tree 前的 leaf-id first pass |
| `vgcov distance-field fallback 256K` | `filter_field_name_="z"`，`setFilterLimits(-1.75,1.75)`，256K dense 点 | 否 | 0.79x | 证明 distance field 路径保持标量语义；该 case 不作为 RVV 主路径性能结论 |
| `vgcov non-dense fallback 256K` | `input_->is_dense=false`，含 NaN / Inf，256K 点 | 否 | 0.95x | 证明显式 RDN 转换恢复 FRM 后，后续 non-dense 标量 fallback 保持原语义；Std/RVV checksum 均为 `5717660375792973828`，不作为 RVV 主路径性能结论 |

## 板卡状态

专项 Makefile 和 `board.mk` 已提供板卡闭环入口：

- `deploy_board`
- `run_board_test`
- `run_board_bench_compare`
- `fetch_board_logs`

板卡 bench compare 已完成，日志已拉回 `test-rvv/filters/voxel_grid_covariance/output/board/`。主路径 case 在 `Milkv-Jupiter` 上约 `1.46x` 到 `1.52x`；distance field 与 non-dense fallback 保持语义一致但不作为 RVV 主路径收益结论。

## 已知限制

- RVV 只预计算 leaf id，不覆盖 `std::map` 插入、leaf 累加、covariance、eigen 和 inverse covariance。
- distance field、non-dense、RGB / RGBA、`downsample_all_data_` 保持标量。
- 本路径引入 `leaf_indices` 临时数组；若真实硬件显示该数组写读成本超过 leaf id 计算收益，应考虑回退或改为更小范围启用。
- `VoxelGridCovariance` 主成本常由 map、matrix 和 kd-tree 主导，leaf id RVV 的收益上限有限。
