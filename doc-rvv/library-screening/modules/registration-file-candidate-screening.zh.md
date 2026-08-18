# registration 模块 RVV 第一轮文件级筛选报告

本文档记录 `registration` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 主覆盖范围：`registration/include/pcl/registration/**`（含 `impl/**`）。
- 覆盖结果：`include` 文件总数 `105`，本表覆盖 `105`（`105/105`）。
- 次关注范围：`registration/src/**`，文件数 `40`，已登记但不纳入主候选优先池。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `registration/include/pcl/registration/`，`src` 文件以 `src/` 开头显示。

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
| 源码文件总数 | 105 | 主覆盖范围为 `registration/include/pcl/registration/**`，`src` 作为次关注登记。 |
| 已判定文件数 | 105 | `105/105` |
| high | 10 | 二轮必查 |
| mid | 60 | 二轮必查 |
| low | 35 | 已覆盖但不进入二轮初始基线 |
| high + mid | 70 | 第一轮候选基线 |
| 候选占比 | 70/105 = 66.7% | high + mid / 源码文件总数 |

## 4. high 候选（10）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/gicp.hpp` | 循环24处，数学/几何计算密集，建议优先优化 |
| `impl/ia_fpcs.hpp` | 循环24处，数学/几何计算密集，建议优先优化 |
| `impl/ndt_2d.hpp` | 循环13处，数学/几何计算密集，建议优先优化 |
| `impl/joint_icp.hpp` | 循环13处，数学/几何计算密集，建议优先优化 |
| `impl/ndt.hpp` | 循环10处，数学/几何计算密集，建议优先优化 |
| `impl/ia_kfpcs.hpp` | 循环10处，数学/几何计算密集，建议优先优化 |
| `impl/icp.hpp` | 循环7处，数学/几何计算密集，建议优先优化 |
| `impl/correspondence_estimation.hpp` | 循环6处，数学/几何计算密集，建议优先优化 |
| `impl/correspondence_estimation_normal_shooting.hpp` | 循环4处，数学/几何计算密集，建议优先优化 |
| `impl/correspondence_estimation_backprojection.hpp` | 循环4处，数学/几何计算密集，建议优先优化 |

