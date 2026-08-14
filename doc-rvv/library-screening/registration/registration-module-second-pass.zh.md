# registration 模块 RVV 二轮筛选报告

本文档记录 `registration` 模块从文件级初筛结果转为 RVV 模块实施队列的第二轮筛选结论。第二轮筛选不是重新做全库筛选，而是把 `registration-function-triage.zh.md` 中 high/mid 候选逐项下钻到公开入口、函数族和主成本覆盖类型，并给出可执行队列。

`registration` 与 `filters` 的主要差异是：很多文件位于 ICP / NDT / GICP / RANSAC / FPCS 等迭代框架中，循环数量多，但主成本常被 search、Eigen solver、SVD、随机采样、候选排序或状态机稀释。因此本轮不按 high/mid 机械排序，而按 RVV 是否覆盖公开入口主成本来分组。

## 1. 输入依据

- 原始模块 triage：`doc-rvv/library-screening/modules/registration-function-triage.zh.md`
- 源码范围：`registration`
- 上游测试：`test/registration/*.cpp`
- 上游 benchmark：当前仓库未发现 `benchmarks/registration/` 目录；registration 主题需要优先建立专项 bench
- 既有 RVV 经验：`common` 的 transform / centroid / norms，`filters` 的 direct-main-path、partial-preprocess、bench 诊断和 follow-up rescreen 经验
- 通用 workflow：`doc-rvv/library-screening/module-optimization-workflow.zh.md`
- 表格文件路径说明：候选表和执行清单中的文件名省略公共前缀 `registration/include/pcl/registration/`；`impl/*.hpp` 保留 `impl/` 前缀

## 2. 二轮筛选统计

| 项目                            | 数量 | 说明                                                      |
| ------------------------------- | ---: | --------------------------------------------------------- |
| 初筛 include 文件总数           |  105 | `registration/include/pcl/registration/**`              |
| 初筛 high                       |   10 | 全部作为二轮初始候选基线                                  |
| 初筛 mid                        |   60 | 全部作为二轮初始候选基线                                  |
| 初筛 low                        |   35 | 本轮未发现必须补入的明显漏筛项                            |
| 第二轮初始候选基线              |   70 | high + mid 必查文件                                       |
| 新增补充候选                    |    0 | 不从 high/mid 之外新增候选                                |
| 第二轮候选总数                  |   70 | high/mid 去重后数量                                       |
| 建议进行 RVV 优化的文件         |   12 | 公开入口主成本较清晰，或可建立生产 / full diagnostic 闭环 |
| 保留实施的候选文件              |   22 | 有局部 RVV 点，但需要后续复筛或 bench 诊断证明收益        |
| 暂缓或不推荐考虑 RVV 优化的文件 |   36 | 公开声明头、薄入口、组合调度或不单独实施文件              |
| 暂缓 / 删除 / 源码冲突          |    0 | 本轮未删除 high/mid 候选，未发现源码冲突                  |
| 从 high/mid 降级为不单独实施    |   36 | 主要是公开声明头、框架入口或调度入口                      |

## 3. 文件级变化理由

### 3.1 建议进行 RVV 优化的文件

这些文件不再拆分实施波次，而是当前二轮筛选认为可以进入函数级评估和专项验证的建议优化队列。执行清单仍给出推荐顺序，用于后续普通主题优化选择第一条未完成主题。

