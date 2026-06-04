# filters 模块后续候选复筛报告

本文档记录 `filters` 模块第一实施波次完成后的后续候选复筛。输入范围限定为 `doc-rvv/library-screening/filters/filters-module-second-pass.zh.md` 中 `1.2.4 后续保留的直接实施候选（26）`；未重新扩大到全模块。复筛结论基于已完成主题的板卡真实性能、回退原因、bench-only 结果和当前源码中的函数级数据流。

## 1. 输入依据与复筛原因

输入依据：

- `doc-rvv/library-screening/filters/filters-module-second-pass.zh.md`
- `doc-rvv/library-screening/module-optimization-workflow.zh.md`
- `doc-rvv/filters/*.zh.md`
- `test-rvv/filters/*/*-evaluation.zh.md`
- `filters/include/pcl/filters/impl/*.hpp` 与 `filters/src/voxel_grid_label.cpp` 中对应候选源码

复筛原因：

- 第一实施波次已经完成，二轮报告中靠前主题 `voxel_grid`、`convolution`、`filter_indices/filter`、`passthrough`、`crop_box`、`voxel_grid_covariance`、`fast_bilateral`、`fast_bilateral_omp` 均已 closeout。
- 已完成主题显示：直接主路径中的大规模线性扫描、organized 内区卷积、mask + `vcompress` 保序输出通常具备强收益；只覆盖前置预处理或尾段压缩的小片段时，整体收益容易被后续 lattice、map、sort、search、Eigen 或整点复制稀释。
- `fast_bilateral` blur RVV 已作为 bench-only 诊断验证，正确性和指令路径成立但板卡为 `0.95x`，不接入生产；`filter_indices` normals prototype 曾约 `0.62x` 后回退；这些结果要求后续候选必须下钻到函数入口，不能只按循环数量排序。

## 2. 筛选统计

| 分类 | 数量 | 说明 |
| --- | ---: | --- |
| 后续保留候选输入总数 | 26 | 来自二轮报告 `1.2.4` |
| 建议进入函数级评估 | 6 | 公开入口直接包含线性几何筛选、organized 下采样或可限定的简单字段条件 |
| bench-only / 诊断主题 | 6 | 有局部 RVV loop，但主成本或语义边界不支持直接生产接入 |
| 暂缓主题 | 14 | search、sort、map、random、Eigen solver、per-leaf 状态或邻域不规则访问主导 |
| 重新纳入 / 合并 / 删除 / 源码冲突 | 0 | 本轮未扩大范围；未发现需要推翻二轮候选全集的源码冲突 |

## 3. 已完成主题经验总结

| 主题 | 主路径收益范围 | 回退 / bench-only / 暂缓结论 | 对后续排序的影响 |
| --- | --- | --- | --- |
| `voxel_grid` | `PCLPointCloud2 getMinMax3D` 基础约 `3.65x`~`4.58x`，indices 约 `3.60x`，distance field 约 `2.53x`~`2.70x` | `PointCloud<PointT>` distance field 因泛型布局暂缓；`applyFilter` voxel index + sort 暂缓 | 规整 min/max 规约是强模式；sort / 分组主导的 voxel 类不能只靠前置 floor loop 升级 |
| `convolution` | dense organized `PointXYZI` 行列内区约 `2.85x`~`3.85x` | 旧列方向大 stride VL chunk 曾约 `0.36x`，改为横向 VL chunk 后成立；non-dense 与 RGB/RGBA 暂缓 | organized 图像式循环可优先，但 lane 组织必须贴合连续访存 |
| `filter_indices/filter` | indices-only sparse xyz 约 `2.21x`~`2.35x`，cloud-out 间接受益约 `1.62x` | normals RVV prototype 约 `0.62x` 后回退 Std | 保序 indices 压缩有效；整点复制或额外字段检查主导时降级 |
| `passthrough` | indices 主路径约 `2.60x`~`3.21x`，cloud-out 约 `1.63x` | 显式 subset indices 与 `PCLPointCloud2` 路径暂缓 | 字段区间判断 + inlier/removed 双路保序输出是强模式；gather/subset 要后置 |
| `crop_box` | dense identity indices 约 `2.09x`~`3.08x`，cloud-out 约 `2.63x` | subset、non-dense、transform、`PCLPointCloud2` 回退 | 多字段比较 + `vcompress` 是 filters 后续几何筛选的主要参考 |
| `voxel_grid_covariance` | dense leaf-id 预计算约 `1.46x`~`1.52x` | distance-field fallback `0.79x`，non-dense fallback `0.95x`；cov/eigen/searchable 状态保持标量 | 前置 leaf-id 可有中等收益，但不应优先于直接输出主路径 |
| `fast_bilateral` | z 预处理整体约 `1.04x`~`1.10x` | lattice blur bench-only RVV 为 `0.95x`，生产 blur 回退 | 只覆盖小前置片段通常是弱收益；邻域 / lattice / 冲突累加不因循环大自动升级 |
| `fast_bilateral_omp` | z 预处理整体约 `1.05x`~`1.16x` | OMP lattice 主体保持标量 | OMP + RVV 需要清晰边界；弱收益只适合极小、低风险、常用入口 |

