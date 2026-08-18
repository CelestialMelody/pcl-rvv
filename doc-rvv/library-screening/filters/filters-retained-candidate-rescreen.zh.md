# filters 模块保留候选复筛报告

本文档记录 `filters` 模块建议进行 RVV 优化的文件队列完成后的保留候选复筛。输入范围限定为 `doc-rvv/library-screening/filters/filters-function-evaluation-queue.zh.md` 中 `3.2 保留实施的候选文件` 的 26 个文件；未重新扩大到全模块。复筛结论基于已完成主题的板卡真实性能、回退原因、diagnostic / bench-only 证据路径结果和当前源码中的函数级数据流。

## 1. 输入依据与复筛原因

输入依据：

- `doc-rvv/library-screening/filters/filters-function-evaluation-queue.zh.md`
- `doc-rvv/library-screening/module-optimization-workflow.zh.md`
- `doc-rvv/filters/*.zh.md`
- `test-rvv/filters/*/*-evaluation.zh.md`
- `filters/include/pcl/filters/impl/*.hpp` 与 `filters/src/voxel_grid_label.cpp` 中对应候选源码

复筛原因：

- 二轮报告中的建议进行 RVV 优化的文件队列已经完成，其中 `voxel_grid`、`convolution`、`filter_indices/filter`、`passthrough`、`crop_box`、`voxel_grid_covariance`、`fast_bilateral`、`fast_bilateral_omp` 均已 closeout。
- 已完成主题显示：直接主路径中的大规模线性扫描、organized 内区卷积、mask + `vcompress` 保序输出通常具备强收益；只覆盖前置预处理或尾段压缩的小片段时，整体收益容易被后续 lattice、map、sort、search、Eigen 或整点复制稀释。
- `fast_bilateral` blur RVV 已作为 diagnostic / bench-only 路径验证，正确性和指令路径成立但板卡为 `1.00x`，不接入生产；`filter_indices` normals prototype 曾约 `0.62x` 后回退；这些结果要求后续候选必须下钻到函数入口，不能只按循环数量排序。

## 2. 筛选统计

| 分类                              | 数量 | 说明                                                                           |
| --------------------------------- | ---: | ------------------------------------------------------------------------------ |
| 保留候选输入总数                  |   26 | 来自函数评估队列 `3.2 保留实施的候选文件`                                     |
| 建议启动函数级评估                |    6 | 公开入口直接包含线性几何筛选、organized 下采样或可限定的简单字段条件           |
| 暂缓 / 不单独实施                 |   20 | 当前不作为保留候选复筛的默认启动队列，其中 19 个保留 diagnostic / bench 路径记录 |
| 重新纳入 / 合并 / 删除 / 源码冲突 |    0 | 本轮未扩大范围；未发现需要推翻二轮候选全集的源码冲突                           |

## 3. 已完成主题经验总结

| 主题                      | 主路径收益范围                                                                                                          | 回退 / 诊断暂缓结论                                                                               | 对后续排序的影响                                                                  |
| ------------------------- | ----------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------- |
| `voxel_grid`            | `PCLPointCloud2 getMinMax3D` 基础约 `3.65x`~`4.58x`，indices 约 `3.60x`，distance field 约 `2.53x`~`2.70x` | `PointCloud<PointT>` distance field 因泛型布局暂缓；`applyFilter` voxel index + sort 暂缓     | 规整 min/max 规约是强模式；sort / 分组主导的 voxel 类不能只靠前置 floor loop 升级 |
| `convolution`           | dense organized`PointXYZI` 行列内区约 `2.85x`~`3.85x`                                                             | 旧列方向大 stride VL chunk 曾约`0.36x`，改为横向 VL chunk 后成立；non-dense 与 RGB/RGBA 暂缓    | organized 图像式循环可优先，但 lane 组织必须贴合连续访存                          |
| `filter_indices/filter` | indices-only sparse xyz 约`2.21x`~`2.35x`，cloud-out 间接受益约 `1.62x`                                           | normals RVV prototype 约`0.62x` 后回退 Std                                                      | 保序 indices 压缩有效；整点复制或额外字段检查主导时降级                           |
| `passthrough`           | indices 主路径约`2.60x`~`3.21x`，cloud-out 约 `1.63x`                                                             | 显式 subset indices 与`PCLPointCloud2` 路径暂缓                                                 | 字段区间判断 + inlier/removed 双路保序输出是强模式；gather/subset 要后置          |
| `crop_box`              | dense identity indices 约`2.09x`~`3.08x`，cloud-out 约 `2.63x`                                                    | subset、non-dense、transform、`PCLPointCloud2` 回退                                             | 多字段比较 +`vcompress` 是 filters 后续几何筛选的主要参考                       |
| `voxel_grid_covariance` | dense leaf-id 预计算约`1.46x`~`1.52x`                                                                               | distance-field fallback`0.79x`，non-dense fallback `1.00x`；cov/eigen/searchable 状态保持标量 | 前置 leaf-id 可有中等收益，但不应优先于直接输出主路径                             |
| `fast_bilateral`        | z 预处理整体约`1.04x`~`1.10x`                                                                                       | lattice blur diagnostic RVV 为`1.00x`，生产 blur 回退                                          | 只覆盖小前置片段通常是弱收益；邻域 / lattice / 冲突累加不因循环大自动升级         |
| `fast_bilateral_omp`    | z 预处理整体约`1.05x`~`1.16x`                                                                                       | OMP lattice 主体保持标量                                                                          | OMP + RVV 需要清晰边界；弱收益只适合极小、低风险、常用入口                        |

