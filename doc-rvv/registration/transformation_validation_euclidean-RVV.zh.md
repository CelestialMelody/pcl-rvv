# registration/transformation_validation_euclidean RVV 可行性诊断

## 入口作用

`TransformationValidationEuclidean::validateTransformation` 是 registration 中用于评估候选变换质量的打分入口。输入是 source cloud、target cloud 和一个 4x4 transformation matrix；输出是有效 nearest-neighbor squared distance 的均值。`isValid` 在此分数基础上与用户设置的 `threshold_` 比较。

上游标量流程：

1. 按 4x4 矩阵把 source 点变换到临时 `input_transformed`；
2. 对每个 transformed point 调用 target KdTree 的 `nearestKSearch(point, 1, ...)`；
3. 跳过 squared distance 大于 `max_range_` 的匹配；
4. 返回有效 squared distance 均值；没有有效匹配时返回 `double::max()`。

## 覆盖范围与 fallback

本轮不接入生产分流，只保留 `test-rvv/registration/transformation_validation_euclidean/` 下的 bench-diagnosis helper。原因是 RVV 只能覆盖第 1 步 transform staging，而公开入口的主要成本很可能被第 2 步 KdTree search 稀释。

诊断覆盖：

- `PointXYZ -> PointXYZ`；
- `Scalar=float` 矩阵；
- `n >= 64` 的 transform staging；
- full diagnostic 中 KdTree search、`max_range` 和 score 累加保持标量。

fallback：

- 非 RVV 编译；
- 小规模点云；
- 泛型点类型、`Scalar=double`、不同 source/target 点类型；
- 生产 `TransformationValidationEuclidean` 公开入口。

## 详细设计

诊断 helper 位于 `test-rvv/.../transformation_validation_euclidean_diag.hpp`：

| 实体                                | 作用                 | 与标量语义的关系                                                   |
| ----------------------------------- | -------------------- | ------------------------------------------------------------------ |
| `transformPointXYZStd`            | 标量 4x4 xyz staging | 与上游 `validateTransformation` 的前置循环同公式                 |
| `transformPointXYZRVV`            | RVV VL chunk staging | 使用同一 3 行 affine 公式，结果仍写入临时 `PointCloud<PointXYZ>` |
| `transformPointXYZCandidate`      | 诊断分流             | RVV 构建且 `n>=64` 时走 RVV，否则走 Std                          |
| `validateTransformationCandidate` | full diagnostic      | 只替换前置 staging，KdTree search 和 score 流程保持标量            |

核心片段：

```cpp
// PointXYZ AoS: x/y/z 用公共 strided xyz wrapper 读取。
pcl::rvv_load::strided_load3_f32m2<kStride, kXOff, kYOff, kZOff>(
    src_base + i * kStride, vl, x, y, z);

tx = m00*x + m01*y + m02*z + m03
ty = m10*x + m11*y + m12*z + m13
tz = m20*x + m21*y + m22*z + m23

// 写回临时 transformed cloud，后续 KdTree 仍按标量流程消费。
pcl::rvv_store::strided_store3_f32m2<kStride, kXOff, kYOff, kZOff>(
    dst_base + i * kStride, vl, tx, ty, tz);
```

这个模式是“前置纯函数 RVV staging + 后续标量 search”。标量实现不需要额外结构，因为逐点 transform 后直接进入后续循环；诊断 RVV 为了保持与原入口相同的 KdTree 调用形态，仍生成同一个 `input_transformed` 临时点云。它不改变输出顺序、不压缩 lane、不修改 KdTree 或 score 状态。

本主题属于新出现的两阶段模式，但与已有 filters 中的 staging 主题不同之处在于：这里的 RVV 只做点坐标变换，后续 search 完全没有批量化空间，因此 staged 临时点云只是为了保留原入口形态和验证 search 稀释，而不是为了接一个更长的 RVV 主链。换言之，它是“可证明的局部 staging”，不是生产中的连续 RVV 直通链。

## 数值算例

取一个 VL chunk 的前三个点：

| lane | `x` | `y` | `z` |
| ---: | ----: | ----: | ----: |
|    0 |     1 |     2 |     3 |
|    1 |     4 |     5 |     6 |
|    2 |    -1 |     0 |     2 |

矩阵前三行：

```text
[ 2 0 0 10 ]
[ 0 3 0 20 ]
[ 0 0 4 30 ]
```

RVV lane 与标量公式一一对应：

| lane | `tx=2*x+10` | `ty=3*y+20` | `tz=4*z+30` |
| ---: | ------------: | ------------: | ------------: |
|    0 |            12 |            26 |            42 |
|    1 |            18 |            35 |            54 |
|    2 |             8 |            20 |            38 |

图示：

```text
PointXYZ AoS chunk: [x0 y0 z0] [x1 y1 z1] [x2 y2 z2] ...
RVV load xyz:       x=[x0 x1 x2] y=[y0 y1 y2] z=[z0 z1 z2]
Affine rows:        tx/ty/tz per lane
Store staging:      [tx0 ty0 tz0] [tx1 ty1 tz1] [tx2 ty2 tz2]
Scalar search:      nearestKSearch(staging[lane])
```

## 证据状态