| 文件                                                           | 关键入口 / 函数族                                                        | 主成本覆盖类型         | 二轮变化理由                                                                                                                                                                                       |
| -------------------------------------------------------------- | ------------------------------------------------------------------------ | ---------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `impl/transformation_validation_euclidean.hpp`                    | `TransformationValidationEuclidean::validateTransformation`            | `direct-main-path`   | 公开入口先按矩阵变换 source，再对 transformed cloud 做 nearestKSearch 和距离累加；RVV 可评估固定 4x4 transform staging，并用 full diagnostic 判断 search 稀释                                      |
| `impl/correspondence_estimation_organized_projection.hpp`         | `determineCorrespondences`                                             | `direct-main-path`   | organized projection 避免 KdTree，主循环为投影、视锥 / depth / distance 判断和 correspondence 输出，适合 AoS load、矩阵投影、mask 和保序写出                                                       |
| `impl/transformation_estimation_point_to_plane_lls.hpp`           | `TransformationEstimationPointToPlaneLLS::estimateRigidTransformation` | `direct-main-path`   | 每个 correspondence 生成 point-to-plane 线性方程并累加 6x6 / 6x1，公式固定；solver 仍为 Eigen，但前置 normal-equation 构造可形成可隔离 full diagnostic                                             |
| `impl/transformation_estimation_point_to_plane_lls_weighted.hpp`  | weighted LLS normal-equation 构造                                        | `direct-main-path`   | 与非 weighted LLS 同结构，额外权重乘 normal；适合验证权重对 RVV 累加和数值语义的影响                                                                                                               |
| `impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` | symmetric point-to-plane LLS                                             | `direct-main-path`   | 同属固定 correspondence 公式和 normal-equation 累加，适合在 LLS 主题中统一比较                                                                                                                     |
| `impl/icp.hpp`                                                    | `IterativeClosestPoint::transformCloud`、`computeTransformation`     | `partial-preprocess` | `transformCloud` 对 PCLPointCloud2 字段 memcpy + 4x4 transform 有局部 RVV 空间；完整 ICP 可能被 correspondence search / rejector / estimator 稀释，因此函数级评估需优先证明 full diagnostic 价值 |
| `impl/correspondence_types.hpp`                                   | correspondence vector helper                                             | `tail-compress`      | correspondence 过滤 / rejected index 生成可能可做 mask 或 set-difference 诊断；需要确认输出顺序和重复 correspondence 语义                                                                          |
| `impl/correspondence_rejection_poly.hpp`                          | `getRemainingCorrespondences`、histogram、Otsu threshold               | `diagnostic`    | accept-rate、histogram 和 Otsu 有局部 loop；随机多边形采样和 pair 组合可能稀释收益，建议先做 bench 诊断而非直接生产分流                                                                            |
| `impl/transformation_estimation_2D.hpp`                           | 2D transformation estimation                                             | `direct-main-path`   | 固定 2D centroid / covariance / transform 公式，有批处理空间；上游覆盖和数据规模需函数级评估确认                                                                                                   |
| `impl/transformation_estimation_svd.hpp`                          | SVD transform estimation                                                 | `partial-preprocess` | centroid / demean / covariance 构造可 RVV，最终 SVD 仍为 Eigen；适合在 LLS 主题后评估前置累加占比                                                                                                  |
| `impl/transformation_estimation_dual_quaternion.hpp`              | dual quaternion transform estimation                                     | `partial-preprocess` | correspondence 累加有固定公式，但最终求解和数值路径复杂；需要专项评估收益和维护边界                                                                                                                |
| `bfgs.h`                                                         | BFGS line search / update                                                | `diagnostic`    | 数学循环存在，但作为优化器内部状态机，单独接生产风险高；仅建议建立诊断问题，判断 GICP / NDT 是否受益                                                                                               |

### 3.2 保留实施的候选文件

这些文件仍保留在 registration 候选池中，但当前证据不足以直接进入建议优化队列。后续应在已完成主题的真实性能、回退原因和专项 bench 经验基础上复筛。