## 4. 筛选口径修正

第二轮建议优化队列中的 `fast_bilateral` / `fast_bilateral_omp` 显示，organized 图像式循环和大规模数据本身不足以支撑生产优先级。两者在文件级具备大循环和深度图像式数据流，但当前可安全接入生产的 RVV 覆盖主要是 finite `z` 的 min/max 规约与 non-finite 替换；后续 lattice splat、blur、插值和 OpenMP 主体仍是主要成本。因此板卡整体收益只有 `1.04x`~`1.16x`，非 OMP blur microbench 正确且命中 RVV 指令，但板卡为 `1.00x`，不接入生产。

相对地，`plane_clipper3D` 与 `frustum_culling` 在二轮报告中属于保留实施的几何裁剪候选，但保留候选复筛后接入生产主路径并分别达到约 `2.48x`~`3.00x` 和 `4.33x`~`5.94x`。这类主题的共同点是 RVV 覆盖公开入口的直接筛选主成本：AoS stride 读取 `x/y/z`、生成几何谓词 mask、用 `vcompress` 保序输出 indices / removed indices。

后续排序因此采用以下修正口径：

- “文件里有大循环”不等同于“RVV 覆盖入口主成本”；
- 只覆盖前置预处理、尾段压缩或小片段的候选，必须先证明该片段在整体入口中占比足够，或先作为后续 topic 内的 diagnostic / bench-only 证据路径；
- diagnostic / bench-only 路径重新纳入生产候选时，不能只依据局部 microbench speedup；必须看 full diagnostic 或生产入口 case 是否在板卡上稳定明显收益，并确认收益覆盖入口主成本、fallback 边界清晰、语义风险和维护复杂度可接受；
- 若局部片段正确且加速，但 sort / search / map / Eigen / 状态机 / 整点复制等后续主成本把 full diagnostic 稀释到弱收益区间，默认保留为 diagnostic / bench-only 证据路径，不接入生产；
- search、sort、map、Eigen solver、lattice、冲突累加、随机采样、邻域不规则访问和整点字段复制会稀释局部 RVV 收益；
- 直接线性扫描、organized 连续访存、规整字段遍历、mask + `vcompress` 保序输出是 filters 已完成主题中最稳定的强收益模式；
- diagnostic / bench-only 路径按状态分层，不再用旧的“6.2 诊断路径记录 / 6.3 观察暂不建 bench”硬切分；未完成且不作为默认继续队列的主题统一标为 `保留 / 待诊断`；
- diagnostic / bench-only 路径只记录能回答明确局部问题或有机会形成明确诊断问题的候选，不把所有“有局部 loop”的暂缓项自动纳入。

主成本覆盖类型使用以下口径：

- `direct-main-path`：RVV 可覆盖公开入口主成本或输出生成主路径；
- `partial-preprocess`：RVV 只覆盖前置预处理，后续主成本可能稀释收益；
- `tail-compress`：RVV 只覆盖后处理 threshold、mask 或压缩；
- `diagnostic`：可做局部实验，但当前不承诺生产分流；
- `non-standalone`：filters 文件本身不承载主要热点或真实循环在其它主题。

## 5. 保留候选逐项复筛

本节按推荐动作拆分逐项复筛结论。第 5 节侧重说明分类理由；第 6 节再给出可执行状态表，避免一个超宽总表同时承担分析和状态跟踪。

| 分类                    | 主题数 | 主要判断口径                                                                                 |
| ----------------------- | -----: | -------------------------------------------------------------------------------------------- |
| 建议启动函数级评估      |      6 | RVV 有机会覆盖公开入口直接主路径，且专项 test/bench 可闭环                                   |
| 暂缓 / 不单独实施       |     20 | 当前不作为独立保留候选复筛默认启动项；其中 19 个保留 diagnostic / bench-only 证据路径记录    |

