# filters `filter_indices` / `filter` RVV 优化说明

## 1. 背景与范围

本主题覆盖 filters 中通用 NaN 清理路径：

- `pcl::removeNaNFromPointCloud(const PointCloud<PointT>&, Indices&)`
- `pcl::removeNaNFromPointCloud(const PointCloud<PointT>&, PointCloud<PointT>&, Indices&)`
- `pcl::removeNaNNormalsFromPointCloud(...)` 作为中优先级候选已尝试 RVV，但因板卡结果不佳，公开入口保持标量。

公开 API 不变。实现采用常驻 `*_Std` helper、`__RVV10__` 下 `*_RVV` helper、公开入口短路选择；主路径 helper 位于 `pcl` 命名空间。

## 2. 分流条件

RVV 只覆盖标准 AoS 点类型的 non-dense xyz 清理：

| 条件 | 处理 |
| --- | --- |
| `__RVV10__` 未定义 | 全部走 Std |
| `PointT` 非标准布局，或 `x/y/z` 不是 `float` | Std |
| `cloud_in.is_dense == true` | Std，保持 legacy `0..N-1` / copy 行为 |
| `size < 64` | Std，避免小规模启动成本 |
| 点数超过 `int` indices 表达范围 | Std |
| `removeNaNNormalsFromPointCloud` | Std，原因见第 6 节 |

## 3. RVV 数据组织

PCL `PointCloud<PointT>` 是 AoS 布局，连续点之间 stride 为 `sizeof(PointT)`。RVV helper 每次处理一个 VL chunk：

```text
cloud.points:
  p[c+0]     p[c+1]     p[c+2]     ... p[c+vl-1]
  x y z ...  x y z ...  x y z ...

RVV:
  strided_load3_fields_f32m2<sizeof(PointT), offsetof(x/y/z)>(chunk)
    -> vx, vy, vz
  m  = finite(vx) & finite(vy) & finite(vz)
  source = c + vid
  vcompress(source, m) -> ordered original indices
```

finite 判定与标量语义一致：

```text
finite(v) = (v == v) && (abs(v) < +Inf)
```

因此 NaN、`+Inf`、`-Inf` 都会被过滤。

## 4. 可手算算例

设一个 VL chunk 覆盖 `c = 8` 开始的 4 个点：

| lane | 原始下标 | x | y | z | 标量 `isfinite(x)&&isfinite(y)&&isfinite(z)` |
| ---: | ---: | ---: | ---: | ---: | --- |
| 0 | 8  | 1 | 2 | 3 | true |
| 1 | 9  | NaN | 5 | 6 | false |
| 2 | 10 | 7 | 8 | +Inf | false |
| 3 | 11 | -1 | -2 | -3 | true |

RVV mask 为：

```text
m = [1, 0, 0, 1]
source = [8, 9, 10, 11]
vcompress(source, m) = [8, 11]
```

标量循环会依次保留点 8 和点 11；RVV 压缩结果与标量输出顺序完全一致。cloud-out 路径随后按 `[8, 11]` 标量复制 `cloud_out[0] = cloud_in[8]`、`cloud_out[1] = cloud_in[11]`，避免破坏 `PointT` 中 xyz 之外的字段。

## 5. 详细设计

主要文件：

- `filters/include/pcl/filters/impl/filter_indices.hpp`
- `filters/include/pcl/filters/impl/filter.hpp`

### 5.1 helper 拆分与命名

本次没有改变公开 API，而是把原函数主体抽成常驻标量 helper：

- `removeNaNFromPointCloudIndicesStd`：保留 indices-only 原标量扫描；
- `removeNaNFromPointCloudStd`：保留 cloud-out 原标量扫描和 dense copy 语义；
- `removeNaNNormalsFromPointCloudStd`：保留 normals 原标量扫描。

实际承载 RVV 指令路径的 helper 使用 `RVV` 后缀，例如 `removeNaNFromPointCloudIndicesRVV` 和 `removeNaNFromPointCloudRVV`。类型检测、阈值常量和 mask 小工具描述的是覆盖条件或通用语义，不是分流入口，因此使用语义名，不在名字中额外带 `RVV/Rvv`。这可以避免读者把 `FilterXYZCompatible` 误解成“只能被 RVV 使用的类型”，它实际表达的是“可用 `offsetof(PointT, x/y/z)` 做 float stride load 的点类型”。

### 5.2 类型兼容检测

RVV 路径需要在模板实例化期确认 `PointT` 有对应字段，且这些字段是 `float`。否则类似 `offsetof(PointT, x)`、`cloud_in[i].x` 或 stride float load 会在不支持 xyz 的点类型上形成非法实例化。实现使用 SFINAE traits 先检测字段存在，再检测标准布局和字段类型：