## 4. 后续保留候选逐项复筛

| 文件 | 关键函数入口 | RVV 适配点 | 预期收益 | 风险 | 测试 / bench 可行性 | 推荐动作 |
| --- | --- | --- | --- | --- | --- | --- |
| `filters/include/pcl/filters/impl/approximate_voxel_grid.hpp` | `ApproximateVoxelGrid<PointT>::applyFilter(PointCloud&)` | finite xyz、`floor`、hash id 计算 | 低到中；只是 hash 表更新前置片段 | `history_` 冲突、`flush`、`Eigen::VectorXf` scratch、RGB 和全字段累加主导 | 可做 leaf-id / hash microbench | bench-only / 诊断 |
| `filters/include/pcl/filters/impl/bilateral.hpp` | `BilateralFilter<PointT>::applyFilter`、`computePointWeight` | 邻域距离和 intensity 权重循环 | 低；radius search 和邻域 gather 主导 | `exp` / `sqrt` 数值顺序、邻域大小变化、PointT intensity 假设 | 可测，但收益难归因 | 暂缓 |
| `filters/include/pcl/filters/impl/box_clipper3D.hpp` | `BoxClipper3D::clipPointCloud3D` | affine box 变换后 `abs(x/y/z)<=1`，保序 indices 输出 | 中；直接几何筛选，但与 `crop_box` 重叠 | 齐次 `w` 修正、subset gather、line/polygon 未实现路径 | 可复用 clipper 测试和 crop_box bench 形态 | 建议进入函数级评估 |
| `filters/include/pcl/filters/impl/conditional_removal.hpp` | `ConditionalRemoval<PointT>::applyFilter(PointCloud&)`、`ConditionBase::evaluate` | 单字段 `FieldComparison<float>` 的字段 load + compare；`keep_organized_` 可做 mask store | 中但低置信；仅简单条件可能成立 | 多态条件树、字段类型多、copyPoint、坏点填充和 removed_indices 语义复杂 | 上游覆盖存在，专项需限定条件形态 | 建议进入函数级评估，但排在几何筛选之后 |
| `filters/include/pcl/filters/impl/convolution_3d.hpp` | `Convolution3D::convolve`、`GaussianKernel::operator()` | 邻域 distances threshold、权重、加权求和 | 低；搜索和 kernel 泛型主导 | search 不规则、`exp`、PointIn/PointOut 泛型累加、OpenMP | 可做 microbench 但生产收益不稳 | 暂缓 |
| `filters/include/pcl/filters/impl/covariance_sampling.hpp` | `initCompute`、`computeCovarianceMatrix`、`applyFilter(Indices&)` | centroid、scaled point、6D vector 构造 | 低；Eigen 6x6、list sort、采样状态主导 | 浮点累加顺序、Eigen solver、list 状态 | 测试成本高 | 暂缓 |
| `filters/include/pcl/filters/impl/crop_hull.hpp` | `CropHull::applyFilter2D`、`applyFilter3D` | 每点 polygon / ray triangle 判定 | 低到中；控制流随 hull 变化 | 多 polygon、多 ray、crossing 语义、removed_indices 保序 | 可测但 case 复杂 | 暂缓 |
| `filters/include/pcl/filters/impl/extract_indices.hpp` | `ExtractIndices::applyFilterIndices`、`applyFilter`、`filterDirectly` | negative / removed set 差集可用 bitmap 方案探索；keep_organized 坏点写可诊断 | 低；现有主逻辑是 sort + `set_difference` 或整点 copy | indices 可能无序 / 重复，negative 语义、字段写、dense 标记 | bench 容易，但生产设计需重构 | bench-only / 诊断 |
| `filters/include/pcl/filters/impl/farthest_point_sampling.hpp` | `FarthestPointSampling::applyFilter(Indices&)` | 每轮距离数组更新和 max 查找 | 低；每轮依赖上一采样点 | 串行状态、随机起点、OpenMP reduction 边界 | 可 bench 但实现侵入 | 暂缓 |
| `filters/include/pcl/filters/impl/frustum_culling.hpp` | `FrustumCulling<PointT>::applyFilter(Indices&)` | 6 平面 dot、mask 合并、`negative_`、inlier/removed 保序压缩 | 中高到强；直接筛选主路径，算术密度高于 crop_box | far plane infinity、camera plane 预计算、subset indices gather、非标准 PointT | 专项 test/bench 可控 | 建议进入函数级评估 |
| `filters/include/pcl/filters/impl/grid_minimum.hpp` | `GridMinimum::applyFilterIndices` | 2D grid id 预计算、floor、可选 min/max | 低到中；后接 sort 和 per-cell min z | `floor`/FRM、sort 主导、indices gather、non-dense skip | 可做 leaf-id microbench | bench-only / 诊断 |
| `filters/include/pcl/filters/impl/local_maximum.hpp` | `LocalMaximum::applyFilterIndices` | 局部 z 比较尾段 | 低；projection + radius search + visited 状态主导 | searcher、visited 标记、neighbor list | bench 可行但覆盖太小 | 暂缓 |
| `filters/include/pcl/filters/impl/median_filter.hpp` | `MedianFilter::applyFilter(PointCloud&)` | organized window z 收集 | 低；`nth_element` median 主导 | 排序类局部操作、finite、边界窗口 | 可构造图像 bench | 暂缓 |
| `filters/include/pcl/filters/impl/model_outlier_removal.hpp` | `ModelOutlierRemoval::applyFilterIndices` | `model_->getDistancesToModel` 后的 threshold + compress | 低到中；只是模型距离后的尾段 | 模型多态、normal 模型、距离生成成本占比未知 | 可做后处理 microbench | bench-only / 诊断 |
| `filters/include/pcl/filters/impl/morphological_filter.hpp` | `applyMorphologicalOperator` | boxSearch 后 `getMinMax3D` min/max | 低；octree search 主导 | open/close 两阶段、search 结果不规则、已有 getMinMax3D 间接受益 | 测试成本偏高 | 暂缓 |
| `filters/include/pcl/filters/impl/normal_space.hpp` | `NormalSpaceSampling::applyFilter`、`findBin` | normal bin id 计算 | 低；list/bin/random sampling 主导 | random、dynamic_bitset、list 迭代、set_difference | 测试复杂 | 暂缓 |
| `filters/include/pcl/filters/impl/plane_clipper3D.hpp` | `PlaneClipper3D::clipPointCloud3D`、`clipPoint3D` | `a*x+b*y+c*z+d` plane dot、mask、`vcompress` 保序输出 | 强；公开入口直接线性筛选，语义比 frustum 更小 | subset indices gather、泛型 PointT 布局、polygon/line clip 非目标 | clipper 测试可复用，bench 容易 | 建议进入函数级评估 |
| `filters/include/pcl/filters/impl/project_inliers.hpp` | `ProjectInliers::applyFilter` | filters 侧主要分派到 sample_consensus model | 低 | 实际热点不在 filters 文件；模型类型和字段复制复杂 | 不适合作为 filters 专项 | 暂缓 |
| `filters/include/pcl/filters/impl/pyramid.hpp` + `filters/src/pyramid.cpp` | `pcl::filters::Pyramid<PointT>::compute` | organized 小 kernel 下采样，dense 路径可横向 VL chunk | 中；直接图像式循环，但点类型和 OMP 复杂 | RGB/RGBA 特化、non-dense threshold、PointT operator、OpenMP 边界 | 可建专项，需同时覆盖 impl/src | 建议进入函数级评估 |
| `filters/include/pcl/filters/impl/radius_outlier_removal.hpp` | `RadiusOutlierRemoval::applyFilterIndices` | `to_keep` 到 indices 的尾段压缩 | 低；radius / KNN search 主导 | searcher、OpenMP、dense/non-dense 两套语义 | 上游和 bench 可用但 RVV 贡献小 | 暂缓 |
| `filters/include/pcl/filters/impl/sampling_surface_normal.hpp` | `findXYZMaxMin`、`computeMeanAndCovarianceMatrix`、`partition` | min/max 与小分区 covariance 规约 | 低到中；递归 partition、`nth_element`、random 和 Eigen plane solve 主导 | 随机输出、浮点累加顺序、递归边界 | 可做数值 microbench | bench-only / 诊断 |
| `filters/include/pcl/filters/impl/shadowpoints.hpp` | `ShadowPoints::applyFilter(Indices&)`、`applyFilter(PointCloud&)` | point + normal 双 AoS dot、`abs` threshold、`negative_`、保序压缩 | 中高；直接线性几何判定 | 双输入 stride load、cloud-out 整点复制、keep_organized 坏点写 | 可建专项和 bench | 建议进入函数级评估 |
| `filters/include/pcl/filters/impl/statistical_outlier_removal.hpp` | `StatisticalOutlierRemoval::applyFilterIndices` | distances mean/stddev 规约、threshold compress | 低到中；KNN search 主导 | search 成本、sqrt、统计浮点顺序、valid_distances 边界 | 可做 distances-only microbench | bench-only / 诊断 |
| `filters/include/pcl/filters/impl/uniform_sampling.hpp` | `UniformSampling::applyFilter(Indices&)` | leaf id、voxel center distance | 低到中；`leaves_` map 和 conflict update 主导 | per-leaf 状态冲突、removed_indices 顺序 | 可复用 voxel 数据，但收益不稳 | 暂缓 |
| `filters/include/pcl/filters/impl/voxel_grid_occlusion_estimation.hpp` | `VoxelGridOcclusionEstimation` ray traversal / occlusion APIs | 坐标转换和 ray step 可局部 RVV | 低；ray traversal 状态机主导 | 分支多、voxel occupancy 访问不规则、输出状态复杂 | 测试 / bench 成本高 | 暂缓 |
| `filters/src/voxel_grid_label.cpp` | `VoxelGridLabel::applyFilter(PointCloud&)` | distance filter、leaf id 计算 | 低到中；后续 sort、label histogram、`std::map` 主导 | label 众数、RGB/field copy、固定点类型但状态复杂 | 可做专项但收益归因差 | 暂缓 |

