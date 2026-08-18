# registration 模块 RVV 二轮筛选报告

本文档记录 `registration` 模块从文件级初筛结果转为 RVV 模块实施队列的第二轮筛选结论。第二轮筛选不是重新做全库筛选，而是把 `registration-file-candidate-screening.zh.md` 中 high/mid 候选逐项下钻到公开入口、函数族和主成本覆盖类型，并给出可执行队列。

`registration` 与 `filters` 的主要差异是：很多文件位于 ICP / NDT / GICP / RANSAC / FPCS 等迭代框架中，循环数量多，但主成本常被 search、Eigen solver、SVD、随机采样、候选排序或状态机稀释。因此本轮不按 high/mid 机械排序，而按 RVV 是否覆盖公开入口主成本来分组。

## 1. 输入依据

- 原始文件候选筛选：`doc-rvv/library-screening/modules/registration-file-candidate-screening.zh.md`
- 源码范围：`registration`
- 上游测试：`test/registration/*.cpp`
- 上游 benchmark：当前仓库未发现 `benchmarks/registration/` 目录；registration 主题需要优先建立专项 bench
- 既有 RVV 经验：`common` 的 transform / centroid / norms，`filters` 的 direct-main-path、partial-preprocess、diagnostic / bench-only 路径和保留候选复筛经验
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
| `impl/correspondence_types.hpp`                                   | `getQueryIndices`、`getMatchIndices`、`getCorDistMeanStd`              | `diagnostic`    | index query / match 抽取是 12-byte AoS 跨步读 + 连续写，distance 统计是小型 reduction；适合建立诊断和 production probe 边界，但不直接承诺生产分流                                             |
| `impl/correspondence_rejection_poly.hpp`                          | `getRemainingCorrespondences`、histogram、Otsu threshold               | `diagnostic`    | 已按 diagnostic-first 路线完成 test-rvv 诊断、Phase 050 临时 production-direct replay 和 rollback/no-production closeout；随机采样、staging、histogram / Otsu 和输出 append 稀释局部收益，不进入生产分流 |
| `impl/transformation_estimation_2D.hpp`                           | 2D transformation estimation                                             | `direct-main-path`   | 固定 2D centroid / covariance / transform 公式，有批处理空间；上游覆盖和数据规模需函数级评估确认                                                                                                   |
| `impl/transformation_estimation_svd.hpp`                          | SVD transform estimation                                                 | `partial-preprocess` | centroid / demean / covariance 构造可 RVV，最终 SVD 仍为 Eigen；适合在 LLS 主题后评估前置累加占比                                                                                                  |
| `impl/transformation_estimation_dual_quaternion.hpp`              | dual quaternion transform estimation                                     | `partial-preprocess` | 已完成 production-ready closeout：前三类 row-source policy 的 C1/C2 累加已接入 RVV；correspondence production RVV 因 64K / 256K 负向保留标量。                                                       |
| `bfgs.h`                                                         | BFGS line search / direction update                                      | `diagnostic`    | 已完成 test-rvv 诊断闭环：QEMU Std/RVV correctness 各 6 tests passed，Milkv-Jupiter direction-update 5-run median 为 `0.648x` / `0.740x` 且 Evidence Doctor `Errors=2`；production 不改，`doc-rvv` 不适用 |

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

### 3.4 相对第一轮筛选的主要调整

