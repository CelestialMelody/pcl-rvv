# filters/bilateral 函数级 RVV 评估

## 1. 入口与真实数据流

主题来自 `filters-retained-candidate-rescreen.zh.md` 的 `6.3` 观察项，本轮按诊断候选提升后完成函数级评估，并在补充误差预算后接入生产 `PointXYZI` 主路径；随后按公共 traits gate 扩展到上游同样预编译的 `PointXYZINormal`。

`pcl::BilateralFilter<PointT>::applyFilter(PointCloud&)` 是 intensity 平滑入口：先复制输入到 `output`，再遍历 `indices_`；每个有效 center point 调用 `tree_->radiusSearch(idx, sigma_s_ * 2, k_indices, k_distances)`，随后 `computePointWeight` 对邻域点 intensity 做 bilateral 加权平均并写回 `output[idx].intensity`。

标量单点公式：

```text
for neighbor id in radiusSearch output order:
  intensity_dist = abs(input[pid].intensity - input[id].intensity)
  dist = sqrt(k_distances[n])
  weight = exp(-(dist * dist)/(2*sigma_s^2)) *
           exp(-(intensity_dist * intensity_dist)/(2*sigma_r^2))
  BF += weight * input[id].intensity
  W += weight
return BF / W
```

真实入口成本包含 radius search、邻域 id gather、AoS intensity gather、两次 `exp`、浮点累加和输出写回。初始 staging-only 诊断只覆盖 gather/sqrt/abs，板卡 full diagnostic 约 `1.04x`，不足以生产接入；复核 common 后发现 `pcl::expf_RVV_f32m2` 可复用，full exp diagnostic 和生产入口均显示稳定收益。

## 2. RVV 决策

| 函数 / 片段 | 决策 | 覆盖条件 | fallback / 风险 |
| --- | --- | --- | --- |
| `BilateralFilter<PointT>::computePointWeight` | 接入生产 RVV helper | `__RVV10__`、`kBilateralXYZIntensityCompatible<PointT>`、邻域数 `>=16`、`indices.size()==distances.size()` | 小邻域、非兼容点型、非 RVV 编译、异常邻域长度回退 `computePointWeightStd` |
| 邻域 intensity 读取 | RVV gather | `radiusSearch` 返回非负 point id，当前 `PointT` 的 traits 注册 intensity 是单个 float，且有可写 `float intensity` 成员 | 非标准 intensity 字段不覆盖；没有直接可写 intensity 成员的 traits-only 类型不覆盖 |
| spatial 权重 | RVV float 参数 | 直接使用 squared distance 计算 `-d2/(2*sigma_s^2)` | 与标量 `sqrt(d2)` 后再平方数学等价；负 distance 异常仍走标量路径处理 |
| intensity 权重 | RVV float 参数 | `-(center-neighbor)^2/(2*sigma_r^2)` | 数值边界从 double/libm `std::exp` 改为 common float 近似 |
| 累加 | 标量顺序累加 | lane 写回 `weights/contribs` 后按 lane 顺序加到 double `BF/W` | 生产 helper 用 `no-tree-vectorize` 禁止编译器把该循环改成自动向量归约 |

生产 helper 放在 `filters/include/pcl/filters/impl/bilateral.hpp` 的入口附近，不改变公开 API。`computePointWeightStd` 常驻为标量 helper，`computePointWeightRVV` 仅在 `__RVV10__` 下定义，公开成员 `computePointWeight` 按本地 traits gate、公共 traits 和宏短路分流。

## 3. 实现说明

生产代码复用 `pcl/rvv_point_load.h`：