## 5. 新的后续执行清单 / 状态表

### 5.1 建议进入函数级评估

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 状态 | 评估文档路径 | 主题文档路径 | 备注 |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | `plane_clipper3D` | `filters/include/pcl/filters/impl/plane_clipper3D.hpp` | `clipPointCloud3D` 的全云、`PointXYZ`、plane dot + mask + 保序输出 | 已完成 | `test-rvv/filters/plane_clipper3D/plane_clipper3D-evaluation.zh.md` | `doc-rvv/filters/plane_clipper3D-RVV.zh.md` | 全云 `PointXYZ` 主路径已接入 RVV；Milkv-Jupiter 主路径约 `2.48x`~`3.00x`，subset 与非 `PointXYZ` fallback 保持标量 |
| 2 | `frustum_culling` | `filters/include/pcl/filters/impl/frustum_culling.hpp` | `applyFilter(Indices&)` 的 6 平面 dot + mask + inlier/removed 压缩 | 已完成 | `test-rvv/filters/frustum_culling/frustum_culling-evaluation.zh.md` | `doc-rvv/filters/frustum_culling-RVV.zh.md` | dense 全云 `PointXYZ` 主路径已接入 RVV；Milkv-Jupiter 主路径约 `4.33x`~`5.94x`，non-dense、subset 与非 `PointXYZ` fallback 保持标量 |
| 3 | `shadowpoints` | `filters/include/pcl/filters/impl/shadowpoints.hpp` | `applyFilter(Indices&)` 的 point + normal dot、`abs` threshold、`negative_` | 待函数级评估 | `test-rvv/filters/shadowpoints/shadowpoints-evaluation.zh.md` | `doc-rvv/filters/shadowpoints-RVV.zh.md` | 验证双 AoS 输入筛选；cloud-out 后置 |
| 4 | `box_clipper3D` | `filters/include/pcl/filters/impl/box_clipper3D.hpp` | `clipPointCloud3D` 的 affine box 判定和保序输出 | 待函数级评估 | `test-rvv/filters/box_clipper3D/box_clipper3D-evaluation.zh.md` | `doc-rvv/filters/box_clipper3D-RVV.zh.md` | 与 crop_box 重叠，函数级评估需先证明独立入口价值 |
| 5 | `pyramid` | `filters/include/pcl/filters/impl/pyramid.hpp`、`filters/src/pyramid.cpp` | dense organized 小 kernel 下采样横向 VL chunk | 待函数级评估 | `test-rvv/filters/pyramid/pyramid-evaluation.zh.md` | `doc-rvv/filters/pyramid-RVV.zh.md` | 图像式循环候选；需同时评估 RGB/RGBA 特化和 OpenMP |
| 6 | `conditional_removal` | `filters/include/pcl/filters/impl/conditional_removal.hpp` | 简单 `FieldComparison<float>` 字段比较 + mask；通用条件 fallback | 待函数级评估 | `test-rvv/filters/conditional_removal/conditional_removal-evaluation.zh.md` | `doc-rvv/filters/conditional_removal-RVV.zh.md` | 低置信生产候选，只限简单条件形态 |