| 调整类型 | 文件或主题 | 变化理由 |
| -------- | ---------- | -------- |
| high 候选降为保留复筛 | `impl/gicp.hpp`、`impl/ndt.hpp`、`impl/ndt_2d.hpp`、`impl/joint_icp.hpp`、`impl/ia_fpcs.hpp`、`impl/ia_kfpcs.hpp`、`impl/correspondence_estimation.hpp`、`impl/correspondence_estimation_normal_shooting.hpp`、`impl/correspondence_estimation_backprojection.hpp` | 第一轮只看到循环和数学密集片段；第二轮下钻后确认完整入口常被 KdTree / radiusSearch、Eigen solver、随机采样、候选排序或状态机稀释，先保留为后续复筛或诊断主题。 |
| mid 候选提升为建议优化 | `impl/transformation_validation_euclidean.hpp`、`impl/correspondence_estimation_organized_projection.hpp`、`impl/transformation_estimation_point_to_plane_lls.hpp`、`impl/transformation_estimation_point_to_plane_lls_weighted.hpp`、`impl/transformation_estimation_symmetric_point_to_plane_lls.hpp`、`impl/transformation_estimation_2D.hpp` | 这些文件虽然第一轮不是 high，但公开入口或 normal-equation / projection / transform staging 公式更规整，能够建立 production 或 full diagnostic 闭环。 |
| 公开声明头合并 | `icp.h`、`gicp.h`、`ndt.h`、各 `transformation_estimation_*.h`、correspondence 相关 `.h` 文件 | `.h` 多数承载公开 API 声明或薄入口，真实循环在对应实现文件；后续主题应以实现文件和公开入口共同恢复调用链。 |
| 框架 / 组合入口不单独实施 | `registration.hpp`、`default_convergence_criteria.h`、`incremental_registration.h`、`meta_registration.h`、`impl/pairwise_graph_registration.hpp`、`impl/elch.hpp`、`impl/joint_icp.hpp` | 这些文件主要负责调度、状态、图注册或子 registration 组合；可作为具体主题的上下文或复筛对象，不单独承诺 RVV 主路径。 |
| 暂不新增外部文件 | 无 | 本轮没有从 70 个 high/mid 候选之外新增文件；若后续 profile 或源码证据显示 low 文件存在明显漏筛，可在保留候选复筛中补入并说明证据。 |

## 4. 执行清单 / 状态表

### 4.1 建议进行 RVV 优化的文件

