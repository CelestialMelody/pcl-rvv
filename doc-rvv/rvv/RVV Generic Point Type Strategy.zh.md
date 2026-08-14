# RVV 泛型点类型 gate 策略

PCL 的很多算法入口使用模板点类型。同一个函数可能接收 `PointXYZ`、
`PointXYZI`、`PointXYZRGB` 或用户自定义点类型，也可能同时处理不同的
`PointSource` 和 `PointTarget`。RVV 优化如果直接按 `PointXYZ` 写死字段位置，
会把泛型入口收窄成单一点类型实现，并可能在字段 offset、POD 类型或结构体大小
不同的点类型上读错内存。

本文是 RVV 泛型点类型 gate 的首选入口文档。公共 API 落点是
`common/include/pcl/rvv_point_traits.h`。该头文件只包含 compile-time
traits / layout gate（编译期类型特征和布局准入判断），不包含算法 dispatch
（选择 RVV 路径还是标量路径的分流逻辑），不包含 RVV intrinsic（RVV 内建函数）
实现，也不包含 topic 特有逻辑。

CEOP（`correspondence_estimation_organized_projection`）和 symmetric LLS
（symmetric point-to-plane LLS）在本文中只作为 gate 选择示例。它们各自的投影、
对应关系写出、法向量累加和 fallback 语义不属于本文的通用规则。

## 1. 不要硬编码 PointXYZ

需要避免的 RVV 写法：

```cpp
const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
constexpr std::size_t kXOff = offsetof(pcl::PointXYZ, x);
constexpr std::size_t kYOff = offsetof(pcl::PointXYZ, y);
constexpr std::size_t kZOff = offsetof(pcl::PointXYZ, z);
```

这段代码只适用于 `PointXYZ`。其它点类型可能满足“有 `x/y/z` 且都是 float”的
算法语义，但字段 offset、`sizeof(PointT)`、`pcl::traits::POD<PointT>::type`
都不同。泛型 RVV 代码必须基于当前模板参数 `PointT` 证明字段语义和布局前提，
不能把某个具体点类型的 offset 复用到所有调用。

推荐思路：

```text
当前模板点类型 PointT
  -> 用 PCL traits 证明字段存在和字段类型
  -> 选择与算法访问方式匹配的公共 RVV gate
  -> 取得当前 PointT 或 POD 的字段 offset
  -> 用当前点云 base、sizeof(PointT) 和当前 offset 调用公共 RVV helper
  -> 任一前提失败时回退标量路径
```

### 1.1 预编译和调用点是泛型需求证据

阅读一个模板算法的 RVV gate 时，除了看当前 helper 用了哪些字段，还要先了解这个
函数在 PCL 源码语境中实际可能以哪些 `PointT` 实例化或调用。预编译宏、显式
实例化列表和真实调用点可以帮助 worker 建立“泛型入口常见点类型”的背景：

- filters 模块常在 `src/*.cpp` 中用 `PCL_XYZ_POINT_TYPES` 预编译模板，例如
  `ApproximateVoxelGrid`、`FrustumCulling`、`PassThrough`、`VoxelGrid` 和
  `VoxelGridCovariance`。当前配置中的 `PCL_XYZ_POINT_TYPES` 包含
  `PointXYZ`、`PointXYZI`、`PointXYZRGBA`、`PointXYZRGB`、
  `PointXYZRGBNormal`、`PointXYZINormal` 等常见 xyz 点类型。
- filters 还有一些显式实例化列表，例如 `FastBilateralFilter` 使用
  `PointXYZ`、`PointXYZRGB`、`PointXYZRGBA`，`ProjectInliers` 使用
  `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`。
- app、tool、tutorial 和 test 里的真实调用同样是证据。例如
  `ApproximateVoxelGrid<PointXYZI>`、`ApproximateVoxelGrid<PointXYZRGBA>` 和
  `ApproximateVoxelGrid<PointXYZRGBNormal>` 都能在仓库调用点中看到。
