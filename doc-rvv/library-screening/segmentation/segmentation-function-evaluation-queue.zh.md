# segmentation 模块 RVV 第二轮函数评估队列

本文档把 `doc-rvv/library-screening/modules/segmentation-file-candidate-screening.zh.md` 中的 29 个 `high/mid` 候选，按公开入口、核心函数族、主成本覆盖类型、trip count 来源、RVV 适配点、测试可行性和维护风险，下钻成 segmentation 模块的第二轮函数评估队列。

本轮只做筛选，不重跑第一轮，不修改 production 源码，不建立 `test-rvv` topic，不跑板卡 bench，不提交。

## 1. 输入依据

- 第一轮文件候选筛选：`doc-rvv/library-screening/modules/segmentation-file-candidate-screening.zh.md`
- 筛选标准：`.agents/skills/rvv-screening/references/screening-criteria.md`
- 阶段与队列规则：`.agents/skills/rvv-screening/references/stage-and-queue-policy.md`
- 证据边界：`.agents/skills/rvv-screening/references/evidence-boundaries.md`
- 函数评估队列模板：`.agents/skills/rvv-screening/references/templates/function-evaluation-queue-template.md`
- segmentation 测试入口：`test/segmentation/CMakeLists.txt`、`test/segmentation/test_segmentation.cpp`、`test/segmentation/test_concave_prism.cpp`、`test/segmentation/test_random_walker.cpp`
- 示例 / 工具入口：`tools/progressive_morphological_filter.cpp`、`examples/segmentation/example_supervoxels.cpp`、`examples/segmentation/example_lccp_segmentation.cpp`、`examples/segmentation/example_cpc_segmentation.cpp`、`examples/segmentation/example_region_growing.cpp`、`examples/segmentation/example_extract_clusters_normals.cpp`、`tools/unary_classifier_segment.cpp`、`tools/train_unary_classifier.cpp`
- 路径显示：候选表沿用第一轮短路径，省略 `segmentation/include/pcl/segmentation/` 公共前缀；`src/*` 保留 `segmentation/src/` 下的相对路径。

## 2. 二轮筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 第一轮源码文件总数 | 76 | `segmentation/**` 源码后缀文件。 |
| 第一轮 high | 6 | 全部进入第二轮必查基线。 |
| 第一轮 mid | 23 | 全部进入第二轮必查基线。 |
| 第一轮 low | 47 | 本轮未发现必须补入的明显漏判项。 |
| 第二轮初始候选基线 | 29 | high + mid 去重后数量。 |
| 新增补充候选 | 0 | 未从 low 或候选外新增文件。 |
| 第二轮候选总数 | 29 | 仍为第一轮 high/mid 基线。 |
| 建议进行 RVV 优化的文件 | 2 | 公开入口主路径清楚，RVV 片段覆盖主成本，测试或工具入口可构造。 |
| 保留实施的候选文件 | 13 | 有明确 RVV 片段，但需先解决 graph/search/solver/state、测试样本、profile 或 component ablation 问题。 |
| 暂缓或不推荐考虑 RVV 优化的文件 | 14 | 主成本被 BFS/search/queue/sort、虚调用 helper、外部 callback 或不独立 comparator 稀释。 |
| 源码冲突 / 删除 / 合并 | 0 | 未发现文件删除或源码冲突；同类主题仅归并论证，不丢失文件级行。 |
| high/mid 中降级为暂缓或不单独实施 | 14 | 主要是 comparator helper 和 search/BFS/state 主导路径。 |

## 3. 文件级变化理由

本轮没有重做第一轮，而是逐项复核 29 个 high/mid 文件。变化口径如下：

- `impl/approximate_progressive_morphological_filter.hpp` 保持为建议队列。源码证据显示 `extract` 同时包含点云到 grid z-min、OpenMP 行列 min/max open 和 height threshold 压缩，trip count 来自 `input_->size()`、`rows * cols`、窗口面积和 `ground.size()`。
- `impl/extract_polygonal_prism_data.hpp` 保持为建议队列。`segment` 中 height mask、`pointToPlaneDistanceSigned`、投影点的 2D polygon predicate 和保序输出 indices 都在公开入口主路径，且已有 `test_segmentation.cpp` 与 `test_concave_prism.cpp` 覆盖基础语义。
- GrabCut 的 `impl/grabcut_segmentation.hpp` 与 `src/grabcut_segmentation.cpp` 作为同一 GrabCut 主题判断，但保留两行独立去向。模板头有 color staging、organized/non-organized beta 和 n-link weight 计算；src 有 GMM build/learn 和 probability density，但 Boykov-Kolmogorov max-flow 是 active set、parent/orphan 和 map/deque 状态机，且未发现直接上游测试，因此放入保留候选。
- organized connected component 与 organized multi-plane 仍是重要候选，但二轮下钻发现主 label pass 存在 left/up 依赖、union-find label merge、虚调用 comparator、refine 中 label 改写和 region plane fitting；因此进入保留候选，而不是直接建议生产 RVV。
- comparator 头文件都不是独立实施文件。它们可作为 organized 主题的热 predicate 或 fused comparator 复核点，但本身只有 `compare(idx1, idx2)` helper，没有批量 loop、trip count 和测试入口，统一进入暂缓 / 不单独实施，并逐文件保留去向。
- clustering/search/state 类文件包含 per-neighbor dot、color/hue predicate、processed mask 或 tail copy，但主要成本来自 `radiusSearch` / `nearestKSearch`、seed queue、BFS、sort、priority queue 或外部 callback。本轮不因循环数量多而提升优先级。
- graph/RANSAC/solver/supervoxel 类文件保留局部数学或 component ablation 价值，但不直接建议进入 RVV 优化。CPC/LCCP/supervoxel 有示例入口，min-cut/random-walker 有上游测试，CRF/unary 有 staging 或工具入口；它们需要先证明目标片段占主成本。

