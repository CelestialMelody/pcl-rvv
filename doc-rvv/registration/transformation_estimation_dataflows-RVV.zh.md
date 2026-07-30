# registration transformation estimation 数据流源码导读

## 1. 读这组代码时先分三层

`TransformationEstimation` 系列代码里，“数据流”这个词容易混在一起。为了读清楚源码和 RVV 优化边界，建议分成三层看：

| 层次 | 关注点 | 代码里看到什么 | RVV 实现要补充什么 |
| --- | --- | --- | --- |
| 公开入口层 | 用户如何描述一组配对点 | full-cloud、indices、correspondences 等 overload | 判断这一组 row 的 source/target/weight 从哪里来。 |
| iterator 统一层 | 标量实现如何复用一套循环 | `ConstCloudIterator` 把不同入口变成逐点 iterator | 这一层隐藏了连续访问和索引访问的差异。 |
| RVV 取数层 | 一个 VL chunk 如何一次取多行 | 源码原本没有显式 VL chunk | 必须拆出 stride load、gather、weight load、mask、compress 等路径。 |

结论先放在前面：源码公开入口至少有四类 source/target 行枚举方式：

- 全云顺序扫描（full-cloud）：第 `k` 行是 `source[k] + target[k]`。
- source 单侧索引（source indices + target full-cloud）：第 `k` 行是 `source[indices_src[k]] + target[k]`。
- 双侧索引（source indices + target indices）：第 `k` 行是 `source[indices_src[k]] + target[indices_tgt[k]]`。
- 对应关系索引（correspondences）：第 `k` 行是 `source[correspondences[k].index_query] + target[correspondences[k].index_match]`。

weighted point-to-plane LLS 还多一条 weight（权重）流。full-cloud 和 indices overload 使用对象成员 `weights_`；correspondences overload 使用每条 `pcl::Correspondence` 里的 `weight` 字段。

这些差异进入 protected helper 后会被抹平。非 weighted helper 只看见：

```text
source_it + target_it
```

weighted helper 只多看见：

```text
source_it + target_it + weights_it
```

因此，标量计算循环本身看不到入口形态。它只看到当前 source 点、当前 target 点，以及 weighted 版本中的当前 weight。RVV 优化必须把这些差异重新显式化，因为连续 AoS（结构数组）字段跨步加载（stride load）和索引驱动的离散加载（gather）是不同的访存形态。

## 2. Correspondence 的实际含义

`pcl::Correspondence` 是一条 source 中某个点与 target 中某个点匹配的记录。它保存点索引，以及一个可解释为 distance（距离）或 weight（权重）的浮点字段。它本身不保存点坐标。

```cpp
// 来源：common/include/pcl/correspondence.h
struct Correspondence
{
  index_t index_query = 0;       // source/query 点索引
  index_t index_match = UNAVAILABLE; // target/match 点索引
  union
  {
    float distance = std::numeric_limits<float>::max();
    float weight;
  };
};

using Correspondences =
    std::vector<pcl::Correspondence,
                Eigen::aligned_allocator<pcl::Correspondence>>;
```

读 correspondences overload 时，可以把第 `k` 条 correspondence 展开成下面这行：

```text
row k:
  source point = cloud_src[correspondences[k].index_query]
  target point = cloud_tgt[correspondences[k].index_match]
```

如果是 weighted point-to-plane LLS 的 correspondences overload，还会多一列：

```text
row k weight = correspondences[k].weight
```

一个容易踩的边界是：`index_match` 默认值可以是 `UNAVAILABLE`，但 transformation estimation 的 correspondences overload 本身并没有逐条检查负 index 或越界 index。通常上游 correspondence estimation / rejection 已经筛好对应关系。诊断代码如果为了安全跳过负 index 或越界 index，应把它写成诊断策略；production 入口没有提供这项检查。

## 3. 公开入口如何枚举行

基类 `TransformationEstimation` 定义了四个 overload。它们使用同一类求解公式；差异在于第 `k` 行法方程由哪些点组成。

```cpp
// 来源：registration/include/pcl/registration/transformation_estimation.h
virtual void
estimateRigidTransformation(const PointCloud<PointSource>& cloud_src,
                            const PointCloud<PointTarget>& cloud_tgt,
                            Matrix4& transformation_matrix) const = 0;

virtual void
estimateRigidTransformation(const PointCloud<PointSource>& cloud_src,
                            const Indices& indices_src,
                            const PointCloud<PointTarget>& cloud_tgt,
                            Matrix4& transformation_matrix) const = 0;

virtual void
estimateRigidTransformation(const PointCloud<PointSource>& cloud_src,
                            const Indices& indices_src,
                            const PointCloud<PointTarget>& cloud_tgt,
                            const Indices& indices_tgt,
                            Matrix4& transformation_matrix) const = 0;

virtual void
estimateRigidTransformation(const PointCloud<PointSource>& cloud_src,
                            const PointCloud<PointTarget>& cloud_tgt,
                            const Correspondences& correspondences,
                            Matrix4& transformation_matrix) const = 0;
```

