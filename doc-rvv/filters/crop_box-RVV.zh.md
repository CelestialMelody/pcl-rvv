# filters/crop_box RVV 优化说明

## 背景与范围

`pcl::CropBox<PointT>` 是按三维轴对齐盒筛选点的 filters 入口。用户通过 `setMin()` / `setMax()` 配置盒范围，通过 `setNegative()` 决定保留盒内还是盒外点，也可以用 `setTranslation()`、`setRotation()`、`setTransform()` 让筛选在变换后的局部坐标中进行。

本轮优化的直接入口是：

```text
CropBox<PointT>::applyFilter(Indices &indices)
```

它在 PCL 中承担“筛选并生成输入点下标”的职责：

- `filter(indices)` 调用该入口，输出 inlier indices；
- `filter(cloud_out)` 通过 `FilterIndices<PointT>` 基类先调用同一 indices 入口，再按 indices 拷贝点到输出点云；
- `extract_removed_indices_` 为 true 时，同一入口还要输出 removed indices；
- `negative_` 反转 inlier / removed 的盒内盒外定义。

因此本轮 RVV 优化的是 CropBox 的直接筛选主路径。点云输出路径只通过 indices 阶段间接受益，后续点拷贝仍保持原实现。

已覆盖：

- `PointCloud<PointT>` 模板路径；
- 标准布局 `PointT`，且 `x/y/z` 为 `float`；
- `input_->is_dense == true`；
- 输入 indices 是 identity 全量索引；
- transform、translation、rotation 都是 identity / zero；
- 大规模输入；
- `negative_` 和可选 `extract_removed_indices_`。

未覆盖路径全部回退原标量逻辑，包括显式 subset indices、non-dense、任一变换配置、小规模输入、非标准点类型和 `PCLPointCloud2` 特化。

## 与上游差异

公开 API 不变。`crop_box.h` 只新增内部受保护 helper 声明：

- `applyFilterIndicesStd`
- `applyFilterIndicesRVV`

`impl/crop_box.hpp` 中原 `applyFilter` 主体保留为 `applyFilterIndicesStd`。新的公开入口 `applyFilter` 只在 `__RVV10__` 且覆盖条件满足时短路调用 RVV helper，否则调用 Std。

主路径 helper 位于 `pcl` 命名空间，没有额外放入 `pcl::detail`。类型检测只承担语义判断，不使用 `RVV` 后缀。

## 实现结构

新增实体：

| 实体 | 作用 |
| --- | --- |
| `kCropBoxIndicesMinPoints` | 小规模 fallback 阈值，避免短输入承担 RVV strip-mining 和压缩开销 |
| `CropBoxScalar` | 去除 cv/ref 后判断字段类型 |
| `CropBoxXYZCompatible` / `kCropBoxXYZCompatible` | 判断 `PointT` 是否标准布局且 `x/y/z` 为 `float` |
| `applyFilterIndicesStd` | 常驻标量 helper，保留原 `applyFilter` 主体 |
| `applyFilterIndicesRVV` | `__RVV10__` 下的 RVV helper，承载 stride load、区间 mask 和压缩写 |

公开入口结构：

```cpp
#if defined(__RVV10__)
  if constexpr (pcl::kCropBoxXYZCompatible<PointT>)
  {
    if (applyFilterIndicesRVV (indices))
      return;
  }
#endif

  applyFilterIndicesStd (indices);
```

RVV helper 内部还会检查 dense、identity indices、点数、transform/translation/rotation 等运行时条件；不满足时返回 false，让入口落回 Std。

核心 RVV helper 代码片段：

```cpp
if (!fake_indices_ || !input_ || !input_->is_dense ||
    n < pcl::kCropBoxIndicesMinPoints ||
    rotation_ != Eigen::Vector3f::Zero () ||
    translation_ != Eigen::Vector3f::Zero () ||
    !transform_.matrix ().isIdentity ())
  return false;

const auto* base = reinterpret_cast<const std::uint8_t*> (input_->data ());
while (i < n)
{
  const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
  const auto* chunk = base + i * sizeof (PointT);
  const auto stride = static_cast<ptrdiff_t> (sizeof (PointT));

  const vfloat32m2_t vx = __riscv_vlse32_v_f32m2 (reinterpret_cast<const float*> (chunk + offsetof (PointT, x)), stride, vl);
  const vfloat32m2_t vy = __riscv_vlse32_v_f32m2 (reinterpret_cast<const float*> (chunk + offsetof (PointT, y)), stride, vl);
  const vfloat32m2_t vz = __riscv_vlse32_v_f32m2 (reinterpret_cast<const float*> (chunk + offsetof (PointT, z)), stride, vl);

  vbool16_t inside = ...;             // six scalar-equivalent bound comparisons
  const vbool16_t keep = negative_ ? __riscv_vmnot_m_b16 (inside, vl) : inside;
  const vint32m2_t kept_i32 = __riscv_vcompress_vm_i32m2 (source_i32, keep, vl);
  __riscv_vse32_v_i32m2 (out + kept, kept_i32, __riscv_vcpop_m_b16 (keep, vl));
}
```