- registration 模块的证据更分散：部分 `src/*.cpp` 显式实例化
  `PointXYZI -> PointXYZI`、`PointXYZRGB -> PointXYZRGB`，例如 ICP、NDT、
  SVD 和 LM；`LUM` 使用 `PCL_XYZ_POINT_TYPES`；也有许多 registration
  算法主要以 header-only 模板形式暴露，不能只按 `src` 宏实例化覆盖范围下结论。

这些证据只能说明“泛型入口和用户语境真实存在”，不能直接替代 RVV gate。它们的作用是
提醒 worker：`PointT` 很可能不是单一 `PointXYZ`，而是 `PointXYZI`、
`PointXYZRGB/RGBA`、normal 复合点类型或用户自定义点类型。生产 RVV 是否覆盖这些
类型，还必须逐项证明：当前算法只读哪些字段、是否构造 `PointT` 输出、是否保留额外
字段、是否依赖 `getVector4fMap()` 的齐次分量、source / target 是否分别 gate，以及
fallback 后是否保持原标量语义。

### 1.2 模板字段访问合同不等于 exact 点型

模板算法源码里的字段访问本身就是标量路径的编译期合同。看到
`PointSource` / `PointTarget` 模板参数时，不能先假设它们实际一定是同一种 PCL
点类型，也不能因为某个 topic 的诊断样本使用 `PointNormal` 就把 production API
写成 exact `PointNormal` 语义。

例如 point-to-plane LLS 这类算法可能只从 source 读取 `x/y/z`，从 target 读取
`x/y/z` 和 `normal_x/normal_y/normal_z`。这表示原标量模板支持的是“source xyz
字段访问可编译、target xyz/normal 字段访问可编译且算法语义成立”的点型组合；
`PointXYZ -> PointNormal` 这类组合可以属于标量合同，而 `PointNormal -> PointNormal`
只是其中一个具体实例。RVV production 如果只批准 exact `PointNormal -> PointNormal`
路径，必须写成有意收窄的 gate，并保证其它可编译模板实例自然 fallback。

决定是否做泛型 RVV 时，先列出：

- source 实际读取哪些字段；
- target 实际读取哪些字段；
- 两侧字段是否可以不同；
- `Scalar` 是否参与布局或数值合同；
- 当前证据覆盖 exact 点型、traits-gated 泛型，还是仅覆盖诊断样本。

只有这些问题闭合后，才能把 exact 点型实现扩展成泛型分流。

## 2. 输入字段 Gate 不等于输出 PointT 语义

公共 xyz / normal traits 只能证明“某些字段可以被 RVV 读取或写入”。它们不证明算法已经
复现了完整 `PointT` 输出语义。这个区别在只读 predicate（谓词判断）和构造新点云的算法之间
尤其重要：

- 如果算法只读 xyz 并输出 index、mask、correspondence、hash 或其它 staging metadata
  （暂存元数据），额外字段通常不参与当前 RVV stage。此时可以用公共 traits 扩大字段读取范围，
  后续对象构造仍可交给标量 tail。
- 如果算法会写出 `PointT`、聚合 `FieldList`、调用 `PointT` 的 `+=` / `*` / `/=`、
  使用 `copyPoint`、`CentroidPoint` 或类似“整点” helper，额外字段就可能是算法语义的一部分。
  RVV 不能只写 xyz 后声称覆盖 `PointXYZI`、`PointXYZRGB/RGBA` 或 normal 复合点类型。
- 如果 `src/*.cpp` 已有 explicit specialization（显式特化），例如 RGB/RGBA 单独处理，
  这通常说明该点类型的非 xyz 字段有特殊语义。泛型 RVV gate 必须尊重这些特化。