| 文件                                                      | 关键入口 / 函数族                                                 | 主成本覆盖类型         | RVV 适配点                        | 暂不进入建议优化队列的原因                                                                     | 后续验证问题                        |
| --------------------------------------------------------- | ----------------------------------------------------------------- | ---------------------- | --------------------------------- | ---------------------------------------------------------------------------------------------- | ----------------------------------- |
| `impl/gicp.hpp`                                              | covariance、correspondence、BFGS / rigid transform update         | `partial-preprocess` | covariance / residual local loops | high 候选但完整 GICP 被 covariance、KdTree correspondence、优化器和矩阵状态共同主导            | 是否拆成 covariance / residual 诊断 |
| `impl/ndt.hpp`                                               | `computeDerivatives`、`updateDerivatives`、Hessian / gradient | `partial-preprocess` | per-point derivative accumulation | neighborhood search、voxel covariance、SVD / Newton step 和 OpenMP reduction 主导              | full diagnostic 是否能超过局部收益  |
| `impl/ndt_2d.hpp`                                            | 2D NDT grid / Newton update                                       | `partial-preprocess` | grid formula / reduction          | grid 查询和 solver 状态明显                                                                    | 是否有代表性 dataset                |
| `impl/joint_icp.hpp`                                         | 多 cloud correspondence / rejector / transform loop               | `non-standalone`     | 子流程间接受益                    | 主要组合 ICP 子流程；真实 RVV 点在 correspondence、transform estimation 或 transformCloud 主题 | 子主题完成后是否仍有独立价值        |
| `impl/correspondence_estimation.hpp`                         | `determineCorrespondences` / reciprocal KNN                     | `tail-compress`      | threshold + append                | 外层线性扫描明显，但每点主成本是 KdTree nearestKSearch                                         | search 后处理占比                   |
| `impl/correspondence_estimation_normal_shooting.hpp`         | KNN 后点到 normal line 距离最小值                                 | `diagnostic`    | k candidates distance min         | KNN 主导，K 个候选内的点线距离只适合 microbench                                                | k 较大时是否可收益                  |
| `impl/correspondence_estimation_backprojection.hpp`          | KNN 后 normal backprojection score                                | `diagnostic`    | normal dot / weighted distance    | search 和 reciprocal search 稀释收益                                                           | score loop 占比                     |
| `impl/elch.hpp`                                              | loop closure graph / registration orchestration                   | `non-standalone`     | 子 registration 主题间接受益      | 图优化和 registration 子流程组合为主                                                           | 等子主题完成后复筛                  |
| `impl/pyramid_feature_matching.hpp`                          | pyramid feature matching loop                                     | `diagnostic`    | feature distance / level loop     | 数据流和使用场景需先明确                                                                       | 是否有上游 case 或专项 dataset      |
| `impl/ppf_registration.hpp`                                  | PPF pair feature、hash map accumulator、pose clustering           | `diagnostic`    | pair feature formula              | hash map / accumulator / clustering 主导                                                       | fixed model/scene case 是否可归因   |
| `impl/lum.hpp`                                               | graph edge / linear system accumulation                           | `partial-preprocess` | matrix accumulation               | graph / solver 主导                                                                            | edge accumulation 占比              |
| `impl/transformation_estimation_point_to_plane_weighted.hpp` | LM point-to-plane weighted residual functor                       | `diagnostic`    | weighted residual                 | LM 每次迭代 residual 可批处理，但 Eigen numerical diff / LM solver 主导                        | 与 weighted LLS 的收益比较          |
| `impl/transformation_estimation_lm.hpp`                      | LM residual functor                                               | `diagnostic`    | warp + residual                   | LM solver / numerical diff 主导                                                                | full residual diagnostic            |
| `impl/sample_consensus_prerejective.hpp`                     | sample selection、feature KNN、fitness                            | `diagnostic`    | fitness / inlier scan             | RANSAC / feature search / random sampling 主导                                                 | fitness loop 是否独立热点           |
| `impl/ia_ransac.hpp`                                         | SAC-IA sample / feature correspondence / error metric             | `diagnostic`    | error reduction                   | feature KNN / random sampling 主导                                                             | transformed cloud + search 后成本   |
| `impl/ia_fpcs.hpp`                                           | FPCS base matching、validate transform                            | `diagnostic`    | residual / score loop             | 多层 pair / radiusSearch / candidate 状态主导                                                  | 局部 residual 是否可代表真实入口    |
| `impl/ia_kfpcs.hpp`                                          | KFPCS validate / final candidate filtering                        | `diagnostic`    | residual / score loop             | candidate sort、nearest search 和状态筛选主导                                                  | 局部 score 是否可代表真实入口       |
| `impl/correspondence_rejection_sample_consensus.hpp`         | SAC correspondence rejector                                       | `diagnostic`    | threshold / inlier scan           | sample consensus 内部随机和模型估计主导                                                        | inlier scan 占比                    |
| `impl/correspondence_rejection_sample_consensus_2d.hpp`      | 2D SAC correspondence rejector                                    | `diagnostic`    | threshold / inlier scan           | 同 3D SAC rejector，且适用面更窄                                                               | 是否有稳定 2D case                  |
| `impl/transformation_estimation_svd_scale.hpp`               | SVD with scale                                                    | `partial-preprocess` | covariance / scale accumulation   | 与 SVD 共用前置累加模式，scale 增加数值边界                                                    | 与普通 SVD 的收益比较               |
| `impl/transformation_estimation_3point.hpp`                  | 3-point transform estimation                                      | `diagnostic`    | fixed small formula               | 输入规模固定且很小，不适合作为批量 RVV 生产主题                                                | 是否只保留为非目标                  |
| `impl/pairwise_graph_registration.hpp`                       | pairwise graph registration orchestration                         | `non-standalone`     | 子主题间接受益                    | 图注册组合流程，本文件不承载独立主循环                                                         | 子主题完成后是否仍需复筛            |

