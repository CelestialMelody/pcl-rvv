# registration 模块文件级筛查清单（105 全覆盖版，重评）

本版基于逐文件重评结果，允许推翻旧结论：凡 `impl/*.hpp` 中存在明确循环与计算密集路径的条目，不再沿用旧版 `low` 标记。

## 1. 覆盖范围与口径

- 主覆盖范围：`registration/include/pcl/registration/**`（含 `impl/**`）。
- 覆盖结果：`include` 文件总数 `105`，本表覆盖 `105`（`105/105`）。
- 次关注范围：`registration/src/**`，文件数 `40`，已登记但不纳入主候选优先池。
- 候选定义：仅 `high/mid` 计入候选，`low` 为已覆盖但暂不进入本轮实现队列。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（10）

| file_path | 说明 |
| --- | --- |
| `registration/include/pcl/registration/impl/gicp.hpp` | 循环24处，数学/几何计算密集，建议优先优化 |
| `registration/include/pcl/registration/impl/ia_fpcs.hpp` | 循环24处，数学/几何计算密集，建议优先优化 |
| `registration/include/pcl/registration/impl/ndt_2d.hpp` | 循环13处，数学/几何计算密集，建议优先优化 |
| `registration/include/pcl/registration/impl/joint_icp.hpp` | 循环13处，数学/几何计算密集，建议优先优化 |
| `registration/include/pcl/registration/impl/ndt.hpp` | 循环10处，数学/几何计算密集，建议优先优化 |
| `registration/include/pcl/registration/impl/ia_kfpcs.hpp` | 循环10处，数学/几何计算密集，建议优先优化 |
| `registration/include/pcl/registration/impl/icp.hpp` | 循环7处，数学/几何计算密集，建议优先优化 |
| `registration/include/pcl/registration/impl/correspondence_estimation.hpp` | 循环6处，数学/几何计算密集，建议优先优化 |
| `registration/include/pcl/registration/impl/correspondence_estimation_normal_shooting.hpp` | 循环4处，数学/几何计算密集，建议优先优化 |
| `registration/include/pcl/registration/impl/correspondence_estimation_backprojection.hpp` | 循环4处，数学/几何计算密集，建议优先优化 |

### 2.2 mid 候选（60）