把这四个 overload 翻译成 row 枚举，就是：

| 公开入口 | 第 `k` 行 source | 第 `k` 行 target | 规模关系 |
| --- | --- | --- | --- |
| full-cloud | `cloud_src[k]` | `cloud_tgt[k]` | source 和 target 点数相同。 |
| source indices | `cloud_src[indices_src[k]]` | `cloud_tgt[k]` | `indices_src.size()` 等于 target 点数。 |
| source + target indices | `cloud_src[indices_src[k]]` | `cloud_tgt[indices_tgt[k]]` | 两个 index list 等长。 |
| correspondences | `cloud_src[index_query]` | `cloud_tgt[index_match]` | row 数来自 correspondence list。 |

这个设计让算法作者可以只写一个“逐行累加 normal-equation”的 helper。代价是，入口的内存形态被 iterator 抽象藏起来了。

## 4. ConstCloudIterator 如何隐藏入口差异

`ConstCloudIterator` 有两种 const iterator。

第一种是默认 cloud iterator。它从 `cloud.begin()` 开始顺序前进，因此 full-cloud 的 row `k` 就是点云里的第 `k` 个点。

```cpp
// 来源：common/include/pcl/impl/cloud_iterator.hpp
DefaultConstIterator(const PointCloud<PointT>& cloud)
  : cloud_(cloud)
  , iterator_(cloud.begin())
{}

const PointT& operator*() const override
{
  return (*iterator_);
}
```

第二种是 index iterator。它自己持有一个 `indices_`，每次先读当前 row 的 index，再到点云里间接取点。

```cpp
// 来源：common/include/pcl/impl/cloud_iterator.hpp
ConstIteratorIdx(const PointCloud<PointT>& cloud, const Indices& indices)
  : cloud_(cloud)
  , indices_(indices)
  , iterator_(indices_.begin())
{}

const PointT& operator*() const override
{
  return cloud_[*iterator_];
}

const PointT* operator->() const override
{
  return &(cloud_.points[*iterator_]);
}
```

correspondences constructor 也是先把 correspondence list 转成普通 index list，再复用同一个 index iterator。source 侧抽 `index_query`，target 侧抽 `index_match`。

```cpp
// 来源：common/include/pcl/impl/cloud_iterator.hpp
ConstCloudIterator(const PointCloud<PointT>& cloud,
                   const Correspondences& corrs,
                   bool source)
{
  Indices indices;
  indices.reserve(corrs.size());
  if (source) {
    for (const auto& corr : corrs)
      indices.push_back(corr.index_query);
  }
  else {
    for (const auto& corr : corrs)
      indices.push_back(corr.index_match);
  }
  iterator_ = new ConstIteratorIdx(cloud, indices);
}
```

这段代码解释了一个入口层事实：在 protected helper 里，correspondences 已经变成 source index iterator 和 target index iterator。correspondences 与 source+target indices 的差别主要在公开入口层：indices 是调用方直接给两个 index list；correspondences 是从 `pcl::Correspondence` 里抽出 query/match 两列。

`ConstIteratorIdx` 还区分两个 index 概念：

- 当前 row 序号：`getCurrentIndex()`，也就是 index list 的位置 `k`。
- 当前真实点索引：`getCurrentPointIndex()`，也就是 `indices[k]`。

full-cloud 下这两个值相同；indices 和 correspondences 下通常不同。RVV 实现要保留 row 顺序、重复 index 和非连续 index，就必须按 row 序号推进 index stream，不能按真实点索引排序或去重。

## 5. point-to-plane LLS：四个入口进入同一个计算循环

普通 point-to-plane LLS 的四个公开入口只负责检查规模关系、构造 iterator，然后调用同一个 protected helper。

```cpp
// 来源：registration/include/pcl/registration/impl/
// transformation_estimation_point_to_plane_lls.hpp

// full-cloud
ConstCloudIterator<PointSource> source_it(cloud_src);
ConstCloudIterator<PointTarget> target_it(cloud_tgt);
estimateRigidTransformation(source_it, target_it, transformation_matrix);

// source indices + target full-cloud
ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
ConstCloudIterator<PointTarget> target_it(cloud_tgt);
estimateRigidTransformation(source_it, target_it, transformation_matrix);

// source indices + target indices
ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
ConstCloudIterator<PointTarget> target_it(cloud_tgt, indices_tgt);
estimateRigidTransformation(source_it, target_it, transformation_matrix);

// correspondences
ConstCloudIterator<PointSource> source_it(cloud_src, correspondences, true);
ConstCloudIterator<PointTarget> target_it(cloud_tgt, correspondences, false);
estimateRigidTransformation(source_it, target_it, transformation_matrix);
```