### 3.3 暂缓或不推荐考虑 RVV 优化的文件

这些文件来自 high/mid 初筛，但当前不建议作为独立 RVV 主题。多数是公开声明头、薄 wrapper、组合调度入口或真实循环在对应 `impl/*.hpp` 中；后续若对应主主题推进，可作为伴随文件复核。

| 文件                                                         | 原始去向或合并到主题                                                | 暂缓、不推荐或不单独实施原因                          |
| ------------------------------------------------------------ | ------------------------------------------------------------------- | ----------------------------------------------------- |
| `correspondence_rejection_poly.h`                          | `impl/correspondence_rejection_poly.hpp`                          | 公开声明头，真实循环在 impl                           |
| `icp.h`                                                    | `impl/icp.hpp`                                                    | 公开声明头，真实流程在 impl                           |
| `gicp.h`                                                   | `impl/gicp.hpp`                                                   | 公开声明头，真实流程在 impl                           |
| `lum.h`                                                    | `impl/lum.hpp`                                                    | 公开声明头，真实流程在 impl                           |
| `registration.h`                                           | `impl/registration.hpp`                                           | 公开框架声明，真实流程在 impl                         |
| `registration.hpp`                                         | registration / ICP / NDT 等主题上下文                              | 框架入口承载 `align` 调度、状态和 convergence 入口，不单独作为 RVV 主题；后续随具体 registration 子主题复核公开调用路径 |
| `ndt.h`                                                    | `impl/ndt.hpp`                                                    | 公开声明头，真实流程在 impl                           |
| `transformation_estimation_svd.h`                          | `impl/transformation_estimation_svd.hpp`                          | 公开声明头，真实循环在 impl                           |
| `transformation_estimation_lm.h`                           | `impl/transformation_estimation_lm.hpp`                           | 公开声明头，真实循环在 impl                           |
| `transformation_estimation_2D.h`                           | `impl/transformation_estimation_2D.hpp`                           | 公开声明头，真实循环在 impl                           |
| `transformation_estimation_point_to_plane_weighted.h`      | `impl/transformation_estimation_point_to_plane_weighted.hpp`      | 公开声明头，真实循环在 impl                           |
| `ndt_2d.h`                                                 | `impl/ndt_2d.hpp`                                                 | 公开声明头，真实流程在 impl                           |
| `ppf_registration.h`                                       | `impl/ppf_registration.hpp`                                       | 公开声明头，真实流程在 impl                           |
| `correspondence_rejection_features.h`                      | correspondence rejector family                                      | 公开声明头，本轮未发现比 impl 主题更合适的独立 RVV 点 |
| `correspondence_estimation_organized_projection.h`         | `impl/correspondence_estimation_organized_projection.hpp`         | 公开声明头，真实循环在 impl                           |
| `transformation_estimation_symmetric_point_to_plane_lls.h` | `impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` | 公开声明头，真实循环在 impl                           |
| `transformation_validation_euclidean.h`                    | `impl/transformation_validation_euclidean.hpp`                    | 公开声明头，真实循环在 impl                           |
| `correspondence_estimation.h`                              | `impl/correspondence_estimation.hpp`                              | 公开声明头，真实循环在 impl                           |
| `elch.h`                                                   | `impl/elch.hpp`                                                   | 公开声明头，真实流程在 impl                           |
| `ia_fpcs.h`                                                | `impl/ia_fpcs.hpp`                                                | 公开声明头，真实流程在 impl                           |
| `transformation_estimation_point_to_plane_lls.h`           | `impl/transformation_estimation_point_to_plane_lls.hpp`           | 公开声明头，真实循环在 impl                           |
| `transformation_estimation_point_to_plane_lls_weighted.h`  | `impl/transformation_estimation_point_to_plane_lls_weighted.hpp`  | 公开声明头，真实循环在 impl                           |
| `correspondence_rejection_sample_consensus.h`              | `impl/correspondence_rejection_sample_consensus.hpp`              | 公开声明头，真实流程在 impl                           |
| `correspondence_estimation_backprojection.h`               | `impl/correspondence_estimation_backprojection.hpp`               | 公开声明头，真实循环在 impl                           |
| `correspondence_estimation_normal_shooting.h`              | `impl/correspondence_estimation_normal_shooting.hpp`              | 公开声明头，真实循环在 impl                           |
| `correspondence_rejection_sample_consensus_2d.h`           | `impl/correspondence_rejection_sample_consensus_2d.hpp`           | 公开声明头，真实流程在 impl                           |
| `ia_kfpcs.h`                                               | `impl/ia_kfpcs.hpp`                                               | 公开声明头，真实流程在 impl                           |
| `ia_ransac.h`                                              | `impl/ia_ransac.hpp`                                              | 公开声明头，真实流程在 impl                           |
| `sample_consensus_prerejective.h`                          | `impl/sample_consensus_prerejective.hpp`                          | 公开声明头，真实流程在 impl                           |
| `joint_icp.h`                                              | `impl/joint_icp.hpp`                                              | 公开声明头，真实流程在 impl                           |
| `pairwise_graph_registration.h`                            | `impl/pairwise_graph_registration.hpp`                            | 公开声明头，真实流程在 impl                           |
| `transformation_estimation_3point.h`                       | `impl/transformation_estimation_3point.hpp`                       | 公开声明头，真实循环在 impl                           |
| `transformation_estimation_dual_quaternion.h`              | `impl/transformation_estimation_dual_quaternion.hpp`              | 公开声明头，真实循环在 impl                           |
| `default_convergence_criteria.h`                           | registration / ICP convergence 状态                                 | 状态检查和收敛条件为主，不单独作为 RVV 主题           |
| `incremental_registration.h`                               | registration orchestration                                          | 增量注册调度入口，真实数值热点在子 registration 主题  |
| `meta_registration.h`                                      | registration orchestration                                          | 多 registration 组合入口，不承载独立 RVV 主循环       |