| 顺序 | 主题                                                       | 主文件                                                         | 状态   | 当前结论 / 下一步条件 |
| ---: | ---------------------------------------------------------- | -------------------------------------------------------------- | ------ | --------------------- |
|    1 | `transformation_validation_euclidean`                      | `impl/transformation_validation_euclidean.hpp`                 | 已完成 / bench 诊断 | transform staging 片段有收益，但 nearest search 稀释 full validation；保留 bench 诊断证据，不接生产分流。 |
|    2 | `correspondence_estimation_organized_projection`           | `impl/correspondence_estimation_organized_projection.hpp`      | 已完成 / production-ready | 已接入 source transform、projection-pixel 和 target-predicate production RVV；append / stored distance 写出保留标量。 |
|    3 | `transformation_estimation_point_to_plane_lls`             | `impl/transformation_estimation_point_to_plane_lls.hpp`        | 已完成 / production-candidate ordered-cloud-pair f32 AoS | 已接入 `Scalar=float` ordered-cloud-pair public overload 的 source xyz / target xyz+normal f32 AoS layout-gated fused-formula block dispatch；代表点型板卡 5-run median 为 `2.80x`~`3.15x`。source-indexed-cloud-pair、dual-indexed-cloud-pair、correspondence-pair、weighted 和 `Scalar=double` 保持边界外。 |
|    4 | `transformation_estimation_point_to_plane_lls_weighted`    | `impl/transformation_estimation_point_to_plane_lls_weighted.hpp` | 已完成 / bounded production-candidate | ordered-cloud-pair public overload 已接入 production RVV；source-indexed-cloud-pair public overload 为 block-fused production probe bounded candidate。ordered-cloud-pair 262144 代表点型板卡 5-run median 为 `2.76x`~`3.00x`，source-indexed-cloud-pair probe median 为 `1.54x`~`1.71x` 且仍有 warning。dual-indexed-cloud-pair、correspondence-pair、`Scalar=double` 和非连续权重保持标量。 |
|    5 | `transformation_estimation_symmetric_point_to_plane_lls`   | `impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` | 已完成 / production-ready 泛型 normal 顺序点云对 | 已接入 generic normal / `float` / ordered-cloud-pair public overload 的 production RVV；板卡 production-direct `PointNormal` 64K / 256K 为 `2.62x` / `2.47x`，`PointXYZINormal` 为 `2.64x` / `1.89x`。indices 和 correspondence-pair 保持标量，correspondence-pair 诊断为 `0.64x` / `0.41x`。 |
|    6 | `icp_transform_cloud`                                      | `impl/icp.hpp`                                                 | 已完成 / production-direct positive | `IterativeClosestPoint::transformCloud` 已接入 `Scalar=float` AoS layout-gated RVV production path；板卡 production-direct 5-run median 为 `PointXYZ 64K=5.68x`、`PointXYZ 256K=5.30x`、`PointNormal 64K=3.76x`、`PointNormal 256K=3.93x`。不声称 ICP end-to-end speedup。 |
|    7 | `correspondence_types`                                     | `impl/correspondence_types.hpp`                                | 已完成 / rollback/no-production | 已建立 index extraction / distance stats 诊断；Phase 011 临时 production direct probe 后 5-run 板卡 median 为 `0.983x` / `0.966x` / `0.877x`，仍为负向，临时 production patch 已回退。 |
|    8 | `correspondence_rejection_poly`                            | `impl/correspondence_rejection_poly.hpp`                       | 已完成 / rollback/no-production | 已建立 deterministic corpus（确定性样本）和 seeded random stress（固定种子随机压力）测试、edge / gather / acceptance 诊断、Phase 050 临时 production-direct replay；2048 / 8192 correspondences 板卡 5-run median 为 `0.904x` / `0.977x`，均 `5/5` degradation，Evidence Doctor `Errors=2`，生产补丁已回滚。 |
|    9 | `transformation_estimation_2D`                             | `impl/transformation_estimation_2D.hpp`                        | 待评估    | 确认 2D 数据规模、上游测试和 fixed formula reduction 收益。 |
|   10 | `transformation_estimation_svd`                            | `impl/transformation_estimation_svd.hpp`                       | 已完成 / production-ready 四条 row source | 已接入 `Scalar=float`、dense、layout-gated xyz AoS、`use_umeyama_ == true` 的 ordered-cloud-pair、source-indexed-cloud-pair、dual-indices-cloud-pair 和 correspondence-pair production RVV。QEMU Std/RVV correctness 各 22 tests passed，板卡 RVV correctness 22 tests passed；production direct 5-run board median 为 ordered `14.372x` / `24.471x` / `23.841x`，source-indexed `9.634x` / `12.217x` / `11.558x`，dual-indices `6.805x` / `6.404x` / `5.964x`，correspondence `8.649x` / `8.644x` / `7.872x`。`Scalar=double`、non-dense 和 `use_umeyama_ == false` 保持标量 fallback。 |
|   11 | `transformation_estimation_dual_quaternion`                | `impl/transformation_estimation_dual_quaternion.hpp`           | 已完成 / production-ready 三类 row source | 已接入 `Scalar=float`、dense、layout-gated xyz AoS 的 `ordered-cloud-pair`、`source-indexed-cloud-pair` 和 `dual-indexed-cloud-pair` production RVV；`correspondence-pair` production RVV 在 64K / 256K 负向，已移除 dispatch 并保持标量。QEMU correctness：Std `28/28`、RVV `32/32`；retained production direct 5-run board median 为 ordered `3.232x` / `3.644x` / `3.656x`，source-indexed `2.540x` / `2.631x` / `2.586x`，dual-indexed `2.015x` / `1.808x` / `2.021x`，三类 Evidence Doctor 均 `0/0/0`。 |
|   12 | `bfgs`                                                     | `bfgs.h`                                                       | 已完成 / diagnostic_stop_no_production | Phase 000/010/020 已闭合；test-only direction-update QEMU correctness 各 6 tests passed、QEMU doctor `0/0/0`，Milkv-Jupiter 5-run median 为 `0.648x` / `0.740x` 且两个 case 均 `5/5` degradation，board doctor `Errors=2`。production `bfgs.h` 保持标量；若重开，只建议用户明确授权的 bounded negative-analysis、caller hotspot audit 或 production probe。 |

### 4.2 建议优化文件状态矩阵