```cpp
template <typename T>
using FilterScalar = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename PointT, typename = void>
struct FilterXYZCompatible : std::false_type {};

template <typename PointT>
struct FilterXYZCompatible<
    PointT,
    std::void_t<decltype(std::declval<PointT>().x),
                decltype(std::declval<PointT>().y),
                decltype(std::declval<PointT>().z)>>
: std::bool_constant<
      std::is_standard_layout_v<PointT> &&
      std::is_same_v<FilterScalar<decltype(std::declval<PointT>().x)>, float> &&
      std::is_same_v<FilterScalar<decltype(std::declval<PointT>().y)>, float> &&
      std::is_same_v<FilterScalar<decltype(std::declval<PointT>().z)>, float>> {};
```

`FilterIndicesScalar` / `FilterScalar` 只负责去掉 `const`、`volatile` 和引用，确保 `const float&` 这类字段访问表达式仍能归一化为 `float` 判断。`FilterIndicesXYZCompatible` 用于 indices-only 文件，`FilterXYZCompatible` 用于 cloud-out xyz 路径，`FilterNormalCompatible` 用于已评估的 normals prototype。

### 5.3 公开入口短路

indices-only 入口只做短路选择，不把 RVV 主体塞进公开 API：

```cpp
#if defined(__RVV10__)
  if constexpr (pcl::kFilterIndicesXYZCompatible<PointT>)
    if (pcl::removeNaNFromPointCloudIndicesRVV (cloud_in, index))
      return;
#endif
  pcl::removeNaNFromPointCloudIndicesStd (cloud_in, index);
```

cloud-out 路径同样先尝试 `removeNaNFromPointCloudRVV`，未命中自然回到 `removeNaNFromPointCloudStd`。运行时 fallback 放在 RVV helper 内部处理，包括 dense、小规模和 indices 溢出风险；这样公开入口只表达“能尝试 RVV 就尝试，失败则保持原路径”。

### 5.4 finite mask、公共 xyz load helper 与 ordered compaction

indices-only RVV 主体按 VL chunk 处理 AoS 点云。每个 chunk 通过公共 `pcl::rvv_load::strided_load3_fields_f32m2` 读取 `x/y/z`，字段 offset 仍是 `offsetof(PointT, x/y/z)`，点间步长仍是 `sizeof(PointT)`。该 helper 只负责字段 layout 下的 load；indices-only 的 dense/sparse 判断、阈值、`int` 下标边界仍由 `removeNaNFromPointCloudIndicesRVV` 控制。

```cpp
vfloat32m2_t vx, vy, vz;
pcl::rvv_load::strided_load3_fields_f32m2<sizeof (PointT),
                                          offsetof (PointT, x),
                                          offsetof (PointT, y),
                                          offsetof (PointT, z)> (
    chunk, vl, vx, vy, vz);

// NaN fails x == x; +/-Inf is rejected by abs(v) < Inf. vcompress keeps the
// scalar scan order while each VL chunk writes only finite source indices.
vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16(vx, vx, vl);
finite = __riscv_vmand_mm_b16(finite, __riscv_vmfeq_vv_f32m2_b16(vy, vy, vl), vl);
finite = __riscv_vmand_mm_b16(finite, __riscv_vmfeq_vv_f32m2_b16(vz, vz, vl), vl);
finite = __riscv_vmand_mm_b16(finite,
    __riscv_vmflt_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vx, vl),
                               std::numeric_limits<float>::infinity(), vl), vl);
```

`vmfeq(v, v)` 排除 NaN，`abs(v) < +Inf` 排除正负无穷。随后使用 `vid + chunk_begin` 得到原始下标向量，`vcompress` 只保留 mask 为 true 的 lane，`vcpop` 得到本 chunk 保留数量。`vcompress` 保持 lane 顺序，因此输出 indices 与标量从小到大扫描一致。

### 5.5 cloud-out 的通用 mask helper

`removeNaNFromPointCloudMask<PointT, kF0Off, kF1Off, kF2Off>` 是 cloud-out 和 normals prototype 共用的小工具。它不带 `RVV` 命名，因为它表达的是“三个 float 字段 finite mask + ordered indices + 标量复制”的通用结构；真正启用它的是 `removeNaNFromPointCloudRVV` / `removeNaNNormalsFromPointCloudRVV`。

该 helper 的 RVV 覆盖只到“找出保留点下标”：