进入 helper 后，循环只写成“两个 iterator 同步前进”。下面摘录保留了关键结构：

```cpp
// 来源：registration/include/pcl/registration/impl/
// transformation_estimation_point_to_plane_lls.hpp
while (source_it.isValid() && target_it.isValid()) {
  if (!std::isfinite(source_it->x) || !std::isfinite(source_it->y) ||
      !std::isfinite(source_it->z) || !std::isfinite(target_it->x) ||
      !std::isfinite(target_it->y) || !std::isfinite(target_it->z) ||
      !std::isfinite(target_it->normal_x) ||
      !std::isfinite(target_it->normal_y) ||
      !std::isfinite(target_it->normal_z)) {
    ++target_it;
    ++source_it;
    continue;
  }

  const float& sx = source_it->x;
  const float& sy = source_it->y;
  const float& sz = source_it->z;
  const float& dx = target_it->x;
  const float& dy = target_it->y;
  const float& dz = target_it->z;
  const float& nx = target_it->normal[0];
  const float& ny = target_it->normal[1];
  const float& nz = target_it->normal[2];

  // 后续构造 a/b/c/d，并累加 ATA/ATb。
}
```

这段 helper 读不出当前 row 是 full-cloud、source indices 还是 correspondences。它的语义是统一的：当前 source 点和当前 target 点构成一行 point-to-plane normal-equation contribution（法方程贡献）。

## 6. weighted point-to-plane LLS：多一条权重流

weighted 版本的 source/target iterator 和普通 LLS 一样，但计算 helper 多一个 `weights_it`。full-cloud 和 indices overload 要求 `weights_.size()` 与 row 数相同，然后从 `weights_.begin()` 开始同步推进。

```cpp
// 来源：registration/include/pcl/registration/impl/
// transformation_estimation_point_to_plane_lls_weighted.hpp
if (weights_.size() != nr_points) {
  PCL_ERROR("Number or weights from the number of correspondences!");
  return;
}

ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
ConstCloudIterator<PointTarget> target_it(cloud_tgt);
auto weights_it = weights_.begin();
estimateRigidTransformation(source_it, target_it, weights_it, transformation_matrix);
```

correspondences overload 不使用对象成员 `weights_`。它会从每条 correspondence 的 union 字段里取 `weight`，复制成临时连续数组。

```cpp
// 来源：registration/include/pcl/registration/impl/
// transformation_estimation_point_to_plane_lls_weighted.hpp
ConstCloudIterator<PointSource> source_it(cloud_src, correspondences, true);
ConstCloudIterator<PointTarget> target_it(cloud_tgt, correspondences, false);

std::vector<Scalar> weights(correspondences.size());
for (std::size_t i = 0; i < correspondences.size(); ++i)
  weights[i] = correspondences[i].weight;

auto weights_it = weights.begin();
estimateRigidTransformation(source_it, target_it, weights_it, transformation_matrix);
```

helper 里三个 iterator 同步前进；无效点也会同步跳过当前 weight。

```cpp
// 来源：registration/include/pcl/registration/impl/
// transformation_estimation_point_to_plane_lls_weighted.hpp
while (source_it.isValid() && target_it.isValid()) {
  if (/* source/target/normal 非有限 */) {
    ++source_it;
    ++target_it;
    ++weights_it;
    continue;
  }

  const float& nx = target_it->normal[0] * (*weights_it);
  const float& ny = target_it->normal[1] * (*weights_it);
  const float& nz = target_it->normal[2] * (*weights_it);

  // 后续构造 a/b/c/d，并累加 ATA/ATb。
  ++source_it;
  ++target_it;
  ++weights_it;
}
```

所以 weighted 的 RVV 取数层要回答两个问题：点字段是 stride load 还是 gather？weight 是已有连续 `weights_`，还是从 correspondences 中先展开出来的连续数组？这两个问题是独立的。

## 7. symmetric point-to-plane LLS：当前 production 接 full-cloud 与 source-indexed

symmetric point-to-plane LLS 的标量结构同样是四个公开入口进入同一个 source/target iterator helper。当前 RVV production dispatch 放在两条已获证据支持的 overload 中：full-cloud，以及 source indices + target full-cloud。两条路径都在 size check 通过后先尝试 row-source policy RVV helper；traits、`Scalar`、VLEN、规模或 byte-offset gate 不满足时，再回到原来的 `ConstCloudIterator` 标量路径。source-indexed 只覆盖 valid-index-only 输入合同，RVV 不定义负数或越界 index 行为。