| 主题                                                       | 函数级评估 | RVV 实现 | 专项测试 | bench  | QEMU   | 反汇编 | 板卡闭环 | 主题文档 | 工作日志 |
| ---------------------------------------------------------- | ---------- | -------- | -------- | ------ | ------ | ------ | -------- | -------- | -------- |
| `transformation_validation_euclidean`                    | 已完成 / bench 诊断 | bench 诊断 | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已记录   |
| `correspondence_estimation_organized_projection`         | 已完成 / production-ready | 已接入 production | 已完成 | 已完成 | 已完成 | 已完成 | 已完成   | 已完成   | 已记录   |
| `transformation_estimation_point_to_plane_lls`           | 已完成 / production-candidate ordered-cloud-pair f32 AoS | 已接入 production-candidate ordered-cloud-pair | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已记录 |
| `transformation_estimation_point_to_plane_lls_weighted`  | 已完成 / bounded production-candidate | 已接入 ordered-cloud-pair；source-indexed-cloud-pair bounded probe | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已记录 |
| `transformation_estimation_symmetric_point_to_plane_lls` | 已完成 / production-ready 泛型 normal 顺序点云对 | 已接入 production | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已完成 | 已记录 |
| `icp_transform_cloud`                                    | 已完成 / production-direct positive | 已接入 production `transformCloud` | 已完成 | 已完成 | 已完成 correctness；QEMU bench compare 禁止 | 已完成 | 已完成 / positive | 已完成 | 已记录 |
| `correspondence_types`                                   | 已完成 / rollback/no-production | 诊断实现；production probe 已回退 | 已完成 | 已完成 | 已完成 | 已完成 / partial attribution | 已完成 / negative | topic-local 已完成；doc-rvv 不适用 | 已记录 |
| `correspondence_rejection_poly`                          | 已完成 / rollback/no-production | 诊断实现；production probe 已回退 | QEMU Std/RVV 各 8 tests passed；board smoke 8 tests passed | QEMU smoke only；board production-direct repeated negative | 已完成 correctness；bench smoke 仅日志形状 | 已生成 bench RVV asm；Phase 050 回滚前可见 RVV / Standard helper | 已完成 / negative | topic-local 已完成；doc-rvv 不适用 | 已记录 |
| `transformation_estimation_2D`                           | 待建       | 未开始   | 未开始   | 未开始 | 未开始 | 未开始 | 未开始   | 未开始   | 已记录   |
| `transformation_estimation_svd`                          | 已完成 / production-ready 四条 row source | 已接入 production ordered / source-indexed / dual-indices / correspondence | 已完成；QEMU Std/RVV 各 22 tests passed，board smoke 22 tests passed | 已完成 production direct 5-run board repeated | 已完成 correctness；QEMU bench 只作日志形状 | 已完成 production public overload 归因 | 已完成 / positive；四组 production direct Evidence Doctor 均 Errors=0 | 已完成；`doc-rvv/registration/transformation_estimation_svd-RVV.zh.md` | 已记录 |
| `transformation_estimation_dual_quaternion`              | 已完成 / production-ready 三类 row source | 已接入 production ordered / source-indexed / dual-indexed；correspondence 保持标量 | 已完成；QEMU Std `28/28`、RVV `32/32`，含 retained production path-hit 和 fallback gate | 已完成 production direct 5-run board repeated | 已完成 correctness；QEMU bench 只作日志形状 | 已生成 retained bench RVV asm；production public overload 可归因 | 已完成 / positive；三类 retained production direct Evidence Doctor 均 `0/0/0`；correspondence production probe 为负向历史证据 | 已完成；`doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md` | 已记录   |
| `bfgs`                                                   | 已完成 / diagnostic_stop_no_production | 未开始；production 不改 | QEMU Std/RVV 各 6 tests passed | QEMU smoke only；board repeated negative | QEMU smoke doctor `0/0/0` | 已生成 bench RVV asm；不是 production hot-symbol attribution | 已完成 / negative；median `0.648x` / `0.740x`，Evidence Doctor `Errors=2` | topic-local scaffold 已建；`doc-rvv` 不适用；本地 stats 已记录 | 已记录 |