```cpp
const vuint32m2_t source = __riscv_vadd_vx_u32m2(local, static_cast<std::uint32_t>(i), vl);
const vint32m2_t compact = __riscv_vcompress_vm_i32m2(source_i32, finite, vl);
const std::size_t count = __riscv_vcpop_m_b16(finite, vl);
__riscv_vse32_v_i32m2(index.data() + kept, compact, count);

// PointT copy remains scalar because arbitrary point structs can carry non-float
// fields; RVV is used for finite-mask and ordered index compaction only.
for (std::size_t k = 0; k < count; ++k)
{
  const int src = index[chunk_out + k];
  cloud_out[kept] = cloud_in[src];
  index[kept] = src;
  ++kept;
}
```

`PointT` 复制保持标量是有意取舍：PCL 点类型可能包含 RGB、label、normal、padding 或用户字段，直接用 RVV 对整个结构做 scatter/copy 容易引入字段覆盖和 alias 风险。当前方案只向量化 finite 判断和 indices 压缩，输出点仍严格等价于标量 `cloud_out[j] = cloud_in[i]`。

### 5.6 小规模与边界

`kRemoveNaNIndicesMinPoints` 和 `kRemoveNaNCloudMinPoints` 当前为 64。低于阈值时直接返回 false 走 Std，避免小输入下 `vsetvl`、mask 和压缩指令的固定成本超过收益。点数超过 `int` 可表达范围也回退 Std，因为 `Indices` 存储的是 `int`，RVV 下标向量最终也写回 `vint32m2_t`。

## 6. 已尝试但回退的 normals 路径

`removeNaNNormalsFromPointCloud` 标为中优先级，因此已实现并测试 RVV mask prototype：

- 使用 normal_x/y/z stride load 生成 finite mask；
- `vcompress` 输出原始 indices；
- 为保持 `cloud_out.is_dense`，还需要对保留点调用 `pcl::isFinite(cloud_in[src])` 检查 xyz；
- 点对象复制仍是标量。

板卡旧版 prototype 在 `removeNaNNormals cloud-out sparse normal 1M` 上约 `0.62x`，主要原因是 normal mask 的收益被标量点复制和额外 xyz dense 检查抵消。最终公开入口保持 Std。最终板卡日志中该 case 为 `1.00x`，说明 RVV 构建下已不再启用退化路径。

## 7. 测试与证据

专项命令：

```bash
make -C test-rvv/filters/filter_indices run_test_compare
make -C test-rvv/filters/filter_indices run_bench_compare dump_bench_rvv
make -C test-rvv/filters/filter_indices run_board_test
make -C test-rvv/filters/filter_indices run_board_bench_compare fetch_board_logs
```

结果：

- QEMU 专项测试：std/RVV 各 7 个用例通过。
- QEMU bench：输出含 `Dataset:`、`Iterations:`，`output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`。
- 反汇编：`build/asm/riscv/bench_filter_indices_rvv.full.asm` 中确认 `vlse32.v`、`vmfeq.vv`、`vmflt.vf`、`vmand.mm`、`vcompress.vm`、`vcpop.m`、`vsetvli`。
- 上游 `test/filters/test_filters.cpp`：补齐 `pcl_io/search/kdtree/flann/lz4/hdf5/zlib/png` 等依赖后 std/RVV 均可构建运行；当前测试程序要求外部 `bun0.pcd` 和 `milk_cartoon_all_small_clorox.pcd` 参数。
- 板卡专项测试：7 个用例通过。

QEMU 只作为构建、正确性、日志格式和指令路径证据，不作为性能结论。

## 8. 板卡性能结论

最终板卡日志：`test-rvv/filters/filter_indices/output/board/analyze_bench_compare.log`，设备标签 `Milkv-Jupiter`，`Iterations: 30`。

| Case | Std ms/iter | RVV ms/iter | Speedup |
| --- | ---: | ---: | ---: |
| `removeNaN indices-only sparse xyz 64K` | 1.9330 | 0.8212 | 2.35x |
| `removeNaN indices-only sparse xyz 1M` | 30.6799 | 13.9112 | 2.21x |
| `removeNaN indices-only dense xyz 1M` | 11.5556 | 11.4960 | 1.01x |
| `removeNaN cloud-out sparse xyz 1M` | 35.8324 | 22.0811 | 1.62x |
| `removeNaNNormals cloud-out sparse normal 1M` | 71.5442 | 71.7044 | 1.00x |

结论：保留 non-dense xyz indices-only 和 xyz cloud-out 的 RVV 路径；dense 与 normals 保持标量。

## 9. 已知限制

- 不覆盖非标准布局或非 `float x/y/z` 点类型。
- 不覆盖 dense indices 填充的手写 RVV。
- 不对 `PointT` 做 RVV scatter/copy，以避免破坏非 xyz 字段和 padding 语义。
- `FilterIndices<PointT>::applyFilter(output)` 仍是 wrapper，不直接 RVV。