```cpp
// 来源：registration/include/pcl/registration/impl/
// transformation_estimation_symmetric_point_to_plane_lls.hpp
if constexpr (std::is_same_v<Scalar, float>) {
  if (detail::estimateSymmetricPointNormalFullCloudRVV(
      cloud_src, cloud_tgt, enforce_same_direction_normals_,
      transformation_matrix))
    return;
}

ConstCloudIterator<PointSource> source_it(cloud_src);
ConstCloudIterator<PointTarget> target_it(cloud_tgt);
estimateRigidTransformation(source_it, target_it, transformation_matrix);

// source indices + target full-cloud overload
if constexpr (std::is_same_v<Scalar, float>) {
  if (detail::estimateSymmetricPointNormalSourceIndicesRVV(
      cloud_src, indices_src, cloud_tgt, enforce_same_direction_normals_,
      transformation_matrix))
    return;
}

ConstCloudIterator<PointSource> source_indexed_it(cloud_src, indices_src);
ConstCloudIterator<PointTarget> target_full_it(cloud_tgt);
estimateRigidTransformation(source_indexed_it, target_full_it, transformation_matrix);
```

source+target indices 和 correspondences overload 没有这个 RVV 分流，仍然直接构造 iterator：

```cpp
// 来源：registration/include/pcl/registration/impl/
// transformation_estimation_symmetric_point_to_plane_lls.hpp
ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
ConstCloudIterator<PointTarget> target_it(cloud_tgt, indices_tgt);
estimateRigidTransformation(source_it, target_it, transformation_matrix);

ConstCloudIterator<PointSource> source_it(cloud_src, correspondences, true);
ConstCloudIterator<PointTarget> target_it(cloud_tgt, correspondences, false);
estimateRigidTransformation(source_it, target_it, transformation_matrix);
```

标量 helper 仍只看当前 source/target 点。symmetric 公式多读 source normal 和 target normal，并按 `enforce_same_direction_normals_` 合成法线：

```cpp
// 来源：registration/include/pcl/registration/impl/
// transformation_estimation_symmetric_point_to_plane_lls.hpp
for (; source_it.isValid() && target_it.isValid(); ++source_it, ++target_it) {
  const Vector3 p(source_it->x, source_it->y, source_it->z);
  const Vector3 q(target_it->x, target_it->y, target_it->z);
  const Vector3 n1(source_it->getNormalVector3fMap().cast<Scalar>());
  const Vector3 n2(target_it->getNormalVector3fMap().cast<Scalar>());

  Vector3 n;
  if (enforce_same_direction_normals_) {
    if (n1.dot(n2) >= 0.)
      n = n1 + n2;
    else
      n = n1 - n2;
  }
  else {
    n = n1 + n2;
  }

  if (!p.array().isFinite().all() || !q.array().isFinite().all() ||
      !n.array().isFinite().all()) {
    continue;
  }

  Vector6 v;
  v << (p + q).cross(n), n;
  M.rankUpdate(v);
  ATb += v * (q - p).dot(n);
}
```

当前 production RVV 只批准 full-cloud，是因为这个入口能直接用 `cloud.points.data()`、`sizeof(PointT)` 和字段 offset 形成规则 stride load，并且已有真实公开入口、反汇编和板卡证据。indices 和 correspondences 虽然后半段公式相同，但前半段取数需要 index/gather 路径，不能自然继承 full-cloud 的生产结论。

## 8. RVV 视角下的完整取数分类

从 RVV 实现角度，更有用的分类是：source、target、weight 这三列在一个 VL chunk（一次向量长度处理的行块）中如何取出。

| 公开入口 | source 取数 | target 取数 | weight 取数 | RVV 风险 |
| --- | --- | --- | --- | --- |
| full-cloud | stride load | stride load | weighted 时连续 load `weights_[k]` | 规则性最好；主要审查字段 offset、VL tail、mask、compress。 |
| source indices + target full-cloud | gather | stride load | weighted 时连续 load `weights_[k]` | 单侧 index/gather，适合做 indexed production 的第一步消融。 |
| source indices + target indices | gather | gather | weighted 时连续 load `weights_[k]` | 双侧不规则访存；重复 index 和 cache locality 风险更大。 |
| correspondences | 从 `index_query` 展开后 gather | 从 `index_match` 展开后 gather | weighted 时从 `correspondence.weight` 展开后连续 load | 比双 indices 多 correspondence 展开和 union 字段解释。 |

