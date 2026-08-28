# segmentation 模块 RVV 第一轮文件级筛选报告

本文档记录 `segmentation` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV（单指令多数据 / RISC-V Vector）片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 源码范围：`segmentation/**`
- 源码后缀：`.h`、`.hpp`、`.c`、`.cc`、`.cpp`、`.cu`
- 覆盖结果：源码文件总数 `76`，已判定 `76`；其中 `include` 目录 `56` 个，`src` 目录 `20` 个。
- 排除口径：本模块未发现 `3rdparty/**` 源码；测试、文档和非主库路径不在本表范围内。
- 重做口径：旧的 `segmentation-file-candidate-screening.zh.md` 仅作为 previous baseline（上一版基线）和覆盖检查参考；本轮结论以当前源码、`rvv-screening` 筛选标准和文件候选筛选模板为准。
- 路径显示：`include` 文件省略公共前缀 `segmentation/include/pcl/segmentation/`；`src` 文件以 `src/` 开头显示。

## 2. 第一轮筛选口径

- 第一轮是文件级粗筛，判断标准是文件内是否存在可向量化循环、数学密集片段、批量字段访问、规约、mask（掩码）/ 压缩、organized（有组织点云，按行列存储）遍历或可诊断的局部 SIMD/RVV 点。
- `high` / `mid` 是第二轮必须复核并交代去向的初始候选基线，不是最终实施全集。
- `low` 表示本轮未发现足以进入二轮基线的证据，不是永久排除；如果第二轮源码下钻发现明显漏判，可以补入并说明证据。
- 第一轮不承诺 RVV 覆盖公开入口主成本；主成本覆盖、测试可行性、fallback（回退路径）条件和维护风险由第二轮筛选继续判断。
- 循环数量、数学项数量或关键词命中只能作为扫描线索，不能单独支撑 `high` / `mid`。候选必须定位到可复核的入口、loop/helper（循环 / 辅助函数）、trip count（循环次数来源）、RVV 适配点和主要风险。

评估维度：

| 维度 | 第一轮判断口径 |
| ---- | -------------- |
| 循环规模 | 是否随点数、像素数、邻域数、label 数、supervoxel 数、GMM component 数或图节点 / 边数量增长 |
| 算术密度 | 是否包含点积、距离、颜色距离、plane predicate（平面判定）、min/max、sum、概率密度、`exp` / `sqrt` 或小矩阵公式 |
| 访存模式 | 是否存在连续点云、固定字段 AoS（结构数组）、organized 行列、indices gather（按索引离散加载）、临时数组或矩阵块 |
| 分支复杂度 | 是否主要是简单谓词 / mask，还是 graph/search（图 / 搜索）、递归、queue、map/set、RANSAC 或 max-flow 状态机主导 |
| 语义风险 | 是否涉及输出顺序、label 合并、union-find、非结合浮点规约、NaN/Inf、点类型字段或外部 solver 语义 |
| 可验证性 | 是否能构造标量 / RVV 对拍、边界 case、fallback case、QEMU 正确性和板卡性能证据 |

优先级含义：

| 优先级 | 含义 |
| ------ | ---- |
| high | 文件内至少有一个强批量 loop 或函数族，满足生产价值、并行合法性、RVV 访存匹配中的两个以上强信号，且没有未解释的语义或测试硬伤 |
| mid | 文件内存在可 SIMD/RVV 片段，但主成本、数据布局、indices/search 稀释、浮点语义、入口覆盖或测试可行性仍需第二轮确认 |
| low | 以声明、薄 wrapper、显式实例化、调度、类型胶水、小规模固定计算、不规则容器/search/solver/状态机或非热点路径为主 |

## 3. 第一轮筛选统计