本轮未补入 low 文件。第一轮 low 中的 public `.h`、显式实例化 `src/*.cpp` 和 thin wrapper 仍未发现明确源码漏判证据。

## 4. 建议进行 RVV 优化的文件

这些文件具备明确 RVV 价值、测试路径相对可行、风险可控，建议优先进入函数级评估。建议队列只授权建立后续函数级评估主题，不承诺 production 接入或最终收益。

| 文件 | 关键入口 / 函数族 | 主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 主要风险 | 测试 / bench 可行性 | 第二轮去向 | 建议实施顺序 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `impl/approximate_progressive_morphological_filter.hpp` | `ApproximateProgressiveMorphologicalFilter<PointT>::extract`；点云到 grid z-min；窗口 min/max open；height threshold 筛选 | progressive / approximate morphological filter | `direct-main-path` | `input_->size()`、`indices_->size()`、`rows * cols`、`window_sizes.size()`、窗口半径内 `j/k` 扫描、`ground.size()` | AoS 点云 xyz 读取到 grid，grid cell min/max 规约，`Z - Zf` threshold mask，保序 indices 压缩；可分阶段评估 z-min、open pass、tail-compress | 现有 `#pragma omp parallel for` 与 RVV 分工；Eigen `MatrixXf` column-major stride；NaN / missing cell sentinel；窗口边界 `rs/re/cs/ce` 语义；indices subset 输出顺序 | `tools/progressive_morphological_filter.cpp` 可作为 public-shaped smoke；需要新增函数级 scalar/RVV 对拍和 synthetic grid case；未发现上游 benchmark | 建议进行 RVV 优化 | 1。先做 grid z-min + window open component evaluation，再决定是否接 tail-compress；不要把普通 PMF 结论简单套用到此文件。 |
| `impl/extract_polygonal_prism_data.hpp` | `ExtractPolygonalPrismData<PointT>::segment`；`isXYPointIn2DXYPolygon`；concave hull polygon 列表 | polygonal prism extraction | `direct-main-path` | `indices_->size()`、`projected_points.size()`、`planar_hull_->size()`、`polygons_.size()`、每个 polygon 顶点数 | height min/max mask，point-to-plane signed distance，投影点 `x/y` polygon parity predicate，保序 `output.indices` compress；可先限制单 polygon / convex-like case，再扩展 concave XOR 语义 | polygon 边界点和 parity 切换语义；concave hull 多 polygon XOR；`projectPoints` 前置成本；输出 indices 顺序必须保持；浮点阈值边界需对拍 | `test/segmentation/test_segmentation.cpp` 有 `ExtractPolygonalPrism`，`test/segmentation/test_concave_prism.cpp` 覆盖 two-rings concave case；可补专项 correctness / QEMU / board bench | 建议进行 RVV 优化 | 2。先评估 height mask + point-to-plane + single-polygon predicate，再用 concave prism test 锁定 XOR 与顺序边界。 |

## 5. 保留实施的候选文件

这些文件存在真实 RVV 片段，但当前还需要 profile、production-shaped diagnostic、component ablation、测试样本或依赖关系确认。本轮保留它们，不建议在建议队列之前直接实施。

