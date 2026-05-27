# recognition 模块文件级筛查清单（全覆盖版，重评）

本版按当前统一标准重评：全文件覆盖、实现优先、允许推翻旧结论；`3rdparty/**` 仅登记覆盖，不纳入候选。

## 1. 覆盖范围与口径

- 覆盖范围：`recognition/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `70`，已判定 `70`（`70/70` 全覆盖）。
- 目录拆分：`include` `53`，`src` `17`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（5）

| file_path | 说明 |
| --- | --- |
| `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` | 循环107处，识别/匹配计算密集，建议优先优化 |
| `recognition/src/linemod.cpp` | 循环86处，识别/匹配计算密集，建议优先优化 |
| `recognition/include/pcl/recognition/surface_normal_modality.h` | 循环59处，识别/匹配计算密集，建议优先优化 |
| `recognition/include/pcl/recognition/color_gradient_modality.h` | 循环38处，识别/匹配计算密集，建议优先优化 |
| `recognition/src/face_detection/rf_face_detector_trainer.cpp` | 循环19处，识别/匹配计算密集，建议优先优化 |

### 2.2 mid 候选（27）

| file_path | 说明 |
| --- | --- |
| `recognition/include/pcl/recognition/impl/linemod/line_rgbd.hpp` | 存在可向量化路径（循环37，数学项37），建议次优先 |
| `recognition/include/pcl/recognition/impl/hv/hv_go.hpp` | 存在可向量化路径（循环34，数学项35），建议次优先 |
| `recognition/src/face_detection/face_detector_data_provider.cpp` | 存在可向量化路径（循环26，数学项19），建议次优先 |
| `recognition/src/ransac_based/obj_rec_ransac.cpp` | 存在可向量化路径（循环23，数学项31），建议次优先 |
| `recognition/include/pcl/recognition/color_modality.h` | 存在可向量化路径（循环22，数学项37），建议次优先 |
| `recognition/src/ransac_based/orr_octree.cpp` | 存在可向量化路径（循环21，数学项16），建议次优先 |
| `recognition/include/pcl/recognition/face_detection/rf_face_utils.h` | 存在可向量化路径（循环18，数学项73），建议次优先 |
| `recognition/include/pcl/recognition/color_gradient_dot_modality.h` | 存在可向量化路径（循环16，数学项30），建议次优先 |
| `recognition/include/pcl/recognition/face_detection/face_common.h` | 存在可向量化路径（循环14，数学项12），建议次优先 |
| `recognition/include/pcl/recognition/impl/hv/hv_papazov.hpp` | 存在可向量化路径（循环12，数学项7），建议次优先 |
| `recognition/src/dotmod.cpp` | 存在可向量化路径（循环12，数学项4），建议次优先 |
| `recognition/include/pcl/recognition/impl/ransac_based/simple_octree.hpp` | 存在可向量化路径（循环11，数学项23），建议次优先 |
| `recognition/src/cg/hough_3d.cpp` | 存在可向量化路径（循环10，数学项26），建议次优先 |
| `recognition/src/ransac_based/orr_octree_zprojection.cpp` | 存在可向量化路径（循环10，数学项3），建议次优先 |
| `recognition/include/pcl/recognition/hv/hv_go.h` | 存在可向量化路径（循环9，数学项12），建议次优先 |
| `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` | 存在可向量化路径（循环8，数学项8），建议次优先 |
| `recognition/include/pcl/recognition/impl/cg/hough_3d.hpp` | 存在可向量化路径（循环7，数学项62），建议次优先 |
| `recognition/include/pcl/recognition/crh_alignment.h` | 存在可向量化路径（循环7，数学项40），建议次优先 |
| `recognition/include/pcl/recognition/impl/hv/greedy_verification.hpp` | 存在可向量化路径（循环7，数学项5），建议次优先 |
| `recognition/include/pcl/recognition/face_detection/rf_face_detector_trainer.h` | 存在可向量化路径（循环6，数学项22），建议次优先 |
| `recognition/src/quantizable_modality.cpp` | 存在可向量化路径（循环6，数学项1），建议次优先 |
| `recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp` | 存在可向量化路径（循环5，数学项25），建议次优先 |
| `recognition/include/pcl/recognition/linemod.h` | 存在可向量化路径（循环4，数学项24），建议次优先 |
| `recognition/src/ransac_based/model_library.cpp` | 存在可向量化路径（循环4，数学项5），建议次优先 |
| `recognition/include/pcl/recognition/hv/greedy_verification.h` | 存在可向量化路径（循环3，数学项13），建议次优先 |
| `recognition/include/pcl/recognition/ransac_based/trimmed_icp.h` | 存在可向量化路径（循环3，数学项11），建议次优先 |
| `recognition/include/pcl/recognition/implicit_shape_model.h` | 存在可向量化路径（循环0，数学项73），建议次优先 |

## 3. 全量文件覆盖表（70/70）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `recognition/include/pcl/recognition/cg/correspondence_grouping.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/cg/geometric_consistency.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/cg/hough_3d.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/color_gradient_dot_modality.h` | `mid` | 是 | 存在可向量化路径（循环16，数学项30），建议次优先 | `-` |
| `recognition/include/pcl/recognition/color_gradient_modality.h` | `high` | 是 | 循环38处，识别/匹配计算密集，建议优先优化 | `-` |
| `recognition/include/pcl/recognition/color_modality.h` | `mid` | 是 | 存在可向量化路径（循环22，数学项37），建议次优先 | `-` |
| `recognition/include/pcl/recognition/crh_alignment.h` | `mid` | 是 | 存在可向量化路径（循环7，数学项40），建议次优先 | `-` |
| `recognition/include/pcl/recognition/dense_quantized_multi_mod_template.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/distance_map.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/dot_modality.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/dotmod.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/face_detection/face_common.h` | `mid` | 是 | 存在可向量化路径（循环14，数学项12），建议次优先 | `-` |
| `recognition/include/pcl/recognition/face_detection/face_detector_data_provider.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/face_detection/rf_face_detector_trainer.h` | `mid` | 是 | 存在可向量化路径（循环6，数学项22），建议次优先 | `-` |
| `recognition/include/pcl/recognition/face_detection/rf_face_utils.h` | `mid` | 是 | 存在可向量化路径（循环18，数学项73），建议次优先 | `-` |
| `recognition/include/pcl/recognition/hv/greedy_verification.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项13），建议次优先 | `-` |
| `recognition/include/pcl/recognition/hv/hv_go.h` | `mid` | 是 | 存在可向量化路径（循环9，数学项12），建议次优先 | `-` |
| `recognition/include/pcl/recognition/hv/hv_papazov.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/hv/hypotheses_verification.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/hv/occlusion_reasoning.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/impl/cg/correspondence_grouping.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项25），建议次优先 | `-` |
| `recognition/include/pcl/recognition/impl/cg/hough_3d.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项62），建议次优先 | `-` |
| `recognition/include/pcl/recognition/impl/hv/greedy_verification.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项5），建议次优先 | `-` |
| `recognition/include/pcl/recognition/impl/hv/hv_go.hpp` | `mid` | 是 | 存在可向量化路径（循环34，数学项35），建议次优先 | `-` |
| `recognition/include/pcl/recognition/impl/hv/hv_papazov.hpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项7），建议次优先 | `-` |
| `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项8），建议次优先 | `-` |
| `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` | `high` | 是 | 循环107处，识别/匹配计算密集，建议优先优化 | `-` |
| `recognition/include/pcl/recognition/impl/linemod/line_rgbd.hpp` | `mid` | 是 | 存在可向量化路径（循环37，数学项37），建议次优先 | `-` |
| `recognition/include/pcl/recognition/impl/ransac_based/simple_octree.hpp` | `mid` | 是 | 存在可向量化路径（循环11，数学项23），建议次优先 | `-` |
| `recognition/include/pcl/recognition/impl/ransac_based/voxel_structure.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/implicit_shape_model.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项73），建议次优先 | `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` |
| `recognition/include/pcl/recognition/linemod.h` | `mid` | 是 | 存在可向量化路径（循环4，数学项24），建议次优先 | `-` |
| `recognition/include/pcl/recognition/linemod/line_rgbd.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/mask_map.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/point_types.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/quantizable_modality.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/quantized_map.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/ransac_based/auxiliary.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/ransac_based/bvh.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/ransac_based/hypothesis.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/ransac_based/model_library.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/ransac_based/obj_rec_ransac.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/ransac_based/orr_graph.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/ransac_based/orr_octree.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/ransac_based/orr_octree_zprojection.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/ransac_based/rigid_transform_space.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/ransac_based/simple_octree.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/ransac_based/trimmed_icp.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项11），建议次优先 | `-` |
| `recognition/include/pcl/recognition/ransac_based/voxel_structure.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/region_xy.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/sparse_quantized_multi_mod_template.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/include/pcl/recognition/surface_normal_modality.h` | `high` | 是 | 循环59处，识别/匹配计算密集，建议优先优化 | `-` |
| `recognition/src/cg/geometric_consistency.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/src/cg/hough_3d.cpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项26），建议次优先 | `-` |
| `recognition/src/dotmod.cpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项4），建议次优先 | `-` |
| `recognition/src/face_detection/face_detector_data_provider.cpp` | `mid` | 是 | 存在可向量化路径（循环26，数学项19），建议次优先 | `-` |
| `recognition/src/face_detection/rf_face_detector_trainer.cpp` | `high` | 是 | 循环19处，识别/匹配计算密集，建议优先优化 | `-` |
| `recognition/src/hv/greedy_verification.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/src/hv/hv_go.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/src/hv/hv_papazov.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/src/hv/occlusion_reasoning.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/src/implicit_shape_model.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/src/linemod.cpp` | `high` | 是 | 循环86处，识别/匹配计算密集，建议优先优化 | `-` |
| `recognition/src/mask_map.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `recognition/src/quantizable_modality.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项1），建议次优先 | `-` |
| `recognition/src/ransac_based/model_library.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项5），建议次优先 | `-` |
| `recognition/src/ransac_based/obj_rec_ransac.cpp` | `mid` | 是 | 存在可向量化路径（循环23，数学项31），建议次优先 | `-` |
| `recognition/src/ransac_based/orr_octree.cpp` | `mid` | 是 | 存在可向量化路径（循环21，数学项16），建议次优先 | `-` |
| `recognition/src/ransac_based/orr_octree_zprojection.cpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项3），建议次优先 | `-` |

## 4. 简要统计

- 模块统计：high `5`，mid `27`，low `38`。
- 候选占比：`32/70 = 45.7%`。
- include 口径：候选 `22/53`。
- src 口径：候选 `10/17`。
- thirdparty 口径：`0` 文件已登记，候选 `0`。