| 项目 | 数量 | 说明 |
| ---- | ---: | ---- |
| 源码文件总数 | 76 | `segmentation/**` 源码文件；无第三方实现进入候选主线 |
| 已判定文件数 | 76 | `76/76` 全覆盖 |
| high | 6 | 二轮必查 |
| mid | 23 | 二轮必查 |
| low | 47 | 已覆盖但不进入二轮初始基线 |
| high + mid | 29 | 第一轮候选基线 |
| 候选占比 | 29/76 = 38.2% | high + mid / 源码文件总数 |

与旧版 previous baseline 对比：旧版为 `high=5`、`mid=35`、`low=36`、`high+mid=40`。本轮主要变化是：声明头和显式实例化 `src/*.cpp` 不再因注释、函数名或数学关键词进入 `mid`；被 organized segmentation 主循环反复调用的 comparator（比较器）头文件从 `low` 上调为 `mid`；graph/search/RANSAC/solver 主导但包含局部批量数学的文件多保持 `mid`，等待第二轮判断主成本覆盖。

## 4. high 候选

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| ---- | --------------- | --------------- | ---------- | -------- | -------------- |
| `impl/approximate_progressive_morphological_filter.hpp` | `ApproximateProgressiveMorphologicalFilter<PointT>::extract` 中点云到栅格 z-min、窗口 min/max open、ground 差值筛选 | `input_->size()`、`rows * cols`、`window_sizes.size()`、窗口面积 | 连续点云字段读取、grid min/max 规约、阈值 mask、输出 indices 压缩 | 现有 OpenMP 并行、Eigen `MatrixXf` stride、NaN 判定和窗口边界语义需要复核 | 公开入口主循环直接处理点数和栅格规模，批量 min/max 与阈值筛选清晰，适合作为二轮直接候选 |
| `impl/extract_polygonal_prism_data.hpp` | `ExtractPolygonalPrismData<PointT>::segment`、`isXYPointIn2DXYPolygon` | `indices_->size()`、`projected_points.size()`、polygon 顶点数、concave hull polygon 数 | 点到平面距离、投影字段读取、height mask、2D polygon predicate、保序压缩输出 indices | polygon parity（奇偶判定）分支、concave hull 多 polygon、输出顺序和边界点语义 | 主成本是逐点几何谓词和输出选择，RVV mask / compress 适配点明确，二轮应优先确认生产入口覆盖 |
| `impl/grabcut_segmentation.hpp` | `GrabCut<PointT>::initCompute`、`computeBetaOrganized`、`computeNLinksOrganized`、`computeBetaNonOrganized`、`extract` | `input_->size()`、`width * height`、`indices_->size()`、`nb_neighbours_` | RGB/RGBA 转 `Color`、邻接 color distance、beta reduction、n-link weight 的 `exp` / `sqrt`、foreground/background 输出 mask | organized 与 non-organized 两条入口不同；KNN 路径有 search 成本；graph solve 在 `src/grabcut_segmentation.cpp` | 模板入口中存在清晰的图像式和 indices 批量预处理，且直接喂给 GrabCut 主流程，值得二轮与 `src` 核心一起复核 |
| `impl/organized_connected_component_segmentation.hpp` | `OrganizedConnectedComponentSegmentation::segment`、`findLabeledRegionBoundary` | `input_->width * input_->height`、`run_ids.size()`、label remap 的 `input_->size()` | organized 行列遍历、finite mask、左右 / 上方邻接比较、label remap、`label_indices` 输出填充 | union-find label 合并有 loop-carried state；`compare_->compare` 是虚调用且具体 comparator 语义外置 | 公开入口就是 organized connected component 标记，主循环规模和访存形态明确，第二轮应判断哪些 pass 可拆成 RVV diagnostic |
| `impl/organized_multi_plane_segmentation.hpp` | `segment` 中 `plane_d` 计算、cluster plane fitting、`refine` 双向 organized pass、`projectToPlaneFromViewpoint` | `input_->size()`、`label_indices.size()`、`inlier_indices`、`labels->width * labels->height` | plane `d` 点积、投影、refinement 邻接 predicate、label/inlier append | `computeMeanAndCovarianceMatrix`、`eigen33` 每 region 执行；refinement 修改 labels，有状态依赖 | 文件把 plane predicate、connected component 和 refinement 串成公开主路径，批量 organized 片段足够强 |
| `src/grabcut_segmentation.cpp` | `buildGMMs`、`learnGMMs`、`GMM::probabilityDensity`、`BoykovKolmogorov::solve` 相关图循环 | `indices.size()`、GMM component 数、graph nodes / edges、active set 大小 | GMM 颜色累加、概率密度小公式、component assignment、部分 edge capacity 批量更新 | Boykov-Kolmogorov max-flow 使用 `map`、deque、parent/orphan 状态机；SVD 和浮点概率语义需隔离 | 非模板核心文件包含 GrabCut GMM 与 graph cut 主循环，局部数学密集但状态复杂，应进入二轮与模板入口联合下钻 |