### 5.1 建议启动函数级评估

| 主题                    | 关键入口                                                                              | 主成本覆盖类型                       | RVV 适配点                                                                                | 主要风险                                                                      | 推荐理由                                                                               |
| ----------------------- | ------------------------------------------------------------------------------------- | ------------------------------------ | ----------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| `plane_clipper3D`     | `PlaneClipper3D::clipPointCloud3D`、`clipPoint3D`                                 | `direct-main-path`                 | `a*x+b*y+c*z+d` plane dot、mask、`vcompress` 保序输出                                 | subset indices gather、泛型 PointT 布局、polygon/line clip 非目标             | 公开入口直接线性筛选，语义比 frustum 更小，clipper 测试可复用，预期收益强              |
| `frustum_culling`     | `FrustumCulling<PointT>::applyFilter(Indices&)`                                     | `direct-main-path`                 | 6 平面 dot、mask 合并、`negative_`、inlier/removed 保序压缩                             | far plane infinity、camera plane 预计算、subset indices gather、非标准 PointT | 直接筛选主路径，算术密度高于 crop_box，专项 test/bench 可控，预期中高到强              |
| `shadowpoints`        | `ShadowPoints::applyFilter(Indices&)`、`applyFilter(PointCloud&)`                 | `direct-main-path`                 | point + normal 双 AoS dot、`abs` threshold、`negative_`、保序压缩                     | 双输入 stride load、cloud-out 整点复制、keep_organized 坏点写                 | 直接线性几何判定，可建专项和 bench；实际完成后已转为 diagnostic-only 生产回退，状态见第 6 节 |
| `box_clipper3D`       | `BoxClipper3D::clipPointCloud3D`                                                    | `direct-main-path`                 | affine box 变换后`abs(x/y/z)<=1`，保序 indices 输出                                     | 齐次`w` 修正、subset gather、line/polygon 未实现路径，与 `crop_box` 重叠  | 直接几何筛选，有中等收益机会；函数级评估需先证明独立入口价值                           |
| `pyramid`             | `pcl::filters::Pyramid<PointT>::compute`                                            | `direct-main-path`                 | organized 小 kernel 下采样，dense 路径可横向 VL chunk                                     | RGB/RGBA 特化、non-dense threshold、PointT operator、OpenMP 边界              | 图像式循环候选，可建专项；需同时覆盖 impl/src 与点类型分支                             |
| `conditional_removal` | `ConditionalRemoval<PointT>::applyFilter(PointCloud&)`、`ConditionBase::evaluate` | `direct-main-path`（限定简单条件） | 单字段`FieldComparison<float>` 字段 load + compare；`keep_organized_` 可做 mask store | 多态条件树、字段类型多、copyPoint、坏点填充和 removed_indices 语义复杂        | 简单条件形态可能成立，上游覆盖存在；低置信生产候选，排在几何筛选之后                   |

### 5.2 暂缓 / 不单独实施（诊断路径记录）

本节保留原诊断路径记录与 `观察 / 暂不建 bench` 的事实材料，但不再把它们作为模块复筛固定队列。它们本质上都是“有局部 RVV 点或可隔离诊断问题，但当前不直接承诺生产分流”；区别用 `诊断成熟度 / 状态` 与“当前结论 / 重新考虑条件”表达：

- `已完成 / 升级生产`：诊断结果证明 full / production 入口收益、fallback 和维护边界成立；
- `已完成 / 生产不接入`：局部实验正确，但 full diagnostic 或生产入口收益不足；
- `可直接建 bench`：诊断问题明确，下一次按专项 test/bench 工作方式即可推进；
- `保留 / 待诊断`：不是默认继续队列；若诊断方向已较明确，需要等待使用场景、profile、数据集或新方案证明目标片段接近入口主成本；若诊断问题尚不成熟，则需先证明 bench 问题能代表真实入口。