| file_path | 说明 |
| --- | --- |
| `registration/include/pcl/registration/impl/elch.hpp` | 存在可向量化路径（循环13，数学项17），建议次优先 |
| `registration/include/pcl/registration/impl/pyramid_feature_matching.hpp` | 存在可向量化路径（循环13，数学项4），建议次优先 |
| `registration/include/pcl/registration/impl/ppf_registration.hpp` | 存在可向量化路径（循环12，数学项39），建议次优先 |
| `registration/include/pcl/registration/impl/lum.hpp` | 存在可向量化路径（循环9，数学项42），建议次优先 |
| `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp` | 存在可向量化路径（循环8，数学项3），建议次优先 |
| `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_weighted.hpp` | 存在可向量化路径（循环7，数学项13），建议次优先 |
| `registration/include/pcl/registration/impl/transformation_estimation_lm.hpp` | 存在可向量化路径（循环6，数学项17），建议次优先 |
| `registration/include/pcl/registration/impl/sample_consensus_prerejective.hpp` | 存在可向量化路径（循环6，数学项6），建议次优先 |
| `registration/include/pcl/registration/impl/ia_ransac.hpp` | 存在可向量化路径（循环5，数学项4），建议次优先 |
| `registration/include/pcl/registration/impl/correspondence_rejection_sample_consensus.hpp` | 存在可向量化路径（循环4，数学项1），建议次优先 |
| `registration/include/pcl/registration/bfgs.h` | 存在可向量化路径（循环3，数学项29），建议次优先 |
| `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` | 存在可向量化路径（循环3，数学项15），建议次优先 |
| `registration/include/pcl/registration/impl/registration.hpp` | 存在可向量化路径（循环3，数学项6），建议次优先 |
| `registration/include/pcl/registration/impl/correspondence_rejection_sample_consensus_2d.hpp` | 存在可向量化路径（循环3，数学项2），建议次优先 |
| `registration/include/pcl/registration/impl/correspondence_types.hpp` | 存在可向量化路径（循环3，数学项1），建议次优先 |
| `registration/include/pcl/registration/correspondence_rejection_poly.h` | 存在可向量化路径（循环3，数学项0），建议次优先 |
| `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` | 存在可向量化路径（循环2，数学项32），建议次优先 |
| `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | 存在可向量化路径（循环2，数学项24），建议次优先 |
| `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp` | 存在可向量化路径（循环2，数学项4），建议次优先 |
| `registration/include/pcl/registration/icp.h` | 存在可向量化路径（循环2，数学项3），建议次优先 |
| `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp` | 存在可向量化路径（循环2，数学项3），建议次优先 |
| `registration/include/pcl/registration/impl/transformation_validation_euclidean.hpp` | 存在可向量化路径（循环2，数学项2），建议次优先 |
| `registration/include/pcl/registration/gicp.h` | 存在可向量化路径（循环1，数学项29），建议次优先 |
| `registration/include/pcl/registration/impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` | 存在可向量化路径（循环1，数学项15），建议次优先 |
| `registration/include/pcl/registration/lum.h` | 存在可向量化路径（循环1，数学项14），建议次优先 |
| `registration/include/pcl/registration/registration.h` | 存在可向量化路径（循环1，数学项7），建议次优先 |
| `registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` | 存在可向量化路径（循环1，数学项4），建议次优先 |
| `registration/include/pcl/registration/default_convergence_criteria.h` | 存在可向量化路径（循环1，数学项3），建议次优先 |
| `registration/include/pcl/registration/impl/pairwise_graph_registration.hpp` | 存在可向量化路径（循环1，数学项3），建议次优先 |
| `registration/include/pcl/registration/incremental_registration.h` | 存在可向量化路径（循环1，数学项0），建议次优先 |
| `registration/include/pcl/registration/meta_registration.h` | 存在可向量化路径（循环1，数学项0），建议次优先 |
| `registration/include/pcl/registration/ndt.h` | 存在可向量化路径（循环0，数学项45），建议次优先 |
| `registration/include/pcl/registration/impl/transformation_estimation_3point.hpp` | 存在可向量化路径（循环0，数学项17），建议次优先 |
| `registration/include/pcl/registration/transformation_estimation_svd.h` | 存在可向量化路径（循环0，数学项17），建议次优先 |
| `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` | 存在可向量化路径（循环0，数学项16），建议次优先 |
| `registration/include/pcl/registration/transformation_estimation_lm.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `registration/include/pcl/registration/transformation_estimation_2D.h` | 存在可向量化路径（循环0，数学项12），建议次优先 |
| `registration/include/pcl/registration/transformation_estimation_point_to_plane_weighted.h` | 存在可向量化路径（循环0，数学项12），建议次优先 |
| `registration/include/pcl/registration/ndt_2d.h` | 存在可向量化路径（循环0，数学项10），建议次优先 |
| `registration/include/pcl/registration/ppf_registration.h` | 存在可向量化路径（循环0，数学项9），建议次优先 |
| `registration/include/pcl/registration/correspondence_rejection_features.h` | 存在可向量化路径（循环0，数学项8），建议次优先 |
| `registration/include/pcl/registration/correspondence_estimation_organized_projection.h` | 存在可向量化路径（循环0，数学项7），建议次优先 |
| `registration/include/pcl/registration/transformation_estimation_symmetric_point_to_plane_lls.h` | 存在可向量化路径（循环0，数学项5），建议次优先 |
| `registration/include/pcl/registration/transformation_validation_euclidean.h` | 存在可向量化路径（循环0，数学项5），建议次优先 |
| `registration/include/pcl/registration/correspondence_estimation.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |
| `registration/include/pcl/registration/elch.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |
| `registration/include/pcl/registration/ia_fpcs.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |
| `registration/include/pcl/registration/transformation_estimation_point_to_plane_lls.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |
| `registration/include/pcl/registration/transformation_estimation_point_to_plane_lls_weighted.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |
| `registration/include/pcl/registration/correspondence_rejection_sample_consensus.h` | 存在可向量化路径（循环0，数学项3），建议次优先 |
| `registration/include/pcl/registration/correspondence_estimation_backprojection.h` | 存在可向量化路径（循环0，数学项2），建议次优先 |
| `registration/include/pcl/registration/correspondence_estimation_normal_shooting.h` | 存在可向量化路径（循环0，数学项2），建议次优先 |
| `registration/include/pcl/registration/correspondence_rejection_sample_consensus_2d.h` | 存在可向量化路径（循环0，数学项2），建议次优先 |
| `registration/include/pcl/registration/ia_kfpcs.h` | 存在可向量化路径（循环0，数学项2），建议次优先 |
| `registration/include/pcl/registration/ia_ransac.h` | 存在可向量化路径（循环0，数学项2），建议次优先 |
| `registration/include/pcl/registration/sample_consensus_prerejective.h` | 存在可向量化路径（循环0，数学项1），建议次优先 |
| `registration/include/pcl/registration/joint_icp.h` | 存在可向量化路径（循环0，数学项0），建议次优先 |
| `registration/include/pcl/registration/pairwise_graph_registration.h` | 存在可向量化路径（循环0，数学项0），建议次优先 |
| `registration/include/pcl/registration/transformation_estimation_3point.h` | 存在可向量化路径（循环0，数学项0），建议次优先 |
| `registration/include/pcl/registration/transformation_estimation_dual_quaternion.h` | 存在可向量化路径（循环0，数学项0），建议次优先 |

## 3. 全量文件覆盖表（105/105）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `registration/include/pcl/registration/bfgs.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项29），建议次优先 | `-` |
| `registration/include/pcl/registration/boost_graph.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/convergence_criteria.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/correspondence_estimation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `registration/include/pcl/registration/impl/correspondence_estimation.hpp` |
| `registration/include/pcl/registration/correspondence_estimation_backprojection.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项2），建议次优先 | `registration/include/pcl/registration/impl/correspondence_estimation_backprojection.hpp` |
| `registration/include/pcl/registration/correspondence_estimation_normal_shooting.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项2），建议次优先 | `registration/include/pcl/registration/impl/correspondence_estimation_normal_shooting.hpp` |
| `registration/include/pcl/registration/correspondence_estimation_organized_projection.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项7），建议次优先 | `registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` |
| `registration/include/pcl/registration/correspondence_rejection.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/correspondence_rejection_distance.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/correspondence_rejection_features.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项8），建议次优先 | `registration/include/pcl/registration/impl/correspondence_rejection_features.hpp` |
| `registration/include/pcl/registration/correspondence_rejection_median_distance.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/correspondence_rejection_one_to_one.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/correspondence_rejection_organized_boundary.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/correspondence_rejection_poly.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项0），建议次优先 | `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp` |
| `registration/include/pcl/registration/correspondence_rejection_sample_consensus.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项3），建议次优先 | `registration/include/pcl/registration/impl/correspondence_rejection_sample_consensus.hpp` |
| `registration/include/pcl/registration/correspondence_rejection_sample_consensus_2d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项2），建议次优先 | `registration/include/pcl/registration/impl/correspondence_rejection_sample_consensus_2d.hpp` |
| `registration/include/pcl/registration/correspondence_rejection_surface_normal.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/correspondence_rejection_trimmed.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/correspondence_rejection_var_trimmed.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/correspondence_sorting.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/correspondence_types.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `registration/include/pcl/registration/impl/correspondence_types.hpp` |
| `registration/include/pcl/registration/default_convergence_criteria.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项3），建议次优先 | `registration/include/pcl/registration/impl/default_convergence_criteria.hpp` |
| `registration/include/pcl/registration/distances.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/edge_measurements.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/elch.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `registration/include/pcl/registration/impl/elch.hpp` |
| `registration/include/pcl/registration/exceptions.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/gicp.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项29），建议次优先 | `registration/include/pcl/registration/impl/gicp.hpp` |
| `registration/include/pcl/registration/gicp6d.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/graph_handler.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/graph_optimizer.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/graph_registration.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/ia_fpcs.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `registration/include/pcl/registration/impl/ia_fpcs.hpp` |
| `registration/include/pcl/registration/ia_kfpcs.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项2），建议次优先 | `registration/include/pcl/registration/impl/ia_kfpcs.hpp` |
| `registration/include/pcl/registration/ia_ransac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项2），建议次优先 | `registration/include/pcl/registration/impl/ia_ransac.hpp` |
| `registration/include/pcl/registration/icp.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项3），建议次优先 | `registration/include/pcl/registration/impl/icp.hpp` |
| `registration/include/pcl/registration/icp_nl.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `registration/include/pcl/registration/impl/icp_nl.hpp` |
| `registration/include/pcl/registration/impl/correspondence_estimation.hpp` | `high` | 是 | 循环6处，数学/几何计算密集，建议优先优化 | `-` |
| `registration/include/pcl/registration/impl/correspondence_estimation_backprojection.hpp` | `high` | 是 | 循环4处，数学/几何计算密集，建议优先优化 | `-` |
| `registration/include/pcl/registration/impl/correspondence_estimation_normal_shooting.hpp` | `high` | 是 | 循环4处，数学/几何计算密集，建议优先优化 | `-` |
| `registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项4），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/correspondence_rejection_features.hpp` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项3），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/correspondence_rejection_sample_consensus.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项1），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/correspondence_rejection_sample_consensus_2d.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项2），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/correspondence_types.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项1），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/default_convergence_criteria.hpp` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/impl/elch.hpp` | `mid` | 是 | 存在可向量化路径（循环13，数学项17），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/gicp.hpp` | `high` | 是 | 循环24处，数学/几何计算密集，建议优先优化 | `-` |
| `registration/include/pcl/registration/impl/ia_fpcs.hpp` | `high` | 是 | 循环24处，数学/几何计算密集，建议优先优化 | `-` |
| `registration/include/pcl/registration/impl/ia_kfpcs.hpp` | `high` | 是 | 循环10处，数学/几何计算密集，建议优先优化 | `-` |
| `registration/include/pcl/registration/impl/ia_ransac.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项4），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/icp.hpp` | `high` | 是 | 循环7处，数学/几何计算密集，建议优先优化 | `-` |
| `registration/include/pcl/registration/impl/icp_nl.hpp` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/impl/incremental_registration.hpp` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/impl/joint_icp.hpp` | `high` | 是 | 循环13处，数学/几何计算密集，建议优先优化 | `-` |
| `registration/include/pcl/registration/impl/lum.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项42），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/meta_registration.hpp` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/impl/ndt.hpp` | `high` | 是 | 循环10处，数学/几何计算密集，建议优先优化 | `-` |
| `registration/include/pcl/registration/impl/ndt_2d.hpp` | `high` | 是 | 循环13处，数学/几何计算密集，建议优先优化 | `-` |
| `registration/include/pcl/registration/impl/pairwise_graph_registration.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项3），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/ppf_registration.hpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项39），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/pyramid_feature_matching.hpp` | `mid` | 是 | 存在可向量化路径（循环13，数学项4），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/registration.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项6），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/sample_consensus_prerejective.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项6），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项16），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/transformation_estimation_3point.hpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项17），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项15），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/transformation_estimation_lm.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项17），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项4），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项3），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_weighted.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项13），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项32），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项24），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项15），建议次优先 | `-` |
| `registration/include/pcl/registration/impl/transformation_validation_euclidean.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项2），建议次优先 | `-` |
| `registration/include/pcl/registration/incremental_registration.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项0），建议次优先 | `registration/include/pcl/registration/impl/incremental_registration.hpp` |
| `registration/include/pcl/registration/joint_icp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项0），建议次优先 | `registration/include/pcl/registration/impl/joint_icp.hpp` |
| `registration/include/pcl/registration/lum.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项14），建议次优先 | `registration/include/pcl/registration/impl/lum.hpp` |
| `registration/include/pcl/registration/matching_candidate.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/meta_registration.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项0），建议次优先 | `registration/include/pcl/registration/impl/meta_registration.hpp` |
| `registration/include/pcl/registration/ndt.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项45），建议次优先 | `registration/include/pcl/registration/impl/ndt.hpp` |
| `registration/include/pcl/registration/ndt_2d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项10），建议次优先 | `registration/include/pcl/registration/impl/ndt_2d.hpp` |
| `registration/include/pcl/registration/pairwise_graph_registration.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项0），建议次优先 | `registration/include/pcl/registration/impl/pairwise_graph_registration.hpp` |
| `registration/include/pcl/registration/ppf_registration.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项9），建议次优先 | `registration/include/pcl/registration/impl/ppf_registration.hpp` |
| `registration/include/pcl/registration/pyramid_feature_matching.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `registration/include/pcl/registration/impl/pyramid_feature_matching.hpp` |
| `registration/include/pcl/registration/registration.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项7），建议次优先 | `registration/include/pcl/registration/impl/registration.hpp` |
| `registration/include/pcl/registration/sample_consensus_prerejective.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项1），建议次优先 | `registration/include/pcl/registration/impl/sample_consensus_prerejective.hpp` |
| `registration/include/pcl/registration/transformation_estimation.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/transformation_estimation_2D.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项12），建议次优先 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` |
| `registration/include/pcl/registration/transformation_estimation_3point.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项0），建议次优先 | `registration/include/pcl/registration/impl/transformation_estimation_3point.hpp` |
| `registration/include/pcl/registration/transformation_estimation_dual_quaternion.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项0），建议次优先 | `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` |
| `registration/include/pcl/registration/transformation_estimation_lm.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `registration/include/pcl/registration/impl/transformation_estimation_lm.hpp` |
| `registration/include/pcl/registration/transformation_estimation_point_to_plane.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/transformation_estimation_point_to_plane_lls.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp` |
| `registration/include/pcl/registration/transformation_estimation_point_to_plane_lls_weighted.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp` |
| `registration/include/pcl/registration/transformation_estimation_point_to_plane_weighted.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项12），建议次优先 | `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_weighted.hpp` |
| `registration/include/pcl/registration/transformation_estimation_svd.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项17），建议次优先 | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| `registration/include/pcl/registration/transformation_estimation_svd_scale.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `registration/include/pcl/registration/transformation_estimation_symmetric_point_to_plane_lls.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项5），建议次优先 | `registration/include/pcl/registration/impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` |
| `registration/include/pcl/registration/transformation_validation.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/transformation_validation_euclidean.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项5），建议次优先 | `registration/include/pcl/registration/impl/transformation_validation_euclidean.hpp` |
| `registration/include/pcl/registration/vertex_estimates.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/warp_point_rigid.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/warp_point_rigid_3d.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `registration/include/pcl/registration/warp_point_rigid_6d.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |

## 4. 简要统计

- `include` 覆盖统计（105 全覆盖版）：high `10`，mid `60`，low `35`。
- `include` 候选占比：`70 / 105 = 66.7%`。
- 含同名 `impl/*.hpp` 关联的 `.h` 文件数：`39`。
- 模块总口径（include+src）：总文件 `145`，候选 `70`（high `10`，mid `60`），候选占比 `48.3%`。

## 5. src 次关注登记

- `registration/src/**` 已登记文件数：`40`。
- 主要文件（示例，非全部）：`registration/src/registration.cpp`、`registration/src/icp.cpp`、`registration/src/gicp.cpp`、`registration/src/ndt.cpp`、`registration/src/ndt_2d.cpp`、`registration/src/lum.cpp`、`registration/src/ppf_registration.cpp`、`registration/src/pairwise_graph_registration.cpp`、`registration/src/transformation_estimation_svd.cpp`、`registration/src/transformation_estimation_lm.cpp`。
- 说明：`src` 其余文件已完成次关注登记，不在本表展开逐行明细。
