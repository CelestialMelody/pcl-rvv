# features 模块 RVV 第一轮文件级筛选报告

本文档记录 `features` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV（单指令多数据 / RISC-V 向量扩展）片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 源码范围：`features/**`
- 源码后缀：`.h`、`.hpp`、`.c`、`.cc`、`.cpp`、`.cu`
- 排除口径：第三方实现、生成文件、测试、文档和非主库路径只登记或排除，不纳入候选主线；本模块本轮未发现 `3rdparty/**` 源码文件。
- 重做口径：既有 `features-file-candidate-screening.zh.md` 只作为 previous baseline（旧基线）和覆盖检查参考，不沿用循环数量或数学项数量作为充分判断依据。
- 路径显示：`include` 文件省略公共前缀 `features/include/pcl/features/`；`src` 文件省略公共前缀 `features/`，因此写成 `src/foo.cpp`。
- 上游验证入口：`test/features` 覆盖 normal、integral image normal、PFH/FPFH/VFH、SHOT/SHOT LRF、BOARD、RIFT、ROPS、GASD、organized edge 等；`benchmarks/features` 当前只有 `normal_3d.cpp` 和 `shot.cpp` 两个模块级 benchmark（性能测试）入口。

## 2. 第一轮筛选口径

- 第一轮是文件级粗筛，判断标准是文件内是否存在可向量化循环、数学密集片段、批量字段访问、规约、mask（掩码）/压缩、图像式 organized（有行列组织的点云）遍历或可诊断的局部 SIMD/RVV 点。
- `high/mid` 是第二轮必须复核并交代去向的初始候选基线，不是最终实施全集。
- `low` 表示本轮未发现足以进入二轮基线的证据，不是永久排除；如果第二轮源码下钻发现明显漏判，可以补入并说明证据。
- 第一轮不承诺 RVV 覆盖公开入口主成本；主成本覆盖、测试可行性、fallback（回退路径）条件和维护风险由第二轮筛选继续判断。
- 循环数量、数学项数量或关键词命中只能作为扫描线索，不能单独支撑 `high` / `mid`。候选必须定位到可复核的入口、loop/helper（循环 / 辅助函数）、trip count（循环次数来源）、RVV 适配点和主要风险。

评估维度：

| 维度 | 第一轮判断口径 |
| ---- | -------------- |
| 循环规模 | 是否随点数、像素数、邻域数、bin 数、indices 数、mesh polygon/triangle 数或描述子长度线性或更高增长 |
| 算术密度 | 是否包含乘加、距离、dot/cross（点积 / 叉积）、统计、几何谓词、权重、规约、矩阵/向量小公式或数学函数 |
| 访存模式 | 是否存在连续、固定 stride（跨步）、AoS 字段、organized 行列、indices gather（按索引离散加载）等访问 |
| 分支复杂度 | 是否主要是简单谓词/mask，还是深分支、状态机、随机采样、map/hash、search 或 Eigen solver 主导 |
| 语义风险 | 是否涉及非结合规约、阈值边界、NaN/Inf、输出顺序、indices、用户自定义点类型或 OpenMP/RVV 叠加 |
| 可验证性 | 是否能构造 std/RVV 对拍、边界 case、fallback case、QEMU 正确性和板卡验证；第一轮只记录入口，不跑测试 |

优先级含义：

| 优先级 | 含义 |
| ------ | ---- |
| high | 文件内存在明显批量循环或数学密集函数族，且粗看具备较强 SIMD/RVV 评估价值 |
| mid | 文件内存在可向量化片段，但主成本、数据布局、语义风险、indices/search 稀释或测试入口需要第二轮继续确认 |
| low | 以声明、薄 wrapper、显式实例化、调度、类型、构建胶水、小规模固定计算或明显不规则状态路径为主 |

## 3. 第一轮筛选统计

| 项目 | 数量 | 说明 |
| ---- | ---: | ---- |
| 源码文件总数 | 137 | `features/**` 源码文件；`include` 97，`src` 40 |
| 已判定文件数 | 137 | `137/137` 全覆盖 |
| high | 13 | 二轮必查 |
| mid | 40 | 二轮必查 |
| low | 84 | 已覆盖但不进入二轮初始基线 |
| high + mid | 53 | 第一轮候选基线 |
| 候选占比 | 38.7% | `53/137` |

与旧基线相比，本轮主要变化是把只含公开声明、`#include <pcl/features/impl/...>`、显式实例化宏或注释数学公式的文件降级为 `low/non-standalone`（非独立实施单元）。旧文档的 `8 high / 85 mid / 44 low` 调整为 `13 high / 40 mid / 84 low`；候选总量减少是因为本轮按源码入口和可复核 loop/helper 重新判定，而不是按循环计数或关键词计数筛选。