- `vle32.v` 读取 `Indices` 中连续 neighbor id；
- `byte_offsets_u32m2<PointT>` 将 AoS point id 转成 byte offset；
- `kRVVXYZPointCompatible<PointT>` 与 `RVVXYZFloatLayout<PointT>` 证明 XYZ 成员/layout 与 search/finiteness 语义一致；
- `RVVFloatFieldLayout<PointT, pcl::fields::intensity>` 证明 intensity 是 traits 注册的单个 float；
- `BilateralIntensityMemberCompatible<PointT>` 证明 `output[idx].intensity = float` 可编译；
- `pcl::traits::offset<PointT, pcl::fields::intensity>` 取得当前点类型的 intensity offset；
- `gather_load_f32m2<PointT, kIntensityOff>` gather 当前 `PointT::intensity`；
- `vle32.v` 读取 `radiusSearch` 的 squared distance；
- 两个指数参数分别传给 `pcl::expf_RVV_f32m2`；
- `vse32.v` 写回 chunk 的 weight 和 contribution，再以标量顺序累加。

`filters/src/bilateral.cpp` 预编译 `PointXYZI` 和 `PointXYZINormal`。从标量源码看，
`applyFilter` 会复制整点输出并只改写 `intensity`，所以 `PointXYZINormal` 的 normal 和
curvature 字段应保持输入值。本轮已把 production gate 从 exact `PointXYZI` 改为
XYZ + writable float intensity layout gate，并新增 `PointXYZINormal` 生产对拍和 extra-field
保留检查。该扩展不是按名字匹配任意 `PointXYZIxxx`，而是按字段、成员和 AoS offset
边界判定。

不使用 `vcompress`：`radiusSearch` 已经给出有效邻域列表，RVV 不筛选或重排 lane。不使用显式 `_rm` intrinsic，也不修改 FRM/FCSR。

生产 helper 外层使用 GCC 局部 pragma：

```cpp
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("no-tree-vectorize")
#endif
...
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
```

该约束只作用于 `computePointWeightRVV`。目的不是关闭手写 RVV intrinsic，而是防止 GCC tree vectorizer 把尾段 `BF/W` 标量累加循环自动改成向量 reduction。`computePointWeight` 的标量语义按 `radiusSearch` 邻域顺序累加；本主题已接受的误差来源是 common `expf_RVV_f32m2` 的 float 近似，如果尾段再被编译器改成归约顺序变化，checksum 和误差归因会混入第二个变量。反汇编复核因此同时确认显式 load/gather/exp/store RVV 路径仍命中，并确认生产尾段不再以自动归约作为证据。`!defined(__clang__)` 用于避免把 GCC 专用优化 pragma 应用到 Clang。

关键边界：

```cpp
if (indices.size () < 16 || indices.size () != distances.size ())
  return computePointWeightStd<PointT> (...);
```

该 fallback 避免短邻域的 vector setup 成本，也避免邻域和距离数组不配对时进入 RVV。

```cpp
const vfloat32m2_t v_spatial_arg = v_squared * spatial_scale;
const vfloat32m2_t v_intensity_arg = (center - intensity)^2 * intensity_scale;
const vfloat32m2_t v_weight =
    expf_RVV_f32m2(v_spatial_arg) * expf_RVV_f32m2(v_intensity_arg);
```

上游标量 `kernel(sqrt(d2), sigma_s)` 内部会平方回 `d2`，所以 RVV 直接使用 squared distance。数值差异来自 float 近似 `expf_RVV_f32m2`，不是邻域重排；专项测试和 bench 输出记录 max abs / max rel / RMSE / P95 / P99。

## 4. 测试与 bench

专项测试覆盖：

- 两邻居手算公式；
- RVV 构建下的 compile-time gate：`PointXYZI` / `PointXYZINormal` 命中，`PointXYZ` 不命中；
- staging RVV 与标量权重对拍；
- common exp RVV 单点权重近似对拍；
- full diagnostic 与标量对拍；
- full exp diagnostic 近似对拍；
- 高对比数据三组 `sigma_s/sigma_r` 误差预算；
- 公开 `BilateralFilter<PointXYZI>` 生产入口近似对拍；
- 公开 `BilateralFilter<PointXYZINormal>` 生产入口近似对拍，并验证 normal/curvature 保持；
- 公开生产入口高对比三组参数误差预算；
- non-dense invalid center skip；
- 小邻域 fallback。