后续普通主题优化应按本表第一条未完成项推进。若函数级评估确认某主题不适合生产 RVV，应在对应评估文档和本状态表中记录回退原因，再进入下一条。

### 5.2 bench-only / 诊断主题

| 主题 | 诊断目标 | 不直接接入生产的原因 |
| --- | --- | --- |
| `approximate_voxel_grid` | finite + leaf-id/hash 预计算 microbench | hash bucket 冲突、flush、scratch 累加主导，生产收益难归因 |
| `grid_minimum` | 2D grid id + floor 预计算 microbench | sort 和 per-cell min z 主导 |
| `extract_indices` | bitmap / set-difference 替代方案与 keep_organized 坏点写诊断 | 当前主逻辑是 sort + `set_difference` 或整点字段写，语义边界复杂 |
| `model_outlier_removal` | 连续 distances 后的 threshold + compress microbench | `getDistancesToModel` 和模型多态主导，后处理占比未知 |
| `sampling_surface_normal` | min/max 与小分区 covariance 规约诊断 | recursive partition、random、Eigen plane solve 主导 |
| `statistical_outlier_removal` | distances mean/stddev 规约与 threshold compress | KNN search 主导，整体收益预计有限 |

bench-only / 诊断主题不改变公开 API 和生产分流。只有诊断结果显示板卡收益、覆盖面和维护成本同时成立，才重新纳入生产候选。