## 4. high 候选

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| ---- | --------------- | --------------- | ---------- | -------- | -------------- |
| `impl/board.hpp` | `BOARDLocalReferenceFrameEstimation::computeFeature` -> `computePointLRF` 的 per-point、per-neighbor LRF 计算 | `indices_->size()` 与每点 `normal_indices` / `nn_indices` | 邻域 dot/cross、方向投票、3x3 frame 写回可做局部 VL 分块或 helper 级对拍 | 邻域 search 在外部，`check_margin_array_` 状态和 LRF 方向符号语义需保序 | 上游有 `test_board_estimation`；文件主体不是声明，包含每点邻域数学和固定字段输出 |
| `impl/brisk_2d.hpp` | `BRISK2DEstimation::compute` 的 integral image 构建、smoothed value、long/short pair descriptor 循环 | `image_data.size()`、`height * width`、`keypoints.size()`、BRISK pair 数 | organized 图像连续遍历、强度采样、pair 比较和 descriptor bit 写入可拆成 mask/批量比较 | bit packing、旋转表、边界采样和关键点局部访问会限制直接收益 | 文件内含核心实现；上游有 `test_brisk`，是少数 2D-like 连续内存路径 |
| `impl/don.hpp` | `DifferenceOfNormalsEstimation::computeFeature` 的逐点大尺度/小尺度 normal 差 | `input_->size()` | 三字段差值、曲率/normal 写回是直接 pointwise（逐点）RVV 形态 | 输入 normal cloud 尺寸和有限值语义要保持；收益可能被前置 normal estimation 稀释 | 文件虽短，但是直接主循环，RVV 形态清楚、fallback 简单 |
| `impl/fpfh.hpp` | `computePointSPFHSignature`、`weightPointSPFHSignature`、`computeFeature` 的邻域直方图与加权 SPFH/FPFH | `indices_->size()`、每点 `nn_indices.size()`、bin 数 | 邻域 pair feature、直方图归一化、邻居加权累加可分 helper 评估 | search、indices gather、直方图冲突累加和非结合浮点规约 | 上游有 PFH/FPFH 测试和 examples；核心 loop 明确，适合二轮下钻 |
| `impl/integral_image2D.hpp` | `IntegralImage2D::setInput` / `setInputImpl` 的行列积分图构建 | `width_ * height_ * Dimension` | 连续 row/col strip-mining、first/second order 累加、固定 stride load | 前缀和有行内依赖，RVV 可能只能覆盖列向量/多维字段或 staged partial | organized 数据入口清晰，是 `integral_image_normal` 的基础批量路径 |
| `impl/integral_image_normal.hpp` | `computeFeature`、`computeFeatureFull`、`computeFeaturePart` 的 depth-change map、distance map、organized normal 输出 | `input_->width * input_->height` 或 `indices_->size()` | organized 行列遍历、distance/depth map、normal/curvature 写回、部分 map 初始化可批量化 | `IntegralImage2D` 前缀依赖、border policy、mirror padding、NaN/Inf 与 finite checks | 上游有 `test_ii_normals` 和 `benchmarks/features/normal_3d.cpp`，公开入口价值高 |
| `impl/moment_of_inertia_estimation.hpp` | `compute`、`computeCovarianceMatrix`、AABB/OBB、projection 和 eccentricity 循环 | `number_of_points` 与固定角度扫描 × 点数 | 点坐标投影、min/max、covariance 累加、bounding box 规约 | Eigen eigen-solver 仍标量；浮点规约顺序和 min/max 边界需对拍 | 大部分成本在点云扫描和投影规约，上游有 `test_moment_of_inertia_estimation` |
| `impl/normal_3d.hpp` | `NormalEstimation::computeFeature` 的每点邻域 normal/curvature 估计 | `indices_->size()` 与每点 `nn_indices.size()` | 邻域 covariance、centroid、normal 翻转 helper 可局部 RVV；与 common centroid 经验可复用 | search 主导、Eigen 3x3 求解、indices gather 和点类型泛型布局 | 模块常用入口，有 `test_normal_estimation` 和 normal benchmark；二轮必须复核 |
| `impl/organized_edge_detection.hpp` | `compute` / `extractEdges` / `assignLabelIndices` 的 organized 邻域分类 | `input_->width * input_->height` | 行列邻域比较、depth/normal/RGB 阈值 mask、label 写回可批量化 | 深分支、边类型多标签、search-neighbor scan 和 label 顺序语义 | 上游有 `test_organized_edge_detection`；organized 主循环明确 |
| `impl/pfh.hpp` | `computePointPFHSignature` 和 `computeFeature` 的 O(k^2) pair feature 直方图 | `indices_->size()`、每点邻域 pair 数、histogram bin 数 | pair feature 批量算术、bin 归一化、histogram 写回可做 component ablation（组件消融） | pair 缓存 map、直方图冲突、atan2/acos、非结合累加 | 上游有 `test_pfh_estimation`，核心开销集中在邻域 pair 数学 |
| `impl/rops_estimation.hpp` | `ROPSEstimation::computeFeature`、LRF、distribution matrix、central moments | `indices_->size()`、局部 triangle/point 数、`number_of_bins_ ^ 2` | 投影、矩阵 bin 累加、central moments、entropy 计算可分段向量化 | mesh triangle set、Eigen eigenvectors、bin scatter 和旋转步骤状态复杂 | 上游有 `test_rops_estimation`，文件包含完整实现且数学密度高 |
| `impl/shot.hpp` | `computePointSHOT`、`computeFeature` 的 SHOT/SHOTColor 描述子计算 | `indices_->size()`、每点 `nn_indices.size()`、`descLength_` | 邻域几何/颜色 bin、descriptor normalize、NaN 填充和写回可批量化 | search/LRF 依赖、bin scatter、颜色路径、OpenMP 伴随实现和输出 NaN 语义 | 有 `test_shot_estimation` 与 `benchmarks/features/shot.cpp`，是模块最明确的热点候选之一 |
| `src/range_image_border_extractor.cpp` | `RangeImageBorderExtractor::computeFeature` 触发的 local surface、border direction、surface change 行列遍历 | `range_image_->width * range_image_->height` 与局部半径窗口 | organized 行列 mask、neighbor score、border trait 写回、blur/threshold 批量化 | `surface_structure_` 指针数组、分类状态、局部窗口深分支和输出标签稳定性 | 少数真实实现位于 `src` 的文件；非显式实例化，包含多段图像式主循环 |