bench 输出包含 `Dataset:`、`Iterations:`、每 case `Total Time`、checksum 和误差统计。生产 case：

- `bilateral production filter 256`：公开 `BilateralFilter<PointXYZI>::filter`，256 点全量 indices；
- `bilateral production filter 1K`：公开入口，1K 点全量 indices；
- `bilateral production normal 256`：公开 `BilateralFilter<PointXYZINormal>::filter`，256 点全量 indices；
- `bilateral production normal 1K`：公开 `BilateralFilter<PointXYZINormal>::filter`，1K 点全量 indices；
- `ProductionErrorCase`：高对比 40x32 点云，三组 sigma 参数，只验证误差预算，不作为性能 case。

上游原始测试复用 `test/filters/test_bilateral.cpp` 的 `FastBilateralFilter.Filters_Bilateral`，用于确认 filters bilateral 相关上游测试仍可运行；`BilateralFilter<PointXYZI>` 和 `BilateralFilter<PointXYZINormal>` 本体由专项测试覆盖。

## 5. 验证结果

- QEMU：`make -C test-rvv/filters/bilateral run_test_compare run_bench_compare dump_bench_rvv` 通过；QEMU 只作为正确性、日志格式和指令路径证据。
- 上游测试：`make -C test-rvv/filters/bilateral run_upstream_test_compare` 通过，std/RVV 均通过 `FastBilateralFilter.Filters_Bilateral`。
- 解析检查：`output/qemu/analyze_bench_compare.log` 与 `output/board/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`。
- 反汇编：`output/qemu/rvv_asm_check.log` 确认生产 helper 命中 `vsetvli`、`vle32.v`、`vluxei32.v`、`vfsub.vv`、`vfmul.vv`、`vfcvt.*.v`、`vsll.vi`、`vfmacc.vv`、`vse32.v` 等路径。
- 板卡：`make -C test-rvv/filters/bilateral run_board_test run_board_bench_compare fetch_board_logs` 通过，日志在 `test-rvv/filters/bilateral/output/board/`。

Milkv-Jupiter 板卡结果：

| case | Std ms/iter | RVV ms/iter | speedup | 说明 |
| --- | ---: | ---: | ---: | --- |
| `bilateral full exp diag 256 radiusSearch` | 19.4169 | 11.2317 | 1.73x | full diagnostic，含真实 radius search 和 common RVV exp |
| `bilateral full exp diag 1K radiusSearch` | 126.6599 | 76.0978 | 1.66x | full diagnostic，1K 全量 indices |
| `bilateral subset full exp diag 1K radiusSearch` | 63.8591 | 38.6635 | 1.65x | subset indices |
| `bilateral production filter 256` | 10.8279 | 3.1566 | 3.43x | 公开生产入口，命中 `PointXYZI` RVV 主路径 |
| `bilateral production filter 1K` | 91.8276 | 44.5187 | 2.06x | 公开生产入口，命中 `PointXYZI` RVV 主路径 |

QEMU rerun 日志新增 `PointXYZINormal` production case：`bilateral production normal 256`
与 `bilateral production normal 1K` 均通过误差检查，用于证明新增覆盖范围可编译、可运行；
QEMU 时间不作为板卡性能结论。

误差统计：

- 常规 production case：max abs `3.814697e-06`，max rel 约 `1.19e-07`，RMSE 约 `1.1e-06`；
- 高对比 `ProductionErrorCase` 三组参数：max abs `1.525879e-05`，max rel 约 `1.0e-07`，RMSE 最大约 `2.10e-06`。

结论：误差远低于专项预算 `max_abs <= 8e-4`、`max_rel <= 5e-5`、`rmse <= 2e-4`，且板卡生产入口收益明显。`bilateral` 从 6.3 观察项升级为 `PointXYZI` 生产 RVV 接入完成，并已在 QEMU 上补证 `PointXYZINormal` 生产覆盖；非兼容类型和短邻域保持标量 fallback。