`transformation_validation_euclidean` 已完成首轮评估与诊断闭环，但板卡结果只证明 transform staging 片段加速，full validation 只有弱收益，因此该主题应保持为 bench 诊断，不进入生产接入队列。`correspondence_estimation_organized_projection` 已完成 production-ready closeout：production RVV 覆盖 source gather / finite / transform staging、projection-pixel staging 和 target-predicate final predicate；append 与 stored distance 写出保留标量。板卡 `board_smoke` 39 个专项测试通过，production identity fake/explicit 为 `1.64x` / `1.65x`，production non-identity fake/explicit 均为 `2.36x`。

`transformation_estimation_point_to_plane_lls` 已从早期 bench 诊断升级为 production-candidate：当前 production RVV 覆盖 `Scalar=float`、ordered-cloud-pair public overload、source xyz / target xyz+normal f32 AoS layout-gated fused-formula block dispatch；三类代表点型 production-dispatch 板卡 5-run median 分别为 `2.80x` / `2.82x`、`3.13x` / `3.15x`、`3.11x` / `3.14x`。source-indexed-cloud-pair、dual-indexed-cloud-pair、correspondence-pair、weighted 和 `Scalar=double` 保持边界外。`transformation_estimation_point_to_plane_lls_weighted` 当前为 bounded production-candidate：ordered-cloud-pair public overload 已接入 production RVV，source-indexed-cloud-pair public overload 为 block-fused production probe；ordered-cloud-pair 262144 三类代表点型 median 为 `2.76x`、`2.98x`、`3.00x`，source-indexed-cloud-pair probe 六个代表 case median 为 `1.54x`~`1.71x`，但 source-indexed-cloud-pair-specific asm、binary identity、taskset metadata 和 262144 长尾 warning 仍未 clean closeout。dual-indexed-cloud-pair、correspondence-pair、`Scalar=double` 和非连续权重保持标量。

`transformation_estimation_symmetric_point_to_plane_lls` 已完成泛型 normal 顺序点云对 production-ready closeout：production RVV 覆盖满足 `x/y/z/normal_x/normal_y/normal_z` 单个 `float` 字段、POD / standard-layout 和 offset alignment gate 的 generic normal 点类型，`Scalar=float`，ordered-cloud-pair public overload。QEMU std/RVV 各 18 个专项测试通过，板卡 RVV test 18 个通过；production-direct `PointNormal` 64K / 256K 为 `2.62x` / `2.47x`，`PointXYZINormal` 为 `2.64x` / `1.89x`。indices、source+target indices、correspondence-pair 和 `Scalar=double` 保持标量；correspondence-pair 诊断仍为 `0.64x` / `0.41x`，退化主因保持待消融假设。

`icp_transform_cloud` 已完成 production-direct positive closeout：production RVV 覆盖
`IterativeClosestPoint::transformCloud` 的 `Scalar=float`、compatible AoS XYZ / XYZ+normal ordered-cloud-pair path，
并通过 runtime offset gate、small-input gate 和 `transformCloudStandard` helper 保留标量 fallback。QEMU
Std/RVV correctness 各 12 个专项测试通过，板卡 RVV correctness 12 个通过；Milkv-Jupiter 5-run
production-direct repeated median 为 `PointXYZ 64K=5.68x`、`PointXYZ 256K=5.30x`、`PointNormal 64K=3.76x`
和 `PointNormal 256K=3.93x`，Evidence Doctor 为 Errors=0、Warnings=2、Suggestions=0。该结论只覆盖
`transformCloud` ordered-cloud-pair microbench，不声称 ICP end-to-end speedup；长期 production 说明在
`doc-rvv/registration/icp-RVV.zh.md`，topic-local 证据在 `test-rvv/registration/icp/doc/`。

