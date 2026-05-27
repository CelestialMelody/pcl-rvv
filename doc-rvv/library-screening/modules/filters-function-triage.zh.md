# filters 模块文件级筛查清单（全覆盖版，重评）

本版按与 `registration/surface` 相同标准重评：全文件覆盖、实现优先、允许推翻旧结论；`3rdparty/**` 仅登记覆盖，不纳入候选。

## 1. 覆盖范围与口径

- 覆盖范围：`filters/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `109`，已判定 `109`（`109/109` 全覆盖）。
- 目录拆分：`include` `75`，`src` `34`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（6）

| file_path | 说明 |
| --- | --- |
| `filters/include/pcl/filters/impl/convolution.hpp` | 循环52处，滤波/统计计算密集，建议优先优化 |
| `filters/include/pcl/filters/impl/voxel_grid.hpp` | 循环20处，滤波/统计计算密集，建议优先优化 |
| `filters/src/voxel_grid_label.cpp` | 循环8处，滤波/统计计算密集，建议优先优化 |
| `filters/include/pcl/filters/voxel_grid.h` | 循环6处，滤波/统计计算密集，建议优先优化 |
| `filters/include/pcl/filters/impl/voxel_grid_covariance.hpp` | 循环6处，滤波/统计计算密集，建议优先优化 |
| `filters/include/pcl/filters/voxel_grid_covariance.h` | 循环2处，滤波/统计计算密集，建议优先优化 |

### 2.2 mid 候选（70）

| file_path | 说明 |
| --- | --- |
| `filters/src/pyramid.cpp` | 存在可向量化路径（循环30，数学项0），建议次优先 |
| `filters/include/pcl/filters/impl/covariance_sampling.hpp` | 存在可向量化路径（循环15，数学项30），建议次优先 |
| `filters/include/pcl/filters/impl/conditional_removal.hpp` | 存在可向量化路径（循环15，数学项6），建议次优先 |
| `filters/include/pcl/filters/impl/fast_bilateral.hpp` | 存在可向量化路径（循环14，数学项6），建议次优先 |
| `filters/src/voxel_grid.cpp` | 存在可向量化路径（循环13，数学项27），建议次优先 |
| `filters/include/pcl/filters/impl/fast_bilateral_omp.hpp` | 存在可向量化路径（循环12，数学项5），建议次优先 |
| `filters/include/pcl/filters/impl/pyramid.hpp` | 存在可向量化路径（循环10，数学项3），建议次优先 |
| `filters/src/extract_indices.cpp` | 存在可向量化路径（循环10，数学项2），建议次优先 |
| `filters/include/pcl/filters/impl/normal_space.hpp` | 存在可向量化路径（循环9，数学项7），建议次优先 |
| `filters/include/pcl/filters/impl/crop_hull.hpp` | 存在可向量化路径（循环8，数学项34），建议次优先 |
| `filters/include/pcl/filters/impl/plane_clipper3D.hpp` | 存在可向量化路径（循环6，数学项20），建议次优先 |
| `filters/src/convolution.cpp` | 存在可向量化路径（循环6，数学项9），建议次优先 |
| `filters/include/pcl/filters/impl/voxel_grid_occlusion_estimation.hpp` | 存在可向量化路径（循环5，数学项69），建议次优先 |
| `filters/include/pcl/filters/impl/sampling_surface_normal.hpp` | 存在可向量化路径（循环5，数学项49），建议次优先 |
| `filters/src/statistical_outlier_removal.cpp` | 存在可向量化路径（循环5，数学项17），建议次优先 |
| `filters/src/passthrough.cpp` | 存在可向量化路径（循环5，数学项16），建议次优先 |
| `filters/src/project_inliers.cpp` | 存在可向量化路径（循环5，数学项11），建议次优先 |
| `filters/include/pcl/filters/impl/grid_minimum.hpp` | 存在可向量化路径（循环5，数学项5），建议次优先 |
| `filters/src/random_sample.cpp` | 存在可向量化路径（循环5，数学项1），建议次优先 |
| `filters/include/pcl/filters/impl/statistical_outlier_removal.hpp` | 存在可向量化路径（循环4，数学项12），建议次优先 |
| `filters/src/crop_box.cpp` | 存在可向量化路径（循环4，数学项8），建议次优先 |
| `filters/include/pcl/filters/impl/extract_indices.hpp` | 存在可向量化路径（循环4，数学项1），建议次优先 |
| `filters/include/pcl/filters/impl/median_filter.hpp` | 存在可向量化路径（循环4，数学项0），建议次优先 |
| `filters/include/pcl/filters/impl/morphological_filter.hpp` | 存在可向量化路径（循环3，数学项13），建议次优先 |
| `filters/src/radius_outlier_removal.cpp` | 存在可向量化路径（循环3，数学项7），建议次优先 |
| `filters/include/pcl/filters/impl/approximate_voxel_grid.hpp` | 存在可向量化路径（循环3，数学项5），建议次优先 |
| `filters/include/pcl/filters/impl/convolution_3d.hpp` | 存在可向量化路径（循环3，数学项5），建议次优先 |
| `filters/include/pcl/filters/impl/radius_outlier_removal.hpp` | 存在可向量化路径（循环3，数学项5），建议次优先 |
| `filters/include/pcl/filters/impl/farthest_point_sampling.hpp` | 存在可向量化路径（循环3，数学项4），建议次优先 |
| `filters/include/pcl/filters/impl/local_maximum.hpp` | 存在可向量化路径（循环3，数学项3），建议次优先 |
| `filters/include/pcl/filters/impl/filter.hpp` | 存在可向量化路径（循环3，数学项1），建议次优先 |
| `filters/include/pcl/filters/impl/filter_indices.hpp` | 存在可向量化路径（循环3，数学项1），建议次优先 |
| `filters/include/pcl/filters/normal_refinement.h` | 存在可向量化路径（循环2，数学项39），建议次优先 |
| `filters/include/pcl/filters/approximate_voxel_grid.h` | 存在可向量化路径（循环2，数学项31），建议次优先 |
| `filters/include/pcl/filters/impl/box_clipper3D.hpp` | 存在可向量化路径（循环2，数学项18），建议次优先 |
| `filters/include/pcl/filters/model_outlier_removal.h` | 存在可向量化路径（循环2，数学项18），建议次优先 |
| `filters/include/pcl/filters/impl/model_outlier_removal.hpp` | 存在可向量化路径（循环2，数学项12），建议次优先 |
| `filters/include/pcl/filters/impl/uniform_sampling.hpp` | 存在可向量化路径（循环2，数学项10），建议次优先 |
| `filters/include/pcl/filters/impl/shadowpoints.hpp` | 存在可向量化路径（循环2，数学项8），建议次优先 |
| `filters/include/pcl/filters/impl/bilateral.hpp` | 存在可向量化路径（循环2，数学项5），建议次优先 |
| `filters/include/pcl/filters/impl/passthrough.hpp` | 存在可向量化路径（循环2，数学项4），建议次优先 |
| `filters/include/pcl/filters/impl/frustum_culling.hpp` | 存在可向量化路径（循环1，数学项45），建议次优先 |
| `filters/include/pcl/filters/impl/project_inliers.hpp` | 存在可向量化路径（循环1，数学项9），建议次优先 |
| `filters/include/pcl/filters/impl/crop_box.hpp` | 存在可向量化路径（循环1，数学项7），建议次优先 |
| `filters/include/pcl/filters/voxel_grid_occlusion_estimation.h` | 存在可向量化路径（循环0，数学项67），建议次优先 |
| `filters/include/pcl/filters/crop_box.h` | 存在可向量化路径（循环0，数学项49），建议次优先 |
| `filters/include/pcl/filters/filter_indices.h` | 存在可向量化路径（循环0，数学项37），建议次优先 |
| `filters/include/pcl/filters/fast_bilateral.h` | 存在可向量化路径（循环0，数学项36），建议次优先 |
| `filters/include/pcl/filters/sampling_surface_normal.h` | 存在可向量化路径（循环0，数学项30），建议次优先 |
| `filters/include/pcl/filters/conditional_removal.h` | 存在可向量化路径（循环0，数学项29），建议次优先 |
| `filters/include/pcl/filters/bilateral.h` | 存在可向量化路径（循环0，数学项28），建议次优先 |
| `filters/include/pcl/filters/convolution.h` | 存在可向量化路径（循环0，数学项27），建议次优先 |
| `filters/include/pcl/filters/statistical_outlier_removal.h` | 存在可向量化路径（循环0，数学项25），建议次优先 |
| `filters/include/pcl/filters/uniform_sampling.h` | 存在可向量化路径（循环0，数学项24），建议次优先 |
| `filters/include/pcl/filters/frustum_culling.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `filters/include/pcl/filters/crop_hull.h` | 存在可向量化路径（循环0，数学项21），建议次优先 |
| `filters/include/pcl/filters/extract_indices.h` | 存在可向量化路径（循环0，数学项19），建议次优先 |
| `filters/include/pcl/filters/filter.h` | 存在可向量化路径（循环0，数学项19），建议次优先 |
| `filters/include/pcl/filters/passthrough.h` | 存在可向量化路径（循环0，数学项13），建议次优先 |
| `filters/include/pcl/filters/box_clipper3D.h` | 存在可向量化路径（循环0，数学项12），建议次优先 |
| `filters/include/pcl/filters/median_filter.h` | 存在可向量化路径（循环0，数学项12），建议次优先 |
| `filters/include/pcl/filters/covariance_sampling.h` | 存在可向量化路径（循环0，数学项11），建议次优先 |
| `filters/include/pcl/filters/project_inliers.h` | 存在可向量化路径（循环0，数学项10），建议次优先 |
| `filters/include/pcl/filters/radius_outlier_removal.h` | 存在可向量化路径（循环0，数学项10），建议次优先 |
| `filters/include/pcl/filters/plane_clipper3D.h` | 存在可向量化路径（循环0，数学项8），建议次优先 |
| `filters/include/pcl/filters/fast_bilateral_omp.h` | 存在可向量化路径（循环0，数学项7），建议次优先 |
| `filters/include/pcl/filters/pyramid.h` | 存在可向量化路径（循环0，数学项6），建议次优先 |
| `filters/include/pcl/filters/grid_minimum.h` | 存在可向量化路径（循环0，数学项5），建议次优先 |
| `filters/include/pcl/filters/normal_space.h` | 存在可向量化路径（循环0，数学项5），建议次优先 |
| `filters/include/pcl/filters/convolution_3d.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |

## 3. 全量文件覆盖表（109/109）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `filters/include/pcl/filters/approximate_voxel_grid.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项31），建议次优先 | `filters/include/pcl/filters/impl/approximate_voxel_grid.hpp` |
| `filters/include/pcl/filters/bilateral.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项28），建议次优先 | `filters/include/pcl/filters/impl/bilateral.hpp` |
| `filters/include/pcl/filters/box_clipper3D.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项12），建议次优先 | `filters/include/pcl/filters/impl/box_clipper3D.hpp` |
| `filters/include/pcl/filters/clipper3D.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/include/pcl/filters/conditional_removal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项29），建议次优先 | `filters/include/pcl/filters/impl/conditional_removal.hpp` |
| `filters/include/pcl/filters/convolution.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项27），建议次优先 | `filters/include/pcl/filters/impl/convolution.hpp` |
| `filters/include/pcl/filters/convolution_3d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `filters/include/pcl/filters/impl/convolution_3d.hpp` |
| `filters/include/pcl/filters/covariance_sampling.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项11），建议次优先 | `filters/include/pcl/filters/impl/covariance_sampling.hpp` |
| `filters/include/pcl/filters/crop_box.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项49），建议次优先 | `filters/include/pcl/filters/impl/crop_box.hpp` |
| `filters/include/pcl/filters/crop_hull.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项21），建议次优先 | `filters/include/pcl/filters/impl/crop_hull.hpp` |
| `filters/include/pcl/filters/experimental/functor_filter.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/include/pcl/filters/extract_indices.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项19），建议次优先 | `filters/include/pcl/filters/impl/extract_indices.hpp` |
| `filters/include/pcl/filters/farthest_point_sampling.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `filters/include/pcl/filters/impl/farthest_point_sampling.hpp` |
| `filters/include/pcl/filters/fast_bilateral.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项36），建议次优先 | `filters/include/pcl/filters/impl/fast_bilateral.hpp` |
| `filters/include/pcl/filters/fast_bilateral_omp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项7），建议次优先 | `filters/include/pcl/filters/impl/fast_bilateral_omp.hpp` |
| `filters/include/pcl/filters/filter.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项19），建议次优先 | `filters/include/pcl/filters/impl/filter.hpp` |
| `filters/include/pcl/filters/filter_indices.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项37），建议次优先 | `filters/include/pcl/filters/impl/filter_indices.hpp` |
| `filters/include/pcl/filters/frustum_culling.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `filters/include/pcl/filters/impl/frustum_culling.hpp` |
| `filters/include/pcl/filters/grid_minimum.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项5），建议次优先 | `filters/include/pcl/filters/impl/grid_minimum.hpp` |
| `filters/include/pcl/filters/impl/approximate_voxel_grid.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项5），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/bilateral.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项5），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/box_clipper3D.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项18），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/conditional_removal.hpp` | `mid` | 是 | 存在可向量化路径（循环15，数学项6），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/convolution.hpp` | `high` | 是 | 循环52处，滤波/统计计算密集，建议优先优化 | `-` |
| `filters/include/pcl/filters/impl/convolution_3d.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项5），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/covariance_sampling.hpp` | `mid` | 是 | 存在可向量化路径（循环15，数学项30），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/crop_box.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项7），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/crop_hull.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项34），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/extract_indices.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项1），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/farthest_point_sampling.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项4），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/fast_bilateral.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项6），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/fast_bilateral_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项5），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/filter.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项1），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/filter_indices.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项1），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/frustum_culling.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项45），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/grid_minimum.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项5），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/local_maximum.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项3），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/median_filter.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项0），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/model_outlier_removal.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项12），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/morphological_filter.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项13），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/normal_refinement.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/include/pcl/filters/impl/normal_space.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项7），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/passthrough.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项4），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/plane_clipper3D.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项20），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/project_inliers.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项9），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/pyramid.hpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项3），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/radius_outlier_removal.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项5），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/random_sample.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/include/pcl/filters/impl/sampling_surface_normal.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项49），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/shadowpoints.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项8），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/statistical_outlier_removal.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项12），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/uniform_sampling.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项10），建议次优先 | `-` |
| `filters/include/pcl/filters/impl/voxel_grid.hpp` | `high` | 是 | 循环20处，滤波/统计计算密集，建议优先优化 | `-` |
| `filters/include/pcl/filters/impl/voxel_grid_covariance.hpp` | `high` | 是 | 循环6处，滤波/统计计算密集，建议优先优化 | `-` |
| `filters/include/pcl/filters/impl/voxel_grid_occlusion_estimation.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项69），建议次优先 | `-` |
| `filters/include/pcl/filters/local_maximum.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `filters/include/pcl/filters/impl/local_maximum.hpp` |
| `filters/include/pcl/filters/median_filter.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项12），建议次优先 | `filters/include/pcl/filters/impl/median_filter.hpp` |
| `filters/include/pcl/filters/model_outlier_removal.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项18），建议次优先 | `filters/include/pcl/filters/impl/model_outlier_removal.hpp` |
| `filters/include/pcl/filters/morphological_filter.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `filters/include/pcl/filters/impl/morphological_filter.hpp` |
| `filters/include/pcl/filters/normal_refinement.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项39），建议次优先 | `filters/include/pcl/filters/impl/normal_refinement.hpp` |
| `filters/include/pcl/filters/normal_space.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项5），建议次优先 | `filters/include/pcl/filters/impl/normal_space.hpp` |
| `filters/include/pcl/filters/passthrough.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项13），建议次优先 | `filters/include/pcl/filters/impl/passthrough.hpp` |
| `filters/include/pcl/filters/plane_clipper3D.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项8），建议次优先 | `filters/include/pcl/filters/impl/plane_clipper3D.hpp` |
| `filters/include/pcl/filters/project_inliers.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项10），建议次优先 | `filters/include/pcl/filters/impl/project_inliers.hpp` |
| `filters/include/pcl/filters/pyramid.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项6），建议次优先 | `filters/include/pcl/filters/impl/pyramid.hpp` |
| `filters/include/pcl/filters/radius_outlier_removal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项10），建议次优先 | `filters/include/pcl/filters/impl/radius_outlier_removal.hpp` |
| `filters/include/pcl/filters/random_sample.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `filters/include/pcl/filters/impl/random_sample.hpp` |
| `filters/include/pcl/filters/sampling_surface_normal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项30），建议次优先 | `filters/include/pcl/filters/impl/sampling_surface_normal.hpp` |
| `filters/include/pcl/filters/shadowpoints.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `filters/include/pcl/filters/impl/shadowpoints.hpp` |
| `filters/include/pcl/filters/statistical_outlier_removal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项25），建议次优先 | `filters/include/pcl/filters/impl/statistical_outlier_removal.hpp` |
| `filters/include/pcl/filters/uniform_sampling.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项24），建议次优先 | `filters/include/pcl/filters/impl/uniform_sampling.hpp` |
| `filters/include/pcl/filters/voxel_grid.h` | `high` | 是 | 循环6处，滤波/统计计算密集，建议优先优化 | `filters/include/pcl/filters/impl/voxel_grid.hpp` |
| `filters/include/pcl/filters/voxel_grid_covariance.h` | `high` | 是 | 循环2处，滤波/统计计算密集，建议优先优化 | `filters/include/pcl/filters/impl/voxel_grid_covariance.hpp` |
| `filters/include/pcl/filters/voxel_grid_label.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/include/pcl/filters/voxel_grid_occlusion_estimation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项67），建议次优先 | `filters/include/pcl/filters/impl/voxel_grid_occlusion_estimation.hpp` |
| `filters/src/approximate_voxel_grid.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/bilateral.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/conditional_removal.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/convolution.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项9），建议次优先 | `-` |
| `filters/src/covariance_sampling.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/crop_box.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项8），建议次优先 | `-` |
| `filters/src/crop_hull.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/extract_indices.cpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项2），建议次优先 | `-` |
| `filters/src/farthest_point_sampling.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/fast_bilateral.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/fast_bilateral_omp.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/filter_indices.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/frustum_culling.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/grid_minimum.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/local_maximum.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/median_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/model_outlier_removal.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/morphological_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/normal_refinement.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/normal_space.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/passthrough.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项16），建议次优先 | `-` |
| `filters/src/project_inliers.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项11），建议次优先 | `-` |
| `filters/src/pyramid.cpp` | `mid` | 是 | 存在可向量化路径（循环30，数学项0），建议次优先 | `-` |
| `filters/src/radius_outlier_removal.cpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项7），建议次优先 | `-` |
| `filters/src/random_sample.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项1），建议次优先 | `-` |
| `filters/src/sampling_surface_normal.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/shadowpoints.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/statistical_outlier_removal.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项17），建议次优先 | `-` |
| `filters/src/uniform_sampling.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/voxel_grid.cpp` | `mid` | 是 | 存在可向量化路径（循环13，数学项27），建议次优先 | `-` |
| `filters/src/voxel_grid_covariance.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `filters/src/voxel_grid_label.cpp` | `high` | 是 | 循环8处，滤波/统计计算密集，建议优先优化 | `-` |
| `filters/src/voxel_grid_occlusion_estimation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |

## 4. 简要统计

- 模块统计：high `6`，mid `70`，low `33`。
- 候选占比：`76/109 = 69.7%`。
- include 口径：候选 `65/75`。
- src 口径：候选 `11/34`。
- thirdparty 口径：`0` 文件已登记，候选 `0`。