| 文件 | 关键入口 / 函数族 | 主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 主要风险 | 测试 / bench 可行性 | 第二轮去向 | 建议实施顺序或保留理由 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `impl/grabcut_segmentation.hpp` | `GrabCut<PointT>::initCompute`、`extract`、`setTerminalWeights`、`setNLinkWeights`、`computeBetaOrganized`、`computeNLinksOrganized`、`computeBetaNonOrganized`、`computeNLinksNonOrganized` | GrabCut | `partial-preprocess` | `input_->size()`、`indices_->size()`、organized `width * height`、non-organized `nb_neighbours_` | RGB/RGBA to `Color` staging，organized neighbor color distance reduction，n-link `exp` / distance weight，terminal probability loops，foreground/background output mask | graph cut solve 在 src 状态机；non-organized KNN search 稀释；`std::exp` / `log` / probability density 数值边界；organized 与 non-organized 两条路径差异大；未见直接单测 | 可构造 synthetic RGB cloud 和 mask 对拍，但需新增专项测试；当前未发现 GrabCut 上游 test / benchmark | 保留实施 | GrabCut 主题第 1 个伴随文件。先做 production-shaped diagnostic，回答 n-link/GMM staging 是否接近总成本。 |
| `src/grabcut_segmentation.cpp` | `buildGMMs`、`learnGMMs`、`GMM::probabilityDensity`、`BoykovKolmogorov::solve` / `expandTrees` / `augmentPath` / `adoptOrphans` | GrabCut | `diagnostic` | `indices.size()`、GMM `K`、graph nodes / edges、active set 和 orphan 队列规模 | GMM component assignment、color accumulation、probability density 小公式、部分 graph capacity reset | Boykov-Kolmogorov 主要是 deque、parent/orphan、map-like edge lookup 和状态机；SVD/covariance 语义；GMM `K` 小且固定，单独 RVV 收益可能弱 | 需跟 `impl/grabcut_segmentation.hpp` 共同建测试；当前缺直接 test | 保留实施 | GrabCut 主题第 2 个伴随文件。只作为 GMM / graph prepass component ablation，不单独先做 max-flow RVV。 |
| `impl/organized_connected_component_segmentation.hpp` | `OrganizedConnectedComponentSegmentation::segment`、`findLabeledRegionBoundary` | organized connected component | `diagnostic` | organized `input_->width * input_->height`、`run_ids.size()`、`input_->size()` label remap、boundary length | finite mask、right/up neighbor predicate staging、label remap、label_indices fill、boundary scan 可做局部诊断 | 主 pass 依赖 left/up label、union-find `findRoot`、`compare_->compare` 虚调用；label merge 顺序影响输出；boundary tracing 是状态机 | 未发现直接上游 test；可构造 small organized clouds 和 comparator stub 做专项对拍 | 保留实施 | organized 主题主文件。先验证 label remap / comparator staging 是否有成本价值，主 CCL pass 不直接承诺 RVV。 |
| `impl/organized_multi_plane_segmentation.hpp` | `segment`、`refine`、`segmentAndRefine`、`segmentAndCalculate*`、`projectToPlaneFromViewpoint` | organized multi-plane segmentation | `partial-preprocess` | `input_->size()`、`label_indices.size()`、`inlier_indices`、organized `labels->width * labels->height`、boundary size | `plane_d[i] = point.dot(normal)`，boundary cloud gather，project-to-plane loop，部分 refine predicate staging | `computeMeanAndCovarianceMatrix`、`eigen33` per-region；refine 双向 pass 改写 labels 和 inlier lists；connected component 和 comparator 依赖外置；projection 仅 boundary 后处理 | 未发现直接上游 test；可用 synthetic organized plane scene 建专项 correctness | 保留实施 | organized 主题主文件。优先评估 `plane_d` / projection / boundary copy；refine label growth 暂不作为首个 RVV 目标。 |
| `impl/progressive_morphological_filter.hpp` | `ProgressiveMorphologicalFilter<PointT>::extract`；`applyMorphologicalOperator` 后 height threshold 筛选 | progressive / approximate morphological filter | `tail-compress` | `window_sizes.size()`、每轮 `ground.size()` | `(*cloud)[p_idx].z - (*cloud_f)[p_idx].z` threshold mask，保序 `pt_indices` 压缩 | 主成本在 filters 模块 `applyMorphologicalOperator` 和 `copyPointCloud`；本文件只剩后处理筛选；收益容易被前置 open 稀释 | `tools/progressive_morphological_filter.cpp` 同时覆盖普通 PMF；需用 component ablation 证明 tail 占比 | 保留实施 | 随 approximate PMF 主题复核，不作为先行独立文件。若 approximate PMF 完成后 tail-compress 模式可复用，再评估。 |
| `impl/cpc_segmentation.hpp` | `CPCSegmentation::applyCuttingPlane`、`WeightedRandomSampleConsensus::computeModel` | CPC / RANSAC / graph | `diagnostic` | Boost graph edges、segment edge points、RANSAC iterations、inlier count | edge centroid / direction staging，RANSAC inlier normal-dot score accumulation，plane side tests | Boost graph edge mutation、maps、recursive cutting、RANSAC random sampling和 sample_consensus model 主导；输出 graph mutation 语义风险高 | `examples/segmentation/example_cpc_segmentation.cpp` 可作场景入口；无直接 unit test | 保留实施 | 仅保留 component ablation。先证明 weighted score loop 热，再考虑局部 helper。 |
| `impl/crf_segmentation.hpp` | `createDataVectorFromVoxelGrid`、`createUnaryPotentials`、`segmentPoints` | CRF / external solver staging | `partial-preprocess` | filtered voxel cloud `N`、grid dimensions、`N * n_labels`、normal cloud size | voxel data vector staging，RGB unpack，normal staging，unary energy fill，output label copy | Dense CRF inference 是外部 solver；代码含大量 debug `std::cout` 与 legacy comparison；缺少直接测试；label semantics 和 old/new CRF output 对齐复杂 | 未发现 direct test；需先建立 small labeled voxel correctness 和 component timing | 保留实施 | 保留为 profile prerequisite。先证明 unary/staging 在真实 CRF 调用中占比，再决定是否启动。 |
| `impl/lccp_segmentation.hpp` | `prepareSegmentation`、`doGrouping`、`mergeSmallSegments`、`applyKconvexity`、`connIsConvex` | LCCP / graph segmentation | `diagnostic` | supervoxel 数、Boost graph edges、neighbor edges、segments | `connIsConvex` 的 centroid/normal dot、cross、angle、`exp` threshold；edge validity scans | Boost graph、set/map mutation、recursive growing 和 segment merge 主导；局部公式没有连续批量布局；supervoxel 输入依赖重 | `examples/segmentation/example_lccp_segmentation.cpp` 可构造场景；无直接 unit test | 保留实施 | 与 supervoxel/CPC 同族保留。只在 profile 指向 `connIsConvex` 或 edge scan 时启动。 |
| `impl/min_cut_segmentation.hpp` | `extract`、`buildGraph`、`calculateUnaryPotential`、`calculateBinaryPotential`、`assembleLabels` | min-cut / graph solver | `diagnostic` | `indices_->size()`、foreground/background point count、KNN neighbours、graph edges | foreground/background min distance unary，binary potential `exp`，source/sink edge updates，label assemble | KNN search、Boost graph mutation和 `boykov_kolmogorov_max_flow` 主导；foreground/background min loop 是 nested gather；edge property update 不规整 | `test/segmentation/test_segmentation.cpp` 有 `MinCutSegmentationTest`；可做 potential component ablation | 保留实施 | 有测试，保留优先级高于无测 graph 类。先隔离 unary/binary potential，验证 solver 稀释比例。 |
| `impl/random_walker.hpp` | `detail::RandomWalker::buildLinearSystem`、`solveLinearSystem`、`assignColors`、`getPotentials` | random walker / Eigen sparse solver | `diagnostic` | graph vertices / edges、seeds、SparseMatrix rows / cols、colors | sparse triplet staging，row max color assignment，potentials row copy | Eigen sparse Cholesky `SimplicialCholesky` 是核心；Boost graph traversal 和 bimap 查找不规则；triplet fill scatter 弱 | `test/segmentation/test_random_walker.cpp` 覆盖 build/segment/potentials | 保留实施 | 适合 no-production diagnostic 或 row-max component ablation；不先动 solver。 |
| `impl/segment_differences.hpp` | `getPointCloudDifference`、`SegmentDifferences::segment` | search + tail threshold | `tail-compress` | `src.size()`、one-NN query count | finite mask，nearest distance threshold，output point copy / compress | `nearestKSearch` 一次每点主导；RVV 只覆盖 search 后处理；输出 cloud 顺序必须保持 | `test/segmentation/test_segmentation.cpp` 有 `SegmentDifferences` | 保留实施 | 有测试且局部简单，但只能做 component ablation。若 search 后处理占比低，转不单独实施。 |
| `impl/supervoxel_clustering.hpp` | `extract`、`selectInitialSupervoxelSeeds`、`computeVoxelData`、`SupervoxelHelper::expand`、`refineNormals`、`updateCentroid`、`voxelDataDistance` | supervoxel clustering | `diagnostic` | input points、octree leaves、seed count、helper leaf sets、adjacency edges | color/normal/spatial distance，centroid reductions，normal/rgb/xyz accumulation，adjacency edge distance | octree leaf iterators、KdTree search、set/list mutation、owner stealing、normal estimation 主导；数据布局高度不连续 | `examples/segmentation/example_supervoxels.cpp`、LCCP/CPC examples 可触达；无 direct test | 保留实施 | 保留为 supervoxel family 的 component ablation。先定位 `voxelDataDistance` / centroid reduction 是否热。 |
| `impl/unary_classifier.hpp` | `convertCloud`、`findClusters`、`kmeansClustering`、`queryFeatureDistances`、`assignLabels`、`train`、`segment` | unary classifier / FLANN / FPFH | `partial-preprocess` | input cloud size、FPFH feature rows、33-bin histogram、query feature size、output size | 33-bin histogram staging，FLANN query input copy，distance threshold label assignment | FPFH、Kmeans、FLANN `knnSearch` 主导； per-query allocation；label field memcpy 和 trained_features layout 复杂 | `tools/unary_classifier_segment.cpp`、`tools/train_unary_classifier.cpp` 可触达；未发现 unit test | 保留实施 | 保留为 profile prerequisite。只在工具场景证明 staging/assignLabels 热时启动。 |