`Pyramid` 是典型例子。泛型 dense 标量路径通过
`next.at(j,i) += previous.at(jj,ii) * kernel` 构造新的 `PointT`。PCL 注册点类型的
point operator 会按注册字段执行字段级运算，所以 `PointXYZI` 会连 `intensity`
一起平滑；而 `PointXYZRGB/RGBA` 在 `filters/src/pyramid.cpp` 有显式颜色特化，手动累加
`r/g/b/a` 后转换回 `std::uint8_t`。因此 `Pyramid` 不能仅凭 xyz traits 泛化到所有
“类似 PointXYZ”的类型；生产 RVV 必须逐字段定义输出语义，或保留标量 fallback。

处理这类算法时先回答：

- 标量路径是只读字段做判断，还是构造新的 `PointT` 输出？
- `PointT` 运算符、`FieldList` 聚合、`copyPoint` 或 centroid helper 会碰哪些额外字段？
- 是否有 explicit specialization 或 `src/*.cpp` 实例化暗示某些点类型有特殊语义？
- 对每个非 xyz 输出字段，RVV 是完全复现、staging 后交给标量，还是让该点类型 fallback？

只有这些问题闭合后，公共 traits gate 才能作为 production dispatch 的一部分。

## 3. PCL Traits 字段语义

PCL 注册点类型通过 traits 描述字段语义。RVV f32 路径通常关心这些信息：

- `pcl::traits::has_xyz<PointT>::value`：点类型是否注册了 `x/y/z` 字段。
- `pcl::traits::has_normal<PointT>::value`：点类型是否注册了
  `normal_x/normal_y/normal_z` 字段。
- `pcl::traits::datatype<PointT, Field>::decomposed::type`：字段标量类型。
- `pcl::traits::datatype<PointT, Field>::decomposed::value`：字段包含的标量个数。
- `pcl::traits::offset<PointT, Field>::value`：字段相对当前点类型存储布局的
  byte offset（字节偏移）。
- `pcl::traits::POD<PointT>::type`：PCL 注册后的底层 POD 表示，常用于
  `offsetof` 风格的字节访问前提检查。

进入 f32 RVV xyz 路径前，通常至少要证明：

- `has_xyz<PointT>` 为 true；
- `x/y/z` 的 `datatype::decomposed::type` 都是 `float`；
- `x/y/z` 的 `datatype::decomposed::value` 都是 `1`，即每个字段是单个 float。

需要直接读 normal 字段的算法还要分别证明：

- `has_normal<PointT>` 为 true；
- `normal_x/normal_y/normal_z` 都是单个 `float`；
- 当前算法所需的 POD、standard-layout、`sizeof` 和 alignment 前提成立。

## 4. 公共 API 落点

`rvv_point_traits.h` 提供以下公共 API。新 RVV topic 应优先复用这些 API，
不要在算法实现里重复定义本地 `XYZFloatLayout` 或 `XYZNormalFloatLayout`。

