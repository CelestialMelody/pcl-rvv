# segmentation 模块 RVV 第一轮文件级筛选报告

本文档记录 `segmentation` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 覆盖范围：`segmentation/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `76`，已判定 `76`（`76/76` 全覆盖）。
- 目录拆分：`include` `56`，`src` `20`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `segmentation/include/pcl/segmentation/`，`src` 文件以 `src/` 开头显示。

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
| 源码文件总数 | 76 | `segmentation/**` 源码文件，第三方实现仅登记覆盖，不纳入候选主线。 |
| 已判定文件数 | 76 | `76/76` |
| high | 5 | 二轮必查 |
| mid | 35 | 二轮必查 |
| low | 36 | 已覆盖但不进入二轮初始基线 |
| high + mid | 40 | 第一轮候选基线 |
| 候选占比 | 40/76 = 52.6% | high + mid / 源码文件总数 |

## 4. high 候选（5）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/supervoxel_clustering.hpp` | 循环40处，分割/聚类计算密集，建议优先优化 |
| `src/grabcut_segmentation.cpp` | 循环28处，分割/聚类计算密集，建议优先优化 |
| `impl/region_growing_rgb.hpp` | 循环26处，分割/聚类计算密集，建议优先优化 |
| `impl/organized_multi_plane_segmentation.hpp` | 循环14处，分割/聚类计算密集，建议优先优化 |
| `impl/min_cut_segmentation.hpp` | 循环13处，分割/聚类计算密集，建议优先优化 |

## 5. mid 候选（35）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/crf_segmentation.hpp` | 存在可向量化路径（循环21，数学项52），建议次优先 |
| `impl/region_growing.hpp` | 存在可向量化路径（循环21，数学项34），建议次优先 |
| `impl/grabcut_segmentation.hpp` | 存在可向量化路径（循环20，数学项48），建议次优先 |
| `impl/lccp_segmentation.hpp` | 存在可向量化路径（循环19，数学项39），建议次优先 |
| `impl/unary_classifier.hpp` | 存在可向量化路径（循环17，数学项32），建议次优先 |
| `impl/approximate_progressive_morphological_filter.hpp` | 存在可向量化路径（循环14，数学项10），建议次优先 |
| `extract_clusters.h` | 存在可向量化路径（循环8，数学项56），建议次优先 |
| `impl/random_walker.hpp` | 存在可向量化路径（循环8，数学项53），建议次优先 |
| `impl/organized_connected_component_segmentation.hpp` | 存在可向量化路径（循环8，数学项38），建议次优先 |
| `impl/cpc_segmentation.hpp` | 存在可向量化路径（循环8，数学项28），建议次优先 |
| `impl/extract_clusters.hpp` | 存在可向量化路径（循环8，数学项7），建议次优先 |
| `impl/seeded_hue_segmentation.hpp` | 存在可向量化路径（循环8，数学项5），建议次优先 |
| `impl/extract_polygonal_prism_data.hpp` | 存在可向量化路径（循环7，数学项36），建议次优先 |
| `impl/extract_labeled_clusters.hpp` | 存在可向量化路径（循环5，数学项5），建议次优先 |
| `impl/conditional_euclidean_clustering.hpp` | 存在可向量化路径（循环4，数学项9），建议次优先 |
| `impl/progressive_morphological_filter.hpp` | 存在可向量化路径（循环3，数学项6），建议次优先 |
| `cpc_segmentation.h` | 存在可向量化路径（循环1，数学项39），建议次优先 |
| `min_cut_segmentation.h` | 存在可向量化路径（循环1，数学项24），建议次优先 |
| `organized_connected_component_segmentation.h` | 存在可向量化路径（循环1，数学项4），建议次优先 |
| `planar_polygon_fusion.h` | 存在可向量化路径（循环1，数学项3），建议次优先 |
| `lccp_segmentation.h` | 存在可向量化路径（循环0，数学项61），建议次优先 |
| `supervoxel_clustering.h` | 存在可向量化路径（循环0，数学项57），建议次优先 |
| `organized_multi_plane_segmentation.h` | 存在可向量化路径（循环0，数学项52），建议次优先 |
| `grabcut_segmentation.h` | 存在可向量化路径（循环0，数学项41），建议次优先 |
| `region_growing_rgb.h` | 存在可向量化路径（循环0，数学项41），建议次优先 |
| `sac_segmentation.h` | 存在可向量化路径（循环0，数学项40），建议次优先 |
| `region_growing.h` | 存在可向量化路径（循环0，数学项39），建议次优先 |
| `random_walker.h` | 存在可向量化路径（循环0，数学项36），建议次优先 |
| `conditional_euclidean_clustering.h` | 存在可向量化路径（循环0，数学项33），建议次优先 |
| `impl/sac_segmentation.hpp` | 存在可向量化路径（循环0，数学项26），建议次优先 |
| `extract_labeled_clusters.h` | 存在可向量化路径（循环0，数学项20），建议次优先 |
| `extract_polygonal_prism_data.h` | 存在可向量化路径（循环0，数学项15），建议次优先 |
| `segment_differences.h` | 存在可向量化路径（循环0，数学项15），建议次优先 |
| `seeded_hue_segmentation.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `crf_segmentation.h` | 存在可向量化路径（循环0，数学项11），建议次优先 |

## 6. 全量文件覆盖表（76/76）

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `approximate_progressive_morphological_filter.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/approximate_progressive_morphological_filter.hpp` |
| `comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `conditional_euclidean_clustering.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项33），建议次优先 | `impl/conditional_euclidean_clustering.hpp` |
| `cpc_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项39），建议次优先 | `impl/cpc_segmentation.hpp` |
| `crf_normal_segmentation.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/crf_normal_segmentation.hpp` |
| `crf_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项11），建议次优先 | `impl/crf_segmentation.hpp` |
| `edge_aware_plane_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `euclidean_cluster_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `euclidean_plane_coefficient_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `extract_clusters.h` | `mid` | 是 | 存在可向量化路径（循环8，数学项56），建议次优先 | `impl/extract_clusters.hpp` |
| `extract_labeled_clusters.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项20），建议次优先 | `impl/extract_labeled_clusters.hpp` |
| `extract_polygonal_prism_data.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项15），建议次优先 | `impl/extract_polygonal_prism_data.hpp` |
| `grabcut_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项41），建议次优先 | `impl/grabcut_segmentation.hpp` |
| `ground_plane_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/approximate_progressive_morphological_filter.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项10），建议次优先 | `-` |
| `impl/conditional_euclidean_clustering.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项9），建议次优先 | `-` |
| `impl/cpc_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项28），建议次优先 | `-` |
| `impl/crf_normal_segmentation.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/crf_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环21，数学项52），建议次优先 | `-` |
| `impl/extract_clusters.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项7），建议次优先 | `-` |
| `impl/extract_labeled_clusters.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项5），建议次优先 | `-` |
| `impl/extract_polygonal_prism_data.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项36），建议次优先 | `-` |
| `impl/grabcut_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环20，数学项48），建议次优先 | `-` |
| `impl/lccp_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环19，数学项39），建议次优先 | `-` |
| `impl/min_cut_segmentation.hpp` | `high` | 是 | 循环13处，分割/聚类计算密集，建议优先优化 | `-` |
| `impl/organized_connected_component_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项38），建议次优先 | `-` |
| `impl/organized_multi_plane_segmentation.hpp` | `high` | 是 | 循环14处，分割/聚类计算密集，建议优先优化 | `-` |
| `impl/planar_polygon_fusion.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/progressive_morphological_filter.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项6），建议次优先 | `-` |
| `impl/random_walker.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项53），建议次优先 | `-` |
| `impl/region_growing.hpp` | `mid` | 是 | 存在可向量化路径（循环21，数学项34），建议次优先 | `-` |
| `impl/region_growing_rgb.hpp` | `high` | 是 | 循环26处，分割/聚类计算密集，建议优先优化 | `-` |
| `impl/sac_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项26），建议次优先 | `-` |
| `impl/seeded_hue_segmentation.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项5），建议次优先 | `-` |
| `impl/segment_differences.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/supervoxel_clustering.hpp` | `high` | 是 | 循环40处，分割/聚类计算密集，建议优先优化 | `-` |
| `impl/unary_classifier.hpp` | `mid` | 是 | 存在可向量化路径（循环17，数学项32），建议次优先 | `-` |
| `lccp_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项61），建议次优先 | `impl/lccp_segmentation.hpp` |
| `min_cut_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项24），建议次优先 | `impl/min_cut_segmentation.hpp` |
| `organized_connected_component_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项4），建议次优先 | `impl/organized_connected_component_segmentation.hpp` |
| `organized_multi_plane_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项52），建议次优先 | `impl/organized_multi_plane_segmentation.hpp` |
| `planar_polygon_fusion.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项3），建议次优先 | `impl/planar_polygon_fusion.hpp` |
| `planar_region.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `plane_coefficient_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `plane_refinement_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `progressive_morphological_filter.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/progressive_morphological_filter.hpp` |
| `random_walker.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项36），建议次优先 | `impl/random_walker.hpp` |
| `region_3d.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `region_growing.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项39），建议次优先 | `impl/region_growing.hpp` |
| `region_growing_rgb.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项41），建议次优先 | `impl/region_growing_rgb.hpp` |
| `rgb_plane_coefficient_comparator.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sac_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项40），建议次优先 | `impl/sac_segmentation.hpp` |
| `seeded_hue_segmentation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `impl/seeded_hue_segmentation.hpp` |
| `segment_differences.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项15），建议次优先 | `impl/segment_differences.hpp` |
| `supervoxel_clustering.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项57），建议次优先 | `impl/supervoxel_clustering.hpp` |
| `unary_classifier.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/unary_classifier.hpp` |
| `src/approximate_progressive_morphological_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/conditional_euclidean_clustering.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/cpc_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/crf_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/extract_clusters.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/extract_polygonal_prism_data.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/grabcut_segmentation.cpp` | `high` | 是 | 循环28处，分割/聚类计算密集，建议优先优化 | `-` |
| `src/lccp_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/min_cut_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/organized_connected_component_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/organized_multi_plane_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/planar_polygon_fusion.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/progressive_morphological_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/region_growing.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/region_growing_rgb.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/seeded_hue_segmentation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/segment_differences.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/supervoxel_clustering.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/unary_classifier.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/segmentation/segmentation-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