| 主题                                | 关键入口                                                                    | 主成本覆盖类型           | 诊断成熟度 / 状态                | 诊断目标 / 局部 RVV 点                                        | 当前结论 / 重新考虑条件                                                                                                                                                                     |
| ----------------------------------- | --------------------------------------------------------------------------- | ------------------------ | -------------------------------- | ------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `approximate_voxel_grid`          | `ApproximateVoxelGrid<PointT>::applyFilter(PointCloud&)`                  | `partial-preprocess`   | 已完成（`PointXYZ` 生产接入）  | finite xyz、`floor`、leaf-id/hash 预计算；full bucket/flush | leaf-id/hash 片段、full`PointXYZ` 诊断和生产入口均成立，已升级生产；非 `PointXYZ`、小规模、泛型 `FieldList`、scratch 和 RGB/RGBA 仍回退标量                                           |
| `grid_minimum`                    | `GridMinimum::applyFilterIndices`                                         | `partial-preprocess`   | 已完成（生产不接入）             | 2D grid id + floor 预计算，可选 min/max                       | cell-id 局部片段有收益，但 full diagnostic 被 sort 和 per-cell min z 稀释；生产入口不接入本主题分流                                                                                         |
| `extract_indices`                 | `ExtractIndices::applyFilterIndices`、`applyFilter`、`filterDirectly` | `diagnostic`      | 已完成（生产不接入）             | bitmap / set-difference 替代方案与 keep_organized 坏点写      | bitmap scan 局部收益明显，但 keep_organized full diagnostic / production 不成立；生产入口不接入本主题分流                                                                                   |
| `bilateral`                       | `BilateralFilter<PointT>::applyFilter`、`computePointWeight`            | `PointXYZI-production` | 已完成（`PointXYZI` 生产接入） | 邻域距离和 intensity 权重循环；common`expf_RVV_f32m2`       | full exp diagnostic 和生产`BilateralFilter<PointXYZI>` 入口板卡收益成立，误差低于专项预算；非覆盖类型和小邻域回退标量                                                                     |
| `model_outlier_removal`           | `ModelOutlierRemoval::applyFilterIndices`                                 | `tail-compress`        | 保留 / 待诊断                    | `getDistancesToModel` 后的 threshold + compress             | `getDistancesToModel` 和模型多态主导，后处理占比未知；仅在距离生成成本已可隔离或模型侧已有热点证据时推进                                                                                  |
| `normal_space`                    | `NormalSpaceSampling::applyFilter`、`findBin`                           | `partial-preprocess`   | 保留 / 待诊断                    | normal bin id 预计算                                          | list/bin/random sampling 主导，生产接入需重构采样状态；仅在固定 bin 预处理占比明确时推进                                                                                                    |
| `radius_outlier_removal`          | `RadiusOutlierRemoval::applyFilterIndices`                                | `tail-compress`        | 已完成（生产不接入）             | `to_keep` 到 indices / removed_indices 的尾段压缩           | tail-only 板卡结果不稳定，full diagnostic 在 search-dominated case 约`0.99x`~`1.02x`；nearestK/radius search 主导，生产入口不接入本主题分流                                             |
| `sampling_surface_normal`         | `findXYZMaxMin`、`computeMeanAndCovarianceMatrix`、`partition`        | `partial-preprocess`   | 保留 / 待诊断                    | min/max 与小分区 covariance 规约                              | recursive partition、random、Eigen plane solve 主导；仅在分区 covariance / minmax 成为独立热点时推进                                                                                        |
| `statistical_outlier_removal`     | `StatisticalOutlierRemoval::applyFilterIndices`                           | `tail-compress`        | 保留 / 待诊断                    | distances mean/stddev 规约与 threshold compress               | KNN search 主导，整体收益预计有限；仅在距离数组已存在且统计尾段占比明确时推进                                                                                                               |
| `uniform_sampling`                | `UniformSampling::applyFilter(Indices&)`                                  | `partial-preprocess`   | 保留 / 待诊断                    | leaf id 与 voxel center distance                              | `leaves_` map 和 per-leaf conflict update 主导；仅在 leaf-id / distance 片段能覆盖真实入口主成本时推进                                                                                    |
| `src/voxel_grid_label.cpp`        | `VoxelGridLabel::applyFilter(PointCloud&)`                                | `partial-preprocess`   | 保留 / 待诊断                    | distance filter 与 leaf id 计算                               | sort、label histogram、`std::map` 和字段聚合主导，固定点类型适合后续按具体数据集做局部诊断                                                                                                |
| `convolution_3d`                  | `Convolution3D::convolve`、`GaussianKernel::operator()`                 | `diagnostic`      | 已完成（生产不接入）             | 邻域 distances threshold、权重、加权求和                      | kernel-only 板卡约`1.85x`，但 full diagnostic 仅约 `1.09x`~`1.11x`，未改生产入口约 `1.00x`；`radiusSearch` 与不规则邻域访问稀释收益，保留 diagnostic / bench-only 证据             |
| `covariance_sampling`             | `initCompute`、`computeCovarianceMatrix`、`applyFilter(Indices&)`     | `partial-preprocess`   | 已完成（生产不接入）             | centroid、scaled point、6D vector 构造                        | 板卡 scaled-point 片段仅约`1.01x`~`1.08x`，6D-vector 有 `0.78x`~`0.79x` 退化 case，full diagnostic 约 `0.99x`~`1.00x`；Std/RVV checksum 不一致，生产语义证据不足，不接生产分流 |
| `crop_hull`                       | `CropHull::applyFilter2D`、`applyFilter3D`                              | `diagnostic`      | 保留 / 待诊断                    | 固定 hull 的 polygon / ray triangle 判定                      | 多 polygon、多 ray、crossing 语义复杂，fixed hull microbench 泛化性弱；需限定常见 hull 形态并证明判定循环占主成本                                                                           |
| `farthest_point_sampling`         | `FarthestPointSampling::applyFilter(Indices&)`                            | `diagnostic`      | 保留 / 待诊断                    | 每轮距离数组更新和 max 查找                                   | 每轮依赖上一采样点且 OpenMP reduction / 随机起点影响语义；需证明能保持采样顺序且距离更新 + max 是主要瓶颈                                                                                   |
| `local_maximum`                   | `LocalMaximum::applyFilterIndices`                                        | `diagnostic`      | 保留 / 待诊断                    | 投影后局部 z 比较尾段                                         | projection、radius search、visited 标记主导；需获得 search 后连续邻域数据形态并证明尾段比较占比                                                                                             |
| `median_filter`                   | `MedianFilter::applyFilter(PointCloud&)`                                  | `diagnostic`      | 保留 / 待诊断                    | organized window z 收集、finite mask、移动限幅                | `nth_element` median 排序类操作主导；需提出不改变 median 语义且代表真实窗口成本的诊断设计                                                                                                 |
| `morphological_filter`            | `applyMorphologicalOperator`                                              | `diagnostic`      | 保留 / 待诊断                    | boxSearch 后 search-result min/max                            | octree boxSearch 主导，且已有`getMinMax3D` 间接受益；需确认 search-result 数组形态和 min/max 占比                                                                                         |
| `voxel_grid_occlusion_estimation` | `VoxelGridOcclusionEstimation` ray traversal / occlusion APIs             | `diagnostic`      | 保留 / 待诊断                    | 坐标转换、ray step、box intersection                          | ray traversal 状态机和 occupancy 访问不规则；需把 ray math 与 occupancy 访问合成可归因诊断 case                                                                                             |