## 6. 暂缓或不推荐考虑 RVV 优化的文件

这些文件不从记录中删除，但当前不建议作为独立 RVV 主题。comparator 文件应随 organized 主文件复核；search/BFS/state 文件若未来有 profile 证明局部 predicate 占主成本，可在保留候选复筛中补回。

| 文件 | 关键入口 / 函数族 | 主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 主要风险 | 测试 / bench 可行性 | 第二轮去向 | 不单独实施理由 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `edge_aware_plane_comparator.h` | `EdgeAwarePlaneComparator::compare` | organized comparator helper | `non-standalone` | 由 organized 邻接边调用次数决定，本文件无 loop | depth-scaled threshold、xyz distance、normal dot、curvature、plane-d、distance map mask | 无批量入口；虚调用；distance_map 边界和 curvature gate 依赖调用方 | 需随 organized connected component 专项测试 | 暂缓 / 不单独实施 | 只作为 organized comparator fusion 的候选 helper，不独立建 topic。 |
| `euclidean_cluster_comparator.h` | `EuclideanClusterComparator::compare` | organized comparator helper | `non-standalone` | 由 organized 邻接边调用次数决定，本文件无 loop | label exclude mask、depth-scaled xyz distance | `std::set` exclude_labels 查找、label cloud 依赖、无独立 trip count | 需随 organized connected component 测试 | 暂缓 / 不单独实施 | helper 含不规则 set 查找，收益依赖调用方。 |
| `euclidean_plane_coefficient_comparator.h` | `EuclideanPlaneCoefficientComparator::compare` | organized comparator helper | `non-standalone` | 由 organized 邻接边调用次数决定，本文件无 loop | xyz distance、normal dot predicate | 无批量 loop；normal/threshold 语义由 organized pass 驱动 | 需随 organized connected component 测试 | 暂缓 / 不单独实施 | 保留为 fused predicate 复核点，不单独实施。 |
| `ground_plane_comparator.h` | `GroundPlaneComparator::compare` | organized comparator helper | `non-standalone` | 由 organized 邻接边调用次数决定，本文件无 loop | normal 与 expected ground axis dot、neighbor normal dot | 无独立入口；plane distance 分支大段注释掉；输入 normals 语义依赖调用方 | 需随 organized connected component 测试 | 暂缓 / 不单独实施 | helper-only。若 organized ground plane 场景有 profile，再随主文件评估。 |
| `plane_coefficient_comparator.h` | `PlaneCoefficientComparator::compare` | organized comparator helper | `non-standalone` | 由 organized 邻接边调用次数决定，本文件无 loop | plane `d` 差、depth-scaled threshold、normal dot | 无批量入口；`plane_coeff_d_` 由 multi-plane 预计算；虚调用 | 需随 organized connected component / multi-plane 测试 | 暂缓 / 不单独实施 | 作为 organized 主题 companion 文件保留，不独立做。 |
| `plane_refinement_comparator.h` | `PlaneRefinementComparator::compare` | organized refinement comparator helper | `non-standalone` | 由 `OrganizedMultiPlaneSegmentation::refine` 邻接 pass 调用次数决定 | label gate、model lookup、point-to-plane distance、depth scaling | refine pass 改写 label；`models_` / `label_to_model_` 间接访问；无独立 loop | 需随 organized multi-plane refine 测试 | 暂缓 / 不单独实施 | companion helper。只随 refine component ablation 复核。 |
| `rgb_plane_coefficient_comparator.h` | `RGBPlaneCoefficientComparator::compare` | organized comparator helper | `non-standalone` | 由 organized 邻接边调用次数决定，本文件无 loop | xyz distance、normal dot、RGB squared distance | 无独立入口；RGB field 和 sqrt/threshold 语义需调用方提供 oracle | 需随 organized connected component 测试 | 暂缓 / 不单独实施 | helper-only，不独立实施。 |
| `extract_clusters.h` | normal-aware inline `extractEuclideanClusters` overloads | clustering / search / BFS | `diagnostic` | `cloud.size()` 或 `indices.size()`、seed queue、radiusSearch neighbors | normal dot threshold，processed mask，cluster copy | `radiusSearch`、BFS seed queue、sort 主导；输出顺序和 cluster membership 不能改变 | 没有 direct unit test；`examples/segmentation/example_extract_clusters_normals.cpp` 触达相关路径 | 暂缓 | public inline 实现存在，但 RVV 只覆盖 neighbor validation，当前不建议单独实施。 |
| `impl/conditional_euclidean_clustering.hpp` | `ConditionalEuclideanClustering::segment` | clustering / search / callback | `diagnostic` | `indices_->size()`、current cluster queue、radiusSearch neighbors | processed mask、neighbor loop prefilter、cluster output copy | 用户 `condition_function_` 不可向量化；search/BFS state 主导；small/large cluster 输出 bucket 语义 | `examples/segmentation/example_extract_clusters_normals.cpp` 有替代路径说明；无 direct test | 暂缓 | 外部 callback 使 RVV 价值难以泛化，需真实 profile 才重新考虑。 |
| `impl/extract_clusters.hpp` | `extractEuclideanClusters` overloads、`EuclideanClusterExtraction::extract` | clustering / search / BFS | `diagnostic` | `cloud.size()`、`indices.size()`、seed queue、radiusSearch neighbors | processed mask、indices gather、cluster copy / sort 前 staging | search、BFS、sort 主导；输出 cluster order 和 sorted indices 是可见语义 | 无 direct unit test，公共 API 常用但局部 RVV 难归因 | 暂缓 | 不建议单独做 RVV；可在 search 后处理 profile 明确时复筛。 |
| `impl/extract_labeled_clusters.hpp` | `extractLabeledEuclideanClusters`、`LabeledEuclideanClusterExtraction::extract` | labeled clustering / search / BFS | `diagnostic` | `cloud.size()`、seed queue、radiusSearch neighbors、label buckets | label equality mask、cluster copy、per-label bucket append | search/BFS/sort 和 per-label output order 主导；labels 与 cloud index 必须严格对齐 | 无 direct unit test | 暂缓 | 比普通 cluster 多 label predicate，但主成本仍不在可向量化 loop。 |
| `impl/region_growing.hpp` | `findPointNeighbours`、`applySmoothRegionGrowingAlgorithm`、`growRegion`、`validatePoint`、colored cloud output | region growing / search / state | `diagnostic` | `indices_->size()`、KNN neighbours、segment count、cluster sizes | normal dot、curvature/residual predicate、label fill、colored output loops | KNN search、seed queue、sorted residuals、region state mutation 主导；predicate 受 flags 组合影响 | `test/segmentation/test_segmentation.cpp` 有 RegionGrowing 多个 case，`examples/segmentation/example_region_growing.cpp` 有示例 | 暂缓 | 有测试但 RVV 片段不占稳定主成本；只在 profile 指向 `validatePoint` 或 output coloring 时复筛。 |
| `impl/region_growing_rgb.hpp` | `findPointNeighbours`、`findSegmentNeighbours`、`applyRegionMergingAlgorithm`、`validatePoint`、`assembleRegions` | RGB region growing / search / merge state | `diagnostic` | `indices_->size()`、segment 数、region neighbour 数、cluster sizes | RGB color difference、normal/residual/distance predicate、region color accumulation | KNN、priority_queue、multi-stage region merge、sort 和 state mutation 主导 | `test/segmentation/test_segmentation.cpp` 有 RegionGrowingRGB case | 暂缓 | 控制流和合并状态复杂，局部 color predicate 不支持独立 RVV 主题。 |
| `impl/seeded_hue_segmentation.hpp` | `seededHueSegmentation` RGB/RGBA overloads | seeded hue / search / BFS | `diagnostic` | `indices_in.indices.size()`、seed queue、radiusSearch neighbors | RGB/RGBA to HSV，hue threshold mask，output copy | `radiusSearch` 和 BFS queue 主导；最终 `std::sort` 影响输出顺序；HSV 转换在不规则 neighbor loop 内 | 未发现 direct test | 暂缓 | 局部 hue predicate 清楚，但缺测试且 search/state 稀释，暂不单独实施。 |