| API | 职责 | 典型用途 |
| --- | --- | --- |
| `pcl::rvv::RVVFieldScalar<T>` | 去掉 cv/ref，得到字段表达式的实际标量类型。 | 兼容旧 member gate 中的 `decltype(point.x)` 判断。 |
| `pcl::rvv::RVVFloatFieldLayout<PointT, Field>` | 判断 PCL traits 注册字段是否为单个 `float`。 | 组合 xyz、normal 或其它字段语义 gate。 |
| `pcl::rvv::RVVXYZFloatLayout<PointT>` | 判断 `x/y/z` 是否是 PCL traits 注册的单个 `float` 字段，并暴露 `kX/kY/kZ` offset。 | CEOP 这类只需要 xyz 字段语义、并由具体 helper / 本地 gate 承担底层访问前提的路径。 |
| `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` | 判断 `x/y/z` 是 PCL traits 注册的单个 `float` 字段，并检查 POD standard-layout、`sizeof(PointT)==sizeof(POD)`、stride 和字段 offset 的 float alignment；暴露 `kX/kY/kZ`。 | TEPTPL 这类 source 只直接按 AoS byte offset 读取 xyz 的 ordered-cloud-pair production 路径。 |
| `pcl::rvv::RVVXYZNormalFloatLayout<PointT>` | 判断 `x/y/z/normal_x/normal_y/normal_z` 是否都是单个 `float`，并检查 POD、standard-layout、`sizeof(PointT)==sizeof(POD)`、`sizeof(PointT)` 和字段 offset 的 float alignment；暴露 `kX/kY/kZ/kNX/kNY/kNZ`。 | symmetric LLS 这类直接按 AoS byte offset 读取 xyz 和 normal 的 ordered-cloud-pair production 路径。 |
| `pcl::rvv::kRVVXYZPointCompatible<PointT>` | 旧 load/store 兼容 gate：要求成员 `x/y/z` 存在、类型都是 `float`，且 `PointT` 是 standard-layout。 | 保持 `rvv_point_load/store` 旧接口和 common 调用点语义稳定。 |
| `pcl::rvv::kRVVXYZAoSPointCompatible<PointT>` | `RVVXYZAoSFloatLayout<PointT>::value` 的变量模板形式。 | 需要变量模板风格 strong xyz AoS gate 的调用点。 |
| `pcl::rvv::kRVVXYZNormalPointCompatible<PointT>` | `RVVXYZNormalFloatLayout<PointT>::value` 的变量模板形式。 | 需要变量模板风格 gate 的调用点。 |
| `pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()` | 返回 `UINT32_MAX / sizeof(PointT)`，即当前 32-bit byte offset helper 可表达的最大合法元素数。 | indexed gather/scatter 使用 32-bit byte offset 时的云规模 gate。 |

`pcl::rvv_load::RVVCoordScalar`、
`pcl::rvv_load::kRVVXYZPointCompatible`、
`pcl::rvv_store::RVVCoordScalar` 和
`pcl::rvv_store::kRVVXYZPointCompatible` 是旧命名空间中的兼容别名。新文档和
新代码应把 `pcl::rvv::*` 视为公共 API 落点。

## 5. 四类 Gate 的区别

不要把所有“有 xyz”的路径都归并成同一个 gate。公共 traits 刻意保留四类
不同语义边界。

| gate 类别 | 公共 API | 证明内容 | 不证明内容 | 适用示例 |
| --- | --- | --- | --- | --- |
| member `x/y/z` + standard-layout | `kRVVXYZPointCompatible<PointT>` | C++ 成员 `x/y/z` 存在、成员表达式类型为 `float`、`PointT` 是 standard-layout。 | 不依赖 PCL traits 的 `has_xyz` / `datatype` 语义；不证明 normal；不证明 `sizeof(PointT)==sizeof(POD)`。 | load/store 旧兼容 gate，以及沿用旧 helper 名称的 common 调用点。 |
| PCL traits xyz 单 float | `RVVXYZFloatLayout<PointT>` | PCL traits 注册了 `x/y/z`，且三个字段都是单个 `float`；提供当前点类型的 xyz offset。 | 不额外要求 POD / standard-layout / `sizeof(PointT)==sizeof(POD)`；不证明 normal。 | CEOP 这类只需要 xyz 字段语义的算法 gate。底层 helper 的 standard-layout / alignment 前提仍需由 helper `static_assert` 或本地 gate 保证。 |
| xyz 单 float + AoS layout 前提 | `RVVXYZAoSFloatLayout<PointT>` 或 `kRVVXYZAoSPointCompatible<PointT>` | PCL traits 注册了 xyz，三个字段都是单个 `float`；POD standard-layout；`sizeof(PointT)==sizeof(POD)`；stride 和字段 offset 满足 float alignment。 | 不证明 normal；不代表构造完整 `PointT` 输出语义；仍不包含算法规模、VLEN、索引类型或输出 `Scalar` 条件。 | TEPTPL ordered-cloud-pair source 侧只读 xyz 的 production RVV 路径。 |
| xyz + normal 单 float + AoS layout 前提 | `RVVXYZNormalFloatLayout<PointT>` 或 `kRVVXYZNormalPointCompatible<PointT>` | PCL traits 注册了 xyz 和 normal，六个字段都是单个 `float`；POD standard-layout；`sizeof(PointT)==sizeof(POD)`；stride 和字段 offset 满足 float alignment。 | 不代表所有 normal 算法都可直接接入；仍不包含算法规模、VLEN、索引类型、输出语义等 dispatch 条件。 | TEPTPL target 侧、symmetric LLS ordered-cloud-pair production RVV 路径，直接按 AoS byte offset 读取 xyz 和 normal。 |