full-cloud 的 VL chunk 可以写成：

```text
base source pointer + k * sizeof(PointSource)
base target pointer + k * sizeof(PointTarget)
  -> vlse32.v / strided load source fields
  -> vlse32.v / strided load target fields
  -> optional vle32.v weights
  -> finite mask
  -> formula staging
  -> vcompress / buffer tail / ATA+ATb
```

indices 或 correspondences 的 VL chunk 多了 index stream：

```text
row k..k+vl:
  -> load source indices
  -> load target indices if present
  -> convert point index to byte offset
  -> gather source fields
  -> gather target fields
  -> optional load or expanded weights
  -> reuse finite mask / formula / compress / tail
```

这解释了为什么 full-cloud 和 correspondences 的性能风险不同：它们后半段可以共享，前半段取数不同。full-cloud 正向收益不能外推到 correspondences；correspondences 退化也不能单独归因成 gather，因为实际组合里还混合了 correspondence 展开、offset 计算、乱序访存、mask、`vcompress`、buffer tail、cache locality 和 row 分布。

## 9. 一个具体 row 例子

假设：

```text
indices_src = [5, 9, 9]
indices_tgt = [20, 21, 30]
correspondences = [
  { index_query = 5, index_match = 20, weight = 0.6 },
  { index_query = 9, index_match = 21, weight = 0.8 },
  { index_query = 9, index_match = 30, weight = 0.7 }
]
```

source+target indices 和 correspondences 可以形成同样的 source/target row 序列：

| row k | source+target indices | correspondences |
| --- | --- | --- |
| 0 | `source[5] + target[20]` | `source[5] + target[20]` |
| 1 | `source[9] + target[21]` | `source[9] + target[21]` |
| 2 | `source[9] + target[30]` | `source[9] + target[30]` |

对标量 helper 来说，这三行只是 iterator 连续返回的三个 point pair。对 RVV 来说，source 真实点索引是 `[5, 9, 9]`，target 真实点索引是 `[20, 21, 30]`，不能用 `base + k * sizeof(PointT)` 顺序读取。重复的 `source[9]` 也不能去重，因为它参与了两条不同 row 的法方程贡献。

weighted correspondences 还会展开出：

```text
weights = [0.6, 0.8, 0.7]
```

这条 weight stream 是连续的，但它的来源不是 `weights_` 成员，而是 correspondence list 中每条记录的 union 字段。

## 10. 为什么此前先诊断 full-cloud 与 correspondences

当前三个 transformation estimation LLS 主题先诊断 full-cloud 和 correspondences，是因为这两类入口分别代表取数规则性的两端。

full-cloud 是最规则的入口。source 和 target 按相同 row 序号顺序扫描，weighted 版本的 `weights_` 也是连续数组。这个入口能先回答一个基础问题：在公式、有限值 mask、`vcompress`、buffer tail 和 Eigen solve 仍保留当前结构时，RVV 字段读取和逐点公式 staging 是否能覆盖这些额外成本。

correspondences 是 ICP（迭代最近点）配准中常见的匹配结果入口。它按 `index_query/index_match` 枚举点对，weighted 版本还从 `correspondence.weight` 生成权重流。这个入口能暴露最不规则的 indexed row 组合：source gather、target gather、correspondence 展开、权重来源切换，以及 row 分布带来的 cache locality 风险。

source indices 和 source+target indices 是公开入口中真实存在的中间形态。此前 transformation estimation normal-equation 诊断没有对它们做独立 RVV 性能诊断；已有工作只证明这些入口在 production fallback（回退路径）下继续走标量语义。本轮已在 symmetric point-to-plane LLS 的 test-rvv 诊断层补了 source 单侧 indices 和双侧 indices 消融。point-to-plane LLS 与 weighted point-to-plane LLS 仍没有这两条中间形态的板卡证据。

已有板卡证据可以作为边界：

| 主题 | full-cloud RVV 信号 | correspondences RVV 信号 | 当前结论 |
| --- | --- | --- | --- |
| point-to-plane LLS | `0.96x` 到 `0.99x` | `0.52x` 到 `0.58x` | bench-only；不接 production。 |
| weighted point-to-plane LLS | 64K `1.18x`，256K `0.92x` | `0.50x` / `0.45x` | bench-only；不接 production。 |
| symmetric point-to-plane LLS | 本轮板卡 rerun 中 production-direct full-cloud `PointNormal` 为 `2.71x` / `2.70x`，`PointXYZINormal` 为 `2.71x` / `2.48x`；production-direct source-indexed `PointNormal` 为 `2.11x` / `1.87x`，`PointXYZINormal` 为 `2.10x` / `1.90x`；diagnostic dual-indices 为 `0.80x` / `0.63x` | diagnostic correspondences `0.77x` / `0.86x` | 只批准 full-cloud 和 source indices + target full-cloud 的 generic normal `Scalar=float` production；dual-indices 和 correspondences 仍停留在 test-rvv 诊断层。 |