## 5. mid 候选（60）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/elch.hpp` | 存在可向量化路径（循环13，数学项17），建议次优先 |
| `impl/pyramid_feature_matching.hpp` | 存在可向量化路径（循环13，数学项4），建议次优先 |
| `impl/ppf_registration.hpp` | 存在可向量化路径（循环12，数学项39），建议次优先 |
| `impl/lum.hpp` | 存在可向量化路径（循环9，数学项42），建议次优先 |
| `impl/correspondence_rejection_poly.hpp` | 存在可向量化路径（循环8，数学项3），建议次优先 |
| `impl/transformation_estimation_point_to_plane_weighted.hpp` | 存在可向量化路径（循环7，数学项13），建议次优先 |
| `impl/transformation_estimation_lm.hpp` | 存在可向量化路径（循环6，数学项17），建议次优先 |
| `impl/sample_consensus_prerejective.hpp` | 存在可向量化路径（循环6，数学项6），建议次优先 |
| `impl/ia_ransac.hpp` | 存在可向量化路径（循环5，数学项4），建议次优先 |
| `impl/correspondence_rejection_sample_consensus.hpp` | 存在可向量化路径（循环4，数学项1），建议次优先 |
| `bfgs.h` | 存在可向量化路径（循环3，数学项29），建议次优先 |
| `impl/transformation_estimation_dual_quaternion.hpp` | 存在可向量化路径（循环3，数学项15），建议次优先 |
| `impl/registration.hpp` | 存在可向量化路径（循环3，数学项6），建议次优先 |
| `impl/correspondence_rejection_sample_consensus_2d.hpp` | 存在可向量化路径（循环3，数学项2），建议次优先 |
| `impl/correspondence_types.hpp` | 存在可向量化路径（循环3，数学项1），建议次优先 |
| `correspondence_rejection_poly.h` | 存在可向量化路径（循环3，数学项0），建议次优先 |
| `impl/transformation_estimation_svd.hpp` | 存在可向量化路径（循环2，数学项32），建议次优先 |
| `impl/transformation_estimation_svd_scale.hpp` | 存在可向量化路径（循环2，数学项24），建议次优先 |
| `impl/transformation_estimation_point_to_plane_lls.hpp` | 存在可向量化路径（循环2，数学项4），建议次优先 |
| `icp.h` | 存在可向量化路径（循环2，数学项3），建议次优先 |
| `impl/transformation_estimation_point_to_plane_lls_weighted.hpp` | 存在可向量化路径（循环2，数学项3），建议次优先 |
| `impl/transformation_validation_euclidean.hpp` | 存在可向量化路径（循环2，数学项2），建议次优先 |
| `gicp.h` | 存在可向量化路径（循环1，数学项29），建议次优先 |
| `impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` | 存在可向量化路径（循环1，数学项15），建议次优先 |
| `lum.h` | 存在可向量化路径（循环1，数学项14），建议次优先 |
| `registration.h` | 存在可向量化路径（循环1，数学项7），建议次优先 |
| `impl/correspondence_estimation_organized_projection.hpp` | 存在可向量化路径（循环1，数学项4），建议次优先 |
| `default_convergence_criteria.h` | 存在可向量化路径（循环1，数学项3），建议次优先 |
| `impl/pairwise_graph_registration.hpp` | 存在可向量化路径（循环1，数学项3），建议次优先 |
| `incremental_registration.h` | 存在可向量化路径（循环1，数学项0），建议次优先 |
| `meta_registration.h` | 存在可向量化路径（循环1，数学项0），建议次优先 |
| `ndt.h` | 存在可向量化路径（循环0，数学项45），建议次优先 |
| `impl/transformation_estimation_3point.hpp` | 存在可向量化路径（循环0，数学项17），建议次优先 |
| `transformation_estimation_svd.h` | 存在可向量化路径（循环0，数学项17），建议次优先 |
| `impl/transformation_estimation_2D.hpp` | 存在可向量化路径（循环0，数学项16），建议次优先 |
| `transformation_estimation_lm.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `transformation_estimation_2D.h` | 存在可向量化路径（循环0，数学项12），建议次优先 |
| `transformation_estimation_point_to_plane_weighted.h` | 存在可向量化路径（循环0，数学项12），建议次优先 |
| `ndt_2d.h` | 存在可向量化路径（循环0，数学项10），建议次优先 |
| `ppf_registration.h` | 存在可向量化路径（循环0，数学项9），建议次优先 |
| `correspondence_rejection_features.h` | 存在可向量化路径（循环0，数学项8），建议次优先 |
| `correspondence_estimation_organized_projection.h` | 存在可向量化路径（循环0，数学项7），建议次优先 |
| `transformation_estimation_symmetric_point_to_plane_lls.h` | 存在可向量化路径（循环0，数学项5），建议次优先 |
| `transformation_validation_euclidean.h` | 存在可向量化路径（循环0，数学项5），建议次优先 |
| `correspondence_estimation.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |
| `elch.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |
| `ia_fpcs.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |
| `transformation_estimation_point_to_plane_lls.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |
| `transformation_estimation_point_to_plane_lls_weighted.h` | 存在可向量化路径（循环0，数学项4），建议次优先 |
| `correspondence_rejection_sample_consensus.h` | 存在可向量化路径（循环0，数学项3），建议次优先 |
| `correspondence_estimation_backprojection.h` | 存在可向量化路径（循环0，数学项2），建议次优先 |
| `correspondence_estimation_normal_shooting.h` | 存在可向量化路径（循环0，数学项2），建议次优先 |
| `correspondence_rejection_sample_consensus_2d.h` | 存在可向量化路径（循环0，数学项2），建议次优先 |
| `ia_kfpcs.h` | 存在可向量化路径（循环0，数学项2），建议次优先 |
| `ia_ransac.h` | 存在可向量化路径（循环0，数学项2），建议次优先 |
| `sample_consensus_prerejective.h` | 存在可向量化路径（循环0，数学项1），建议次优先 |
| `joint_icp.h` | 存在可向量化路径（循环0，数学项0），建议次优先 |
| `pairwise_graph_registration.h` | 存在可向量化路径（循环0，数学项0），建议次优先 |
| `transformation_estimation_3point.h` | 存在可向量化路径（循环0，数学项0），建议次优先 |
| `transformation_estimation_dual_quaternion.h` | 存在可向量化路径（循环0，数学项0），建议次优先 |

## 6. 全量文件覆盖表（105/105）

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `bfgs.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项29），建议次优先 | `-` |
| `boost_graph.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `convergence_criteria.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `correspondence_estimation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `impl/correspondence_estimation.hpp` |
| `correspondence_estimation_backprojection.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项2），建议次优先 | `impl/correspondence_estimation_backprojection.hpp` |
| `correspondence_estimation_normal_shooting.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项2），建议次优先 | `impl/correspondence_estimation_normal_shooting.hpp` |
| `correspondence_estimation_organized_projection.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项7），建议次优先 | `impl/correspondence_estimation_organized_projection.hpp` |
| `correspondence_rejection.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `correspondence_rejection_distance.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `correspondence_rejection_features.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项8），建议次优先 | `impl/correspondence_rejection_features.hpp` |
| `correspondence_rejection_median_distance.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `correspondence_rejection_one_to_one.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `correspondence_rejection_organized_boundary.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `correspondence_rejection_poly.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项0），建议次优先 | `impl/correspondence_rejection_poly.hpp` |
| `correspondence_rejection_sample_consensus.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项3），建议次优先 | `impl/correspondence_rejection_sample_consensus.hpp` |
| `correspondence_rejection_sample_consensus_2d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项2），建议次优先 | `impl/correspondence_rejection_sample_consensus_2d.hpp` |
| `correspondence_rejection_surface_normal.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `correspondence_rejection_trimmed.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `correspondence_rejection_var_trimmed.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `correspondence_sorting.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `correspondence_types.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `impl/correspondence_types.hpp` |
| `default_convergence_criteria.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项3），建议次优先 | `impl/default_convergence_criteria.hpp` |
| `distances.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `edge_measurements.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `elch.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `impl/elch.hpp` |
| `exceptions.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `gicp.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项29），建议次优先 | `impl/gicp.hpp` |
| `gicp6d.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `graph_handler.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `graph_optimizer.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `graph_registration.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `ia_fpcs.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `impl/ia_fpcs.hpp` |
| `ia_kfpcs.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项2），建议次优先 | `impl/ia_kfpcs.hpp` |
| `ia_ransac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项2），建议次优先 | `impl/ia_ransac.hpp` |
| `icp.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项3），建议次优先 | `impl/icp.hpp` |
| `icp_nl.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `impl/icp_nl.hpp` |
| `impl/correspondence_estimation.hpp` | `high` | 是 | 循环6处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/correspondence_estimation_backprojection.hpp` | `high` | 是 | 循环4处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/correspondence_estimation_normal_shooting.hpp` | `high` | 是 | 循环4处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/correspondence_estimation_organized_projection.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项4），建议次优先 | `-` |
| `impl/correspondence_rejection_features.hpp` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `impl/correspondence_rejection_poly.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项3），建议次优先 | `-` |
| `impl/correspondence_rejection_sample_consensus.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项1），建议次优先 | `-` |
| `impl/correspondence_rejection_sample_consensus_2d.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项2），建议次优先 | `-` |
| `impl/correspondence_types.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项1），建议次优先 | `-` |
| `impl/default_convergence_criteria.hpp` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `impl/elch.hpp` | `mid` | 是 | 存在可向量化路径（循环13，数学项17），建议次优先 | `-` |
| `impl/gicp.hpp` | `high` | 是 | 循环24处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/ia_fpcs.hpp` | `high` | 是 | 循环24处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/ia_kfpcs.hpp` | `high` | 是 | 循环10处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/ia_ransac.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项4），建议次优先 | `-` |
| `impl/icp.hpp` | `high` | 是 | 循环7处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/icp_nl.hpp` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `impl/incremental_registration.hpp` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `impl/joint_icp.hpp` | `high` | 是 | 循环13处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/lum.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项42），建议次优先 | `-` |
| `impl/meta_registration.hpp` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `impl/ndt.hpp` | `high` | 是 | 循环10处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/ndt_2d.hpp` | `high` | 是 | 循环13处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/pairwise_graph_registration.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项3），建议次优先 | `-` |
| `impl/ppf_registration.hpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项39），建议次优先 | `-` |
| `impl/pyramid_feature_matching.hpp` | `mid` | 是 | 存在可向量化路径（循环13，数学项4），建议次优先 | `-` |
| `impl/registration.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项6），建议次优先 | `-` |
| `impl/sample_consensus_prerejective.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项6），建议次优先 | `-` |
| `impl/transformation_estimation_2D.hpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项16），建议次优先 | `-` |
| `impl/transformation_estimation_3point.hpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项17），建议次优先 | `-` |
| `impl/transformation_estimation_dual_quaternion.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项15），建议次优先 | `-` |
| `impl/transformation_estimation_lm.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项17），建议次优先 | `-` |
| `impl/transformation_estimation_point_to_plane_lls.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项4），建议次优先 | `-` |
| `impl/transformation_estimation_point_to_plane_lls_weighted.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项3），建议次优先 | `-` |
| `impl/transformation_estimation_point_to_plane_weighted.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项13），建议次优先 | `-` |
| `impl/transformation_estimation_svd.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项32），建议次优先 | `-` |
| `impl/transformation_estimation_svd_scale.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项24），建议次优先 | `-` |
| `impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项15），建议次优先 | `-` |
| `impl/transformation_validation_euclidean.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项2），建议次优先 | `-` |
| `incremental_registration.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项0），建议次优先 | `impl/incremental_registration.hpp` |
| `joint_icp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项0），建议次优先 | `impl/joint_icp.hpp` |
| `lum.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项14），建议次优先 | `impl/lum.hpp` |
| `matching_candidate.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `meta_registration.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项0），建议次优先 | `impl/meta_registration.hpp` |
| `ndt.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项45），建议次优先 | `impl/ndt.hpp` |
| `ndt_2d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项10），建议次优先 | `impl/ndt_2d.hpp` |
| `pairwise_graph_registration.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项0），建议次优先 | `impl/pairwise_graph_registration.hpp` |
| `ppf_registration.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项9），建议次优先 | `impl/ppf_registration.hpp` |
| `pyramid_feature_matching.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `impl/pyramid_feature_matching.hpp` |
| `registration.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项7），建议次优先 | `impl/registration.hpp` |
| `sample_consensus_prerejective.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项1），建议次优先 | `impl/sample_consensus_prerejective.hpp` |
| `transformation_estimation.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `transformation_estimation_2D.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项12），建议次优先 | `impl/transformation_estimation_2D.hpp` |
| `transformation_estimation_3point.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项0），建议次优先 | `impl/transformation_estimation_3point.hpp` |
| `transformation_estimation_dual_quaternion.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项0），建议次优先 | `impl/transformation_estimation_dual_quaternion.hpp` |
| `transformation_estimation_lm.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `impl/transformation_estimation_lm.hpp` |
| `transformation_estimation_point_to_plane.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `transformation_estimation_point_to_plane_lls.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `impl/transformation_estimation_point_to_plane_lls.hpp` |
| `transformation_estimation_point_to_plane_lls_weighted.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项4），建议次优先 | `impl/transformation_estimation_point_to_plane_lls_weighted.hpp` |
| `transformation_estimation_point_to_plane_weighted.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项12），建议次优先 | `impl/transformation_estimation_point_to_plane_weighted.hpp` |
| `transformation_estimation_svd.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项17），建议次优先 | `impl/transformation_estimation_svd.hpp` |
| `transformation_estimation_svd_scale.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `impl/transformation_estimation_svd_scale.hpp` |
| `transformation_estimation_symmetric_point_to_plane_lls.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项5），建议次优先 | `impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` |
| `transformation_validation.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `transformation_validation_euclidean.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项5），建议次优先 | `impl/transformation_validation_euclidean.hpp` |
| `vertex_estimates.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `warp_point_rigid.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `warp_point_rigid_3d.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |
| `warp_point_rigid_6d.h` | `low` | 否 | 当前以声明/类型/控制逻辑为主，暂不纳入本轮候选 | `-` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/registration/registration-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