这段代码展示了三个维护边界：运行时 fallback 只放行 dense identity 主路径；AoS 用 stride load 读取 xyz；输出只压缩 identity source index，因此能保持标量扫描顺序。

## RVV 数据组织

PCL 点云是 AoS。一个 VL chunk 里，RVV 用 `sizeof(PointT)` 作为 stride，从同一批点中分别读 `x/y/z`：

```text
内存:
  p[c]       p[c+1]     p[c+2]     p[c+3]
  x y z ...  x y z ...  x y z ...  x y z ...

RVV:
  vx = vlse32(base + offsetof(x), stride)
  vy = vlse32(base + offsetof(y), stride)
  vz = vlse32(base + offsetof(z), stride)
```

mask 组织：

```text
inside =
  !(x < min_x) & !(y < min_y) & !(z < min_z) &
  !(x > max_x) & !(y > max_y) & !(z > max_z)

keep         = negative ? !inside : inside
removed_mask = negative ? inside : !inside
```

`keep` 用于 `vcompress(source_index, keep)` 写 `indices`，`removed_mask` 在 `extract_removed_indices_` 为 true 时写 `removed_indices_`。当前只覆盖 identity indices，因此 `source_index = c + lane`，不需要先 gather 用户 subset。

为什么 dense 路径不做 finite mask：原标量代码只在 `!input_->is_dense` 时调用 `isFinite()`。dense 路径直接执行边界比较。为了保持 NaN/Inf 语义一致，RVV dense 路径也只表达六个区间比较；non-dense 回退 Std。

## 数值算例

假设 `PointXYZ`，CropBox 设置为：

```text
min = (-1.0, -1.0, -1.0)
max = ( 1.0,  1.0,  1.0)
negative = false
```

某个 VL chunk 从 `c=20` 开始，VL=4：

| lane | source index | x | y | z | inside | keep |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| 0 | 20 | -1.2 | 0.0 | 0.0 | false | false |
| 1 | 21 | -0.5 | 0.8 | 0.1 | true | true |
| 2 | 22 | 0.2 | 1.1 | 0.0 | false | false |
| 3 | 23 | 0.9 | 0.2 | -0.7 | true | true |

标量公式逐点执行：

```text
inside = min_x <= x <= max_x &&
         min_y <= y <= max_y &&
         min_z <= z <= max_z
if !negative && inside -> indices
else if extract_removed_indices -> removed_indices
```

因此该 chunk 输出：

```text
indices chunk         = [21, 23]
removed_indices chunk = [20, 22]
```

RVV 对应：

```text
source_index = [20, 21, 22, 23]
inside mask  = [0, 1, 0, 1]
keep mask    = [0, 1, 0, 1]
removed mask = [1, 0, 1, 0]

vcompress(source_index, keep)         -> [21, 23]
vcompress(source_index, removed_mask) -> [20, 22]
```

这与标量扫描顺序一致。若 `negative=true`，`keep` 和 `removed_mask` 互换。

## 分流与回退

RVV helper 返回 false 的情况：

- `fake_indices_ == false`；
- `input_` 为空；
- `input_->is_dense == false`；
- 点数小于 64；
- 点数超过 `int` 范围；
- `rotation_` 非 zero；
- `translation_` 非 zero；
- `transform_` 非 identity。

公开入口还会在以下情况下直接落回 Std：

- 非 RVV 编译；
- `PointT` 不满足 `kCropBoxXYZCompatible`；
- `PCLPointCloud2` 特化。

## 已暂缓项

显式 subset indices 暂缓：需要先读取 `(*indices_)[lane]`，再按 source index gather AoS `x/y/z`，最后仍要用 `vcompress` 保序输出。该路径访存更不规整，收益不如 identity 主路径明确。

non-dense 暂缓：标量对 invalid 点是 `continue`，不会写入 inlier 或 removed。RVV 可以做 finite mask，但需要额外处理 invalid 与 inside/outside 的 removed 语义，本轮保持 Std。

translation/rotation/transform 暂缓：带变换路径需要把矩阵变换、平移扣除和 inverse rotation 与区间判断融合到一个 VL loop，并证明浮点顺序与标量 `transformPoint` / Eigen 路径一致。本轮先覆盖 identity 主路径。

`PCLPointCloud2` 暂缓：indices 路径位于 `filters/src/crop_box.cpp`，可用字节 stride 扩展；但 cloud 输出路径还涉及 `keep_organized_`、整点复制和 filtered xyz 写 `user_filter_value_`，适合作为后续独立主题。

## 测试与验证

专项测试：

```text
make -C test-rvv/filters/crop_box run_test_compare
```

结果：std 与 RVV 二进制均通过 7 个测试，覆盖 dense identity 主路径、`negative_`、removed indices、cloud 输出、subset fallback、non-dense fallback 和 translation fallback。

上游测试：

