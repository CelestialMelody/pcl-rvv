# filters 模块 RVV 第一轮文件级筛选报告

本文档记录 `filters` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 覆盖范围：`filters/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `109`，已判定 `109`（`109/109` 全覆盖）。
- 目录拆分：`include` `75`，`src` `34`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `filters/include/pcl/filters/`，`src` 文件以 `src/` 开头显示。

## 2. 第一轮筛选口径

- 第一轮是文件级粗筛，判断标准是文件内是否存在可向量化循环、数学密集片段、批量字段访问、规约、mask/压缩、图像式 organized 遍历或可诊断的局部 SIMD/RVV 点。
- `high/mid` 是第二轮必须复核并交代去向的初始候选基线，不是最终实施全集。
- `low` 表示本轮未发现足以进入二轮基线的证据，不是永久排除；如果第二轮源码下钻发现明显漏判，可以补入并说明证据。
- 第一轮不承诺 RVV 覆盖公开入口主成本；主成本覆盖、测试可行性、fallback 条件和维护风险由第二轮筛选继续判断。

优先级含义：

| 优先级 | 含义 |
| --- | --- |
| `high` | 文件内存在明显批量循环或数学密集片段，且粗看具备较强 SIMD/RVV 评估价值 |
| `mid` | 文件内存在可向量化片段，但主成本、数据布局、语义风险或测试入口需要第二轮继续确认 |
| `low` | 以声明、薄 wrapper、调度、类型、构建胶水、小规模固定计算或明显不规则状态路径为主 |

## 3. 第一轮筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 源码文件总数 | 109 | `filters/**` 源码文件，第三方实现仅登记覆盖，不纳入候选主线。 |
| 已判定文件数 | 109 | `109/109` |
| high | 6 | 二轮必查 |
| mid | 70 | 二轮必查 |
| low | 33 | 已覆盖但不进入二轮初始基线 |
| high + mid | 76 | 第一轮候选基线 |
| 候选占比 | 76/109 = 69.7% | high + mid / 源码文件总数 |

## 4. high 候选（6）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/convolution.hpp` | 循环52处，滤波/统计计算密集，建议优先优化 |
| `impl/voxel_grid.hpp` | 循环20处，滤波/统计计算密集，建议优先优化 |
| `src/voxel_grid_label.cpp` | 循环8处，滤波/统计计算密集，建议优先优化 |
| `voxel_grid.h` | 循环6处，滤波/统计计算密集，建议优先优化 |
| `impl/voxel_grid_covariance.hpp` | 循环6处，滤波/统计计算密集，建议优先优化 |
| `voxel_grid_covariance.h` | 循环2处，滤波/统计计算密集，建议优先优化 |

## 5. mid 候选（70）

| 文件 | 第一轮证据 |
| --- | --- |
| `src/pyramid.cpp` | 存在可向量化路径（循环30，数学项0），建议次优先 |
| `impl/covariance_sampling.hpp` | 存在可向量化路径（循环15，数学项30），建议次优先 |
| `impl/conditional_removal.hpp` | 存在可向量化路径（循环15，数学项6），建议次优先 |
| `impl/fast_bilateral.hpp` | 存在可向量化路径（循环14，数学项6），建议次优先 |
| `src/voxel_grid.cpp` | 存在可向量化路径（循环13，数学项27），建议次优先 |
| `impl/fast_bilateral_omp.hpp` | 存在可向量化路径（循环12，数学项5），建议次优先 |
| `impl/pyramid.hpp` | 存在可向量化路径（循环10，数学项3），建议次优先 |
| `src/extract_indices.cpp` | 存在可向量化路径（循环10，数学项2），建议次优先 |
| `impl/normal_space.hpp` | 存在可向量化路径（循环9，数学项7），建议次优先 |
| `impl/crop_hull.hpp` | 存在可向量化路径（循环8，数学项34），建议次优先 |
| `impl/plane_clipper3D.hpp` | 存在可向量化路径（循环6，数学项20），建议次优先 |
| `src/convolution.cpp` | 存在可向量化路径（循环6，数学项9），建议次优先 |
| `impl/voxel_grid_occlusion_estimation.hpp` | 存在可向量化路径（循环5，数学项69），建议次优先 |
| `impl/sampling_surface_normal.hpp` | 存在可向量化路径（循环5，数学项49），建议次优先 |
| `src/statistical_outlier_removal.cpp` | 存在可向量化路径（循环5，数学项17），建议次优先 |
| `src/passthrough.cpp` | 存在可向量化路径（循环5，数学项16），建议次优先 |
| `src/project_inliers.cpp` | 存在可向量化路径（循环5，数学项11），建议次优先 |
| `impl/grid_minimum.hpp` | 存在可向量化路径（循环5，数学项5），建议次优先 |
| `src/random_sample.cpp` | 存在可向量化路径（循环5，数学项1），建议次优先 |
| `impl/statistical_outlier_removal.hpp` | 存在可向量化路径（循环4，数学项12），建议次优先 |
| `src/crop_box.cpp` | 存在可向量化路径（循环4，数学项8），建议次优先 |
| `impl/extract_indices.hpp` | 存在可向量化路径（循环4，数学项1），建议次优先 |
| `impl/median_filter.hpp` | 存在可向量化路径（循环4，数学项0），建议次优先 |
| `impl/morphological_filter.hpp` | 存在可向量化路径（循环3，数学项13），建议次优先 |
| `src/radius_outlier_removal.cpp` | 存在可向量化路径（循环3，数学项7），建议次优先 |
| `impl/approximate_voxel_grid.hpp` | 存在可向量化路径（循环3，数学项5），建议次优先 |
| `impl/convolution_3d.hpp` | 存在可向量化路径（循环3，数学项5），建议次优先 |
| `impl/radius_outlier_removal.hpp` | 存在可向量化路径（循环3，数学项5），建议次优先 |
| `impl/farthest_point_sampling.hpp` | 存在可向量化路径（循环3，数学项4），建议次优先 |
| `impl/local_maximum.hpp` | 存在可向量化路径（循环3，数学项3），建议次优先 |
| `impl/filter.hpp` | 存在可向量化路径（循环3，数学项1），建议次优先 |
| `impl/filter_indices.hpp` | 存在可向量化路径（循环3，数学项1），建议次优先 |
| `normal_refinement.h` | 存在可向量化路径（循环2，数学项39），建议次优先 |
| `approximate_voxel_grid.h` | 存在可向量化路径（循环2，数学项31），建议次优先 |
| `impl/box_clipper3D.hpp` | 存在可向量化路径（循环2，数学项18），建议次优先 |
| `model_outlier_removal.h` | 存在可向量化路径（循环2，数学项18），建议次优先 |
| `impl/model_outlier_removal.hpp` | 存在可向量化路径（循环2，数学项12），建议次优先 |
| `impl/uniform_sampling.hpp` | 存在可向量化路径（循环2，数学项10），建议次优先 |
| `impl/shadowpoints.hpp` | 存在可向量化路径（循环2，数学项8），建议次优先 |
| `impl/bilateral.hpp` | 存在可向量化路径（循环2，数学项5），建议次优先 |
| `impl/passthrough.hpp` | 存在可向量化路径（循环2，数学项4），建议次优先 |
| `impl/frustum_culling.hpp` | 存在可向量化路径（循环1，数学项45），建议次优先 |
| `impl/project_inliers.hpp` | 存在可向量化路径（循环1，数学项9），建议次优先 |
| `impl/crop_box.hpp` | 存在可向量化路径（循环1，数学项7），建议次优先 |
| `voxel_grid_occlusion_estimation.h` | 存在可向量化路径（循环0，数学项67），建议次优先 |
| `crop_box.h` | 存在可向量化路径（循环0，数学项49），建议次优先 |
| `filter_indices.h` | 存在可向量化路径（循环0，数学项37），建议次优先 |
| `fast_bilateral.h` | 存在可向量化路径（循环0，数学项36），建议次优先 |
| `sampling_surface_normal.h` | 存在可向量化路径（循环0，数学项30），建议次优先 |
| `conditional_removal.h` | 存在可向量化路径（循环0，数学项29），建议次优先 |
| `bilateral.h` | 存在可向量化路径（循环0，数学项28），建议次优先 |
| `convolution.h` | 存在可向量化路径（循环0，数学项27），建议次优先 |
| `statistical_outlier_removal.h` | 存在可向量化路径（循环0，数学项25），建议次优先 |
| `uniform_sampling.h` | 存在可向量化路径（循环0，数学项24），建议次优先 |
| `frustum_culling.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `crop_hull.h` | 存在可向量化路径（循环0，数学项21），建议次优先 |
| `extract_indices.h` | 存在可向量化路径（循环0，数学项19），建议次优先 |
| `filter.h` | 存在可向量化路径（循环0，数学项19），建议次优先 |
| `passthrough.h` | 存在可向量化路径（循环0，数学项13），建议次优先 |
| `box_clipper3D.h` | 存在可向量化路径（循环0，数学项12），建议次优先 |
| `median_filter.h` | 存在可向量化路径（循环0，数学项12），建议次优先 |
| `covariance_sampling.h` | 存在可向量化路径（循环0，数学项11），建议次优先 |
| `project_inliers.h` | 存在可向量化路径（循环0，数学项10），建议次优先 |
| `radius_outlier_removal.h` | 存在可向量化路径（循环0，数学项10），建议次优先 |
| `plane_clipper3D.h` | 存在可向量化路径（循环0，数学项8），建议次优先 |
| `fast_bilateral_omp.h` | 存在可向量化路径（循环0，数学项7），建议次优先 |
| `pyramid.h` | 存在可向量化路径（循环0，数学项6），建议次优先 |
| `grid_minimum.h` | 存在可向量化路径（循环0，数学项5），建议次优先 |
| `normal_space.h` | 存在可向量化路径（循环0，数学项5），建议次优先 |
| `convolution_3d.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |

## 6. 全量文件覆盖表（109/109）

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `approximate_voxel_grid.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项31），建议次优先 | `impl/approximate_voxel_grid.hpp` |
| `bilateral.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项28），建议次优先 | `impl/bilateral.hpp` |
| `box_clipper3D.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项12），建议次优先 | `impl/box_clipper3D.hpp` |
| `clipper3D.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `conditional_removal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项29），建议次优先 | `impl/conditional_removal.hpp` |
| `convolution.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项27），建议次优先 | `impl/convolution.hpp` |
| `convolution_3d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `impl/convolution_3d.hpp` |
| `covariance_sampling.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项11），建议次优先 | `impl/covariance_sampling.hpp` |
| `crop_box.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项49），建议次优先 | `impl/crop_box.hpp` |
| `crop_hull.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项21），建议次优先 | `impl/crop_hull.hpp` |
| `experimental/functor_filter.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `extract_indices.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项19），建议次优先 | `impl/extract_indices.hpp` |
| `farthest_point_sampling.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/farthest_point_sampling.hpp` |
| `fast_bilateral.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项36），建议次优先 | `impl/fast_bilateral.hpp` |
| `fast_bilateral_omp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项7），建议次优先 | `impl/fast_bilateral_omp.hpp` |
| `filter.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项19），建议次优先 | `impl/filter.hpp` |
| `filter_indices.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项37），建议次优先 | `impl/filter_indices.hpp` |
| `frustum_culling.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `impl/frustum_culling.hpp` |
| `grid_minimum.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项5），建议次优先 | `impl/grid_minimum.hpp` |
| `impl/approximate_voxel_grid.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项5），建议次优先 | `-` |
| `impl/bilateral.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项5），建议次优先 | `-` |
| `impl/box_clipper3D.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项18），建议次优先 | `-` |
| `impl/conditional_removal.hpp` | `mid` | 是 | 存在可向量化路径（循环15，数学项6），建议次优先 | `-` |
| `impl/convolution.hpp` | `high` | 是 | 循环52处，滤波/统计计算密集，建议优先优化 | `-` |
| `impl/convolution_3d.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项5），建议次优先 | `-` |
| `impl/covariance_sampling.hpp` | `mid` | 是 | 存在可向量化路径（循环15，数学项30），建议次优先 | `-` |
| `impl/crop_box.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项7），建议次优先 | `-` |
| `impl/crop_hull.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项34），建议次优先 | `-` |
| `impl/extract_indices.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项1），建议次优先 | `-` |
| `impl/farthest_point_sampling.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项4），建议次优先 | `-` |
| `impl/fast_bilateral.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项6），建议次优先 | `-` |
| `impl/fast_bilateral_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项5），建议次优先 | `-` |
| `impl/filter.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项1），建议次优先 | `-` |
| `impl/filter_indices.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项1），建议次优先 | `-` |
| `impl/frustum_culling.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项45），建议次优先 | `-` |
| `impl/grid_minimum.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项5），建议次优先 | `-` |
| `impl/local_maximum.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项3），建议次优先 | `-` |
| `impl/median_filter.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项0），建议次优先 | `-` |
| `impl/model_outlier_removal.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项12），建议次优先 | `-` |
| `impl/morphological_filter.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项13），建议次优先 | `-` |
| `impl/normal_refinement.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/normal_space.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项7），建议次优先 | `-` |
| `impl/passthrough.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项4），建议次优先 | `-` |
| `impl/plane_clipper3D.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项20），建议次优先 | `-` |
| `impl/project_inliers.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项9），建议次优先 | `-` |
| `impl/pyramid.hpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项3），建议次优先 | `-` |
| `impl/radius_outlier_removal.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项5），建议次优先 | `-` |
| `impl/random_sample.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/sampling_surface_normal.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项49），建议次优先 | `-` |
| `impl/shadowpoints.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项8），建议次优先 | `-` |
| `impl/statistical_outlier_removal.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项12），建议次优先 | `-` |
| `impl/uniform_sampling.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项10），建议次优先 | `-` |
| `impl/voxel_grid.hpp` | `high` | 是 | 循环20处，滤波/统计计算密集，建议优先优化 | `-` |
| `impl/voxel_grid_covariance.hpp` | `high` | 是 | 循环6处，滤波/统计计算密集，建议优先优化 | `-` |
| `impl/voxel_grid_occlusion_estimation.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项69），建议次优先 | `-` |
| `local_maximum.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/local_maximum.hpp` |
| `median_filter.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项12），建议次优先 | `impl/median_filter.hpp` |
| `model_outlier_removal.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项18），建议次优先 | `impl/model_outlier_removal.hpp` |
| `morphological_filter.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/morphological_filter.hpp` |
| `normal_refinement.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项39），建议次优先 | `impl/normal_refinement.hpp` |
| `normal_space.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项5），建议次优先 | `impl/normal_space.hpp` |
| `passthrough.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项13），建议次优先 | `impl/passthrough.hpp` |
| `plane_clipper3D.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项8），建议次优先 | `impl/plane_clipper3D.hpp` |
| `project_inliers.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项10），建议次优先 | `impl/project_inliers.hpp` |
| `pyramid.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项6），建议次优先 | `impl/pyramid.hpp` |
| `radius_outlier_removal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项10），建议次优先 | `impl/radius_outlier_removal.hpp` |
| `random_sample.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/random_sample.hpp` |
| `sampling_surface_normal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项30），建议次优先 | `impl/sampling_surface_normal.hpp` |
| `shadowpoints.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/shadowpoints.hpp` |
| `statistical_outlier_removal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项25），建议次优先 | `impl/statistical_outlier_removal.hpp` |
| `uniform_sampling.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项24），建议次优先 | `impl/uniform_sampling.hpp` |
| `voxel_grid.h` | `high` | 是 | 循环6处，滤波/统计计算密集，建议优先优化 | `impl/voxel_grid.hpp` |
| `voxel_grid_covariance.h` | `high` | 是 | 循环2处，滤波/统计计算密集，建议优先优化 | `impl/voxel_grid_covariance.hpp` |
| `voxel_grid_label.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `voxel_grid_occlusion_estimation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项67），建议次优先 | `impl/voxel_grid_occlusion_estimation.hpp` |
| `src/approximate_voxel_grid.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/bilateral.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/conditional_removal.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/convolution.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项9），建议次优先 | `-` |
| `src/covariance_sampling.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/crop_box.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项8），建议次优先 | `-` |
| `src/crop_hull.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/extract_indices.cpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项2），建议次优先 | `-` |
| `src/farthest_point_sampling.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/fast_bilateral.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/fast_bilateral_omp.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/filter_indices.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/frustum_culling.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/grid_minimum.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/local_maximum.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/median_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/model_outlier_removal.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/morphological_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/normal_refinement.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/normal_space.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/passthrough.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项16），建议次优先 | `-` |
| `src/project_inliers.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项11），建议次优先 | `-` |
| `src/pyramid.cpp` | `mid` | 是 | 存在可向量化路径（循环30，数学项0），建议次优先 | `-` |
| `src/radius_outlier_removal.cpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项7），建议次优先 | `-` |
| `src/random_sample.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项1），建议次优先 | `-` |
| `src/sampling_surface_normal.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/shadowpoints.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/statistical_outlier_removal.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项17），建议次优先 | `-` |
| `src/uniform_sampling.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/voxel_grid.cpp` | `mid` | 是 | 存在可向量化路径（循环13，数学项27），建议次优先 | `-` |
| `src/voxel_grid_covariance.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/voxel_grid_label.cpp` | `high` | 是 | 循环8处，滤波/统计计算密集，建议优先优化 | `-` |
| `src/voxel_grid_occlusion_estimation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/filters/filters-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
