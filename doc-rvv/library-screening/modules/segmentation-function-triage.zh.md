# segmentation 模块文件级筛查清单（全覆盖版，重评）

本版按当前统一标准重评：全文件覆盖、实现优先、允许推翻旧结论；`3rdparty/**` 仅登记覆盖，不纳入候选。

## 1. 覆盖范围与口径

- 覆盖范围：`segmentation/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `76`，已判定 `76`（`76/76` 全覆盖）。
- 目录拆分：`include` `56`，`src` `20`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（5）

| file_path | 说明 |
| --- | --- |
| `segmentation/include/pcl/segmentation/impl/supervoxel_clustering.hpp` | 循环40处，分割/聚类计算密集，建议优先优化 |
| `segmentation/src/grabcut_segmentation.cpp` | 循环28处，分割/聚类计算密集，建议优先优化 |
| `segmentation/include/pcl/segmentation/impl/region_growing_rgb.hpp` | 循环26处，分割/聚类计算密集，建议优先优化 |
| `segmentation/include/pcl/segmentation/impl/organized_multi_plane_segmentation.hpp` | 循环14处，分割/聚类计算密集，建议优先优化 |
| `segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp` | 循环13处，分割/聚类计算密集，建议优先优化 |

### 2.2 mid 候选（35）

| file_path | 说明 |
| --- | --- |
| `segmentation/include/pcl/segmentation/impl/crf_segmentation.hpp` | 存在可向量化路径（循环21，数学项52），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/region_growing.hpp` | 存在可向量化路径（循环21，数学项34），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` | 存在可向量化路径（循环20，数学项48），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/lccp_segmentation.hpp` | 存在可向量化路径（循环19，数学项39），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/unary_classifier.hpp` | 存在可向量化路径（循环17，数学项32），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` | 存在可向量化路径（循环14，数学项10），建议次优先 |
| `segmentation/include/pcl/segmentation/extract_clusters.h` | 存在可向量化路径（循环8，数学项56），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/random_walker.hpp` | 存在可向量化路径（循环8，数学项53），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/organized_connected_component_segmentation.hpp` | 存在可向量化路径（循环8，数学项38），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/cpc_segmentation.hpp` | 存在可向量化路径（循环8，数学项28），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/extract_clusters.hpp` | 存在可向量化路径（循环8，数学项7），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/seeded_hue_segmentation.hpp` | 存在可向量化路径（循环8，数学项5），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` | 存在可向量化路径（循环7，数学项36），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/extract_labeled_clusters.hpp` | 存在可向量化路径（循环5，数学项5），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/conditional_euclidean_clustering.hpp` | 存在可向量化路径（循环4，数学项9），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/progressive_morphological_filter.hpp` | 存在可向量化路径（循环3，数学项6），建议次优先 |
| `segmentation/include/pcl/segmentation/cpc_segmentation.h` | 存在可向量化路径（循环1，数学项39），建议次优先 |
| `segmentation/include/pcl/segmentation/min_cut_segmentation.h` | 存在可向量化路径（循环1，数学项24），建议次优先 |
| `segmentation/include/pcl/segmentation/organized_connected_component_segmentation.h` | 存在可向量化路径（循环1，数学项4），建议次优先 |
| `segmentation/include/pcl/segmentation/planar_polygon_fusion.h` | 存在可向量化路径（循环1，数学项3），建议次优先 |
| `segmentation/include/pcl/segmentation/lccp_segmentation.h` | 存在可向量化路径（循环0，数学项61），建议次优先 |
| `segmentation/include/pcl/segmentation/supervoxel_clustering.h` | 存在可向量化路径（循环0，数学项57），建议次优先 |
| `segmentation/include/pcl/segmentation/organized_multi_plane_segmentation.h` | 存在可向量化路径（循环0，数学项52），建议次优先 |
| `segmentation/include/pcl/segmentation/grabcut_segmentation.h` | 存在可向量化路径（循环0，数学项41），建议次优先 |
| `segmentation/include/pcl/segmentation/region_growing_rgb.h` | 存在可向量化路径（循环0，数学项41），建议次优先 |
| `segmentation/include/pcl/segmentation/sac_segmentation.h` | 存在可向量化路径（循环0，数学项40），建议次优先 |
| `segmentation/include/pcl/segmentation/region_growing.h` | 存在可向量化路径（循环0，数学项39），建议次优先 |
| `segmentation/include/pcl/segmentation/random_walker.h` | 存在可向量化路径（循环0，数学项36），建议次优先 |
| `segmentation/include/pcl/segmentation/conditional_euclidean_clustering.h` | 存在可向量化路径（循环0，数学项33），建议次优先 |
| `segmentation/include/pcl/segmentation/impl/sac_segmentation.hpp` | 存在可向量化路径（循环0，数学项26），建议次优先 |
| `segmentation/include/pcl/segmentation/extract_labeled_clusters.h` | 存在可向量化路径（循环0，数学项20），建议次优先 |
| `segmentation/include/pcl/segmentation/extract_polygonal_prism_data.h` | 存在可向量化路径（循环0，数学项15），建议次优先 |
| `segmentation/include/pcl/segmentation/segment_differences.h` | 存在可向量化路径（循环0，数学项15），建议次优先 |
| `segmentation/include/pcl/segmentation/seeded_hue_segmentation.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `segmentation/include/pcl/segmentation/crf_segmentation.h` | 存在可向量化路径（循环0，数学项11），建议次优先 |