- 专项测试：通过。
- QEMU bench：通过，`output/qemu/analyze_bench_compare.log` 可解析且无 `未解析`、`n/a`、`Total Time 不计算`。
- 反汇编：已确认 `vlsseg3e32.v`、`vfmacc.vf`、`vssseg3e32.v`、`vsetvli e32,m2` 路径。
- 板卡：通过。`transform-staging` 约 `2.89x`~`3.89x`，`full-validation` 仅 `1.01x`~`1.03x`。

Milkv-Jupiter 结果：

| case                                    | 函数入口 / 诊断层级                                 | 数据规模 / 参数                                           | Std ms/iter | RVV ms/iter | speedup | 证明点                                  |
| --------------------------------------- | --------------------------------------------------- | --------------------------------------------------------- | ----------: | ----------: | ------: | --------------------------------------- |
| `tve transform-staging pointxyz 64K`  | `transformPointXYZCandidate` 局部片段             | 64K `PointXYZ`，固定 4x4 transform                      |      3.2105 |      0.8258 |   3.89x | 证明 4x4 xyz staging 本身可 RVV 化      |
| `tve transform-staging pointxyz 256K` | `transformPointXYZCandidate` 局部片段             | 256K `PointXYZ`，固定 4x4 transform                     |     12.1858 |      4.2220 |   2.89x | 放大规模后局部片段仍有收益              |
| `tve full-validation pointxyz 64K`    | `validateTransformationCandidate` full diagnostic | 64K source/target，`max_range=1.0`，包含 KdTree search  |    269.2100 |    267.4663 |   1.01x | full 入口收益被 search 稀释             |
| `tve full-validation pointxyz 256K`   | `validateTransformationCandidate` full diagnostic | 256K source/target，`max_range=1.0`，包含 KdTree search |   1255.8260 |   1219.2291 |   1.03x | full 入口仍是弱收益，不足以支撑生产接入 |

speedup 计算方式为 `Std Avg / RVV Avg`。前两个 case 是局部片段诊断，不代表生产入口收益；后两个 case 才是本主题生产接入判断的主要依据。

transform-staging case 的 std/RVV checksum 不完全一致，原因是 RVV path 使用 FMA，而标量 path 按普通表达式求值，逐点坐标存在约 `1e-7` 量级浮点差。专项测试用 `1e-6f` 容差对拍坐标，full validation score 对拍通过。该 helper 不使用 `_rm` intrinsic，不修改 FRM/FCSR。

## 性能归因

full validation 只剩 `1.01x`~`1.03x` 的原因是：RVV 只覆盖前置 4x4 transform staging，而公开入口的大部分时间花在 KdTree setup / search，尤其是逐点调用：

```cpp
tree.nearestKSearch(point, 1, nn_indices, nn_dists);
```

以及 `tree.setInputCloud(cloud_tgt)` 后的 target tree 准备工作。RVV 诊断 helper 不改变这些流程。

板卡时间拆解显示 staging 占比很低：

| 规模 | Std transform-staging | Std full-validation | staging 占 full | 只优化 staging 的理论上限 |
| ---- | --------------------: | ------------------: | --------------: | ------------------------: |
| 64K  |             3.2105 ms |         269.2100 ms |           1.19% |                 约 1.012x |
| 256K |            12.1858 ms |        1255.8260 ms |           0.97% |                 约 1.010x |

64K full case 的 `1.01x` 与理论上限一致。256K full case 的 `1.03x` 高于单纯 staging 占比，可能来自 KdTree traversal / cache / 运行波动，或 FMA 后坐标微差导致的搜索路径细微变化；当前没有 search-only counter 能把这部分归因到 RVV transform，因此不作为生产接入依据。

### 源码链路

调用点位于 `registration/include/pcl/registration/impl/transformation_validation_euclidean.hpp`：

```cpp
for (const auto& point : input_transformed) {
  tree_->nearestKSearch(point, 1, nn_indices, nn_dists);
  if (nn_dists[0] > max_range_)
    continue;
  fitness_score += nn_dists[0];
  ++nr;
}
```

这一层看起来像“每点调用 nearestKSearch”，但公开搜索接口在 `search/include/pcl/search/impl/kdtree.hpp:99-105` 里只是：

```cpp
return (tree_->nearestKSearch (point, k, k_indices, k_sqr_distances));
```

实际实现落在 `kdtree/include/pcl/kdtree/impl/kdtree_flann.hpp`。关键步骤是：

```cpp
point_representation_->vectorize (static_cast<PointT> (point), query);
knn_search(*flann_index_,
           ::flann::Matrix<float>(query.data(), 1, dim_),
           k_indices,
           k_distances_mat,
           k,
           param_k_);
```

这里的 `vectorize` 只是把 query 点展开成 3 维 float；真正成本在 `knn_search` 的 FLANN/kd-tree 查询。这个过程依赖树结构、分支和候选维护，不是规整的 VL chunk 线性循环，因此不适合直接做当前这类“手写 RVV 主路径”。换句话说，`nearestKSearch(point, 1, ...)` 是可以定位到的热点边界，但它本身不是一个适合当前 RVV 模式的批量算子。

## 生产接入判断

当前结论：暂不修改上游生产入口。板卡已经证明 transform-staging microbench 可加速，但 full validation 只剩 `1.01x`~`1.03x` 的弱收益，说明 KdTree search 仍是主成本。本主题保持 bench-diagnosis，不接生产分流。只有 full diagnostic 或真实生产入口稳定明显收益，并且泛型点类型、fallback、小规模和上游测试边界补齐后，才重新评估生产接入。