这张表只说明已经测过的入口形态。symmetric LLS 的 indexed 诊断给出两个新边界：source 单侧 gather 在当前输入上仍有板卡收益；双侧 gather 使用独立 target index stream 后转为负向，但仍可与 correspondences 的额外展开成本分离。correspondences 的退化仍是多因素待消融假设，候选原因包括 correspondence parsing（对应关系解析）、index 展开、offset 计算、双侧 gather、不规则 cache locality、mask、`vcompress`、buffer tail、自动 partial vector accumulation（部分向量累加）和测试数据 row 分布。

## 11. symmetric LLS 已补的两个 indexed 消融

本轮在 `test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls/` 中补了两个 diagnostic（诊断）消融，并在 reviewer 认可后把 source indices + target full-cloud 接成 production direct。dual-indices 仍不修改 production，也不把 source+target indices 公开入口接到 RVV 分流；价值是把 correspondences 的混合成本拆开，让后续 worker 或 reviewer 能用板卡数据判断下一步。

第一个消融是 source 单侧索引：

```text
row k:
  source point = cloud_src[indices_src[k]]
  target point = cloud_tgt[k]
  weight       = weights_[k]          // 仅 weighted 版本
```

这个入口对应公开 overload 中的 `cloud_src + indices_src + cloud_tgt`。RVV 取数形态是 source gather、target stride load、weight continuous load。symmetric LLS 没有 weight 流。本轮 production 采用 `SourceIndexedRowSource` policy：公开入口 size check 通过后，如果 `__RVV10__`、`Scalar=float`、generic normal layout gate、规模 gate、VLEN gate 和 32-bit byte offset gate 都满足，则在 production 中读取 source index stream、gather source 字段并 stride load target 字段；否则回退原 `ConstCloudIterator` 标量路径。bench 计时前仍用同一 index stream 生成紧凑 target cloud，使计时内只包含 source index load、source gather、target stride load、formula staging、mask、`vcompress`、buffer tail、Eigen solve 和矩阵构造。

VL chunk 例子：

```text
row k..k+vl:
  -> vle32.v 读取 source_indices[k..]
  -> index * sizeof(PointNormal) 形成 source byte offsets
  -> vluxei32.v gather source xyz 和 source normal
  -> vlse32.v stride load target[k..] xyz 和 target normal
  -> 复用 symmetric normal-select / finite mask / formula / compress / tail
```

第二个消融是双侧 indices：

```text
row k:
  source point = cloud_src[indices_src[k]]
  target point = cloud_tgt[indices_tgt[k]]
  weight       = weights_[k]          // 仅 weighted 版本
```

这个入口对应公开 overload 中的 `cloud_src + indices_src + cloud_tgt + indices_tgt`。RVV 取数形态是 source gather、target gather、weight continuous load。symmetric LLS 没有 weight 流。实现方式是：bench 计时前构造两条独立的 `pcl::Indices`；计时内不扫描 `pcl::Correspondence`，也不复制 query/match/weight。它回答的问题是：双侧 gather 本身造成多大成本。它没有 correspondence parsing 和 `correspondence.weight` 展开，因此可以把“双侧 gather 成本”和“correspondences 额外入口展开成本”分开。

VL chunk 例子：

```text
row k..k+vl:
  -> vle32.v 读取 source_indices[k..]
  -> vle32.v 读取 target_indices[k..]
  -> 分别形成 source/target byte offsets
  -> vluxei32.v gather source xyz 和 source normal
  -> vluxei32.v gather target xyz 和 target normal
  -> 复用 symmetric normal-select / finite mask / formula / compress / tail
```

correspondences 可以作为第三步：

```text
row k:
  source point = cloud_src[correspondences[k].index_query]
  target point = cloud_tgt[correspondences[k].index_match]
  weight       = correspondences[k].weight  // 仅 weighted correspondences overload
```

这一步把双侧 gather、query/match 展开、weight union 字段读取和 row 分布放回同一个入口形态。只有在前两个 indexed 消融给出足够信号后，correspondences production candidate 才值得重新评估。

本轮实现位置：