## 5. mid 候选

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| ---- | --------------- | --------------- | ---------- | -------- | -------------- |
| `from_meshes.h` | `computeApproximateNormals` / `computeApproximateCovariances` | polygon 数、polygon vertex 数、point 数 | cross product、normal accumulation、per-point covariance 初始化 | mesh polygon vertex 数不规则，normal 写入存在多 polygon 累加 | 头内有真实 inline loop，但入口偏辅助，需二轮判断主成本 |
| `normal_3d.h` | `flipNormalTowardsNormalsMean` 与 `computePointNormal` inline helper | `normal_indices.size()` 或调用方邻域数 | normal sum、dot、flip 可做局部 helper RVV | 主要公开成本在 `impl/normal_3d.hpp`，此文件多为声明/包装 | 保留为 companion candidate（伴随候选），避免漏掉 inline helper |
| `rsd.h` | `getFeaturePointCloud` 的 2D histogram flatten | `histograms2D.size() * rows * cols` | matrix-to-histogram 连续复制/转换 | 主要 RSD 计算在 `impl/rsd.hpp`，这里多为声明和轻量转换 | 有真实模板 helper，但不宜单独实施 |
| `impl/3dsc.hpp` | `ShapeContext3DEstimation::computeFeature` / `computePoint` | `indices_->size()`、每点邻域数、radius/elevation/azimuth bin 数 | 邻域投影、角度、bin 归一化 | search 与 LRF 依赖、acos/angle 边界、histogram scatter | 上游 SHOT 测试覆盖 3DSC，值得二轮复核但主成本受邻域 search 影响 |
| `impl/boundary.hpp` | `BoundaryEstimation::computeFeature` / `isBoundaryPoint` | `indices_->size()`、每点邻域角度数 | 邻域角度计算、max gap 比较、boundary flag 写回 | 角度排序/顺序、search 和阈值边界 | 有测试入口，局部片段清楚但收益可能被 search 稀释 |
| `impl/cppf.hpp` | `CPPFEstimation::computeFeature` 的 all-pairs colored PPF | `indices_->size() * input_->size()` | pair dot/cross、HSV feature、angle/bin 写回 | O(N^2) 但输出 vector append 和 identity pair 分支需保序 | 上游有 CPPF 测试；适合二轮判断是否作为 pair-feature family 一起处理 |
| `impl/crh.hpp` | `CRHEstimation::computeFeature` 的 roll histogram 和 FFT 后写回 | `indices_->size()`、grid point 数、90-bin histogram | normal transform、atan2 bin、histogram/frequency copy | `compute` 调用和 FFT 后端可能主导，bin 冲突 | 有明确 histogram 片段，但主成本边界需确认 |
| `impl/cvfh.hpp` | `extractEuclideanClustersSmooth`、filter/copy normals、VFH delegation | cloud size、cluster size、dominant normal 数 | normal filtering、cluster centroid/normal accumulation | cluster search、NormalEstimation/VFH 调用主导，状态复杂 | 候选主要是 CVFH 前处理与聚合，不宜直接升 high |
| `impl/esf.hpp` | `computeESF`、voxelize/cleanup、histogram normalization | sample size、grid size、histogram bin 数 | random sample 后的 D2/D3/A3 统计、hist normalize | random sampling、voxel state、输出随机性和三维 grid 分支 | 数学密集但随机和状态路径明显，需二轮压实证据 |
| `impl/flare.hpp` | `computeFeature` / `computePointLRF` | `indices_->size()`、tangent neighbors 数 | fitted normal dot/cross、signed distance、LRF 写回 | normal estimation helper 和 shape score 依赖，LRF 方向稳定性 | 有 `test_flare_estimation`，但主成本和风险需下钻 |
| `impl/fpfh_omp.hpp` | `FPFHEstimationOMP::computeFeature` | `indices_->size()`、邻域数 | 与 `impl/fpfh.hpp` 同源的 SPFH/FPFH helper，可复用候选 | OpenMP/RVV 嵌套、shared hist buffer 和线程调度 | 伴随 OMP 入口，二轮应与 `impl/fpfh.hpp` 合并评估 |
| `impl/gasd.hpp` | `GASDEstimation` / `GASDColorEstimation::computeFeature` | shape/color sample 数、grid_size^3 | shape/color histogram coords、trilinear interpolation、copy-out | covariance/eigen 前置、histogram scatter、颜色分支 | 有 `test_gasd_estimation`，局部片段清楚但主成本需确认 |
| `impl/gfpfh.hpp` | `GFPFHEstimation::computeFeature` 及 transition/distance histogram helper | occupied cell pair 数、line histogram 长度、class/bin 数 | transition histogram、mean histogram、HIK distance 规约 | line traversal 和 vector-of-vector 布局不规整 | 存在批量 histogram math，但容器布局和线段遍历风险高 |
| `impl/grsd.hpp` | `GRSDEstimation::computeFeature` | downsampled voxel 数、neighbor 数、class transition bin 数 | transition matrix 统计和 histogram flatten | voxel grid/search 主导，RSD 子估计调用较重 | 适合作为 RSD/GRSD family 复核，不单独 high |
| `impl/intensity_gradient.hpp` | `IntensityGradientEstimation::computeFeature` / `computePointIntensityGradient` | `indices_->size()`、每点邻域数 | intensity mean、gradient least-squares、normal projection | search、Eigen solve、intensity accessor 和 OpenMP 分支 | 有 `test_gradient_estimation`，局部数学明确 |
| `impl/intensity_spin.hpp` | `IntensitySpinEstimation::computeFeature` | `indices_->size()`、邻域数、histogram size | intensity-domain spin histogram、Gaussian weight | search 和 histogram scatter 稀释收益 | 有 spin 测试覆盖，需与 `impl/spin_image.hpp` 对齐 |
| `impl/linear_least_squares_normal.hpp` | `LinearLeastSquaresNormalEstimation::computeFeature` | `indices_->size()`、邻域数 | least-squares matrix accumulation、normal output | Eigen solve 和 search 主导，泛型点类型风险 | 与 normal family 相关，保留二轮复核 |
| `impl/moment_invariants.hpp` | `computePointMomentInvariants` / `computeFeature` | `indices_->size()`、每点邻域数 | centroid 后 moment 累加 | centroid helper/Eigen 规约顺序、search 主导 | 有 `test_invariants_estimation`，局部候选明确 |
| `impl/multiscale_feature_persistence.hpp` | `computeFeaturesAtAllScales`、mean/variance、unique selection | scale 数、每尺度 feature 数 | feature histogram mean/variance 和差异筛选 | 调用外部 feature estimator、list/vector 状态和选择语义 | 候选是后处理统计，不承诺主成本 |
| `impl/normal_3d_omp.hpp` | `NormalEstimationOMP::computeFeature` | `indices_->size()`、邻域数 | 与 `impl/normal_3d.hpp` 共享 normal helper；可评估 OMP/RVV 边界 | OpenMP 调度与 RVV 分流叠加，收益归因复杂 | OMP 伴随入口，必须二轮交代但通常不单独实施 |
| `impl/normal_based_signature.hpp` | `NormalBasedSignatureEstimation::computeFeature` | `indices_->size()`、`N_ * M_`、邻域数 | signature matrix 填充、covariance-like 统计、维度规约 | matrix layout、search 和多层 loop | 数学片段清楚，但可验证入口和热度一般 |
| `impl/our_cvfh.hpp` | `OURCVFHEstimation::computeFeature` / `computeRFAndShapeDistribution` | cloud size、cluster size、grid points、308-bin copy | dominant orientation、shape distribution、histogram copy | region growing/search、cluster state、VFH delegation 主导 | 旧文档列 high；本轮因 search/cluster 稀释降为 mid |
| `impl/pfhrgb.hpp` | `PFHRGBEstimation::computePointPFHRGBSignature` | `indices.size()^2`、histogram bin 数 | RGB pair feature、bin index、histogram copy | all-pairs scatter、RGB ratio 分支、atan2 | 与 `src/pfh.cpp` helper 强相关，二轮可合并 |
| `impl/ppf.hpp` | `PPFEstimation::computeFeature` | `indices_->size() * input_->size()` | pair dot/cross、angle、PPF output append | O(N^2) 输出顺序、identity pair 分支、append 容器 | 有 `test_ppf_estimation`，但 output vector 语义需复核 |
| `impl/ppfrgb.hpp` | `PPFRGBEstimation` / `PPFRGBRegionEstimation::computeFeature` | all-pairs 或 neighbor pair 数 | colored pair feature、region-limited pair loop | 输出 append、RGB helper、region search | 与 PPF/CPPF family 一起二轮复核 |
| `impl/principal_curvatures.hpp` | `computePointPrincipalCurvatures` / `computeFeature` | `indices_->size()`、每点邻域数 | projected normal sum、covariance accumulation | Eigen eigen solver、search 和规约顺序 | 有 curvature 测试，局部数学可 RVV 化但 solver 边界需确认 |
| `impl/range_image_border_extractor.hpp` | `getCoordinateFrameTransformation`、score/helper 函数 | 调用方 range image 像素和局部窗口 | 小公式 helper、neighbor score | 主循环在 `src/range_image_border_extractor.cpp`，本文件多为辅助 | 作为 `src` 主候选的 companion 文件保留 |
| `impl/rift.hpp` | `computeRIFT` / `computeFeature` | `indices_->size()`、每点邻域数、distance/gradient bin 数 | gradient magnitude/angle、bilinear histogram update | acos、histogram scatter、gradient 输入和 search | 有 `test_rift_estimation`，候选明确但风险较多 |
| `impl/rsd.hpp` | `computeRSD` / `RSDEstimation::computeFeature` | `indices_->size()`、每点邻域数、subdivision bin 数 | normal-angle distribution、histogram/minmax | sorted search result 前置、histogram scatter、浮点边界 | 有 `test_rsd_estimation`，需二轮判断 search 稀释 |
| `impl/shot_lrf.hpp` | `SHOTLocalReferenceFrameEstimation::computeFeature` / `getLocalRF` | `indices_->size()`、每点邻域数 | distance weights、dot/cross、LRF sign disambiguation | LRF 稳定性、median sign logic、search | 有 `test_shot_lrf_estimation`；常作为 SHOT 前置 |
| `impl/shot_lrf_omp.hpp` | `SHOTLocalReferenceFrameEstimationOMP::computeFeature` | `indices_->size()`、邻域数 | 与 `impl/shot_lrf.hpp` 共享 LRF math | OpenMP/RVV 叠加和收益归因 | 伴随 OMP 入口，二轮与 SHOT LRF 合并 |
| `impl/shot_omp.hpp` | `SHOTEstimationOMP` / `SHOTColorEstimationOMP::computeFeature` | `indices_->size()`、邻域数、descriptor 长度 | 与 `impl/shot.hpp` 共享 per-point SHOT helper | OpenMP 调度、线程私有 buffer、RVV 归因 | 有 benchmark 入口，但应与 `impl/shot.hpp` 一起评估 |
| `impl/spin_image.hpp` | `computeSiForPoint` / `computeFeature` | `indices_->size()`、邻域数、spin image rows/cols | alpha/beta projection、Gaussian/bin 插值、histogram copy | search、Eigen matrix layout、histogram scatter | 有 `test_spin_estimation`，候选明确但数据布局需确认 |
| `impl/statistical_multiscale_interest_region_extraction.hpp` | `computeRegionsOfInterest` / scale statistics | scale 数、每尺度 point/feature 数 | per-scale统计、均值/阈值筛选 | 依赖其它 feature estimator，list/vector 状态 | 后处理候选，保留但不 high |
| `impl/usc.hpp` | `UniqueShapeContext::computeFeature` / `computePoint` | `indices_->size()`、每点邻域数、bin 数 | 与 3DSC 类似的邻域投影、角度和 descriptor bin | LRF/frame 输入、acos/bin 边界、histogram scatter | 有 SHOT 测试间接覆盖 USC，需二轮确认入口 |
| `impl/vfh.hpp` | `VFHEstimation::computeFeature` / `computePointSPFHSignature` | `indices_->size()`、normal centroid、308-bin copy | centroid/normal sum、SPFH-like pair feature、viewpoint bin | centroid规约、normalize_bins、VFH 常被 CVFH/OURCVFH 调用 | 有 PFH 测试覆盖 VFH，保留二轮复核 |
| `src/cppf.cpp` | `computeCPPFPairFeature` / `RGBtoHSV` | 调用方 pair loop 次数 | pair dot、distance、HSV 转换可做 batched helper | 本文件自身无大 loop；实际 trip count 来自 `impl/cppf.hpp` | 非独立但位于 all-pairs 内层，不能按显式实例化文件处理 |
| `src/narf.cpp` | `Narf` / `NarfDescriptor::computeFeature` 的 descriptor patch、orientation 和 range image 遍历 | descriptor size、patch size、range image width/height、interest point 数 | patch descriptor、orientation score、range image mask 可局部向量化 | `std::multimap`、动态分配、rotation candidates 和 stateful NARF 对象 | 少数 `src` 中真实实现；因状态复杂降为 mid |
| `src/pfh.cpp` | `computePairFeatures` / `computeRGBPairFeatures` | 调用方 PFH/FPFH/VFH/PFHRGB 的邻域 pair 次数 | Darboux frame dot/cross、norm、atan2、RGB ratio helper | 本文件自身无 loop，需 caller-shaped（调用方形态）证据；atan2/acos 语义 | inner helper 热度高，二轮应与 PFH/FPFH/VFH family 绑定 |
| `src/ppf.cpp` | `computePPFPairFeature` | 调用方 PPF pair 次数 | delta norm、normal dot、f1-f4 写回 | 本文件自身无 loop，缺少除零 guard 差异风险需对齐 | pair helper 简洁但非独立，适合 component ablation |