diagnostic / bench-only 路径记录不改变公开 API 和生产分流。只有诊断结果显示板卡收益、覆盖面和维护成本同时成立，才重新纳入生产候选。

### 5.3 暂缓 / 不单独实施（非独立 filters 主题）

| 主题                | 关键入口                        | 主成本覆盖类型     | 暂缓原因                                                                                           | 重新考虑条件                                                                                     |
| ------------------- | ------------------------------- | ------------------ | -------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| `project_inliers` | `ProjectInliers::applyFilter` | `non-standalone` | filters 侧主要分派到 sample_consensus model，实际热点不在 filters 文件本身；模型类型和字段复制复杂 | 若后续 sample_consensus 模型侧形成独立 RVV 主题，可作为模型主题的一部分复核，不作为 filters 专项 |

## 6. 新的后续执行清单 / 状态表

### 6.1 建议启动函数级评估

| 顺序 | 主题                    | 主文件                                                                        | 推荐入口 / 第一 RVV 目标                                                          | 状态                           | 当前结论 / 下一步条件                                                                                                                                                                                                         |
| ---: | ----------------------- | ----------------------------------------------------------------------------- | --------------------------------------------------------------------------------- | ------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
|    1 | `plane_clipper3D`     | `filters/include/pcl/filters/impl/plane_clipper3D.hpp`                      | `clipPointCloud3D` 的全云、XYZ-compatible、plane dot + mask + 保序输出        | 已完成                         | 全云`PointXYZ` 主路径已接入 RVV；当前源码 gate 已扩大为 XYZ-compatible，`PointXYZI` 正确性已由 QEMU 对拍覆盖，板卡类型 case 需后续重命名并重跑                                                                           |
|    2 | `frustum_culling`     | `filters/include/pcl/filters/impl/frustum_culling.hpp`                      | `applyFilter(Indices&)` 的 6 平面 dot + mask + inlier/removed 压缩            | 已完成                         | dense 全云`PointXYZ` 主路径已接入 RVV；当前源码 gate 已扩大为 XYZ-compatible，`PointXYZI` 正确性已由 QEMU 对拍覆盖，板卡类型 case 需后续重命名并重跑                                                                     |
|    3 | `shadowpoints`        | `filters/include/pcl/filters/impl/shadowpoints.hpp`                         | `applyFilter(Indices&)` 的 point + normal dot、`abs` threshold、`negative_` | 已完成（diagnostic-only，生产回退） | diagnostic RVV helper 正确且指令命中，但 Milkv-Jupiter 主诊断 case 仅`0.44x`~`0.63x`，不接入生产；上游源码已回退，实验保留在 `test-rvv`；fallback case 约 `1.00x`                                                     |
|    4 | `box_clipper3D`       | `filters/include/pcl/filters/impl/box_clipper3D.hpp`                        | `clipPointCloud3D` 的 affine box 判定和保序输出                                 | 已完成                         | 全云`PointXYZ` 主路径已接入 RVV；当前源码 gate 已扩大为 XYZ-compatible，`PointXYZI` 正确性已由 QEMU 对拍覆盖，板卡类型 case 需后续重命名并重跑                                                                           |
|    5 | `pyramid`             | `filters/include/pcl/filters/impl/pyramid.hpp`、`filters/src/pyramid.cpp` | dense organized 小 kernel 下采样横向 VL chunk                                     | 已完成                         | dense`PointXYZ` small-kernel 单线程主路径已接入 RVV；Milkv-Jupiter 640x480 / 1280x720 small-kernel 约 `2.16x` / `2.13x`，显式多线程、5x5 large-kernel、non-dense、非 `PointXYZ` 与 RGB/RGBA/RGB 特化保持标量 fallback |
|    6 | `conditional_removal` | `filters/include/pcl/filters/impl/conditional_removal.hpp`                  | 简单`FieldComparison<float>` 字段比较 + mask；通用条件 fallback                 | 已完成                         | XYZ-compatible 点类型、全云、dense、单个`FieldComparison<float>` 的 `GT/GE/LT/LE` 主路径已接入 RVV；Milkv-Jupiter 主路径约 `2.87x`~`4.65x`，`keep_organized`、subset、复合条件与 `EQ` fallback 保持标量           |