## 5. mid 候选

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| ---- | --------------- | --------------- | ---------- | -------- | -------------- |
| `edge_aware_plane_comparator.h` | `EdgeAwarePlaneComparator::compare` | 被 organized connected component 邻接比较按像素边调用 | depth-scaled threshold、xyz 差、`sqrt` distance、normal dot、curvature / plane-d mask | 无外层 loop，收益依赖调用方；虚调用和 distance map 边界需复核 | 单次 helper 数学密集，是 organized plane segmentation 的热点 predicate 候选 |
| `euclidean_cluster_comparator.h` | `EuclideanClusterComparator::compare` | organized 邻接比较次数，通常近似像素边数 | label exclude mask、depth-scaled threshold、xyz norm | `std::set` label exclude 和 depth 分支可能稀释；无独立入口 | comparator 本身无批量循环，但在 organized segmentation 中可被大量调用，进入二轮作为 helper 候选 |
| `euclidean_plane_coefficient_comparator.h` | `EuclideanPlaneCoefficientComparator::compare` | organized 邻接比较次数 | xyz 差、`sqrt` distance、normal dot | 无独立 loop；阈值和 normal 输入来自调用方 | 每邻接边执行同构几何谓词，适合二轮确认是否能和 organized pass 融合 |
| `extract_clusters.h` | normal-aware `extractEuclideanClusters` inline overloads | `cloud.size()` 或 `indices.size()`、seed queue、radiusSearch 返回邻域数 | normal dot 与 `cos_eps_angle` mask、cluster output copy | radiusSearch 和 BFS queue 主导；输出顺序需要保持 | 公共头内有真实实现，不是纯声明；局部 neighbor validation 可向量化但 search 成本显著 |
| `ground_plane_comparator.h` | `GroundPlaneComparator::compare` | organized 邻接比较次数 | normal 与 road axis / neighbor normal 的 dot predicate | 无外层 loop；只作为 comparator helper | 两个 dot predicate 在 organized 主循环中重复执行，保留二轮 helper 复核 |
| `impl/conditional_euclidean_clustering.hpp` | `ConditionalEuclideanClustering::segment` | `indices_->size()`、seed queue、radiusSearch 邻域数 | processed mask、用户 condition 前后的邻域过滤、cluster output copy | 用户提供 `condition_function_` 不可向量化；radiusSearch 和 BFS state 主导 | 存在批量邻域处理，但主成本和语义由 search / callback 决定，降为 `mid` |
| `impl/cpc_segmentation.hpp` | `CPCSegmentation::applyCuttingPlane`、`WeightedRandomSampleConsensus::computeModel` | graph edges、segment edge points、RANSAC iterations、inliers | edge centroid / normal 计算、weights、plane dot score accumulation | Boost graph、RANSAC 随机采样、cluster extraction 和 edge mutation 主导 | 局部公式密集但实现边界复杂，第二轮应先确认是否仅做 component ablation（组件消融） |
| `impl/crf_segmentation.hpp` | `createDataVectorFromVoxelGrid`、`createUnaryPotentials`、`segmentPoints` output map | filtered cloud size、`N * n_labels`、normal size | voxel grid coordinate staging、RGB unpack、unary energy fill、output label copy | Dense CRF 外部推理主导；存在大量 debug output 和 commented legacy path | 有明确批量 staging / unary 初始化，但主成本覆盖不确定 |
| `impl/extract_clusters.hpp` | `extractEuclideanClusters` 两个 overload、`EuclideanClusterExtraction::extract` | `cloud.size()` / `indices.size()`、seed queue、radiusSearch 邻域数 | processed mask、indices gather、cluster copy/sort 前 staging | radiusSearch、BFS queue、sort 主导；输出稳定顺序不能改变 | clustering 主循环有可 SIMD 片段，但整体更像 search/state 路径 |
| `impl/extract_labeled_clusters.hpp` | `extractLabeledEuclideanClusters`、`LabeledEuclideanClusterExtraction::extract` | `cloud.size()`、seed queue、radiusSearch 邻域数、label buckets | label equality mask、cluster copy、labeled bucket append | search/BFS 和 per-label output order 主导 | 相比普通 cluster 多 label predicate，适合作为中优先级复核 |
| `impl/lccp_segmentation.hpp` | `mergeSmallSegments`、`prepareSegmentation`、`doGrouping`、`applyKconvexity`、`connIsConvex` | supervoxel 数、graph edges、neighbor edges、segments | centroid/normal dot-cross-angle、edge validity mask、segment size scans | Boost graph、recursive growing、map/set mutation 和 `std::exp` threshold 语义复杂 | 几何 helper 很密集，但主流程是 graph segmentation，先保留为 `mid` |
| `impl/min_cut_segmentation.hpp` | `buildGraph`、`calculateUnaryPotential`、`calculateBinaryPotential` | `indices_->size()`、foreground points、KNN 邻域数 | source/sink unary weights、foreground distance min、binary potential formula | KNN search、Boost max-flow、graph edge mutation主导 | 批量 potential 计算明确，但生产收益可能被 graph solver 稀释 |
| `impl/progressive_morphological_filter.hpp` | `ProgressiveMorphologicalFilter::extract` | `window_sizes.size()`、`ground.size()` | per-point height difference threshold、indices 压缩 | `applyMorphologicalOperator` 外部调用是每轮主成本；只剩后处理筛选 | 有可向量化尾段筛选，但主成本覆盖弱于 approximate 版本 |
| `impl/random_walker.hpp` | graph Laplacian assembly、`solveLinearSystem`、`assignColors`、`getPotentials` | graph vertices / edges、seed 数、matrix rows / cols | sparse triplet fill、column solve loop、row max label assignment | Eigen sparse Cholesky solver 主导；graph traversal 和 bimap 查找不规则 | 存在批量矩阵和 row max 片段，但整体应先确认 solver 边界 |
| `impl/region_growing.hpp` | `findPointNeighbours`、`applySmoothRegionGrowingAlgorithm`、`growRegion`、`validatePoint`、colored cloud output | `indices_->size()`、neighbor count、segment 数、cluster sizes | normal dot、curvature/residual predicate、label fill、color output loops | KNN search、seed queue、sorted residuals 和 region growing state 主导 | 有 per-neighbor 数学 predicate，但主成本和输出依赖 search/state |
| `impl/region_growing_rgb.hpp` | `findPointNeighbours`、`findSegmentNeighbours`、`applyRegionMergingAlgorithm`、`validatePoint` | `indices_->size()`、segment 数、region_neighbour_number、cluster sizes | RGB color difference、normal/residual/distance predicate、region color accumulation | 多层 region merge、search、state mutation 和输出组装复杂 | RGB/normal predicate 密集，但不适合第一轮直接升 high |
| `impl/seeded_hue_segmentation.hpp` | `seededHueSegmentation` RGB/RGBA overloads | `indices_in.indices.size()`、seed queue、radiusSearch 邻域数 | RGB/RGBA to HSV、hue threshold mask、indices output copy | radiusSearch 和 seed queue 主导；最终 sort 影响输出顺序 | 局部 hue predicate 清晰，但 search/state 路径占主成本 |
| `impl/segment_differences.hpp` | `getPointCloudDifference`、`SegmentDifferences::segment` | `src.size()` | finite mask、nearest distance threshold、indices 压缩 | one-NN search 主导，RVV 只覆盖后处理 | 简单阈值筛选适合诊断，但不宜作为 high |
| `impl/supervoxel_clustering.hpp` | `selectInitialSupervoxelSeeds`、`SupervoxelHelper::expand/refineNormals/updateCentroid`、`voxelDataDistance`、adjacency list build | voxel centroids、seed 数、leaf neighbors、supervoxel helpers、adjacency edges | color/normal/spatial distance、centroid normal/rgb/xyz reductions、neighbor update predicates | octree、KdTree、set/list mutation、owner stealing、normal estimation 主导 | 有多个数学密集 helper，但访存和状态高度不规则，降为 `mid` |
| `impl/unary_classifier.hpp` | `queryFeatureDistances`、`assignLabels`、`train`、`segment` | trained feature rows、33-bin FPFH histogram、query feature size、output size | histogram copy/staging、distance result threshold、label assignment | FPFH、Kmeans 和 FLANN 搜索主导；手写 RVV 只能覆盖 staging/assignment | 局部 33-bin 和 label loops 明确，但主成本在外部算法 |
| `plane_coefficient_comparator.h` | `PlaneCoefficientComparator::compare` | organized 邻接比较次数 | depth-scaled plane-d threshold、normal dot predicate | 无外层 loop；plane coefficients 和 normal inputs 来自调用方 | organized plane segmentation 中的核心 comparator，旧版漏判为 `low` |
| `plane_refinement_comparator.h` | `PlaneRefinementComparator::compare` | refinement organized pass 的邻接比较次数 | label gate、point-to-plane distance、depth-scaled threshold | label/refine state 和 model lookup 不规则；无独立入口 | 与 `OrganizedMultiPlaneSegmentation::refine` 绑定，作为 helper 进入二轮 |
| `rgb_plane_coefficient_comparator.h` | `RGBPlaneCoefficientComparator::compare` | organized 邻接比较次数 | xyz distance、normal dot、RGB squared distance mask | 无独立 loop；`sqrt` 和 color threshold 语义需对拍 | per-neighbor RGB plane predicate 同构，适合作为 fused comparator 复核对象 |