选择原则：

- 只是保持旧 load/store common helper 的兼容判断时，用 `kRVVXYZPointCompatible`。
- 算法只需要 PCL 注册的 xyz 单 float 字段语义时，用 `RVVXYZFloatLayout`。
- 算法要直接用 AoS byte offset 读取 xyz，且不读取 normal 时，用
  `RVVXYZAoSFloatLayout`。
- 算法要直接用 AoS byte offset 读取 xyz 和 normal，且依赖 POD / alignment 前提时，
  用 `RVVXYZNormalFloatLayout`。
- 算法还有规模阈值、VLEN buffer、变换矩阵、输出顺序或表达式一致性要求时，这些仍是
  算法本地 dispatch / fallback 条件，不应塞进公共 traits。

## 6. Source 和 Target 必须分别 Gate

registration 类算法经常同时包含：

```cpp
PointSource
PointTarget
```

这两侧可能是不同点类型，例如 `PointXYZ -> PointXYZI`。source 和 target 必须分别
执行 layout gate，分别取得 offset，不能把一侧的 offset 或 `sizeof` 复用到另一侧。

source load 使用：

- `sizeof(PointSource)`；
- `typename pcl::traits::POD<PointSource>::type`；
- `pcl::traits::offset<PointSource, pcl::fields::x/y/z>`，或
  `RVVXYZAoSFloatLayout<PointSource>::kX/kY/kZ`。

target load 使用：

- `sizeof(PointTarget)`；
- `typename pcl::traits::POD<PointTarget>::type`；
- `pcl::traits::offset<PointTarget, pcl::fields::x/y/z/normal_x/normal_y/normal_z>`，或
  `RVVXYZNormalFloatLayout<PointTarget>::kX/kY/kZ/kNX/kNY/kNZ`。

典型入口 gate：

```cpp
if constexpr (!pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value ||
              !pcl::rvv::RVVXYZNormalFloatLayout<PointTarget>::value) {
  return false;
}
```

如果只有 target 阶段会读取 target 点云，也可以只在该阶段 gate `PointTarget`，但不能
用前一阶段 source 的 offset 或 stride 推导 target。

## 7. 取得当前点类型 Offset

字段 offset 应来自当前模板点类型。可以直接使用 PCL traits：

```cpp
constexpr std::size_t kXOff = pcl::traits::offset<PointT, pcl::fields::x>::value;
constexpr std::size_t kYOff = pcl::traits::offset<PointT, pcl::fields::y>::value;
constexpr std::size_t kZOff = pcl::traits::offset<PointT, pcl::fields::z>::value;
```

也可以在已经选择公共 layout gate 后使用它暴露的 offset：

```cpp
using Layout = pcl::rvv::RVVXYZFloatLayout<PointT>;
constexpr std::size_t kXOff = Layout::kX;
constexpr std::size_t kYOff = Layout::kY;
constexpr std::size_t kZOff = Layout::kZ;
```

随后把当前点云 base pointer、当前点类型的 byte offset 和字段 offset 交给公共
load/store helper：