## 7. 执行清单 / 状态表

### 7.1 建议进行 RVV 优化的文件

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- |
| 1 | approximate PMF grid/open | `impl/approximate_progressive_morphological_filter.hpp` | `ApproximateProgressiveMorphologicalFilter::extract` 中 grid z-min 与 height threshold tail | 已采纳；topic closed；已提交 | 已建立 test-rvv topic 并完成 production integration loop。接入后的 production public board 5-run 显示 `PointXYZ` dense median 1.60x、non-dense median 1.35x；040 点型扩展显示 `PointXYZ` 1.48x / 1.28x、`PointXYZI` 1.52x / 1.42x、`PointXYZRGB` 1.50x / 1.43x、`PointXYZRGBA` 1.51x / 1.43x，Evidence Doctor 均为 Errors=0 / Warnings=0 / Suggestions=0。当前 production patch 为 adopted production behavior。window-open RVV 不接入 production。050 tail 剩余比较 / 输出压缩探针相对 040 RVV 基线整体 neutral，已回退；当前无值得自动推进的下一生产优化方向。提交：`ea2ba420a` topic 主体，`7932089a3` board evidence summaries。统计记录：`tmp/rvv-topic-stats/2/segmentation-approximate-progressive-morphological-filter-stats.zh.md`。 |
| 2 | polygonal prism predicate | `impl/extract_polygonal_prism_data.hpp` | `ExtractPolygonalPrismData::segment` 的 height mask + point-to-plane + polygon predicate | 已采纳；topic closed；已提交 | 已建立 `test-rvv/segmentation/extract_polygonal_prism_data` topic，并完成 clean split production patch。production public board 5-run 显示 single polygon dense / indexed median 均为 1.75x；Phase 050 concave hull 多 polygon XOR 已接入，nested dense median 2.18x、nested indexed median 2.13x；Phase 060 点型扩展已采纳 `PointXYZI` median 1.85x、`PointXYZRGB` median 1.83x、`PointXYZRGBA` median 1.84x，`PointXYZINormal` 因宽 stride 不稳定显式回退标量；Phase 070 已把规模阈值从 64 降到 32，32 点 confirm5 median 1.19x、min 1.18x、Evidence Doctor 0 / 0 / 0。Phase 080 已完成 production dispatch / fallback、doc-suite parity 和 artifact tracking 收尾审计。当前 production patch 为 adopted production behavior，正式文档为 `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md`。topic 主体提交 `6baf3b978`，board evidence 提交 `eed4f441c`；统计记录为 `tmp/rvv-topic-stats/2/segmentation-extract-polygonal-prism-data-stats.zh.md`。剩余 wide-stride、`projectPoints` 或自定义点型方向需要专项 phase / 新 topic，当前不建议继续自动推进。 |