## 3. 全量文件覆盖表（76/76）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `segmentation/include/pcl/segmentation/approximate_progressive_morphological_filter.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` |
| `segmentation/include/pcl/segmentation/comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/conditional_euclidean_clustering.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项33），建议次优先 | `segmentation/include/pcl/segmentation/impl/conditional_euclidean_clustering.hpp` |
| `segmentation/include/pcl/segmentation/cpc_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项39），建议次优先 | `segmentation/include/pcl/segmentation/impl/cpc_segmentation.hpp` |
| `segmentation/include/pcl/segmentation/crf_normal_segmentation.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `segmentation/include/pcl/segmentation/impl/crf_normal_segmentation.hpp` |
| `segmentation/include/pcl/segmentation/crf_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项11），建议次优先 | `segmentation/include/pcl/segmentation/impl/crf_segmentation.hpp` |
| `segmentation/include/pcl/segmentation/edge_aware_plane_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/euclidean_cluster_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/euclidean_plane_coefficient_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/extract_clusters.h` | `mid` | 是 | 存在可向量化路径（循环8，数学项56），建议次优先 | `segmentation/include/pcl/segmentation/impl/extract_clusters.hpp` |
| `segmentation/include/pcl/segmentation/extract_labeled_clusters.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项20），建议次优先 | `segmentation/include/pcl/segmentation/impl/extract_labeled_clusters.hpp` |
| `segmentation/include/pcl/segmentation/extract_polygonal_prism_data.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项15），建议次优先 | `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` |
| `segmentation/include/pcl/segmentation/grabcut_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项41），建议次优先 | `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` |
| `segmentation/include/pcl/segmentation/ground_plane_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项10），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/conditional_euclidean_clustering.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项9），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/cpc_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项28），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/crf_normal_segmentation.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/impl/crf_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环21，数学项52），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/extract_clusters.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项7），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/extract_labeled_clusters.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项5），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项36），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环20，数学项48），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/lccp_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环19，数学项39），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp` | `high` | 是 | 循环13处，分割/聚类计算密集，建议优先优化 | `-` |
| `segmentation/include/pcl/segmentation/impl/organized_connected_component_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项38），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/organized_multi_plane_segmentation.hpp` | `high` | 是 | 循环14处，分割/聚类计算密集，建议优先优化 | `-` |
| `segmentation/include/pcl/segmentation/impl/planar_polygon_fusion.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/impl/progressive_morphological_filter.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项6），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/random_walker.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项53），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/region_growing.hpp` | `mid` | 是 | 存在可向量化路径（循环21，数学项34），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/region_growing_rgb.hpp` | `high` | 是 | 循环26处，分割/聚类计算密集，建议优先优化 | `-` |
| `segmentation/include/pcl/segmentation/impl/sac_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项26），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/seeded_hue_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项5），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/impl/segment_differences.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/impl/supervoxel_clustering.hpp` | `high` | 是 | 循环40处，分割/聚类计算密集，建议优先优化 | `-` |
| `segmentation/include/pcl/segmentation/impl/unary_classifier.hpp` | `mid` | 是 | 存在可向量化路径（循环17，数学项32），建议次优先 | `-` |
| `segmentation/include/pcl/segmentation/lccp_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项61），建议次优先 | `segmentation/include/pcl/segmentation/impl/lccp_segmentation.hpp` |
| `segmentation/include/pcl/segmentation/min_cut_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项24），建议次优先 | `segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp` |
| `segmentation/include/pcl/segmentation/organized_connected_component_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项4），建议次优先 | `segmentation/include/pcl/segmentation/impl/organized_connected_component_segmentation.hpp` |
| `segmentation/include/pcl/segmentation/organized_multi_plane_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项52），建议次优先 | `segmentation/include/pcl/segmentation/impl/organized_multi_plane_segmentation.hpp` |
| `segmentation/include/pcl/segmentation/planar_polygon_fusion.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项3），建议次优先 | `segmentation/include/pcl/segmentation/impl/planar_polygon_fusion.hpp` |
| `segmentation/include/pcl/segmentation/planar_region.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/plane_coefficient_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/plane_refinement_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/progressive_morphological_filter.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `segmentation/include/pcl/segmentation/impl/progressive_morphological_filter.hpp` |
| `segmentation/include/pcl/segmentation/random_walker.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项36），建议次优先 | `segmentation/include/pcl/segmentation/impl/random_walker.hpp` |
| `segmentation/include/pcl/segmentation/region_3d.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/region_growing.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项39），建议次优先 | `segmentation/include/pcl/segmentation/impl/region_growing.hpp` |
| `segmentation/include/pcl/segmentation/region_growing_rgb.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项41），建议次优先 | `segmentation/include/pcl/segmentation/impl/region_growing_rgb.hpp` |
| `segmentation/include/pcl/segmentation/rgb_plane_coefficient_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/include/pcl/segmentation/sac_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项40），建议次优先 | `segmentation/include/pcl/segmentation/impl/sac_segmentation.hpp` |
| `segmentation/include/pcl/segmentation/seeded_hue_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `segmentation/include/pcl/segmentation/impl/seeded_hue_segmentation.hpp` |
| `segmentation/include/pcl/segmentation/segment_differences.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项15），建议次优先 | `segmentation/include/pcl/segmentation/impl/segment_differences.hpp` |
| `segmentation/include/pcl/segmentation/supervoxel_clustering.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项57），建议次优先 | `segmentation/include/pcl/segmentation/impl/supervoxel_clustering.hpp` |
| `segmentation/include/pcl/segmentation/unary_classifier.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `segmentation/include/pcl/segmentation/impl/unary_classifier.hpp` |
| `segmentation/src/approximate_progressive_morphological_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/conditional_euclidean_clustering.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/cpc_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/crf_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/extract_clusters.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/extract_polygonal_prism_data.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/grabcut_segmentation.cpp` | `high` | 是 | 循环28处，分割/聚类计算密集，建议优先优化 | `-` |
| `segmentation/src/lccp_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/min_cut_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/organized_connected_component_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/organized_multi_plane_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/planar_polygon_fusion.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/progressive_morphological_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/region_growing.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/region_growing_rgb.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/sac_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/seeded_hue_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/segment_differences.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/supervoxel_clustering.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `segmentation/src/unary_classifier.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |

## 4. 简要统计

- 模块统计：high `5`，mid `35`，low `36`。
- 候选占比：`40/76 = 52.6%`。
- include 口径：候选 `39/56`。
- src 口径：候选 `1/20`。
- thirdparty 口径：`0` 文件已登记，候选 `0`。