```cpp
const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
const vuint32m2_t offsets =
    __riscv_vmul_vx_u32m2(indices_u, static_cast<std::uint32_t>(sizeof(PointT)), vl);

pcl::rvv_load::indexed_load3_fields_f32m2<
    typename pcl::traits::POD<PointT>::type, kXOff, kYOff, kZOff>(
    base, offsets, vl, x, y, z);
```

这个形态保留 PCL 模板入口的泛型性。满足 gate 的 `PointXYZ`、`PointXYZI` 或自定义
点类型都使用自己的 `sizeof`、POD 和 offset。

## 8. 32-bit Byte Offset Helper 边界

当前公共 indexed gather / scatter helper 使用 32-bit byte offsets。典型计算是：

```cpp
offsets = indices * sizeof(PointT);
```

`rvvMaxU32ByteOffsetElements<PointT>()` 返回：

```text
UINT32_MAX / sizeof(PointT)
```

如果入口能依赖 PCL 已验证的 index 有效性，可以用点云规模证明：

```text
cloud.size() <= rvvMaxU32ByteOffsetElements<PointT>()
```

这表示所有合法 index 对应的 `index * sizeof(PointT)` 都可用 32-bit byte offset
表达。该条件成立时不需要逐个扫描 index。

边界注意：

- helper 只证明 byte offset 可表示，不证明 index 数组内容一定合法。
- 如果入口接收不可信 raw index 数组，或没有其它代码保证 index 落在 cloud 范围内，
  仍需要逐个检查 index，或回退标量路径。
- source cloud 和 target cloud 的 `sizeof(PointT)` 可能不同，32-bit gate 必须分别按
  `PointSource` / `PointTarget` 计算。
- 对 staged candidate 数组的 offset gate 应按 candidate struct 的 `sizeof` 和实际
  work item count 另行判断，不要误用点云类型 helper。

## 9. 选择 Load / Store Helper

生产代码优先复用 `pcl::rvv_load` 和 `pcl::rvv_store` 的公共 wrapper。避免在每个算法里
复制裸 intrinsic。

load helper 需要区分固定策略 primitive 和自动分发 wrapper：

| helper | 访问形态 | 是否自动选择 segment |
| --- | --- | --- |
| `indexed_load3_fields_f32m2` | indexed AoS，3 次字段 gather，分别执行 `vluxei32`。 | 否 |
| `indexed_load3_seg_f32m2` | indexed AoS，要求从第一个字段开始三字段连续。 | 否 |
| `indexed_load3_f32m2` | indexed AoS dispatch，字段紧密连续时走 segment，否则走 fields。 | 是 |
| `strided_load3_fields_f32m2` | 顺序 / strided AoS，3 次字段 load。 | 否 |
| `strided_load3_seg_f32m2` | 顺序 / strided AoS，要求三字段连续。 | 否 |
| `strided_load3_f32m2` | strided AoS dispatch，字段紧密连续时走 segment，否则走 fields。 | 是 |
| `indexed_load_field_f32m2<PointT, Field>` | 基于 PCL field tag 的单字段 indexed gather，要求该字段是 traits 注册的单个 `float`。 | 否 |
| `strided_load_field_f32m2<PointT, Field>` | 基于 PCL field tag 的单字段 strided load，要求该字段是 traits 注册的单个 `float`。 | 否 |

XY 或其它双字段读写优先用两个单字段 field-tag helper 表达。暂不提供
`load2` / `store2` dispatch API，除非后续有具体 production call site 和 bench 证据证明
二字段 tuple helper 能减少真实重复或带来稳定收益。

store helper 也有同样的 primitive / dispatch 分层。需要特别注意 contiguous segmented
store 的 buffer layout：`contiguous_seg3_store_f32m2` 和
`contiguous_seg4_store_f32m2` 写入的是 packed tuple buffer（例如 `xyzxyz...` 或
`f0f1f2f3...`），不是三个或四个独立 SoA 数组。独立数组应使用
`contiguous_store3_f32m2` 或 `contiguous_store4_f32m2`。
单字段 store 可用 `strided_store_f32m2<sizeof(PointT)>`、
`masked_strided_store_f32m2<sizeof(PointT)>`、
`strided_store_field_f32m2<PointT, Field>` 或
`scatter_store_field_f32m2<PointT, Field>`；field-tag helper 只证明并写一个 traits
注册的单个 `float` 字段，不代表算法已经覆盖整个 `PointT` 输出语义。