| 文件 | 新增内容 | 证据角色 |
| --- | --- | --- |
| `test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls/transformation_estimation_symmetric_point_to_plane_lls_diag.hpp` | `accumulate_std_source_indices`、`accumulate_candidate_source_indices`、`accumulate_std_dual_indices`、`accumulate_candidate_dual_indices`，以及共同的 `accumulate_loaded_rows` | 把 source gather + target stride、双侧 gather 两条取数形态与共同公式 pipeline 分离。 |
| `test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls/test_transformation_estimation_symmetric_point_to_plane_lls.cpp` | `SourceIndicesCandidateMatchesStd`、`DualIndicesCandidateMatchesStd` | QEMU 和板卡 correctness gate。 |
| `test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls/bench_transformation_estimation_symmetric_point_to_plane_lls.cpp` | `symmetric lls source-indices pointnormal`、`symmetric lls dual-indices pointnormal` | 目标硬件 indexed 消融性能证据。 |

本轮运行结果：

| 证据 | 命令 | 结果 |
| --- | --- | --- |
| QEMU correctness | `make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls run_test_compare` | std/RVV 各 25 个测试通过；source-indexed production direct、source-indexed `Scalar=double` fallback、source-indexed mixed layout、source-indexed invalid lane、dual-indices 独立 target stream、accepted_points、finite mask 和 4x4 矩阵结果均覆盖。 |
| QEMU bench 输出合同 | `make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls run_bench_compare` | 新增 case 可解析，checksum 对齐到当前容差；QEMU 不作为性能证据。 |
| 反汇编 | `make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls dump_bench_rvv` | source-indices lambda 中可见 `vle32.v`、source `vluxei32.v` 和 target `vlse32.v`；dual-indices lambda 中可见两条 index load，以及 source/target 两组 `vluxei32.v`。 |
| 板卡 | `make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls board_smoke` | production-direct source-indexed `PointNormal` 64K / 256K 为 `2.11x` / `1.87x`，`PointXYZINormal` 为 `2.10x` / `1.90x`；diagnostic dual-indices 为 `0.80x` / `0.63x`；correspondences 为 `0.77x` / `0.86x`。 |

证据边界：

- source 单侧 gather 在当前 synthetic PointNormal 输入、当前公式 staging 和当前压缩尾段下仍有板卡收益，因此 indexed 路径不能被一概排除。
- 双侧 gather 使用独立 target index stream 后为 `0.80x` / `0.63x`。这说明双侧 gather 成本仍能与 correspondences 的额外展开成本分离，但当前数据流已经不足以作为 production 候选。
- correspondences 仍退化，且退化不能归因为 gather 单一原因。当前双侧 indices 转负，而 correspondences 也负向，说明双侧 gather、correspondence parsing、临时 index vector 分配/填充、row 分布、cache locality、压缩尾段和 solver 占比都仍在候选原因内。
- 本轮 indexed 消融是 valid-index-only（只覆盖有效索引）诊断合同。标量参考 helper 对负数或越界 index 有防御性跳过，但 RVV candidate 不做同等过滤；因此 correctness 不能写成覆盖非法 index 行为，也不能把这条防御逻辑解释成 production 语义。
- 新增消融没有覆盖 `Scalar=double`、非法 index 行为、真实 ICP correspondence 分布或 weighted `correspondence.weight` 展开；generic normal source-indexed production dispatch 已在本轮接入，剩余缺口只在 dual-indices 和 correspondences。

这两个 indexed 消融的 bench 后续仍需要分别记录：

- correctness：矩阵结果与原 `ConstCloudIterator` 标量路径对拍。
- 反汇编：source 单侧索引应能看到 source gather 与 target stride load；双侧 indices 应能看到两侧 gather。
- 板卡性能：按 64K / 256K 等规模记录真实硬件速度。QEMU 只作为 correctness 和路径证据。
- 不能证明什么：source 单侧索引不能代表双侧 gather；双侧 indices 不能代表 correspondence parsing / weight 展开；correspondences 不能单因归咎于 gather。

## 12. 统一 policy 框架的可尝试形态

源码可以借助模板、overload 和 `ConstCloudIterator` 写出统一 helper。RVV 实现也可以做统一代码组织，但高性能实现不能只依赖运行时 iterator 抽象。原因是 iterator 每次 `operator->()` 只返回一个点，编译器难以从这个抽象恢复出一个 VL chunk 的连续字段地址、index stream 和 gather offset。

较可控的做法是编译期 policy（策略类型）框架。公开 overload 根据入口形态选择 policy；公共 RVV pipeline（流水线）复用 mask、公式 staging、compress/tail 和 normal-equation 累加。

```text
public overload
  -> RowSourcePolicy
  -> WeightPolicy
  -> common RVV row pipeline:
       load source fields
       load target fields
       load optional weight
       build finite mask
       compute row formula
       vcompress / buffer tail or future reduction
       accumulate ATA/ATb
       solve
```