## 6. 全量文件覆盖表

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| ---- | ------ | -------- | -------- | -------- |
| `approximate_progressive_morphological_filter.h` | low | 否 | public class 声明和参数接口为主，真实批量 grid / threshold loop 在实现头 | `impl/approximate_progressive_morphological_filter.hpp` |
| `comparator.h` | low | 否 | 抽象 comparator 基类，仅保存 input cloud 和纯虚 `compare` 合同 | comparator 派生类 |
| `conditional_euclidean_clustering.h` | low | 否 | public API、参数、callback 类型和状态字段为主 | `impl/conditional_euclidean_clustering.hpp` |
| `cpc_segmentation.h` | low | 否 | CPC public class 与 weighted RANSAC 类型声明为主，少量 inline weight filter 不构成独立候选 | `impl/cpc_segmentation.hpp` |
| `crf_normal_segmentation.h` | low | 否 | 轻量派生类声明，未承载批量 CRF 实现 | `impl/crf_normal_segmentation.hpp` |
| `crf_segmentation.h` | low | 否 | CRF 参数和成员声明为主，批量数据构造在实现头 | `impl/crf_segmentation.hpp` |
| `edge_aware_plane_comparator.h` | mid | 是 | inline `compare` 对每个 organized 邻接边执行 depth threshold、xyz distance、normal dot、curvature 和 plane-d predicate | `impl/organized_connected_component_segmentation.hpp` |
| `euclidean_cluster_comparator.h` | mid | 是 | inline `compare` 对邻接点执行 label mask、depth-scaled distance 和 xyz norm | `impl/organized_connected_component_segmentation.hpp` |
| `euclidean_plane_coefficient_comparator.h` | mid | 是 | inline `compare` 执行 xyz distance 与 normal dot，适合作为 organized helper 下钻 | `impl/organized_connected_component_segmentation.hpp` |
| `extract_clusters.h` | mid | 是 | public 头内包含 normal-aware Euclidean cluster 实现，循环遍历 cloud/indices、radiusSearch 邻域和 normal dot predicate | `impl/extract_clusters.hpp` |
| `extract_labeled_clusters.h` | low | 否 | 声明和 wrapper class 为主，真实 labeled BFS cluster 在实现头 | `impl/extract_labeled_clusters.hpp` |
| `extract_polygonal_prism_data.h` | low | 否 | public API 和参数字段为主，点到平面 / polygon 批量判断在实现头 | `impl/extract_polygonal_prism_data.hpp` |
| `grabcut_segmentation.h` | low | 否 | GrabCut、GMM、graph 类型声明为主，核心 loop 在实现头和 `src/grabcut_segmentation.cpp` | `impl/grabcut_segmentation.hpp`, `src/grabcut_segmentation.cpp` |
| `ground_plane_comparator.h` | mid | 是 | inline `compare` 执行 ground normal 与 neighbor normal dot predicate，在 organized loop 中热调用 | `impl/organized_connected_component_segmentation.hpp` |
| `impl/approximate_progressive_morphological_filter.hpp` | high | 是 | `extract` 包含点云到 grid z-min、窗口 min/max 和 ground mask/indices 筛选，规模随点数、grid 和窗口增长 | - |
| `impl/conditional_euclidean_clustering.hpp` | mid | 是 | `segment` 遍历 indices、seed queue 和 radiusSearch 邻域，callback predicate 限制独立 RVV 价值 | - |
| `impl/cpc_segmentation.hpp` | mid | 是 | CPC graph edges、weighted RANSAC score 和 plane dot loops 有局部数学，但 graph/RANSAC 主导 | - |
| `impl/crf_normal_segmentation.hpp` | low | 否 | 仅 include 对应声明头，未承载独立实现 | - |
| `impl/crf_segmentation.hpp` | mid | 是 | filtered cloud staging、unary potentials 和 output label map 有批量 loop，Dense CRF 推理主导 | - |
| `impl/extract_clusters.hpp` | mid | 是 | Euclidean cluster BFS、processed mask、radiusSearch 邻域和 cluster copy/sort，search/state 主导 | - |
| `impl/extract_labeled_clusters.hpp` | mid | 是 | labeled Euclidean cluster BFS，增加 label equality predicate 和 per-label output bucket | - |
| `impl/extract_polygonal_prism_data.hpp` | high | 是 | `segment` 中逐点 height / polygon predicate 与 indices compress 是直接主路径 | - |
| `impl/grabcut_segmentation.hpp` | high | 是 | GrabCut 模板入口包含 organized / non-organized beta、n-link weight 和 color staging 批量 loop | - |
| `impl/lccp_segmentation.hpp` | mid | 是 | supervoxel graph grouping、edge convexity dot/cross/angle 和 segment merge loops，graph state 主导 | - |
| `impl/min_cut_segmentation.hpp` | mid | 是 | graph build 中 unary/binary potential 计算随点数和 KNN 邻域增长，但 max-flow/search 主导 | - |
| `impl/organized_connected_component_segmentation.hpp` | high | 是 | organized row/col connected component pass、label remap 和 boundary tracing 是公开入口主逻辑 | - |
| `impl/organized_multi_plane_segmentation.hpp` | high | 是 | plane-d dot、organized component segmentation、region plane fit 和 refinement pass 串联主路径 | - |
| `impl/planar_polygon_fusion.hpp` | low | 否 | 仅显式实例化宏，无批量实现 | - |
| `impl/progressive_morphological_filter.hpp` | mid | 是 | progressive filter 后的 per-point height threshold 和 indices 压缩清晰，但 `applyMorphologicalOperator` 主导 | - |
| `impl/random_walker.hpp` | mid | 是 | sparse matrix assembly、column solve loop 和 row max assignment 存在批量片段，Eigen solver 主导 | - |
| `impl/region_growing.hpp` | mid | 是 | KNN 邻域、normal / curvature / residual predicate、label fill 和 colored output loops，search/state 主导 | - |
| `impl/region_growing_rgb.hpp` | mid | 是 | RGB color difference、normal/residual/distance predicate 和 region merge loops，状态复杂 | - |
| `impl/sac_segmentation.hpp` | low | 否 | 主要选择 sample_consensus model、调用 `computeModel` / `selectWithinDistance`，真实热点在 `sample_consensus` 模块 | - |
| `impl/seeded_hue_segmentation.hpp` | mid | 是 | seeded hue BFS 中 HSV 转换和 hue threshold predicate 清晰，radiusSearch / queue 主导 | - |
| `impl/segment_differences.hpp` | mid | 是 | `getPointCloudDifference` 逐点 nearestK 后做 distance threshold 和 output indices，search 主导 | - |
| `impl/supervoxel_clustering.hpp` | mid | 是 | supervoxel distance、centroid reductions、leaf expansion 和 adjacency build 有局部批量数学，octree/set/list state 主导 | - |
| `impl/unary_classifier.hpp` | mid | 是 | FPFH histogram staging、FLANN query output 和 label assignment loops 明确，但 FPFH/Kmeans/FLANN 主导 | - |
| `lccp_segmentation.h` | low | 否 | public class、graph typedef 和参数声明为主 | `impl/lccp_segmentation.hpp` |
| `min_cut_segmentation.h` | low | 否 | public API、graph typedef 和参数字段为主 | `impl/min_cut_segmentation.hpp` |
| `organized_connected_component_segmentation.h` | low | 否 | public class 声明和 `findRoot` 小 helper，真实 row/col pass 在实现头 | `impl/organized_connected_component_segmentation.hpp` |
| `organized_multi_plane_segmentation.h` | low | 否 | public API 和参数声明为主 | `impl/organized_multi_plane_segmentation.hpp` |
| `planar_polygon_fusion.h` | low | 否 | 只有 small vector append wrapper，未形成明显 RVV 主题 | `impl/planar_polygon_fusion.hpp` |
| `planar_region.h` | low | 否 | `PlanarRegion` 数据结构和访问器为主 | - |
| `plane_coefficient_comparator.h` | mid | 是 | inline `compare` 执行 plane-d threshold、depth scaling 和 normal dot，在 organized loop 中热调用 | `impl/organized_connected_component_segmentation.hpp` |
| `plane_refinement_comparator.h` | mid | 是 | inline `compare` 执行 label gate、model lookup 和 point-to-plane distance，用于 refinement pass | `impl/organized_multi_plane_segmentation.hpp` |
| `progressive_morphological_filter.h` | low | 否 | public class 声明和参数接口为主 | `impl/progressive_morphological_filter.hpp` |
| `random_walker.h` | low | 否 | template API、graph typedef 和函数声明为主 | `impl/random_walker.hpp` |
| `region_3d.h` | low | 否 | region 数据结构和 small accessor 为主 | - |
| `region_growing.h` | low | 否 | RegionGrowing public API 和参数字段为主 | `impl/region_growing.hpp` |
| `region_growing_rgb.h` | low | 否 | RGB region growing public API 和参数字段为主 | `impl/region_growing_rgb.hpp` |
| `rgb_plane_coefficient_comparator.h` | mid | 是 | inline `compare` 执行 xyz distance、normal dot 和 RGB squared distance | `impl/organized_connected_component_segmentation.hpp` |
| `sac_segmentation.h` | low | 否 | SAC wrapper public API 和 model selection 参数为主，热点在 sample_consensus | `impl/sac_segmentation.hpp` |
| `seeded_hue_segmentation.h` | low | 否 | seeded hue function declarations 为主 | `impl/seeded_hue_segmentation.hpp` |
| `segment_differences.h` | low | 否 | public wrapper 声明和参数字段为主 | `impl/segment_differences.hpp` |
| `supervoxel_clustering.h` | low | 否 | Supervoxel public API、types 和状态字段为主，批量逻辑在实现头 | `impl/supervoxel_clustering.hpp` |
| `unary_classifier.h` | low | 否 | UnaryClassifier public API、参数和训练数据字段为主 | `impl/unary_classifier.hpp` |
| `src/approximate_progressive_morphological_filter.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/approximate_progressive_morphological_filter.hpp` |
| `src/conditional_euclidean_clustering.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/conditional_euclidean_clustering.hpp` |
| `src/cpc_segmentation.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/cpc_segmentation.hpp` |
| `src/crf_segmentation.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/crf_segmentation.hpp` |
| `src/extract_clusters.cpp` | low | 否 | 显式实例化 `extractEuclideanClusters` / labeled cluster，无独立实现 | `impl/extract_clusters.hpp`, `impl/extract_labeled_clusters.hpp` |
| `src/extract_polygonal_prism_data.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/extract_polygonal_prism_data.hpp` |
| `src/grabcut_segmentation.cpp` | high | 是 | GMM fitting / learning、probability density 和 max-flow graph loop 构成 GrabCut 非模板核心 | `impl/grabcut_segmentation.hpp` |
| `src/lccp_segmentation.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/lccp_segmentation.hpp` |
| `src/min_cut_segmentation.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/min_cut_segmentation.hpp` |
| `src/organized_connected_component_segmentation.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/organized_connected_component_segmentation.hpp` |
| `src/organized_multi_plane_segmentation.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/organized_multi_plane_segmentation.hpp` |
| `src/planar_polygon_fusion.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/planar_polygon_fusion.hpp` |
| `src/progressive_morphological_filter.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/progressive_morphological_filter.hpp` |
| `src/region_growing.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/region_growing.hpp` |
| `src/region_growing_rgb.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/region_growing_rgb.hpp` |
| `src/sac_segmentation.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/sac_segmentation.hpp` |
| `src/seeded_hue_segmentation.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/seeded_hue_segmentation.hpp` |
| `src/segment_differences.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/segment_differences.hpp` |
| `src/supervoxel_clustering.cpp` | low | 否 | 显式特化 / 实例化和 octree adjacency 容器 glue 为主，批量算法在实现头 | `impl/supervoxel_clustering.hpp` |
| `src/unary_classifier.cpp` | low | 否 | 显式实例化编译单元，无独立 loop | `impl/unary_classifier.hpp` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮建议先合并同一主题的 companion 文件：`impl/grabcut_segmentation.hpp` 与 `src/grabcut_segmentation.cpp` 应作为同一 GrabCut 主题复核；organized comparator 头文件应与 `impl/organized_connected_component_segmentation.hpp` / `impl/organized_multi_plane_segmentation.hpp` 一起判断，而不是拆成独立实施队列。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块 / 全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/segmentation/segmentation-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
- 本轮只重写第一轮筛选文档；未修改 production 源码，未创建 `test-rvv` topic，未运行 bench（性能测试），未提交。