## 6. 全量文件覆盖表

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| ---- | ------ | -------- | -------- | -------- |
| `3dsc.h` | low | 否 | 公开类声明和 impl include；真实 loop 在 `impl/3dsc.hpp` | `impl/3dsc.hpp` |
| `board.h` | low | 否 | 公开类声明和参数/访问器；真实 LRF loop 在 `impl/board.hpp` | `impl/board.hpp` |
| `boundary.h` | low | 否 | 公开类声明、chunk size 和 include；真实边界判断在 impl | `impl/boundary.hpp` |
| `brisk_2d.h` | low | 否 | 公开类声明和 BRISK 内部类型；compute 在 impl | `impl/brisk_2d.hpp` |
| `cppf.h` | low | 否 | 公开类声明；all-pairs loop 在 impl，pair helper 在 src | `impl/cppf.hpp` |
| `crh.h` | low | 否 | 公开类声明；roll histogram 在 impl | `impl/crh.hpp` |
| `cvfh.h` | low | 否 | 公开类声明和状态访问；真实 clustering/aggregation 在 impl | `impl/cvfh.hpp` |
| `don.h` | low | 否 | 公开类声明；逐点差值 loop 在 impl | `impl/don.hpp` |
| `esf.h` | low | 否 | 构造函数初始化 LUT，有小固定 grid loop；主 ESF 在 impl | `impl/esf.hpp` |
| `feature.h` | low | 否 | base class、search 调度和纯虚入口声明，非 RVV 主计算 | `impl/feature.hpp` |
| `flare.h` | low | 否 | 公开类声明和参数；LRF 数学在 impl | `impl/flare.hpp` |
| `fpfh.h` | low | 否 | 公开类声明和参数；SPFH/FPFH loop 在 impl | `impl/fpfh.hpp` |
| `fpfh_omp.h` | low | 否 | OMP 入口声明；真实 loop 在 impl | `impl/fpfh_omp.hpp` |
| `from_meshes.h` | mid | 是 | inline mesh normal/covariance helper 含 polygon/point loops | - |
| `gasd.h` | low | 否 | 公开类声明和 descriptor size；histogram 实现在 impl | `impl/gasd.hpp` |
| `gfpfh.h` | low | 否 | 公开类声明；transition histogram 在 impl | `impl/gfpfh.hpp` |
| `grsd.h` | low | 否 | 公开类声明；GRSD transition loop 在 impl | `impl/grsd.hpp` |
| `impl/3dsc.hpp` | mid | 是 | 3D shape context per-neighbor projection/bin loop | - |
| `impl/board.hpp` | high | 是 | BOARD LRF per-point/per-neighbor dot/cross and frame output | - |
| `impl/boundary.hpp` | mid | 是 | boundary point per-neighbor angle/gap loop | - |
| `impl/brisk_2d.hpp` | high | 是 | integral image、keypoint sampling、descriptor pair loops | - |
| `impl/cppf.hpp` | mid | 是 | all-pairs CPPF compute loop with colored pair features | - |
| `impl/crh.hpp` | mid | 是 | roll histogram、atan2 binning and FFT copy-out | - |
| `impl/cvfh.hpp` | mid | 是 | cluster/filter/normal aggregation loops but search/delegation heavy | - |
| `impl/don.hpp` | high | 是 | direct pointwise normal difference loop | - |
| `impl/esf.hpp` | mid | 是 | ESF sample/histogram/grid loops with random/state risk | - |
| `impl/feature.hpp` | low | 否 | base `Feature::compute` 调度、search 初始化和 output resize | - |
| `impl/flare.hpp` | mid | 是 | FLARE LRF neighbor scoring and signed-distance output | - |
| `impl/fpfh.hpp` | high | 是 | SPFH/FPFH neighbor histogram and weighted accumulation | - |
| `impl/fpfh_omp.hpp` | mid | 是 | OMP FPFH wrapper with same feature loops as FPFH | - |
| `impl/gasd.hpp` | mid | 是 | shape/color histogram coords and interpolation loops | - |
| `impl/gfpfh.hpp` | mid | 是 | occupied-cell pair, transition histogram and HIK distance loops | - |
| `impl/grsd.hpp` | mid | 是 | voxel neighbor transition matrix and histogram flatten | - |
| `impl/integral_image2D.hpp` | high | 是 | organized integral image construction over width/height | - |
| `impl/integral_image_normal.hpp` | high | 是 | organized depth/distance map and normal output loops | - |
| `impl/intensity_gradient.hpp` | mid | 是 | per-point intensity mean/gradient and projection loops | - |
| `impl/intensity_spin.hpp` | mid | 是 | intensity spin histogram loops | - |
| `impl/linear_least_squares_normal.hpp` | mid | 是 | per-point least-squares normal loop with Eigen solve boundary | - |
| `impl/moment_invariants.hpp` | mid | 是 | moment invariant neighborhood accumulation loops | - |
| `impl/moment_of_inertia_estimation.hpp` | high | 是 | point-cloud covariance/projection/minmax reductions | - |
| `impl/multiscale_feature_persistence.hpp` | mid | 是 | scale-feature statistics and uniqueness loops | - |
| `impl/narf.hpp` | low | 否 | mostly `Narf` inline copy/comparison helpers and commented code; real implementation in `src/narf.cpp` | `src/narf.cpp` |
| `impl/normal_3d.hpp` | high | 是 | normal estimation per-index loop calling neighborhood covariance | - |
| `impl/normal_3d_omp.hpp` | mid | 是 | OMP normal estimation wrapper over same per-index helper | - |
| `impl/normal_based_signature.hpp` | mid | 是 | N/M signature matrix and neighborhood accumulation loops | - |
| `impl/organized_edge_detection.hpp` | high | 是 | organized edge classification over width/height and neighbors | - |
| `impl/our_cvfh.hpp` | mid | 是 | OUR-CVFH shape distribution loops, but cluster/search/delegation heavy | - |
| `impl/pfh.hpp` | high | 是 | PFH O(k^2) pair feature histogram loops | - |
| `impl/pfhrgb.hpp` | mid | 是 | PFHRGB all-pairs color/geometry histogram loops | - |
| `impl/ppf.hpp` | mid | 是 | PPF all-pairs feature generation loop | - |
| `impl/ppfrgb.hpp` | mid | 是 | PPFRGB all-pairs/region pair feature loops | - |
| `impl/principal_curvatures.hpp` | mid | 是 | projected-normal covariance loops plus Eigen solver boundary | - |
| `impl/range_image_border_extractor.hpp` | mid | 是 | helper math for range image border scoring; main loops in src | `src/range_image_border_extractor.cpp` |
| `impl/rift.hpp` | mid | 是 | RIFT gradient/distance histogram loops | - |
| `impl/rops_estimation.hpp` | high | 是 | RoPS LRF, projection, distribution matrix and moments loops | - |
| `impl/rsd.hpp` | mid | 是 | RSD normal-angle distribution and histogram loops | - |
| `impl/shot.hpp` | high | 是 | SHOT/SHOTColor neighborhood descriptor loops | - |
| `impl/shot_lrf.hpp` | mid | 是 | SHOT LRF neighbor weighted frame loops | - |
| `impl/shot_lrf_omp.hpp` | mid | 是 | OMP wrapper over SHOT LRF computation | - |
| `impl/shot_omp.hpp` | mid | 是 | OMP wrapper over SHOT descriptor computation | - |
| `impl/spin_image.hpp` | mid | 是 | spin image per-neighbor histogram loops | - |
| `impl/statistical_multiscale_interest_region_extraction.hpp` | mid | 是 | multiscale post-processing statistics loops | - |
| `impl/usc.hpp` | mid | 是 | Unique Shape Context neighbor projection/bin loops | - |
| `impl/vfh.hpp` | mid | 是 | VFH centroid, SPFH-like histogram and viewpoint component loops | - |
| `integral_image2D.h` | low | 否 | traits/class declaration; integral image construction in impl | `impl/integral_image2D.hpp` |
| `integral_image_normal.h` | low | 否 | class declaration and mode settings; real loops in impl | `impl/integral_image_normal.hpp` |
| `intensity_gradient.h` | low | 否 | class declaration/accessors; gradient loop in impl | `impl/intensity_gradient.hpp` |
| `intensity_spin.h` | low | 否 | class declaration/accessors; spin histogram in impl | `impl/intensity_spin.hpp` |
| `linear_least_squares_normal.h` | low | 否 | class declaration; least-squares loop in impl | `impl/linear_least_squares_normal.hpp` |
| `moment_invariants.h` | low | 否 | class declaration; moment loops in impl | `impl/moment_invariants.hpp` |
| `moment_of_inertia_estimation.h` | low | 否 | exported class declaration/accessors; heavy loops in impl | `impl/moment_of_inertia_estimation.hpp` |
| `multiscale_feature_persistence.h` | low | 否 | class declaration and API; statistics loops in impl | `impl/multiscale_feature_persistence.hpp` |
| `narf.h` | low | 否 | NARF class declaration; implementation in `src/narf.cpp` | `src/narf.cpp` |
| `narf_descriptor.h` | low | 否 | NarfDescriptor declaration; implementation in `src/narf.cpp` | `src/narf.cpp` |
| `normal_3d.h` | mid | 是 | inline normal flip/mean helper plus NormalEstimation declaration | `impl/normal_3d.hpp` |
| `normal_3d_omp.h` | low | 否 | OMP class declaration; loop in impl | `impl/normal_3d_omp.hpp` |
| `normal_based_signature.h` | low | 否 | class declaration; signature matrix loops in impl | `impl/normal_based_signature.hpp` |
| `organized_edge_detection.h` | low | 否 | organized edge class declarations and enums; loops in impl | `impl/organized_edge_detection.hpp` |
| `our_cvfh.h` | low | 否 | class declaration/state accessors; OUR-CVFH loops in impl | `impl/our_cvfh.hpp` |
| `pfh.h` | low | 否 | class declaration; histogram loops in impl | `impl/pfh.hpp` |
| `pfh_tools.h` | low | 否 | pair helper declarations only; definitions in `src/pfh.cpp` | `src/pfh.cpp` |
| `pfhrgb.h` | low | 否 | class declaration; PFHRGB loops in impl | `impl/pfhrgb.hpp` |
| `ppf.h` | low | 否 | class declaration and pair helper declaration; definitions elsewhere | `impl/ppf.hpp`, `src/ppf.cpp` |
| `ppfrgb.h` | low | 否 | class declarations; PPFRGB loops in impl | `impl/ppfrgb.hpp` |
| `principal_curvatures.h` | low | 否 | class declaration and OMP chunk setting; loops in impl | `impl/principal_curvatures.hpp` |
| `range_image_border_extractor.h` | low | 否 | exported class and LocalSurface declarations; implementation in src/impl | `src/range_image_border_extractor.cpp` |
| `rift.h` | low | 否 | class declaration; RIFT histogram loops in impl | `impl/rift.hpp` |
| `rops_estimation.h` | low | 否 | class declaration/accessors; RoPS loops in impl | `impl/rops_estimation.hpp` |
| `rsd.h` | mid | 是 | inline histogram point-cloud flatten helper plus RSD declarations | `impl/rsd.hpp` |
| `shot.h` | low | 否 | SHOT class declarations; descriptor loops in impl | `impl/shot.hpp` |
| `shot_lrf.h` | low | 否 | class declaration; LRF loops in impl | `impl/shot_lrf.hpp` |
| `shot_lrf_omp.h` | low | 否 | OMP LRF class declaration; loop in impl | `impl/shot_lrf_omp.hpp` |
| `shot_omp.h` | low | 否 | OMP SHOT class declarations; loops in impl | `impl/shot_omp.hpp` |
| `spin_image.h` | low | 否 | class declaration/accessors; spin loops in impl | `impl/spin_image.hpp` |
| `statistical_multiscale_interest_region_extraction.h` | low | 否 | class declaration; multiscale loops in impl | `impl/statistical_multiscale_interest_region_extraction.hpp` |
| `usc.h` | low | 否 | class declaration; USC loops in impl | `impl/usc.hpp` |
| `vfh.h` | low | 否 | class declaration and small accessor loop; VFH loops in impl | `impl/vfh.hpp` |
| `src/3dsc.cpp` | low | 否 | explicit template instantiation only | `impl/3dsc.hpp` |
| `src/board.cpp` | low | 否 | explicit template instantiation only | `impl/board.hpp` |
| `src/boundary.cpp` | low | 否 | explicit template instantiation only | `impl/boundary.hpp` |
| `src/brisk_2d.cpp` | low | 否 | source shell without candidate loop; implementation in impl/header | `impl/brisk_2d.hpp` |
| `src/cppf.cpp` | mid | 是 | colored pair helper and RGB-to-HSV math used by all-pairs CPPF | `impl/cppf.hpp` |
| `src/crh.cpp` | low | 否 | explicit template instantiation only | `impl/crh.hpp` |
| `src/cvfh.cpp` | low | 否 | explicit template instantiation only | `impl/cvfh.hpp` |
| `src/don.cpp` | low | 否 | explicit template instantiation only | `impl/don.hpp` |
| `src/esf.cpp` | low | 否 | explicit template instantiation only | `impl/esf.hpp` |
| `src/flare.cpp` | low | 否 | explicit template instantiation only | `impl/flare.hpp` |
| `src/fpfh.cpp` | low | 否 | explicit template instantiation only for FPFH/FPFHOMP | `impl/fpfh.hpp` |
| `src/from_meshes.cpp` | low | 否 | explicit template instantiation only | `from_meshes.h` |
| `src/gasd.cpp` | low | 否 | explicit template instantiation only | `impl/gasd.hpp` |
| `src/gfpfh.cpp` | low | 否 | explicit template instantiation only | `impl/gfpfh.hpp` |
| `src/grsd.cpp` | low | 否 | explicit template instantiation only | `impl/grsd.hpp` |
| `src/integral_image_normal.cpp` | low | 否 | explicit template instantiation only | `impl/integral_image_normal.hpp` |
| `src/intensity_gradient.cpp` | low | 否 | explicit template instantiation only | `impl/intensity_gradient.hpp` |
| `src/intensity_spin.cpp` | low | 否 | explicit template instantiation only | `impl/intensity_spin.hpp` |
| `src/linear_least_squares_normal.cpp` | low | 否 | explicit template instantiation only | `impl/linear_least_squares_normal.hpp` |
| `src/moment_invariants.cpp` | low | 否 | explicit template instantiation only | `impl/moment_invariants.hpp` |
| `src/moment_of_inertia_estimation.cpp` | low | 否 | explicit template instantiation only | `impl/moment_of_inertia_estimation.hpp` |
| `src/multiscale_feature_persistence.cpp` | low | 否 | explicit template instantiation only | `impl/multiscale_feature_persistence.hpp` |
| `src/narf.cpp` | mid | 是 | real NARF implementation with range image, descriptor and orientation loops | `narf.h`, `narf_descriptor.h` |
| `src/normal_3d.cpp` | low | 否 | explicit template instantiation only | `impl/normal_3d.hpp` |
| `src/normal_based_signature.cpp` | low | 否 | explicit template instantiation only | `impl/normal_based_signature.hpp` |
| `src/organized_edge_detection.cpp` | low | 否 | explicit template instantiation only | `impl/organized_edge_detection.hpp` |
| `src/our_cvfh.cpp` | low | 否 | explicit template instantiation only | `impl/our_cvfh.hpp` |
| `src/pfh.cpp` | mid | 是 | `computePairFeatures` / `computeRGBPairFeatures` inner helper math | `impl/pfh.hpp`, `impl/fpfh.hpp`, `impl/vfh.hpp` |
| `src/ppf.cpp` | mid | 是 | `computePPFPairFeature` helper math used by PPF all-pairs loops | `impl/ppf.hpp` |
| `src/principal_curvatures.cpp` | low | 否 | explicit template instantiation only | `impl/principal_curvatures.hpp` |
| `src/range_image_border_extractor.cpp` | high | 是 | real range image border implementation with organized image loops | `range_image_border_extractor.h` |
| `src/rift.cpp` | low | 否 | explicit template instantiation only | `impl/rift.hpp` |
| `src/rops_estimation.cpp` | low | 否 | explicit template instantiation only | `impl/rops_estimation.hpp` |
| `src/rsd.cpp` | low | 否 | explicit template instantiation only | `impl/rsd.hpp` |
| `src/shot.cpp` | low | 否 | explicit template instantiation only | `impl/shot.hpp`, `impl/shot_omp.hpp` |
| `src/shot_lrf.cpp` | low | 否 | explicit template instantiation only | `impl/shot_lrf.hpp`, `impl/shot_lrf_omp.hpp` |
| `src/spin_image.cpp` | low | 否 | explicit template instantiation only | `impl/spin_image.hpp` |
| `src/statistical_multiscale_interest_region_extraction.cpp` | low | 否 | explicit template instantiation only | `impl/statistical_multiscale_interest_region_extraction.hpp` |
| `src/usc.cpp` | low | 否 | explicit template instantiation only | `impl/usc.hpp` |
| `src/vfh.cpp` | low | 否 | explicit template instantiation only | `impl/vfh.hpp` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
- 本轮未修改生产源码、未创建 `test-rvv/features/*` topic、未运行 bench，也未提交。