CEOP production 当前使用 `indexed_load3_fields_f32m2`，即固定三字段 gather。它没有
自动切换到 segment 指令。

### 9.1 Load / Store Helper 职责边界

`rvv_point_load.h` 和 `rvv_point_store.h` 只负责在调用方已经证明字段 layout 后，
把 AoS / indexed / strided / contiguous 访问映射到 RVV load/store intrinsic。它们不负责
决定某个算法是否应该进入 RVV production 路径。

边界应保持清楚：

- helper 负责字段 offset、stride、indexed byte offset 对应的 RVV load/store 形态；
- traits 负责 compile-time gate，例如字段是否存在、是否是单个 `float`、是否满足当前
  helper 需要的 layout / alignment 前提；
- 算法 dispatch 仍由各算法自己控制，包括 point type 支持范围、source / target 分别
  gate、规模阈值、VLEN scratch buffer、索引合法性、输出顺序和 fallback；
- 不能因为存在公共 helper，就把只验证过 `PointXYZ` 的路径自动扩展到
  `PointXYZI`、`PointXYZINormal`、`PointXYZRGB/RGBA` 或 `PointXY`。

字段语义也不能只按名称猜测。`PointXY` 没有 `z`，只能进入明确的 XY-only API 或本地
XY helper；`PointXYZI` 的 `intensity` 是否参与算法输出要按标量路径证明；
`PointXYZINormal` 需要分别证明 xyz、normal 和 intensity 的读写语义；`PointXYZRGB/RGBA`
如果颜色字段参与输出，必须保留颜色语义或 fallback。

## 10. 输入规模 Gate 不固定为 indices.size()

小规模 fallback 是常见 production gate。文档和代码注释应描述为当前 RVV stage 的
work item count 低于收益阈值，而不是固定写成 `indices.size()`。

不同阶段的 work item 可能是：

- source index count；
- cloud point count；
- candidate count；
- projected candidate count；
- neighbor pair count；
- output upper bound。

CEOP 的 source staging 使用 `indices.size()`。projection-pixel staging 使用
`candidates.size()`。target-predicate staging 使用 `projected.size()`。其它算法可能没有
`indices`，也可能使用其它 RVV stage 输入。

## 11. 稀疏输出与 Staging

泛型点类型的字段加载通常只是 RVV 优化的前半段。若后续 predicate 会稀疏保留 lane，
并需要输出多个字段，推荐复用“多字段压缩 staging”模式：

```text
同一个 keep mask
  -> 每个字段 vcompress
  -> vcpop 得到 count
  -> 临时 SoA buffer
  -> 短标量循环组装 AoS staging
```

固定栈 buffer 必须有与实际 SEW / LMUL 绑定的 `vlmax <= buffer_capacity` gate。后续
RVV stage 如果因为规模、VLEN 或布局条件失败，应从已有 staging 进入对应 scalar tail。
已有 staging 保留已完成的 RVV 工作。

详细模式见 `doc-rvv/rvv/RVV Multi-Field Compress Staging.zh.md`。本文不重复展开
`vcompress` 和 staging 写出细节。

`ApproximateVoxelGrid` 是另一种 staging 形态：RVV 不直接输出完整 `PointT`，而是为
XYZ-compatible `PointT` 生成保序的 `LeafHash{ix,iy,iz,hash,source_index}`。后续
history bucket 冲突、`FieldList` 字段聚合、`downsample_all_data_` 和 RGB/RGBA
packing 继续走原标量状态机。这个模式适合“前置 leaf/hash 是逐点纯函数，但后续
flush 顺序和对象构造有状态依赖”的算法。它也说明预编译 / 调用点证据需要转化为
具体语义设计：`PointXYZI`、`PointXYZRGB/RGBA` 可以共享 leaf/hash RVV staging，
但 intensity 或 packed color 字段本身不因此自动 RVV 化。