### 7.2 保留实施的候选文件

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- |
| 3 | GrabCut staging / n-link | `impl/grabcut_segmentation.hpp` | `initGraph()` unknown trimap terminal batch；`learnGMMs()` assignment | 已采纳；topic closed；已提交 | 保留候选复筛后已启动并关闭。production patch 采纳 `initGraph()` terminal helper 和 `learnGMMs()` assignment；Phase 120 stop profile 显示 n-link、non-organized KNN、max-flow 和其它剩余方向当前没有默认继续价值。提交：`33f8a4e4a`。 |
| 4 | GrabCut GMM / max-flow boundary | `src/grabcut_segmentation.cpp` | `learnGMMs()` assignment；max-flow 保持标量边界 | 已采纳；topic closed；已提交 | 与模板头同一 GrabCut 主题完成。`src` 侧采纳 GMM assignment 窄范围 production RVV；Boykov-Kolmogorov max-flow 仍是 deque、parent/orphan、edge lookup 状态机，作为明确 scalar-only 边界记录。提交：`33f8a4e4a`。 |
| 5 | organized connected component | `impl/organized_connected_component_segmentation.hpp` | label remap / comparator staging diagnostic | 保留 | 先用 comparator stub 和 small organized cloud 锁定 label 语义，再判断主 pass 是否有可拆 RVV 片段。 |
| 6 | organized multi-plane | `impl/organized_multi_plane_segmentation.hpp` | `plane_d` dot loop、boundary gather、projection loop | 已完成 no-production closeout；已提交 | 保留候选复筛后已启动并关闭。component ablation 有局部正向，但 production-shaped `region_projected` median 0.89x、`region_gather_only` median 0.80x，5/5 退化；当前不接 production。提交：`5b2f32d50`。 |
| 7 | progressive PMF tail | `impl/progressive_morphological_filter.hpp` | `applyMorphologicalOperator` 后 height threshold compress | 保留 | 随 approximate PMF 复核；只有 tail 占比被证明时单独推进。 |
| 8 | min-cut potentials | `impl/min_cut_segmentation.hpp` | unary / binary potential component | 已完成 no-production closeout；已提交 | 保留候选复筛后已启动并关闭。component path 有正向，但 `buildGraph`-shaped median 1.02x、2/5 低于 1，Evidence Doctor 为 Error；当前不接 production。提交：`355dfdbac`。 |
| 9 | random walker row operations | `impl/random_walker.hpp` | `assignColors` row max / `getPotentials` copy | 保留 | Eigen sparse solve 不动；用现有 random_walker tests 先做 no-production diagnostic。 |
| 10 | segment differences tail | `impl/segment_differences.hpp` | one-NN 后 threshold / output copy | 保留 | 有测试但 search 主导；只适合测后处理上界。 |
| 11 | supervoxel local math | `impl/supervoxel_clustering.hpp` | `voxelDataDistance`、centroid reduction | 保留 | 需要 example-shaped profile；octree/set/list state 是主风险。 |
| 12 | LCCP convexity | `impl/lccp_segmentation.hpp` | `connIsConvex` / edge scan component | 保留 | 依赖 supervoxel example；只在 edge convexity 热时启动。 |
| 13 | CPC weighted RANSAC score | `impl/cpc_segmentation.hpp` | weighted score dot accumulation | 保留 | 依赖 CPC example；RANSAC random/state 不作为 RVV 目标。 |
| 14 | CRF staging / unary | `impl/crf_segmentation.hpp` | data vector staging 或 `N * n_labels` unary fill | 保留 | 先补小样本 correctness 并确认 DenseCRF 外部 solver 稀释比例。 |
| 15 | unary classifier staging | `impl/unary_classifier.hpp` | 33-bin histogram staging / `assignLabels` | 保留 | 需要工具场景 profile；FLANN/FPFH/Kmeans 不作为 RVV 目标。 |

