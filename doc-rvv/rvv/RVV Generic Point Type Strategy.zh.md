# RVV 泛型点类型字段加载策略

PCL 的很多算法入口使用模板点类型。同一个函数可能接收 `PointXYZ`、`PointXYZI`、`PointXYZRGB`，也可能同时处理不同的 `PointSource` 和 `PointTarget`。RVV 优化如果直接按 `PointXYZ` 写死字段位置，会把泛型入口收窄成单一点类型实现。

本文讨论面对 PCL 泛型点类型时，RVV 优化应如何判断 `x/y/z` 字段、取得字段 offset、选择公共 load helper，并设置必要的 fallback gate。CEOP（`correspondence_estimation_organized_projection`）作为示例出现，但它的 organized projection、投影矩阵和 correspondence 写出语义不属于本文的通用规则。

## 1. 不要硬编码 PointXYZ

需要避免的 RVV 写法：

```cpp
const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
constexpr std::size_t kXOff = offsetof(pcl::PointXYZ, x);
constexpr std::size_t kYOff = offsetof(pcl::PointXYZ, y);
constexpr std::size_t kZOff = offsetof(pcl::PointXYZ, z);
```

这段代码只适用于 `PointXYZ`。`PointXYZI`、`PointXYZRGB` 或自定义点类型可能有不同的结构体大小、POD 类型和字段 offset。PCL 的模板参数表达当前调用者提供的点类型。RVV helper 必须基于当前 `PointT` 证明字段可访问。

正确方向是：

```text
当前模板点类型 PointT
  -> traits 证明存在 x/y/z
  -> traits 证明每个字段是单个 float
  -> traits 取得当前点类型的 offset
  -> 公共 RVV load helper 按当前 PointT/POD/offset 访问
```

## 2. 用 PCL traits 证明字段

进入 f32 RVV xyz 路径前，至少要证明：

- `pcl::traits::has_xyz<PointT>::value` 为 true；
- `x/y/z` 的 `datatype::decomposed::type` 都是 `float`；
- `x/y/z` 的 `datatype::decomposed::value` 都是 `1`，也就是每个字段是单个 float。

一个常见 gate 形态是：

```cpp
template <typename PointT, bool HasXYZ = pcl::traits::has_xyz<PointT>::value>
struct RVVXYZFloatLayout : std::false_type {};

template <typename PointT>
struct RVVXYZFloatLayout<PointT, true>
: std::bool_constant<
      std::is_standard_layout_v<typename pcl::traits::POD<PointT>::type> &&
      std::is_same_v<typename pcl::traits::datatype<PointT, pcl::fields::x>::decomposed::type, float> &&
      std::is_same_v<typename pcl::traits::datatype<PointT, pcl::fields::y>::decomposed::type, float> &&
      std::is_same_v<typename pcl::traits::datatype<PointT, pcl::fields::z>::decomposed::type, float> &&
      pcl::traits::datatype<PointT, pcl::fields::x>::decomposed::value == 1 &&
      pcl::traits::datatype<PointT, pcl::fields::y>::decomposed::value == 1 &&
      pcl::traits::datatype<PointT, pcl::fields::z>::decomposed::value == 1> {};
```

这个 gate 同时检查字段存在、字段类型、字段 count 和 POD layout 前提。不满足这些条件时，生产路径回退到原标量实现。

## 3. POD、standard-layout 与 alignment

只证明 `float x/y/z` 还不够。PCL 的公共 RVV load/store wrapper 还依赖几个底层访问前提：

- 传给 wrapper 的 `typename pcl::traits::POD<PointT>::type` 必须满足 `std::is_standard_layout_v<T>`；
- 字段 offset 必须满足 f32 load/store 的 alignment 前提；
- `sizeof(PointT)` 或 `sizeof(POD)` 对应当前 AoS stride；
- indexed gather 使用的 byte offset 不应溢出 helper 采用的 offset 类型。

文档或代码注释应写清覆盖条件：traits 证明 `x/y/z` 是单个 float，且当前 POD / standard-layout / offset alignment 满足公共 RVV load/store wrapper 前提。

## 4. 取得当前点类型的 offset