后续普通主题优化应按本表第一条未完成项推进。若函数级评估确认某主题不适合生产 RVV，应在对应评估文档和本状态表中记录回退原因，再进入下一条。

### 6.2 暂缓 / 不单独实施（诊断路径记录）

诊断路径记录的共同问题是局部 RVV 点或诊断问题不能直接等价于生产主路径收益。`保留 / 待诊断` 不是默认继续队列：若诊断方向已较明确，需要等待明确使用场景、profile、数据集或新方案证明目标片段接近入口主成本；若诊断问题尚不成熟，则需先证明 bench 问题能代表真实入口，再决定是否建立专项目录。diagnostic / bench-only 路径默认不修改公开 API、不接入生产分流；只有 full diagnostic 或生产入口在板卡上稳定明显收益，且覆盖面、fallback 和维护成本同时成立，才重新纳入生产候选。

| 顺序 | 主题                                | 诊断目标 / 局部 RVV 点                                                                                                            | 状态                             | 诊断文档 / 证据路径                                                                                                                                                                                                           | 当前结论 / 下一步条件                                                                                                                                                                                                                            |
| ---: | ----------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- | -------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
|    1 | `approximate_voxel_grid`          | finite + leaf-id/hash 预计算 microbench；full`PointXYZ` bucket/flush/centroid 诊断；生产 `PointXYZ` 主路径                    | 已完成（`PointXYZ` 生产接入）  | `test-rvv/filters/approximate_voxel_grid/approximate_voxel_grid-evaluation.zh.md`；`doc-rvv/filters/approximate_voxel_grid-RVV.zh.md`；`test-rvv/filters/approximate_voxel_grid/output/board/analyze_bench_compare.log` | leaf-id/hash 片段板卡约`1.98x`~`2.00x`，full `PointXYZ` 诊断约 `1.67x`~`1.68x`，生产 `ApproximateVoxelGrid<PointXYZ>` 约 `1.92x`；非 `PointXYZ`、小规模、泛型 `FieldList`、`Eigen::VectorXf scratch` 和 RGB/RGBA 仍回退标量 |
|    2 | `grid_minimum`                    | 2D grid id + floor 预计算 microbench                                                                                              | 已完成（生产不接入）             | `test-rvv/filters/grid_minimum/grid_minimum-evaluation.zh.md`；`doc-rvv/filters/grid_minimum-RVV.zh.md`；`test-rvv/filters/grid_minimum/output/board/analyze_bench_compare.log`                                         | cell-id 片段板卡约`1.33x`~`1.52x`，但 full diagnostic 仅约 `1.09x`~`1.14x`，sort 和 per-cell min z 稀释收益；生产入口不接入本主题分流                                                                                                   |
|    3 | `extract_indices`                 | bitmap / set-difference 替代方案与 keep_organized 坏点写诊断                                                                      | 已完成（生产不接入）             | `test-rvv/filters/extract_indices/extract_indices-evaluation.zh.md`；`doc-rvv/filters/extract_indices-RVV.zh.md`；`test-rvv/filters/extract_indices/output/board/analyze_bench_compare.log`                             | bitmap scan 局部片段板卡约`2.21x`~`4.84x`，但 keep_organized full diagnostic 仅约 `1.22x`~`1.24x` 或 `0.99x`，未修改生产入口约 `0.99x`；生产入口不接入本主题分流                                                                    |
|    4 | `bilateral`                       | `BilateralFilter<PointT>::applyFilter` / `computePointWeight` 的邻域距离和 intensity 权重循环；common `pcl::expf_RVV_f32m2` | 已完成（`PointXYZI` 生产接入） | `test-rvv/filters/bilateral/bilateral-evaluation.zh.md`；`doc-rvv/filters/bilateral-RVV.zh.md`；`test-rvv/filters/bilateral/output/board/analyze_bench_compare.log`                                                     | 保留标量 double/libm`exp` 时 full 约 `1.04x`，复用 common `expf_RVV_f32m2` 后 full exp diagnostic 为 `1.65x`~`1.73x`，生产入口为 `2.06x`~`3.43x`；误差低于专项预算，已升级生产                                                    |
|    5 | `model_outlier_removal`           | 连续 distances 后的 threshold + compress microbench                                                                               | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | `getDistancesToModel` 和模型多态主导，后处理占比未知；仅在距离生成成本已可隔离或模型侧已有热点证据时推进                                                                                                                                       |
|    6 | `normal_space`                    | normal bin id 预计算 microbench                                                                                                   | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | list/bin/random sampling 主导，生产接入需重构采样状态；仅在固定 bin 预处理占比明确时推进                                                                                                                                                         |
|    7 | `radius_outlier_removal`          | `to_keep` 到 indices / removed_indices 的尾段压缩 microbench                                                                    | 已完成（生产不接入）             | `test-rvv/filters/radius_outlier_removal/radius_outlier_removal-evaluation.zh.md`；`doc-rvv/filters/radius_outlier_removal-RVV.zh.md`；`test-rvv/filters/radius_outlier_removal/output/board/analyze_bench_compare.log` | tail-only 板卡结果从`0.91x` 到 `1.32x`，只有 no-removed 1M 单 case 有局部收益；search-dominated full diagnostic 约 `0.99x`~`1.02x`，生产入口不接入本主题分流                                                                             |
|    8 | `sampling_surface_normal`         | min/max 与小分区 covariance 规约诊断                                                                                              | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | recursive partition、random、Eigen plane solve 主导；仅在分区 covariance / minmax 成为独立热点时推进                                                                                                                                             |
|    9 | `statistical_outlier_removal`     | distances mean/stddev 规约与 threshold compress                                                                                   | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | KNN search 主导，整体收益预计有限；仅在距离数组已存在且统计尾段占比明确时推进                                                                                                                                                                    |
|   10 | `uniform_sampling`                | leaf id 与 voxel center distance microbench                                                                                       | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | `leaves_` map 和 per-leaf conflict update 主导；仅在 leaf-id / distance 片段能覆盖真实入口主成本时推进                                                                                                                                         |
|   11 | `src/voxel_grid_label.cpp`        | distance filter 与 leaf id 计算 microbench                                                                                        | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | sort、label histogram、`std::map` 和字段聚合主导，固定点类型适合后续按具体数据集做局部诊断                                                                                                                                                     |
|   12 | `convolution_3d`                  | 邻域 distances threshold 与 kernel 加权求和                                                                                       | 已完成（生产不接入）             | `test-rvv/filters/convolution_3d/convolution_3d-evaluation.zh.md`；`doc-rvv/filters/convolution_3d-RVV.zh.md`；`test-rvv/filters/convolution_3d/output/board/analyze_bench_compare.log`                                 | kernel-only microbench 板卡约`1.85x`，但 full diagnostic 仅约 `1.09x`~`1.11x`，production unchanged 约 `1.00x`；局部收益被 `radiusSearch` 和不规则邻域访问稀释，不接生产分流                                                           |
|   13 | `covariance_sampling`             | centroid、scaled point、6D vector 构造                                                                                            | 已完成（生产不接入）             | `test-rvv/filters/covariance_sampling/covariance_sampling-evaluation.zh.md`；`doc-rvv/filters/covariance_sampling-RVV.zh.md`；`test-rvv/filters/covariance_sampling/output/board/analyze_bench_compare.log`             | 板卡 scaled-point 片段仅约`1.01x`~`1.08x`，6D-vector 有 `0.78x`~`0.79x` 退化 case，full diagnostic 约 `0.99x`~`1.00x`；Std/RVV checksum 不一致，生产语义证据不足，不接生产分流                                                      |
|   14 | `crop_hull`                       | 固定 hull 的 polygon / ray triangle 判定                                                                                          | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | 多 polygon、多 ray、crossing 语义复杂，fixed hull microbench 泛化性弱；需限定常见 hull 形态并证明判定循环占主成本                                                                                                                                |
|   15 | `farthest_point_sampling`         | 距离数组更新和 max 查找                                                                                                           | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | 每轮依赖上一采样点且 OpenMP reduction / 随机起点影响语义，生产接入侵入性高；需保持采样顺序语义并证明距离更新 + max 是主要瓶颈                                                                                                                    |
|   16 | `local_maximum`                   | 固定邻域 z 比较                                                                                                                   | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | projection、radius search、visited 标记主导，局部比较覆盖真实入口太小；需获得 search 后连续邻域数据形态并证明尾段比较占比                                                                                                                        |
|   17 | `median_filter`                   | organized window z 收集和 finite mask                                                                                             | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | `nth_element` median 排序类操作主导，window gather 诊断暂不能说明整体收益；需提出不改变 median 语义且代表真实窗口成本的诊断设计                                                                                                                |
|   18 | `morphological_filter`            | search-result min/max                                                                                                             | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | octree boxSearch 主导，且已有`getMinMax3D` 间接受益；需确认 search-result 数组形态和 min/max 占比                                                                                                                                              |
|   19 | `voxel_grid_occlusion_estimation` | ray step、坐标转换、box intersection                                                                                              | 保留 / 待诊断                    | 待建                                                                                                                                                                                                                          | ray traversal 状态机和 occupancy 访问不规则，ray math microbench 与真实输出状态解耦过强；需把 ray math 与 occupancy 访问合成可归因诊断 case                                                                                                      |