```text
make -C test-rvv/filters/crop_box run_upstream_test_compare
```

结果：std 与 RVV 两套 `test/filters/test_clipper.cpp` 均通过 2 个上游测试，包括 `CropBox.Filters`。该上游测试不需要 PCD 参数，专项 Makefile 的 `UPSTREAM_TEST_ARGS` 默认为空。

QEMU bench 和反汇编：

```text
make -C test-rvv/filters/crop_box run_bench_compare dump_bench_rvv
```

结果：

- `output/qemu/analyze_bench_compare.log` 可解析；
- 无 `未解析`、`n/a`、`Total Time 不计算`；
- 反汇编确认 `vlse32.v`、`vcompress.vm`、`vcpop.m`、`vmflt.vf`、`vmfgt.vf`、`vmand.mm`、`vmnot.m`、`vsetvli ... e32,m2`。

QEMU 下 RVV 主路径慢于标量，这只说明 QEMU 模拟执行成本，不作为真实性能结论。

## bench case 含义

所有 speedup 均由分析脚本按 `Std avg / RVV avg` 计算。QEMU 日志只用于构建、正确性补充、日志格式和指令路径证据；真实性能结论使用下方 Milkv-Jupiter 板卡日志。

| case | 函数入口与数据 | 路径 | 证明点 |
| --- | --- | --- | --- |
| `crop_box indices identity 64K` | `filter(indices)`，64K `PointXYZ` dense identity indices | RVV 主路径 | 小于 1M 的主路径仍正确，输出 checksum 与 Std 一致 |
| `crop_box indices identity 1M` | `filter(indices)`，1M `PointXYZ` dense identity indices | RVV 主路径 | 大规模常见筛选场景，验证六个区间比较和保序压缩 |
| `crop_box indices negative 1M` | `filter(indices)`，1M，`negative=true` | RVV 主路径 | 证明盒内/盒外反转后 inlier 顺序正确 |
| `crop_box indices removed 1M` | `filter(indices)`，1M，`extract_removed_indices=true` | RVV 主路径 | 证明 inlier 和 removed 两路 `vcompress` 均保序 |
| `crop_box cloud-out identity 1M` | `filter(cloud_out)`，1M dense identity | 间接受益 | 基类通过 RVV indices 路径获益，点拷贝仍是标量逻辑 |
| `crop_box explicit subset fallback 1M` | 显式 subset indices | fallback | 证明未覆盖 gather/subset 路径保持 Std 语义和接近成本 |
| `crop_box non-dense fallback 1M` | non-dense，含 NaN/Inf | fallback | 证明 invalid 点处理仍由标量路径负责 |
| `crop_box translation fallback 1M` | `setTranslation()` 非 zero | fallback | 证明变换语义路径未被 RVV 主路径误覆盖 |

## 板卡结果

专项 Makefile 已提供：

- `deploy_board`
- `run_board_test`
- `run_board_bench_compare`
- `fetch_board_logs`
- `board_smoke`

`run_board_bench_compare` 会在板卡侧完成 std/RVV bench 和 compare 分析；`fetch_board_logs` 会拉回 `output/board`。

本轮已完成板卡验证：

```text
make -C test-rvv/filters/crop_box run_board_bench_compare fetch_board_logs
```

日志已拉回：

```text
test-rvv/filters/crop_box/output/board/analyze_bench_compare.log
test-rvv/filters/crop_box/output/board/run_bench_std.log
test-rvv/filters/crop_box/output/board/run_bench_rvv.log
```

板卡结果：

| case | Std ms/iter | RVV ms/iter | speedup | 说明 |
| --- | ---: | ---: | ---: | --- |
| `crop_box indices identity 64K` | 1.3925 | 0.4514 | 3.08x | 64K dense identity 主路径，证明中等规模区间 mask + 压缩输出有效 |
| `crop_box indices identity 1M` | 21.9432 | 7.2891 | 3.01x | 1M dense identity 主路径，证明大规模常见筛选场景收益 |
| `crop_box indices negative 1M` | 26.4233 | 11.5818 | 2.28x | `negative=true`，证明盒内/盒外 mask 反转后仍有收益 |
| `crop_box indices removed 1M` | 31.2424 | 14.9327 | 2.09x | `extract_removed_indices=true`，两路 `vcompress` 均执行 |
| `crop_box cloud-out identity 1M` | 23.2527 | 8.8406 | 2.63x | `filter(cloud_out)` 通过 RVV indices 阶段间接受益，点云拷贝仍为原路径 |
| `crop_box explicit subset fallback 1M` | 11.7976 | 11.8345 | 1.00x | 显式 subset fallback，不作为 RVV 主路径性能结论 |
| `crop_box non-dense fallback 1M` | 40.0735 | 40.1955 | 1.00x | non-dense fallback，证明 invalid 点处理保持 Std |
| `crop_box translation fallback 1M` | 23.5203 | 23.6056 | 1.00x | translation fallback，证明 transform 相关路径未被误覆盖 |