`RowSourcePolicy` 可以拆成：

| policy | 入口形态 | source load | target load | 主要用途 |
| --- | --- | --- | --- | --- |
| `FullCloudRowSource` | full-cloud | stride load | stride load | 当前 symmetric production 已覆盖的形态。 |
| `SourceIndexedRowSource` | source indices + target full-cloud | gather | stride load | 已接入 production 的首个 indexed row source；test-rvv 继续保留对拍/边界诊断。 |
| `DualIndexedRowSource` | source indices + target indices | gather | gather | 双侧 gather 消融或后续候选。 |
| `CorrespondenceRowSource` | correspondences | query 展开后 gather | match 展开后 gather | 完整 correspondences 候选。 |

`WeightPolicy` 可以拆成：

| policy | 权重来源 | 适用入口 |
| --- | --- | --- |
| `NoWeights` | 不读取权重 | 非 weighted LLS 和 symmetric LLS。 |
| `ExternalWeights` | 连续 `weights_[k]` | weighted full-cloud 和 weighted indices overload。 |
| `CorrespondenceWeights` | 从 `correspondence.weight` 展开 | weighted correspondences overload。 |

这个框架需要的额外工作包括：

- 给每个 row source policy 写明确的 VL chunk load 合同，包括 index 保序、重复 index、32-bit byte offset gate 和 invalid index 的诊断边界。
- 把字段 offset / point type gate 复用到 source 和 target 两侧，遵循 `RVV Generic Point Type Strategy`，但每个 policy 仍独立决定 stride 或 gather。
- 把 weight 来源作为单独 policy，避免把 `weights_` 和 `correspondence.weight` 混成同一种入口语义。
- 给 common formula pipeline 定义 staging 合同：哪些量进入 buffer，mask 后是否保序，tail 如何映射回 production 的 `continue` 语义。
- 避免在内层 RVV loop 使用 virtual iterator 或 `std::function`。入口外层可以统一调度，内层应让模板实例编译成各自的 stride/gather 指令。
- 每个 policy 单独建立 correctness、反汇编和板卡 bench。生产接入只批准有独立证据的 policy。

本轮已经把 `FullCloudRowSource` 和 `SourceIndexedRowSource` 落成 production/test-rvv 共用 helper：它们共享同一条 `accumulate_loaded_rows` common pipeline，但分别负责全云 stride 和 source gather + target stride。这个框架已经服务 full-cloud 与 source-indexed production direct；test-rvv 继续保留 dual-indices 和 correspondences 诊断基础设施。若后续要继续扩大范围，应该优先从 dual-indices 与 correspondences 的消融证据补起，而不是回头把 source-indexed 再当作候选。

本轮 symmetric LLS 的 indexed 消融给这个框架增加了一个优先级信号：`SourceIndexedRowSource` 已在 production direct 中获得 `PointNormal` 64K / 256K `2.11x` / `1.87x`、`PointXYZINormal` `2.10x` / `1.90x`，已经是 production-ready 的 indexed row source；`DualIndexedRowSource` 使用独立 target index stream 后为 `0.80x` / `0.63x`，说明“双侧 gather”与“同一份 index list”不能混为一谈，当前只应继续留在 test-rvv 诊断；`CorrespondenceRowSource` 仍负向，必须继续把 query/match 展开、临时 index vector、row 分布和 weight 来源拆开，而不是直接把 dual gather policy 套到 correspondences production。

## 13. 实现和评估时应保持的边界

读这些 registration RVV 优化时，可以用下面的边界检查自己有没有把层次混掉：

- 源码公开入口有 full-cloud、source indices、source+target indices、correspondences。
- protected helper 只证明逐点公式复用，不证明各种入口的 RVV 取数成本相同。
- full-cloud 是规则顺序访问，适合先做 production direct。
- source indices 是单侧 gather，是当前 symmetric LLS 已获 production direct 证据的 indexed 子集；它仍要求 valid-index-only 输入合同。
- source+target indices 和 correspondences 都是双侧 gather，但 correspondences 还多 query/match/weight 展开语义。
- weighted 的 weight 流要单独说明来源；不能把 `weights_` 和 `correspondence.weight` 混成一个入口语义。
- correspondences 的负向性能只能说明当前组合不适合直接接 production；不能把退化主因写成单一原因。
- 统一 policy 框架不等于统一生产接入结论；每个 policy 都需要自己的 correctness、反汇编和板卡性能证据。

后续如果要继续扩大 indexed/correspondences production 候选，建议从 source+target indices、correspondences 的顺序逐步消融。每一步都需要独立的 correctness、反汇编归属和板卡性能证据；source-indexed 当前证据不能外推到 dual-indices 或 correspondences。