### 3.4 相对第一轮 triage 的主要调整

| 调整类型 | 文件或主题 | 变化理由 |
| -------- | ---------- | -------- |
| high 候选降为保留复筛 | `impl/gicp.hpp`、`impl/ndt.hpp`、`impl/ndt_2d.hpp`、`impl/joint_icp.hpp`、`impl/ia_fpcs.hpp`、`impl/ia_kfpcs.hpp`、`impl/correspondence_estimation.hpp`、`impl/correspondence_estimation_normal_shooting.hpp`、`impl/correspondence_estimation_backprojection.hpp` | 第一轮只看到循环和数学密集片段；第二轮下钻后确认完整入口常被 KdTree / radiusSearch、Eigen solver、随机采样、候选排序或状态机稀释，先保留为后续复筛或诊断主题。 |
| mid 候选提升为建议优化 | `impl/transformation_validation_euclidean.hpp`、`impl/correspondence_estimation_organized_projection.hpp`、`impl/transformation_estimation_point_to_plane_lls.hpp`、`impl/transformation_estimation_point_to_plane_lls_weighted.hpp`、`impl/transformation_estimation_symmetric_point_to_plane_lls.hpp`、`impl/transformation_estimation_2D.hpp` | 这些文件虽然第一轮不是 high，但公开入口或 normal-equation / projection / transform staging 公式更规整，能够建立 production 或 full diagnostic 闭环。 |
| 公开声明头合并 | `icp.h`、`gicp.h`、`ndt.h`、各 `transformation_estimation_*.h`、correspondence 相关 `.h` 文件 | `.h` 多数承载公开 API 声明或薄入口，真实循环在对应实现文件；后续主题应以实现文件和公开入口共同恢复调用链。 |
| 框架 / 组合入口不单独实施 | `registration.hpp`、`default_convergence_criteria.h`、`incremental_registration.h`、`meta_registration.h`、`impl/pairwise_graph_registration.hpp`、`impl/elch.hpp`、`impl/joint_icp.hpp` | 这些文件主要负责调度、状态、图注册或子 registration 组合；可作为具体主题的上下文或复筛对象，不单独承诺 RVV 主路径。 |
| 暂不新增外部文件 | 无 | 本轮没有从 70 个 high/mid 候选之外新增文件；若后续 profile 或源码证据显示 low 文件存在明显漏筛，可在 follow-up rescreen 中补入并说明证据。 |

## 4. 执行清单 / 状态表

### 4.1 建议优化文件队列