### 5.3 暂缓主题

| 主题 | 暂缓原因 |
| --- | --- |
| `bilateral` | radius search、邻域 gather、`exp` / `sqrt` 和浮点顺序主导 |
| `convolution_3d` | search 与 kernel 泛型主导，RVV 只覆盖邻域尾段 |
| `covariance_sampling` | Eigen 6x6、list sort、采样状态主导 |
| `crop_hull` | 多 polygon / ray triangle 控制流不规则 |
| `farthest_point_sampling` | 迭代采样状态依赖强 |
| `local_maximum` | projection、radius search、visited 标记主导 |
| `median_filter` | `nth_element` median 排序类操作主导 |
| `morphological_filter` | octree boxSearch 主导，`getMinMax3D` 只是间接受益 |
| `normal_space` | list/bin/random sampling 与 set_difference 主导 |
| `project_inliers` | filters 文件主要分派，实际热点在 sample_consensus 模型 |
| `radius_outlier_removal` | nearestK/radius search 主导 |
| `uniform_sampling` | `leaves_` map 与 per-leaf conflict update 主导 |
| `voxel_grid_occlusion_estimation` | ray traversal 状态机和 occupancy 访问不规则 |
| `src/voxel_grid_label.cpp` | sort、label histogram、`std::map` 与字段聚合主导 |