字段 offset 应来自当前模板点类型：

```cpp
constexpr std::size_t kXOff = pcl::traits::offset<PointT, pcl::fields::x>::value;
constexpr std::size_t kYOff = pcl::traits::offset<PointT, pcl::fields::y>::value;
constexpr std::size_t kZOff = pcl::traits::offset<PointT, pcl::fields::z>::value;
```

随后把当前点云 base pointer、当前点类型的 byte offsets 和这些字段 offset 交给公共 wrapper：

```cpp
const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
const vuint32m2_t offsets =
    __riscv_vmul_vx_u32m2(indices_u, static_cast<std::uint32_t>(sizeof(PointT)), vl);

pcl::rvv_load::indexed_load3_fields_f32m2<
    typename pcl::traits::POD<PointT>::type, kXOff, kYOff, kZOff>(
    base, offsets, vl, x, y, z);
```

这个形态保留 PCL 模板入口的泛型性。`PointXYZ`、`PointXYZI` 或其它满足 gate 的点类型都使用自己的 `sizeof`、POD 和 offset。

## 5. Source 和 Target 分别处理

registration 类算法经常有两个点类型参数：

```cpp
PointSource
PointTarget
```

这两者需要独立 layout 判断。source load 使用：

- `sizeof(PointSource)`；
- `typename pcl::traits::POD<PointSource>::type`；
- `pcl::traits::offset<PointSource, pcl::fields::x/y/z>`。

target load 使用：

- `sizeof(PointTarget)`；
- `typename pcl::traits::POD<PointTarget>::type`；
- `pcl::traits::offset<PointTarget, pcl::fields::x/y/z>`。

入口 gate 也要分别判断：

```cpp
if constexpr (!RVVXYZFloatLayout<PointSource>::value ||
              !RVVXYZFloatLayout<PointTarget>::value) {
  return false;
}
```

CEOP 覆盖 `PointXYZ -> PointXYZI`。该路径对 source 和 target 分别执行 traits gate 和 offset 计算。

## 6. 选择 load helper

生产代码优先复用 `pcl::rvv_load` 的公共 wrapper。避免在每个算法里复制裸 intrinsic。选择 helper 时要区分固定策略 primitive 和自动分发 wrapper。

| helper | 访问形态 | 是否自动选择 segment |
| --- | --- | --- |
| `indexed_load3_fields_f32m2` | indexed AoS，3 次字段 gather，分别执行 `vluxei32` | 否 |
| `indexed_load3_seg_f32m2` | indexed AoS，要求从第一个字段开始三字段连续 | 否 |
| `indexed_load3_f32m2` | indexed AoS dispatch，字段紧密连续时可走 segment，否则走 fields | 是 |
| `strided_load3_fields_f32m2` | 顺序 / strided AoS，3 次字段 load | 否 |
| `strided_load3_seg_f32m2` | 顺序 / strided AoS，要求三字段连续 | 否 |
| `strided_load3_f32m2` | strided AoS dispatch，字段紧密连续时可走 segment，否则走 fields | 是 |

`_fields` 后缀表示按字段访问。它不会自动切到 segment 指令。若需要根据 `x/y/z` 是否紧密连续自动选择 segment 或 fields，应调用 `indexed_load3_f32m2` 或 `strided_load3_f32m2` 这类 dispatch wrapper，并用 bench / 反汇编确认实际路径。

CEOP production 当前选择 `indexed_load3_fields_f32m2`。这个 helper 固定执行 3 字段 gather。

## 7. 32-bit byte offset gate

当前公共 indexed load helper 使用 32-bit byte offsets。典型计算是：

```cpp
offsets = indices * sizeof(PointT);
```

gate 的目标是证明所有有效访问都能落在 `uint32_t` byte offset 范围内。

如果入口能依赖 PCL 已验证的 indices 有效性，可以用点云规模证明：

```text
cloud.size() <= UINT32_MAX / sizeof(PointT)
```

这表示所有合法 index 对应的 `index * sizeof(PointT)` 都可用 32-bit byte offset 表达。该条件成立时无需逐个扫描 index。

如果 helper 接收不可信 raw index 数组，或者入口没有证明 index 落在 cloud 范围内，需要额外验证每个 index，或回退标量路径。