| 顺序 | 主题                                                       | 主文件                                                         | 状态   | 当前结论 / 下一步条件 |
| ---: | ---------------------------------------------------------- | -------------------------------------------------------------- | ------ | --------------------- |
|    1 | `transformation_validation_euclidean`                      | `impl/transformation_validation_euclidean.hpp`                 | 已完成 / bench 诊断 | transform staging 片段有收益，但 nearest search 稀释 full validation；保留 bench 诊断证据，不接生产分流。 |
|    2 | `correspondence_estimation_organized_projection`           | `impl/correspondence_estimation_organized_projection.hpp`      | 已完成 / production-ready | 已接入 source transform、projection-pixel 和 target-predicate production RVV；append / stored distance 写出保留标量。 |
|    3 | `transformation_estimation_point_to_plane_lls`             | `impl/transformation_estimation_point_to_plane_lls.hpp`        | 已完成 / production-candidate ordered-cloud-pair f32 AoS | 已接入 `Scalar=float` ordered-cloud-pair public overload 的 source xyz / target xyz+normal f32 AoS layout-gated fused-formula block dispatch；代表点型板卡 5-run median 为 `2.80x`~`3.15x`。source-indexed-cloud-pair、dual-indexed-cloud-pair、correspondence-pair、weighted 和 `Scalar=double` 保持边界外。 |
|    4 | `transformation_estimation_point_to_plane_lls_weighted`    | `impl/transformation_estimation_point_to_plane_lls_weighted.hpp` | 已完成 / bounded production-candidate | ordered-cloud-pair public overload 已接入 production RVV；source-indexed-cloud-pair public overload 为 block-fused production probe bounded candidate。ordered-cloud-pair 262144 代表点型板卡 5-run median 为 `2.76x`~`3.00x`，source-indexed-cloud-pair probe median 为 `1.54x`~`1.71x` 且仍有 warning。dual-indexed-cloud-pair、correspondence-pair、`Scalar=double` 和非连续权重保持标量。 |
|    5 | `transformation_estimation_symmetric_point_to_plane_lls`   | `impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` | 已完成 / production-ready 泛型 normal 顺序点云对 | 已接入 generic normal / `float` / ordered-cloud-pair public overload 的 production RVV；板卡 production-direct `PointNormal` 64K / 256K 为 `2.62x` / `2.47x`，`PointXYZINormal` 为 `2.64x` / `1.89x`。indices 和 correspondence-pair 保持标量，correspondence-pair 诊断为 `0.64x` / `0.41x`。 |
|    6 | `icp_transform_cloud`                                      | `impl/icp.hpp`                                                 | 待评估    | 先做 transformCloud/full diagnostic，确认是否被 ICP search 主成本稀释。 |
|    7 | `correspondence_types`                                     | `impl/correspondence_types.hpp`                                | 待评估    | 评估 correspondence helper 的输出顺序、重复输入和 set-difference / mask 语义。 |
|    8 | `correspondence_rejection_poly`                            | `impl/correspondence_rejection_poly.hpp`                       | 待评估    | 优先诊断 histogram / Otsu / accept-rate 局部收益，不直接承诺生产分流。 |
|    9 | `transformation_estimation_2D`                             | `impl/transformation_estimation_2D.hpp`                        | 待评估    | 确认 2D 数据规模、上游测试和 fixed formula reduction 收益。 |
|   10 | `transformation_estimation_svd`                            | `impl/transformation_estimation_svd.hpp`                       | 待评估    | 评估 centroid / covariance 前置累加是否被 Eigen SVD 稀释。 |
|   11 | `transformation_estimation_dual_quaternion`                | `impl/transformation_estimation_dual_quaternion.hpp`           | 待评估    | 评估 dual quaternion 累加公式和数值维护边界。 |
|   12 | `bfgs`                                                     | `bfgs.h`                                                       | 待评估    | 仅建立优化器局部诊断问题，确认是否服务 GICP / NDT 热点。 |

### 4.2 建议优化文件状态矩阵

