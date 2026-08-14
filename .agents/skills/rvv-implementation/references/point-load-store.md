# RVV Point Load/Store 封装

生产 RVV 代码优先复用公共访存封装：

```text
pcl/common/rvv_point_load.h
pcl/common/impl/rvv_point_load.hpp
pcl/common/rvv_point_store.h
pcl/common/impl/rvv_point_store.hpp
```

## 使用优先级

- 标准 `x/y/z` 字段：优先使用 xyz wrapper。
- normal、intensity、label 或自定义字段：先判断 primitive 是否能表达。
- 公共封装缺少必要通用形态时，优先扩展公共封装并补测试 / 文档。
- 只有 bench 诊断原型、单次诊断或公共封装会引入明显多余语义约束时，才局部写裸 intrinsic。

## 覆盖检查

使用公共封装不替代生产 gate。入口或 traits 仍需明确：

- standard-layout。
- 字段类型。
- 字段 offset。
- stride。
- indexed / non-indexed。
- dense / non-dense。

文档应说明访存选择原因，特别是 AoS stride、segment load/store、gather 和 `vcompress` 的成本与语义边界。

## PCL 泛型点类型

PCL 模板入口不能硬编码 `PointXYZ` 或复用某个具体点类型的 offset。标准 `x/y/z` RVV 路径应通过 PCL traits 证明当前模板点类型满足访问前提：

- `pcl::traits::has_xyz<PointT>::value` 为 true。
- `pcl::traits::datatype<PointT, pcl::fields::x/y/z>::decomposed::type` 都是 `float`。
- `decomposed::value` 都是 `1`，即每个字段是单个 float。
- 使用 `pcl::traits::offset<PointT, pcl::fields::x/y/z>::value` 取得当前点类型 offset。
- 传给公共 wrapper 的 `typename pcl::traits::POD<PointT>::type` 满足 wrapper 的 `standard-layout` 前提。
- 字段 offset 满足 f32 load/store alignment 前提。
- `sizeof(PointT)`、AoS stride、POD / wrapper view、standard-layout 和 alignment 与当前 RVV load/store 方式一致。

`PointSource` 和 `PointTarget` 必须分别 gate、分别取 `sizeof`、POD、standard-layout、alignment、stride 和 offset。
不能把 source 的 layout 假设复用到 target；`PointXYZ -> PointXYZI` 这类组合只有在两端各自证明
`float x/y/z`、offset 和 load/store stride 后才可进入 RVV。

如果当前阶段为了先闭合 production direct 证据而使用 `std::is_same_v<PointXYZ>`、`PointNormal` 或其它
exact-type gate，必须把它写成 phase-local exception（阶段局部例外）：列出 traits / wrapper 阻塞理由、
未覆盖点类型、其它模板实例 fallback 测试和下一 point-type expansion phase。该写法不能成为最终
PointXYZ-like / PointNormal-like 泛型生产门控，也不能替代 source / target 分别的 traits 审计。

基于法线的算法不能只证明 `x/y/z`。如果 RVV 路径读取 `normal_x/normal_y/normal_z`，还必须分别对 `PointSource`
和 `PointTarget` 证明 normal 字段存在、字段类型是单个 `float`、offset 可由 traits 取得且满足 wrapper 前提。
如果当前 PCL traits 或公共 wrapper 暂时不能优雅表达 normal 字段 gate，生产补丁应收窄到已验证具体点类型并让其它模板实例 fallback，不能把 `PointNormal` 诊断证据写成泛型点类型证据。

## Helper 选择

命名中带 `_fields` 的 helper 表示固定按字段访问，不做 segment/field 自动选择：

- `indexed_load3_fields_f32m2` 是 3 次 `vluxei32` 字段 gather。
- `strided_load3_fields_f32m2` 是 3 次 `vlse32` 字段 load。

需要根据 `x/y/z` 是否紧密连续自动选择 segment 或 fields 时，使用 dispatch wrapper：

- `indexed_load3_f32m2`。
- `strided_load3_f32m2`。

生产代码优先复用这些公共 wrapper；只有公共封装无法表达、或诊断 bench 需要固定某一种 intrinsic 策略作公平对比时，才局部写裸 intrinsic。