`correspondence_types` 已完成 no-production closeout：test-rvv 资产覆盖 `getQueryIndices` / `getMatchIndices`
index extraction 和 `getCorDistMeanStd` distance stats 诊断，QEMU correctness 与板卡 correctness 均通过。
Phase 010 diagnostic board repeated 的三个 index case median 为 `0.959x` / `0.960x` / `0.987x`；由于 diagnostic
negative 不能直接禁止 production probe，Phase 011 又临时接入真实 production helper 做同边界复核，production
direct board repeated median 为 `0.983x` / `0.966x` / `0.877x`，仍全部低于 `weak_positive`。临时 production
patch 已回退，当前 production 源码保持标量；`doc-rvv` 长期主题文档不适用，当前证据归属在
`test-rvv/registration/correspondence_types/doc/correspondence_types-evaluation.zh.md` 和 phase 文档中。

`transformation_estimation_svd` 已完成四条 row source production-ready closeout：production RVV 覆盖
ordered-cloud-pair、source-indexed-cloud-pair、dual-indices-cloud-pair 和 correspondence-pair 四个 public overload，
共同 gate 为 `Scalar=float`、dense、layout-gated xyz AoS、`use_umeyama_ == true` 和 `n >= 16`。QEMU
Std/RVV correctness 各 22 个专项测试通过，板卡 RVV correctness 22 个通过；production direct 5-run
board median 分别为 ordered `14.372x` / `24.471x` / `23.841x`，source-indexed `9.634x` / `12.217x` /
`11.558x`，dual-indices `6.805x` / `6.404x` / `5.964x`，correspondence `8.649x` / `8.644x` / `7.872x`。
四组 Evidence Doctor 均为 Errors=0；ordered 4K group outlier 以及 dual-indices / correspondence 256K
long-tail warning 已按 size 分开解释。`Scalar=double`、non-dense 输入和 `use_umeyama_ == false` 继续走标量
fallback；长期 production 说明在 `doc-rvv/registration/transformation_estimation_svd-RVV.zh.md`。

`transformation_estimation_dual_quaternion` 已完成三类 row source production-ready closeout：production RVV
覆盖 `Scalar=float`、dense、layout-gated xyz AoS 的 ordered-cloud-pair、source-indexed-cloud-pair 和
dual-indexed-cloud-pair 三个 public overload，RVV 只接管 C1/C2 前置累加，Eigen 4x4 solve 和
quaternion 到 transformation matrix 的后段保持标量。QEMU correctness 为 Std `28/28`、RVV `32/32`；
Milkv-Jupiter production direct 5-run median 分别为 ordered `3.232x` / `3.644x` / `3.656x`，
source-indexed `2.540x` / `2.631x` / `2.586x`，dual-indexed `2.015x` / `1.808x` / `2.021x`，
三类 Evidence Doctor 均为 Errors=0、Warnings=0、Suggestions=0。correspondence-pair 的 production RVV
probe 在 64K / 256K 为负向，已移除 production dispatch，公开入口继续使用原 iterator 标量路径；长期
production 说明在 `doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md`。

`correspondence_rejection_poly` 已完成 no-production closeout：test-rvv 资产覆盖
`getRemainingCorrespondences` 的 public entry guard（公开入口保护）、random sampling（随机采样）、
`thresholdPolygon` / `thresholdEdgeLength`、accept rate、histogram / Otsu 和输出保序语义。测试数据同时包含
deterministic corpus（确定性样本）和 seeded random stress（固定种子随机压力样本）；QEMU Std/RVV correctness
各 8 个测试通过，board correctness 8 个测试通过。局部 `edge_batch`、`edge_gather_staging` 和
`acceptance_filter` 诊断不能替代真实 production-direct；Phase 050 临时恢复 Standard / RVV 分层后重跑
production-direct，2048 / 8192 correspondences 板卡 5-run median 为 `0.904x` / `0.977x`，两组均 `5/5`
degradation，Evidence Doctor 为 Errors=2、Warnings=0、Suggestions=0。用户已确认不接入，临时 production
patch 已回滚，当前 production 源码保持标量；`doc-rvv` 长期主题文档不适用，当前证据归属在
`test-rvv/registration/correspondence_rejection_poly/doc/correspondence_rejection_poly-evaluation.zh.md`、Phase 050
result 和 evidence registry 中。