诊断路径记录不是删除项。后续只有当能提出可隔离、可归因、可板卡验证的诊断问题，或已存在足够 profile 证明局部片段接近入口主成本时，才从本表重新启动专项评估。

### 6.3 暂缓 / 不单独实施（非独立 filters 主题）

| 主题                | 暂缓原因                                                                                             | 重新考虑条件                                                            |
| ------------------- | ---------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| `project_inliers` | filters 文件主要分派到 sample_consensus 模型，实际热点不在 filters 文件本身；不适合作为 filters 专项 | 若 sample_consensus 模型侧形成独立 RVV 主题，可作为模型主题的一部分复核 |

## 7. 阶段性收尾 / 后续策略

本阶段 filters 保留候选复筛已完成直接主路径候选闭环：6.1 的直接主路径候选全部完成。统一后的 diagnostic / bench-only 路径记录也已有一些结果：

- `approximate_voxel_grid` 证明前置 staging 若能带动 full / production 入口，可以升级为生产路径；
- `grid_minimum` 证明局部 cell-id 加速会被 sort 与 per-cell min-z 稀释；
- `extract_indices` 证明局部 bitmap scan 加速不能直接代表 keep_organized full diagnostic 或真实生产入口；
- `bilateral` 证明原观察项只要诊断设计能覆盖真实入口主成本，并补齐 common math helper、误差预算、full diagnostic 和生产入口证据，也可以升级为生产路径。

其余所有有局部 RVV 点或可隔离实验问题的主题统一放入 `暂缓 / 不单独实施` 中的 diagnostic / bench-only 路径记录，再用状态表达成熟度。后续策略改为：

- 已完成主题进入维护和复核阶段，优先处理文档、日志、提交拆分和已发现问题；
- `保留 / 待诊断` 主题不是默认继续队列；若诊断方向已较明确，需等待上游使用场景、profile、数据集或新方案证明“目标片段接近入口主成本”时再启动；若诊断问题尚不成熟，需先提出可隔离、可归因、可板卡验证的诊断问题，再决定是否建立 `test-rvv` 专项目录；
- 若后续重新打开 filters 新主题，应优先选择直接主路径、连续访存、mask + `vcompress` 能覆盖主要成本的入口，而不是只证明局部片段 speedup。