### 7.3 暂缓或不推荐考虑 RVV 优化的文件

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- |
| 16 | organized comparator helper | `edge_aware_plane_comparator.h` | 随 organized comparator fusion 评估 | 暂缓 / 不单独实施 | 无独立 loop；只随主文件复核。 |
| 17 | organized comparator helper | `euclidean_cluster_comparator.h` | 随 organized comparator fusion 评估 | 暂缓 / 不单独实施 | set / label 依赖明显，不独立做。 |
| 18 | organized comparator helper | `euclidean_plane_coefficient_comparator.h` | 随 organized comparator fusion 评估 | 暂缓 / 不单独实施 | helper-only。 |
| 19 | organized comparator helper | `ground_plane_comparator.h` | 随 organized comparator fusion 评估 | 暂缓 / 不单独实施 | helper-only。 |
| 20 | organized comparator helper | `plane_coefficient_comparator.h` | 随 organized comparator fusion 评估 | 暂缓 / 不单独实施 | helper-only。 |
| 21 | organized refinement helper | `plane_refinement_comparator.h` | 随 multi-plane refine component 评估 | 暂缓 / 不单独实施 | helper-only 且 label mutation 风险高。 |
| 22 | organized comparator helper | `rgb_plane_coefficient_comparator.h` | 随 organized comparator fusion 评估 | 暂缓 / 不单独实施 | helper-only。 |
| 23 | normal-aware cluster | `extract_clusters.h` | neighbor normal predicate | 暂缓 | search/BFS/sort 主导；需 profile 才复筛。 |
| 24 | conditional euclidean clustering | `impl/conditional_euclidean_clustering.hpp` | callback 前后 neighbor filter | 暂缓 | 用户 callback 不可向量化，暂不单独实施。 |
| 25 | euclidean cluster | `impl/extract_clusters.hpp` | processed mask / cluster copy | 暂缓 | search/BFS/sort 主导。 |
| 26 | labeled euclidean cluster | `impl/extract_labeled_clusters.hpp` | label equality predicate | 暂缓 | search/BFS/order 主导。 |
| 27 | region growing | `impl/region_growing.hpp` | `validatePoint` predicate 或 output coloring | 暂缓 | 虽有测试，但主成本是 KNN/queue/state。 |
| 28 | RGB region growing | `impl/region_growing_rgb.hpp` | color predicate 或 region color accumulation | 暂缓 | merge state、priority queue、sort 主导。 |
| 29 | seeded hue segmentation | `impl/seeded_hue_segmentation.hpp` | HSV / hue predicate | 暂缓 | search/BFS/sort 主导且缺 direct test。 |

### 7.4 状态矩阵