| 主题                                                       | 函数级评估 | RVV 实现 | 专项测试 | bench  | QEMU   | 反汇编 | 板卡闭环 | 主题文档 | 工作日志 |
| ---------------------------------------------------------- | ---------- | -------- | -------- | ------ | ------ | ------ | -------- | -------- | -------- |
| `transformation_validation_euclidean`                    | 已完成 / bench 诊断 | bench 诊断 | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已记录   |
| `correspondence_estimation_organized_projection`         | 已完成 / production-ready | 已接入 production | 已完成 | 已完成 | 已完成 | 已完成 | 已完成   | 已完成   | 已记录   |
| `transformation_estimation_point_to_plane_lls`           | 已完成 / production-candidate ordered-cloud-pair f32 AoS | 已接入 production-candidate ordered-cloud-pair | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已记录 |
| `transformation_estimation_point_to_plane_lls_weighted`  | 已完成 / bounded production-candidate | 已接入 ordered-cloud-pair；source-indexed-cloud-pair bounded probe | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已记录 |
| `transformation_estimation_symmetric_point_to_plane_lls` | 已完成 / production-ready 泛型 normal 顺序点云对 | 已接入 production | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已记录 |
| `icp_transform_cloud`                                    | 待建       | 未开始   | 未开始   | 未开始 | 未开始 | 未开始 | 未开始   | 未开始   | 已记录   |
| `correspondence_types`                                   | 待建       | 未开始   | 未开始   | 未开始 | 未开始 | 未开始 | 未开始   | 未开始   | 已记录   |
| `correspondence_rejection_poly`                          | 待建       | 未开始   | 未开始   | 未开始 | 未开始 | 未开始 | 未开始   | 未开始   | 已记录   |
| `transformation_estimation_2D`                           | 待建       | 未开始   | 未开始   | 未开始 | 未开始 | 未开始 | 未开始   | 未开始   | 已记录   |
| `transformation_estimation_svd`                          | 待建       | 未开始   | 未开始   | 未开始 | 未开始 | 未开始 | 未开始   | 未开始   | 已记录   |
| `transformation_estimation_dual_quaternion`              | 待建       | 未开始   | 未开始   | 未开始 | 未开始 | 未开始 | 未开始   | 未开始   | 已记录   |
| `bfgs`                                                   | 待建       | 未开始   | 未开始   | 未开始 | 未开始 | 未开始 | 未开始   | 未开始   | 已记录   |

`transformation_validation_euclidean` 已完成首轮评估与诊断闭环，但板卡结果只证明 transform staging 片段加速，full validation 只有弱收益，因此该主题应保持为 bench 诊断，不进入生产接入队列。`correspondence_estimation_organized_projection` 已完成 production-ready closeout：production RVV 覆盖 source gather / finite / transform staging、projection-pixel staging 和 target-predicate final predicate；append 与 stored distance 写出保留标量。板卡 `board_smoke` 39 个专项测试通过，production identity fake/explicit 为 `1.64x` / `1.65x`，production non-identity fake/explicit 均为 `2.36x`。

`transformation_estimation_point_to_plane_lls` 已从早期 bench 诊断升级为 production-candidate：当前 production RVV 覆盖 `Scalar=float`、ordered-cloud-pair public overload、source xyz / target xyz+normal f32 AoS layout-gated fused-formula block dispatch；三类代表点型 production-dispatch 板卡 5-run median 分别为 `2.80x` / `2.82x`、`3.13x` / `3.15x`、`3.11x` / `3.14x`。source-indexed-cloud-pair、dual-indexed-cloud-pair、correspondence-pair、weighted 和 `Scalar=double` 保持边界外。`transformation_estimation_point_to_plane_lls_weighted` 当前为 bounded production-candidate：ordered-cloud-pair public overload 已接入 production RVV，source-indexed-cloud-pair public overload 为 block-fused production probe；ordered-cloud-pair 262144 三类代表点型 median 为 `2.76x`、`2.98x`、`3.00x`，source-indexed-cloud-pair probe 六个代表 case median 为 `1.54x`~`1.71x`，但 source-indexed-cloud-pair-specific asm、binary identity、taskset metadata 和 262144 长尾 warning 仍未 clean closeout。dual-indexed-cloud-pair、correspondence-pair、`Scalar=double` 和非连续权重保持标量。

`transformation_estimation_symmetric_point_to_plane_lls` 已完成泛型 normal 顺序点云对 production-ready closeout：production RVV 覆盖满足 `x/y/z/normal_x/normal_y/normal_z` 单个 `float` 字段、POD / standard-layout 和 offset alignment gate 的 generic normal 点类型，`Scalar=float`，ordered-cloud-pair public overload。QEMU std/RVV 各 18 个专项测试通过，板卡 RVV test 18 个通过；production-direct `PointNormal` 64K / 256K 为 `2.62x` / `2.47x`，`PointXYZINormal` 为 `2.64x` / `1.89x`。indices、source+target indices、correspondence-pair 和 `Scalar=double` 保持标量；correspondence-pair 诊断仍为 `0.64x` / `0.41x`，退化主因保持待消融假设。