## 8. 输入规模 gate 不一定叫 indices.size()

小规模 fallback 是常见生产 gate。文档应描述为当前 RVV stage 的 work item count 低于收益阈值。不要固定写成 `indices.size()`。

不同阶段的 work item 可能是：

- source index count；
- cloud point count；
- candidate count；
- projected candidate count；
- neighbor pair count；
- output upper bound。

CEOP 的 source staging 使用 `indices.size()`。后续 projection-pixel staging 使用 `candidates.size()`。target-predicate staging 使用 `projected.size()`。其它算法可能没有 `indices`，也可能使用其它 RVV stage 输入。

## 9. 稀疏输出与 staging

泛型点类型的字段加载通常只是 RVV 优化的前半段。若后续 predicate 会稀疏保留 lane，并需要输出多个字段，推荐复用“多字段压缩 staging”模式：

```text
同一个 keep mask
  -> 每个字段 vcompress
  -> vcpop 得到 count
  -> 临时 SoA buffer
  -> 短标量循环组装 AoS staging
```

固定栈 buffer 必须有与实际 SEW/LMUL 绑定的 `vlmax <= buffer_capacity` gate。后续 RVV stage 如果因为规模、VLEN 或布局条件失败，应从已有 staging 进入对应 scalar tail。已有 staging 保留已完成的 RVV 工作。

详细模式见 `doc-rvv/rvv/RVV Multi-Field Compress Staging.zh.md`。本文不重复展开 `vcompress` 和 staging 写出细节。

## 10. CEOP 示例

CEOP production RVV 把泛型点类型字段加载方法应用到 source 和 target 两侧：

```text
PointSource:
  indices -> byte offsets -> gather source x/y/z -> finite / transform / z predicate

PointTarget:
  projected target_index -> byte offsets -> gather target x/y/z -> target finite / depth / distance predicate
```

通用字段加载部分：

- 不硬编码 `PointXYZ`；
- source / target 分别判断 `float x/y/z`；
- source / target 分别取 `sizeof`、POD 和 `x/y/z` offset；
- 使用公共 `pcl::rvv_load::indexed_load3_fields_f32m2`；
- 用 cloud size 证明 32-bit byte offset；
- 小规模和 VLEN buffer 条件不满足时 fallback；
- 分阶段 RVV 失败时从已有 staging 进入 scalar tail。

CEOP 专属部分：

- target cloud 必须 organized；
- source 点通过 `src_to_tgt_transformation_` 变换到 target camera 坐标；
- `u/v` 投影和图像 bounds check；
- `target_index = v * target.width + u`；
- 当前 `projection_matrix_` 第三行保持 `[0, 0, 1]`，因此 `uv[2] == z`；
- identity transform fast path；
- 非 identity transform 按 Eigen row-dot lowering 对齐 FMA / add 结构；
- projection-pixel staging 按标量 RVV build 的 FMA contraction 对齐像素边界；
- `depth_threshold_`；
- distance predicate 的边界处理；
- accepted lane 写出前用标量 Eigen `norm()` 重算并再次检查，保持 `pcl::Correspondence::distance` 与生产标量表达式一致；
- `determineReciprocalCorrespondences()` 转调 `determineCorrespondences()`。

这些 CEOP 细节只描述 organized projection production case。其它 PCL RVV 主题需要按各自标量语义重新判断。

## 11. 适用边界

本文方法适合：

- 模板点类型中 `x/y/z` 是单个 float；
- AoS 点云数据可通过 PCL traits 得到可靠 offset；
- POD / standard-layout / alignment 满足公共 RVV wrapper 前提；
- source / target 或多个点类型可分别 gate；
- indexed 或 strided load 能由公共 helper 表达；
- fallback 后可保持原标量语义。

回退条件：

- 非 float 坐标或 double 语义；
- 未注册或 traits 不完整的自定义点类型；
- xyz 字段为数组、多元素字段或其它非单标量表达；
- raw index 输入无法证明有效范围；
- 输出对象构造、bit pattern 或复杂状态机无法通过 staging / scalar tail 保持语义的路径。