## 12. 什么时候不要迁移到公共 Trait

公共 `rvv_point_traits.h` 只收纳可复用的字段语义和布局 gate。以下判断不要默认迁移：

- Z-only gate：算法只依赖 `z`，或只对深度字段有特殊语义时，应保留算法本地判断，
  除非先抽象出清晰的公共 Z-field 语义。
- exact `PointXYZ` gate：某些算法特化只想覆盖精确的 `pcl::PointXYZ`，这不是泛型
  xyz float gate，迁移会扩大语义。迁移前要区分两种情况：如果算法只读取 xyz 并输出
  indices / mask，且 `PointXYZI`、`PointXYZRGB` 等点类型的额外字段不会参与语义，
  可以评估迁到 `kRVVXYZPointCompatible`；如果算法会构造 `PointT` 输出、聚合
  `FieldList`、处理 RGB/RGBA packing、或依赖 `getVector4fMap()` 的完整对象语义，
  exact gate 可能是输出语义 gate，不能只按 xyz layout 放宽。
- 算法特化 predicate：例如 organized target、projection matrix 形态、identity
  transform fast path、normal orientation、输出顺序、distance 表达式一致性等，
  都属于算法 dispatch，不属于点类型 traits。
- topic 临时诊断 gate：用于 benchmark、实验或 debug 的本地判断，不应因为名字相似
  就进入公共头。

迁移前应先回答：这个判断是否只描述“字段存在、字段类型、字段 offset / layout”。
如果答案不是明确的 yes，就应留在算法本地。

## 13. CEOP 示例

CEOP production RVV 把泛型点类型字段加载方法应用到 source 和 target 两侧：

```text
PointSource:
  indices -> byte offsets -> gather source x/y/z -> finite / transform / z predicate

PointTarget:
  projected target_index -> byte offsets -> gather target x/y/z -> target finite / depth / distance predicate
```

通用字段加载部分：

- 不硬编码 `PointXYZ`；
- source / target 分别判断 `RVVXYZFloatLayout`；
- source / target 分别取 `sizeof`、POD 和 `x/y/z` offset；
- 使用公共 `pcl::rvv_load::indexed_load3_fields_f32m2`；
- 用 cloud size 和 `rvvMaxU32ByteOffsetElements` 证明 32-bit byte offset；
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
- accepted lane 写出前用标量 Eigen `norm()` 重算并再次检查，保持
  `pcl::Correspondence::distance` 与 production 标量表达式一致；
- `determineReciprocalCorrespondences()` 转调 `determineCorrespondences()`。

这些 CEOP 细节只描述 organized projection production case。其它 PCL RVV 主题需要按
各自标量语义重新判断。

## 14. 适用边界

本文方法适合：

- 模板点类型中 `x/y/z` 是单个 float；
- AoS 点云数据可通过 PCL traits 得到可靠 offset；
- 当前访问方式所需的 POD / standard-layout / alignment 前提已由公共 gate、helper
  `static_assert` 或算法本地 gate 覆盖；
- source / target 或多个点类型可分别 gate；
- indexed、strided 或 contiguous load/store 能由公共 helper 表达；
- fallback 后可保持原标量语义。

回退条件：

- 非 float 坐标或 double 语义；
- 未注册或 traits 不完整的自定义点类型；
- xyz 字段为数组、多元素字段或其它非单标量表达；
- raw index 输入无法证明有效范围；
- helper 的 32-bit byte offset 边界无法证明；
- 输出对象构造、bit pattern 或复杂状态机无法通过 staging / scalar tail 保持语义的路径。