| 阶段 | segmentation 模块当前状态 |
| --- | --- |
| 模块文件候选筛选 | 已完成，第一轮文档为 `doc-rvv/library-screening/modules/segmentation-file-candidate-screening.zh.md`。 |
| 函数评估队列 | 本文档完成第二轮队列，覆盖 29/29 个 high/mid 基线候选。 |
| RVV 实现 | 第一主题 `impl/approximate_progressive_morphological_filter.hpp` 已完成 production closeout 并拆分为 topic commit `ea2ba420a` 和 evidence commit `7932089a3`；040 点型扩展已采纳，050 进一步 tail 压缩探针已拒绝并回退。第二主题 `impl/extract_polygonal_prism_data.hpp` 已完成 S11 production closeout、Phase 050 concave hull XOR 扩展、Phase 060 点型扩展、Phase 070 规模阈值调优和 Phase 080 提交前收尾审计，当前 production patch 为 adopted production behavior，topic 主体提交 `6baf3b978`，board evidence 提交 `eed4f441c`。保留复筛后，GrabCut 已采纳 `initGraph()` terminal helper 和 `learnGMMs()` assignment production patch 并以 `33f8a4e4a` 关闭；min-cut 与 organized multi-plane 已完成 no-production closeout，提交分别为 `355dfdbac`、`5b2f32d50`。 |
| 专项测试 | 已创建 `test-rvv/segmentation/approximate_progressive_morphological_filter`、`test-rvv/segmentation/extract_polygonal_prism_data` 和 `test-rvv/segmentation/grabcut_segmentation`；min-cut、organized multi-plane 在 `test-rvv/segmentation/min_cut_segmentation`、`test-rvv/segmentation/organized_multi_plane_segmentation` 留存 topic-local 诊断证据。polygonal prism Std 2 tests、RVV 16 tests 通过，板卡 correctness 16 tests 通过；GrabCut 完成 production-detail、production-public 与接入后 stop profile 证据链。 |
| bench | approximate PMF 已运行 production public 板卡 5-run：`PointXYZ` 030 证据为 1.60x / 1.35x，040 点型扩展为 `PointXYZ` 1.48x / 1.28x、`PointXYZI` 1.52x / 1.42x、`PointXYZRGB` 1.50x / 1.43x、`PointXYZRGBA` 1.51x / 1.43x。polygonal prism 已运行 diagnostic dense / indexed、production public single polygon dense / indexed 和 nested polygon dense / indexed 板卡 5-run；single polygon 两组 median 均为 1.75x，nested dense median 2.18x，nested indexed median 2.13x。GrabCut production-detail `initGraph()` median 3.1280x、production-public `extract()` median 1.112866x、`learnGMMs()` detail median 2.755010x、接入后 public median 1.207907x。min-cut `buildGraph`-shaped median 1.02x 且 Evidence Doctor Error；organized multi-plane production-shaped `region_projected` median 0.89x、`region_gather_only` median 0.80x。 |
| QEMU / 反汇编 / 目标硬件闭环 | 第一主题已完成 QEMU correctness/log-shape、production asm、040 点型扩展板卡 Evidence Doctor 和 050 RVV-vs-RVV board A/B。第二主题已完成 QEMU smoke、production `segmentRvv` 反汇编归属、production public 板卡 repeated 和 Evidence Doctor 0 / 0 / 0。GrabCut 完成 topic-local correctness、production-detail / public / stop-profile 板卡证据；min-cut 和 organized multi-plane 完成 no-production 诊断证据并记录回退原因。 |
| 第一条未完成主题 | 两条二轮建议主题和复筛后建议启动的 GrabCut、min-cut、organized multi-plane 均已关闭；保留复筛输出中当前建议启动函数级评估主题数为 0。segmentation 模块当前无待启动主题，可以收束；仅当后续出现新的源码变化、真实 profile / 上游使用证据，或其它模块完成主题暴露出可复用模式并明确指向 segmentation 队列外文件时，再重新复筛。 |

## 8. Closeout

- 输出文档：`doc-rvv/library-screening/segmentation/segmentation-function-evaluation-queue.zh.md`
- 第一轮统计：总文件 `76`，high `6`，mid `23`，low `47`，high+mid `29`。
- 第二轮统计：初始候选 `29`，新增补充候选 `0`，第二轮总候选 `29`。
- 三类队列：建议进行 RVV 优化 `2`，保留实施候选 `13`，暂缓或不推荐 `14`。
- 第一条建议主题 `impl/approximate_progressive_morphological_filter.hpp` 已完成当前授权范围内的生产优化闭环：040 已采纳，050 已尝试并回退；topic 主体提交为 `ea2ba420a`，摘要证据提交为 `7932089a3`，统计记录为 `tmp/rvv-topic-stats/2/segmentation-approximate-progressive-morphological-filter-stats.zh.md`。第二条建议主题 `impl/extract_polygonal_prism_data.hpp` 已完成生产接入闭环 PI2-PI5、S11 closeout、Phase 050 concave hull XOR 扩展、Phase 060 点型扩展、Phase 070 规模阈值调优和 Phase 080 提交前收尾审计，当前 single / nested polygon dense / indexed production RVV 路径与 `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` 点型已采纳，`PointXYZINormal` 保持标量 fallback，规模阈值已降到 32；topic 主体提交 `6baf3b978`，board evidence 提交 `eed4f441c`，统计记录为 `tmp/rvv-topic-stats/2/segmentation-extract-polygonal-prism-data-stats.zh.md`。
- 保留候选复筛已输出 `doc-rvv/library-screening/segmentation/segmentation-retained-candidate-rescreen.zh.md`：输入覆盖 13 个保留候选文件，不补入队列外文件；复筛后建议启动的 GrabCut、min-cut、organized multi-plane 均已完成或关闭，其中 GrabCut 已采纳 production patch，min-cut 与 organized multi-plane 均完成 no-production closeout；剩余 9 个主题继续暂缓 / 不单独实施。
- 当前 segmentation 模块没有下一条建议启动的函数级评估主题；模块优化可在当前证据边界内收束。
- 本轮没有发现需要补充到 `rvv-screening`、`rvv-workflow`、`rvv-test` 或 implementation 文档的通用规则。
