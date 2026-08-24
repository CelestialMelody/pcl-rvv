# features 模块 RVV 第二轮函数评估队列

本文档基于 `doc-rvv/library-screening/modules/features-file-candidate-screening.zh.md` 的第一轮 `high/mid` 文件候选，执行第二轮函数级下钻筛选。第二轮只形成后续函数级评估队列，不修改 production 源码，不建立 `test-rvv` topic，不运行板卡 bench，也不提交。

## 1. 输入依据

- 指令与模板：`.agents/skills/rvv-screening/SKILL.md`、`screening-criteria.md`、`stage-and-queue-policy.md`、`evidence-boundaries.md`、`function-evaluation-queue-template.md`。
- 第一轮基线：`features-file-candidate-screening.zh.md`，统计为 `137` 个源码文件、`13 high / 40 mid / 84 low`。
- 源码范围：`features/include/pcl/features/**`、`features/src/**`。
- 测试入口：`test/features/CMakeLists.txt` 中的 normal、PFH/FPFH/VFH、SHOT、SHOT LRF、boundary、curvatures、spin、RSD/GRSD、invariants、BOARD、FLARE、CPPF、PPF、integral image normal、moment of inertia、ROPS、GASD、organized edge、BRISK、NARF 等测试。
- benchmark 入口：`benchmarks/features/normal_3d.cpp` 与 `benchmarks/features/shot.cpp`。第二轮未运行这些 benchmark，只登记其可用性。

## 2. 二轮筛选统计

| 项目 | 数量 | 说明 |
| ---- | ---: | ---- |
| 第一轮源码文件总数 | 137 | `features/**` 文件级覆盖数 |
| 第一轮 high | 13 | 第二轮必查 |
| 第一轮 mid | 40 | 第二轮必查 |
| 第一轮 low | 84 | 默认不进入第二轮，除非发现明确漏判 |
| 第二轮初始候选基线 | 53 | `high + mid` 去重后数量 |
| 新增补充候选 | 0 | 未发现需要从 `low` 补入的明确源码漏判 |
| 第二轮候选总数 | 53 | 初始基线 `53` + 补入 `0` |
| 建议进行 RVV 优化的文件 | 8 | 覆盖公开入口主路径，且测试/bench 路径相对清楚 |
| 保留实施的候选文件 | 29 | 有 RVV 片段，但需先回答数据布局、主成本、helper 边界或测试形态 |
| 暂缓或不推荐考虑 RVV 优化的文件 | 16 | 当前证据不支持独立 RVV 主题 |
| 源码冲突、删除或需要修正第一轮路径 | 0 | 未发现文件删除或路径冲突 |
| 明确不单独实施的 high/mid 文件 | 7 | 多为 companion header 或 OMP wrapper；可随主文件复用证据 |
| 第一轮 high/mid 降级为暂缓或不推荐 | 16 | 主要原因是 helper-only、OpenMP-wrapper-only、search/FFT/cluster/state 主导或测试/bench 不足 |

## 3. 文件级变化理由

第二轮没有补入 `low` 文件。第一轮 `low` 中的公开 `.h` 文件、显式实例化 `.cpp` 和薄 wrapper 与当前源码一致，真实循环已由对应 `impl/*.hpp` 或 `src/*.cpp` 的 `high/mid` 行覆盖。

主题归并只用于共享判断，不改变后续文件级推进粒度：

| 主题簇 | 共享判断 |
| ------ | -------- |
| normal / covariance | `impl/normal_3d.hpp` 有 benchmark 和公开主入口，优先进入建议队列；`normal_3d.h`、`normal_3d_omp.hpp` 只作为 helper/OMP 伴随证据，不单独实施。`linear_least_squares_normal`、`principal_curvatures`、`moment_invariants` 有局部规约价值，但 search/Eigen/organized 条件需要先证实。 |
| organized image | `integral_image_normal` 和 `organized_edge_detection` 覆盖 organized 主路径；`integral_image2D` 前缀和依赖强，先保留；`range_image_border_extractor.cpp` 真实循环多但标签状态复杂，保留做 production-shaped diagnostic。 |
| PFH / FPFH / VFH / PFHRGB | `PFH` 与 `FPFH` 的 per-neighbor/pair histogram 是公开主成本，建议进入函数级评估；`VFH`、`PFHRGB`、`src/pfh.cpp` 需要依赖 caller-shaped helper 证据，保留。 |
| SHOT / LRF / shape descriptors | `SHOT` 有专用 benchmark，建议先做；`SHOT OMP` 与 `SHOT LRF OMP` 不单独实施；`SHOT LRF`、`3DSC`、`USC`、`spin`、`RIFT` 保留，先回答 LRF、histogram scatter 和 descriptor layout 风险。 |
| PPF / PPFRGB / CPPF | all-pairs loop 有规模价值，但输出 `push_back`、identity pair 分支、helper API 和顺序语义需要先做 component ablation，暂不排入建议队列。 |
| global descriptor family | `moment_of_inertia` 的点云规约/投影循环直接且测试清楚，建议；`ROPS`、`GASD`、`ESF` 保留；`CVFH/OUR-CVFH/GFPFH/GRSD` 当前主要被 search、cluster、voxel 或 vector-of-vector 状态稀释。 |
| multiscale/postprocess | `multiscale_feature_persistence` 与 `statistical_multiscale_interest_region_extraction` 主要调用外部 estimator 或维护 list/vector 状态，不建议作为独立 RVV 主题。 |

## 4. 建议进行 RVV 优化的文件

| 文件路径 | 关键入口 / 函数族 | 所属主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 主要风险 | 测试/bench 可行性 | 第二轮去向 | 建议实施顺序 |
| -------- | ---------------- | ---------- | -------------- | ------------------------- | ---------- | -------- | ---------------- | ---------- | ------------ |
| `features/include/pcl/features/impl/don.hpp` | `DifferenceOfNormalsEstimation::computeFeature` | normal / pointwise | `direct-main-path` | `input_->size()`，逐点比较小尺度/大尺度 normal | 三个 normal 分量差、曲率/输出字段写回，适合 VL strip-mining 与 mask finite 对拍 | 真实成本可能被前置 normal estimation 稀释；需保持 NaN/finite 与 normal cloud 尺寸检查 | 可用小型 synthetic cloud 做 scalar/RVV 对拍；无专用 bench，可建立轻量 micro/feature bench | 建议进行 RVV 优化 | 1 |
| `features/include/pcl/features/impl/normal_3d.hpp` | `NormalEstimation::computeFeature` -> `computePointNormal` | normal / covariance | `direct-main-path` | `indices_->size()` 与每点 `nn_indices.size()`；benchmark 以 KSearch 50/100 覆盖 | 邻域 centroid/covariance 累加、normal 翻转 helper、NaN 输出填充可拆分评估 | search 与 Eigen 3x3 求解可能稀释收益；indices gather、浮点规约顺序和点类型布局需对拍 | `test_normal_estimation` 与 `benchmarks/features/normal_3d.cpp` 可支撑 correctness + production-shaped bench | 建议进行 RVV 优化 | 2 |
| `features/include/pcl/features/impl/integral_image_normal.hpp` | `IntegralImageNormalEstimation::computeFeature`、`computeFeatureFull`、`computeFeaturePart` | organized image / normal | `direct-main-path` | `input_->width * input_->height` 或 `indices_->size()` | depth-change map、distance map、organized output fill、finite mask、normal/curvature 写回 | `IntegralImage2D` 前缀依赖、border/mirror policy、NaN/Inf 与 bad-point mask 语义 | `test_ii_normals` 覆盖 table scene；可建立 organized synthetic 对拍，bench 可复用 normal_3d 数据形态但需新增专项 bench | 建议进行 RVV 优化 | 3 |
| `features/include/pcl/features/impl/shot.hpp` | `SHOTEstimation::computePointSHOT`、`SHOTColorEstimation::computePointSHOT`、`computeFeature` | SHOT descriptor | `direct-main-path` | `indices_->size()`、每点 `nn_indices.size()`、`descLength_` 352/1344 | 邻域几何/颜色 bin 计算、descriptor normalize、invalid LRF 时 NaN fill、输出 copy | search/LRF 依赖、histogram scatter、颜色路径、OpenMP 伴随实现、NaN 输出语义 | `test_shot_estimation` 与 `benchmarks/features/shot.cpp` 直接可用 | 建议进行 RVV 优化 | 4 |
| `features/include/pcl/features/impl/fpfh.hpp` | `computePointSPFHSignature`、`weightPointSPFHSignature`、`computeFeature` | PFH family | `direct-main-path` | `indices_->size()`、SPFH 邻域数、FPFH 加权邻居数、bin 数 | pair feature 批量计算、SPFH histogram、邻居距离权重、33-bin copy/normalize | search/indices gather、histogram 冲突累加、非结合浮点规约、`src/pfh.cpp` helper 绑定 | `test_pfh_estimation` 覆盖 FPFH/PFH/VFH；可建立 component ablation 与 caller-shaped helper 对拍 | 建议进行 RVV 优化 | 5 |
| `features/include/pcl/features/impl/pfh.hpp` | `computePointPFHSignature`、`computeFeature` | PFH family | `direct-main-path` | `indices_->size()` 与每点邻域 pair 数 `k*(k-1)/2`、histogram bin 数 | O(k^2) pair feature、bin index、histogram normalize/copy | all-pairs histogram scatter、`atan2`/`acos`、pair order、非结合累加 | `test_pfh_estimation` 可用；建议与 `src/pfh.cpp` helper 共享 component ablation | 建议进行 RVV 优化 | 6 |
| `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` | `compute`、`computeCovarianceMatrix`、AABB/OBB、projection/eccentricity loops | global descriptor / reductions | `direct-main-path` | `number_of_points`，另有固定角度扫描乘以点数 | 点坐标 min/max、centroid/covariance、投影规约、AABB/OBB 边界更新 | Eigen eigen-solver 仍为标量；浮点规约顺序、min/max tie 与角度扫描稳定性 | `test_moment_of_inertia_estimation` 可用；可构造 scalar/RVV 对拍和 component bench | 建议进行 RVV 优化 | 7 |
| `features/include/pcl/features/impl/organized_edge_detection.hpp` | `OrganizedEdgeBase::extractEdges`、RGB/normal variants、`assignLabelIndices` | organized image / edge | `direct-main-path` | `input_->width * input_->height`、局部 neighbor scan、label output size | organized 行列阈值比较、depth/normal/RGB mask、label 初始化与部分 label index 收集 | 边类型多标签、深分支、search-neighbor scan、label index 输出顺序 | `test_organized_edge_detection` 可用；需先做 label-equivalence correctness 和 production-shaped diagnostic | 建议进行 RVV 优化 | 8 |

## 5. 保留实施的候选文件

| 文件路径 | 关键入口 / 函数族 | 所属主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 主要风险 | 测试/bench 可行性 | 第二轮去向 | 建议实施顺序 |
| -------- | ---------------- | ---------- | -------------- | ------------------------- | ---------- | -------- | ---------------- | ---------- | ------------ |
| `features/include/pcl/features/impl/board.hpp` | `BOARDLocalReferenceFrameEstimation::computeFeature`、`computePointLRF` | LRF / BOARD | `diagnostic` | `indices_->size()`、每点 Rz/Rx 邻域数、`check_margin_array_` | 邻域 dot/cross、distance weight、方向投票与 frame 写回 | margin state、LRF 符号稳定性、search 主导、局部控制流复杂 | `test_board_estimation` 可用；需先做 LRF sign-preserving diagnostic | 保留实施 | 9 |
| `features/include/pcl/features/impl/brisk_2d.hpp` | `BRISK2DEstimation::compute`、integral image、long/short pair descriptor loops | organized 2D descriptor | `diagnostic` | `image_data.size()`、`height * width`、`keypoints.size()`、BRISK pair 数 | image integral、smoothed intensity、pair compare、descriptor bit generation | integral prefix 依赖、rotated sampling、border checks、bit packing | `test_brisk` 受 `BUILD_keypoints` 条件约束；可做 image-shaped diagnostic | 保留实施 | 10 |
| `features/include/pcl/features/impl/integral_image2D.hpp` | `IntegralImage2D::setInput`、`computeIntegralImages` | organized image / integral base | `partial-preprocess` | `width_ * height_ * Dimension` | 多维字段 load、second-order cross term、row/column staging | 行内 prefix sum 强依赖；收益需证明能覆盖 `integral_image_normal` 主路径 | 可被 `test_ii_normals` 间接覆盖；应跟随 `integral_image_normal` 评估 | 保留实施 | 11 |
| `features/include/pcl/features/impl/rops_estimation.hpp` | `ROPSEstimation::computeFeature`、`computeLRF`、distribution matrix、central moments | ROPS descriptor | `diagnostic` | `indices_->size()`、局部 triangles/points、旋转轴/角度、`number_of_bins_^2` | 投影、distribution matrix、central moments、entropy/feature copy | mesh triangle set、Eigen、bin scatter、旋转循环状态复杂 | `test_rops_estimation` 有专用 mesh/indices/triangles 输入；适合 component ablation | 保留实施 | 12 |
| `features/src/range_image_border_extractor.cpp` | `RangeImageBorderExtractor::computeFeature`、surface change、border traits、blur/threshold | range image / border | `diagnostic` | `range_image_->width * range_image_->height` 与局部半径窗口 | organized mask、neighbor score、border trait 写回、blur/threshold loops | `surface_structure_` 指针数组、标签状态、窗口深分支、输出类别稳定性 | 当前 `test/features` 未见专用 range border test；需先补 production-shaped diagnostic | 保留实施 | 13 |
| `features/include/pcl/features/impl/3dsc.hpp` | `ShapeContext3DEstimation::computeFeature`、`computePoint` | shape context descriptor | `diagnostic` | `indices_->size()`、每点邻域数、radius/elevation/azimuth bins | 邻域投影、角度、bin index、descriptor normalize | search/LRF 依赖、`acos` 边界、histogram scatter | 可借 SHOT/3DSC 测试路径确认；缺 bench | 保留实施 | 14 |
| `features/include/pcl/features/impl/boundary.hpp` | `BoundaryEstimation::computeFeature`、`isBoundaryPoint` | boundary / normal | `diagnostic` | `indices_->size()` 与每点邻域角度数 | 邻域角度、max gap、boundary flag | 角度排序与 gap 顺序语义、search 稀释、阈值边界 | `test_boundary_estimation` 可用；需先隔离排序前后成本 | 保留实施 | 15 |
| `features/include/pcl/features/impl/cppf.hpp` | `CPPFEstimation::computeFeature` | PPF pair family | `diagnostic` | `indices_->size() * input_->size()` | colored pair feature、HSV/angle/distance、output field fill | O(N^2) 输出 `push_back`、identity pair 分支、helper 绑定、顺序语义 | `test_cppf_estimation` 可用；先做 output-staging component ablation | 保留实施 | 16 |
| `features/include/pcl/features/impl/esf.hpp` | `ESFEstimation::computeESF`、`voxelize9`、`cleanup9`、hist normalize | global descriptor / ESF | `diagnostic` | sample size、local cloud size、voxel grid、histogram bins | D2/D3/A3 统计、histogram normalize、voxel occupancy loops | random sample/state、grid branch、single global descriptor，收益需 profile | 可从 `test_gasd_estimation` 类似输入另建 ESF diagnostic；当前无 CMake test 行 | 保留实施 | 17 |
| `features/include/pcl/features/impl/flare.hpp` | `FLARELocalReferenceFrameEstimation::computeFeature`、`computePointLRF` | LRF / FLARE | `diagnostic` | `indices_->size()`、tangent/support neighbors | fitted normal、signed distance、LRF frame 写回 | internal normal estimation、search、LRF sign 与 shape score 稳定性 | `test_flare_estimation` 可用；需先 LRF equivalence 对拍 | 保留实施 | 18 |
| `features/include/pcl/features/impl/gasd.hpp` | `GASDEstimation::computeFeature`、`GASDColorEstimation::computeFeature` | global descriptor / GASD | `diagnostic` | shape/color samples、`grid_size^3`、histogram bins | shape/color sample projection、trilinear interpolation、histogram copy | covariance/eigen 前置、histogram scatter、颜色分支 | `test_gasd_estimation` 可用；需要 component bench 判定主成本 | 保留实施 | 19 |
| `features/include/pcl/features/impl/intensity_gradient.hpp` | `computePointIntensityGradient`、`computeFeature` | intensity / gradient | `diagnostic` | `indices_->size()`、每点邻域数 | intensity mean、gradient least-squares accumulation、normal projection | search、Eigen solve、intensity accessor、OpenMP 分支 | `test_gradient_estimation` 可用；需隔离 solve 前后成本 | 保留实施 | 20 |
| `features/include/pcl/features/impl/intensity_spin.hpp` | `IntensitySpinEstimation::computeFeature` | intensity descriptor | `diagnostic` | `indices_->size()`、邻域数、intensity/distance bins | intensity-domain spin histogram、Gaussian/bin 插值、descriptor copy | search、histogram scatter、bin 边界 | `test_spin_estimation` 覆盖 spin family；可做 histogram-only diagnostic | 保留实施 | 21 |
| `features/include/pcl/features/impl/linear_least_squares_normal.hpp` | `LinearLeastSquaresNormalEstimation::computeFeature`、`computePointNormal` | normal / organized | `diagnostic` | organized `width * height` 与 smoothing window sample | organized window sample、least-squares accumulation、normal output | Eigen solve、finite checks、organized-only 入口热度不明 | 无专门测试行；可借 normal synthetic organized cloud 建 diagnostic | 保留实施 | 22 |
| `features/include/pcl/features/impl/moment_invariants.hpp` | `computePointMomentInvariants`、`computeFeature` | normal / moments | `diagnostic` | `indices_->size()` 与每点邻域数 | centroid 后 moment 累加、three-value output | search、centroid helper、浮点规约顺序 | `test_invariants_estimation` 可用；需确认 helper 覆盖主成本 | 保留实施 | 23 |
| `features/include/pcl/features/impl/normal_based_signature.hpp` | `NormalBasedSignatureEstimation::computeFeature` | normal descriptor | `diagnostic` | `indices_->size()`、`N_ * M_`、邻域数 | signature matrix fill、neighborhood accumulation、small matrix copy | matrix layout、search、多层 loop、测试入口不明显 | 需新增 focused correctness input；无现成 benchmark | 保留实施 | 24 |
| `features/include/pcl/features/impl/pfhrgb.hpp` | `computePointPFHRGBSignature`、`computeFeature` | PFH family / color | `diagnostic` | `indices_->size()`、每点邻域 pair 数、color bins | RGB pair feature、bin index、histogram copy | all-pairs scatter、RGB ratio 分支、`atan2`、helper 绑定 | `test_pfh_estimation` 间接覆盖 PFHRGB 需确认；可跟随 PFH helper ablation | 保留实施 | 25 |
| `features/include/pcl/features/impl/ppf.hpp` | `PPFEstimation::computeFeature` | PPF pair family | `diagnostic` | `indices_->size() * input_->size()` | pair dot/cross/angle、PPF output fields、identity mask | output `push_back` 顺序、identity skip、当前实现调用 `computePairFeatures` 而非 `computePPFPairFeature` | `test_ppf_estimation` 可用；先做 output-staging 与 helper-choice ablation | 保留实施 | 26 |
| `features/include/pcl/features/impl/ppfrgb.hpp` | `PPFRGBEstimation`、`PPFRGBRegionEstimation::computeFeature` | PPF pair family / color | `diagnostic` | all-pairs 或 region neighbor pair 数 | colored pair feature、region-limited loop、output fields | output append、RGB helper、region search、顺序语义 | `test_ppf_estimation` 可能覆盖 PPFRGB 需确认；建议跟随 PPF/CPPF | 保留实施 | 27 |
| `features/include/pcl/features/impl/principal_curvatures.hpp` | `computePointPrincipalCurvatures`、`computeFeature` | normal / curvature | `diagnostic` | `indices_->size()`、每点邻域 normal 数 | projected normal sum、covariance accumulation | Eigen eigen solver、search、规约顺序 | `test_curvatures_estimation` 可用；先做 covariance-only ablation | 保留实施 | 28 |
| `features/include/pcl/features/impl/rift.hpp` | `computeRIFT`、`computeFeature` | intensity / RIFT | `diagnostic` | `indices_->size()`、邻域数、gradient/distance bins | gradient magnitude/angle、bilinear histogram update | `acos`、histogram scatter、gradient 输入、search | `test_rift_estimation` 可用；需 histogram-only diagnostic | 保留实施 | 29 |
| `features/include/pcl/features/impl/rsd.hpp` | `computeRSD`、`RSDEstimation::computeFeature` | RSD / local descriptor | `diagnostic` | `indices_->size()`、每点邻域数、subdivision bins | normal-angle distribution、min/max by distance、histogram flatten | sorted search result 前置、histogram scatter、浮点边界 | `test_rsd_estimation` 可用；先隔离 sorted search 与 computeRSD | 保留实施 | 30 |
| `features/include/pcl/features/impl/shot_lrf.hpp` | `SHOTLocalReferenceFrameEstimation::getLocalRF`、`computeFeature` | SHOT LRF | `diagnostic` | `indices_->size()`、每点 radius neighbors | distance weights、covariance/eigen 前后、sign disambiguation、RF write | LRF 稳定性、median/sign logic、search 与 Eigen | `test_shot_lrf_estimation` 可用；应跟随 SHOT 主题复用输入 | 保留实施 | 31 |
| `features/include/pcl/features/impl/spin_image.hpp` | `computeSiForPoint`、`computeFeature` | spin descriptor | `diagnostic` | `indices_->size()`、邻域数、spin rows/cols | alpha/beta projection、Gaussian/bin 插值、descriptor matrix copy | search、Eigen/ArrayXXd layout、histogram scatter、normalization checks | `test_spin_estimation` 可用；需 layout/staging diagnostic | 保留实施 | 32 |
| `features/include/pcl/features/impl/usc.hpp` | `UniqueShapeContext::computePointDescriptor`、`computeFeature` | shape context descriptor | `diagnostic` | `indices_->size()`、邻域数、radius/elevation/azimuth bins | 邻域投影、angle/bin、descriptor normalize | LRF/frame 输入、`acos` 边界、histogram scatter | 可与 3DSC/SHOT shape tests 共享输入；缺 bench | 保留实施 | 33 |
| `features/include/pcl/features/impl/vfh.hpp` | `VFHEstimation::computeFeature`、`computePointSPFHSignature` | PFH family / global | `diagnostic` | `indices_->size()`、centroid/normal centroid、308-bin output | centroid/normal sum、SPFH-like pair features、viewpoint component | centroid 规约、normalize bins、常被 CVFH/OUR-CVFH 调用导致成本归因复杂 | `test_pfh_estimation` 覆盖 VFH；可跟随 PFH helper 评估 | 保留实施 | 34 |
| `features/src/pfh.cpp` | `computePairFeatures`、`computeRGBPairFeatures` | PFH shared helper | `non-standalone` | 无本地 loop；trip count 来自 PFH/FPFH/VFH/PFHRGB caller 的 pair 次数 | Darboux frame dot/cross、norm、`atan2`、RGB ratio helper 的 batched caller-shaped 版本 | 文件自身无批量入口；需要 caller API/staging，数学函数语义敏感 | 通过 PFH/FPFH/VFH tests 间接验证；只随 caller topic 实施 | 保留实施 | 35 |
| `features/src/ppf.cpp` | `computePPFPairFeature` | PPF shared helper | `non-standalone` | 无本地 loop；trip count 来自 PPF/PPFRGB caller pair 次数 | delta norm、normal dot、f1-f4 写回的 batched helper | 当前 `impl/ppf.hpp` 调用路径偏向 `computePairFeatures`；除零/angle 语义需核对 | 通过 `test_ppf_estimation` 间接验证；先确认真实 caller 使用 | 保留实施 | 36 |
| `features/src/cppf.cpp` | `computeCPPFPairFeature`、`RGBtoHSV` | CPPF shared helper | `non-standalone` | 无本地 loop；trip count 来自 `impl/cppf.hpp` all-pairs | pair dot/distance/angle、HSV conversion batching | helper-only、HSV branch、all-pairs output append 仍在 caller | `test_cppf_estimation` 可间接验证；随 CPPF caller 实施 | 保留实施 | 37 |

## 6. 暂缓或不推荐考虑 RVV 优化的文件

| 文件路径 | 关键入口 / 函数族 | 所属主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 主要风险 | 测试/bench 可行性 | 第二轮去向 | 建议实施顺序 |
| -------- | ---------------- | ---------- | -------------- | ------------------------- | ---------- | -------- | ---------------- | ---------- | ------------ |
| `features/include/pcl/features/from_meshes.h` | `computeApproximateNormals`、`computeApproximateCovariances` inline helpers | mesh helper | `diagnostic` | polygon 数、polygon vertices、point 数 | polygon normal cross product、per-point covariance initialization | polygon vertex 数不规则，normal accumulation 跨 polygon 写入冲突，入口偏辅助 | 仅显式实例化与间接使用；缺专门 bench | 暂缓；仅在 mesh-to-feature profile 指向时重开 | 不排期 |
| `features/include/pcl/features/normal_3d.h` | `flipNormalTowardsNormalsMean`、inline `computePointNormal` declarations/helpers | normal companion | `non-standalone` | helper 的 `normal_indices.size()` 来自 `impl/normal_3d.hpp` caller | normal sum、dot、flip | 公开主成本已在 `impl/normal_3d.hpp`；本文件多为声明/inline helper | 随 `impl/normal_3d.hpp` 测试即可 | 不推荐单独实施；作为 normal 主题伴随文件 | 不排期 |
| `features/include/pcl/features/rsd.h` | `getFeaturePointCloud` histogram flatten helper | RSD companion | `non-standalone` | `histograms2D.size() * rows * cols` | Matrix-to-histogram copy/flatten | 主要 RSD 成本在 `impl/rsd.hpp`；这里偏转换和输出组装 | 随 RSD test 间接覆盖 | 不推荐单独实施；可随 RSD 主题检查 | 不排期 |
| `features/include/pcl/features/impl/fpfh_omp.hpp` | `FPFHEstimationOMP::computeFeature` | PFH family / OMP | `non-standalone` | `indices_->size()`、邻域数，与 FPFH scalar 共享 | 与 `impl/fpfh.hpp` 相同 SPFH/FPFH helper | OpenMP/RVV 嵌套、线程调度、收益归因复杂 | 可随 FPFH tests；无单独 bench | 不推荐单独实施；等待 scalar FPFH 证据 | 不排期 |
| `features/include/pcl/features/impl/crh.hpp` | `CRHEstimation::computeFeature` roll histogram、FFT copy-out | CRH / histogram | `tail-compress` | `indices_->size()`、grid points、90-bin histogram | normal transform、atan2 bin、frequency copy | FFT 后端与 histogram conflict 可能主导；bin 数固定且较小 | 当前无 CMake 测试行；验证成本偏高 | 暂缓；需 profile 证明 roll histogram 是热点 | 不排期 |
| `features/include/pcl/features/impl/cvfh.hpp` | `extractEuclideanClustersSmooth`、normal filter/copy、VFH delegation | CVFH global descriptor | `partial-preprocess` | cloud size、cluster size、dominant normal 数 | normal filtering、cluster centroid/normal accumulation | region growing/search 与 VFH delegation 主导，状态复杂 | `test_cvfh_estimation` 可用但难隔离收益 | 暂缓；等 VFH/PFH 证据和 cluster profile | 不排期 |
| `features/include/pcl/features/impl/gfpfh.hpp` | occupied-cell pair line traversal、transition histogram、HIK distance | GFPFH / grid descriptor | `diagnostic` | occupied cell pair 数、line histogram 长度、class/bin 数 | transition histogram、mean histogram、distance reduction | vector-of-vector 布局、line traversal 不规整、branch-heavy | 当前无专门 CMake test 行；可构造性一般 | 暂缓；需代表数据与 layout redesign 证据 | 不排期 |
| `features/include/pcl/features/impl/grsd.hpp` | `GRSDEstimation::computeFeature` voxel neighbor transition | RSD / voxel descriptor | `partial-preprocess` | downsampled voxel 数、neighbor 数、class transition bins | transition matrix 与 histogram flatten | voxel grid/search 主导，RSD 子估计与 neighbor traversal 稀释收益 | `test_grsd_estimation` 可用；但主成本隔离难 | 暂缓；随 RSD/GASD 证据复筛 | 不排期 |
| `features/include/pcl/features/impl/multiscale_feature_persistence.hpp` | `computeFeaturesAtAllScales`、mean/persistence/unique selection | multiscale postprocess | `tail-compress` | scale 数、每尺度 feature 数 | feature mean/variance、distance-to-mean | 外部 feature estimator 主导，list/vector selection 语义和动态容器 | 无明确 bench；测试入口不集中 | 不推荐独立 RVV；只有 profile 指向后处理时重开 | 不排期 |
| `features/include/pcl/features/impl/normal_3d_omp.hpp` | `NormalEstimationOMP::computeFeature` | normal / OMP | `non-standalone` | `indices_->size()`、邻域数，与 normal scalar 共享 | 与 `impl/normal_3d.hpp` helper 相同 | OpenMP/RVV 叠加、线程 chunk、收益归因复杂 | `benchmarks/features/normal_3d.cpp` 有 OMP bench；先等 scalar normal 证据 | 不推荐单独实施；作为 normal 主题对照 | 不排期 |
| `features/include/pcl/features/impl/our_cvfh.hpp` | smooth clustering、dominant orientation、shape distribution、VFH-like output | OUR-CVFH global descriptor | `partial-preprocess` | cloud size、cluster size、grid points、308-bin copy | dominant orientation loops、shape distribution、hist copy | region growing/search、cluster state、VFH delegation、复杂转换集合 | `test_cvfh_estimation` 覆盖 OUR-CVFH；收益隔离成本高 | 暂缓；等 VFH/PFH 与 cluster profile | 不排期 |
| `features/include/pcl/features/impl/range_image_border_extractor.hpp` | coordinate/frame/helper scoring functions | range image companion | `non-standalone` | 调用方像素与窗口半径 | 小公式 helper、neighbor score | 主循环在 `features/src/range_image_border_extractor.cpp`；本文件多为 helper | 随 src 文件 diagnostic 覆盖 | 不推荐单独实施；作为 range-image src 伴随文件 | 不排期 |
| `features/include/pcl/features/impl/shot_lrf_omp.hpp` | `SHOTLocalReferenceFrameEstimationOMP::computeFeature` | SHOT LRF / OMP | `non-standalone` | `indices_->size()`、邻域数 | 与 `impl/shot_lrf.hpp` LRF math 相同 | OpenMP/RVV 叠加、LRF 稳定性、收益归因 | `test_shot_lrf_estimation` 可随 scalar LRF 覆盖 | 不推荐单独实施；等待 scalar SHOT LRF 证据 | 不排期 |
| `features/include/pcl/features/impl/shot_omp.hpp` | `SHOTEstimationOMP`、`SHOTColorEstimationOMP::computeFeature` | SHOT / OMP | `non-standalone` | `indices_->size()`、邻域数、descriptor length | 与 `impl/shot.hpp` per-point helper 相同 | OpenMP 调度、线程私有 buffer、RVV 归因 | `benchmarks/features/shot.cpp` 有 OMP variants；先做 scalar SHOT | 不推荐单独实施；作为 SHOT 主题对照 | 不排期 |
| `features/include/pcl/features/impl/statistical_multiscale_interest_region_extraction.hpp` | geodesic/density/statistical ROI loops | multiscale/statistical ROI | `diagnostic` | scale 数、`input_->size()^2` 或每尺度 search 结果 | density/statistics、threshold selection | geodesic distances、search、O(N^2) state arrays、ROI selection 语义 | 当前无专门 test/bench；构造 oracle 成本高 | 暂缓；需 profile 和 representative ROI oracle | 不排期 |
| `features/src/narf.cpp` | `NarfDescriptor::computeFeature`、descriptor patch、orientation scoring | NARF / range image | `diagnostic` | descriptor size、patch size、range image width/height、interest point 数 | patch descriptor、orientation score、range image masks | `std::multimap`、dynamic allocation、rotation candidates、stateful `Narf` objects | `test_narf` 可用，但优化边界难局部化 | 暂缓；优先保留为 range-image diagnostic 后续复筛项 | 不排期 |

## 7. 执行清单 / 状态表

执行清单只列入第二轮建议队列中可直接启动函数级评估的文件。保留候选与暂缓候选已在第 5/6 节逐文件列出；后续复筛从对应表读取，不在状态表中重复做主题汇总。

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 状态 | 当前结论 / 下一步条件 |
| ----: | ---- | ------ | ------------------------ | ---- | -------------------- |
| 1 | DON pointwise normal difference | `impl/don.hpp` | `computeFeature` 的 normal 差与输出写回 | 暂停（no-production） | 已建立 `test-rvv/features/don/`；helper-only diagnostic 5-run board 为 `weak_positive`，但 production-public 5-run board 为 `negative`（median `0.908x`，`B/A < 1 = 5/5`，Doctor Errors=1）。用户已确认回滚 RVV 分流；phase 050 消融显示 finite-only no-mask median `0.721x`、no-sqrt zero-curvature median `1.125x` 但破坏 curvature 语义、normal-only median `1.025x` near-threshold。当前没有保持 production 语义且值得继续推进的 RVV production 方向；只保留 `PCLBase::initCompute()` 独立正确性修复，可另行评审 |
| 2 | NormalEstimation covariance helper | `impl/normal_3d.hpp` | `computePointNormal` 中邻域 centroid/covariance 累加 | 已结束（诊断，不接 production） | `test-rvv/features/normal_3d` Phase 000 已证明 common covariance RVV component median `1.43x`，public `NormalEstimation` median `1.02x` 且 Doctor 标记 near-threshold；已提交 `b27453324`，当前不建议修改 `normal_3d.hpp` |
| 3 | Integral image normal organized output | `impl/integral_image_normal.hpp` | depth/distance maps 与 `computeFeatureFull/Part` 输出 loops | 未启动 | 先做 organized synthetic correctness；`integral_image2D` 作为伴随保留 |
| 4 | SHOT descriptor | `impl/shot.hpp` | `computePointSHOT` descriptor bin/normalize/copy | 进行中 | 已建立 `test-rvv/features/shot` Phase 000 诊断计划和自包含测试 / bench scaffold；OMP 文件仅作对照，不单独实施 |
| 5 | FPFH descriptor | `impl/fpfh.hpp` | SPFH/FPFH pair histogram 与 weighted SPFH | 未启动 | 先建立 caller-shaped `src/pfh.cpp` helper ablation |
| 6 | PFH descriptor | `impl/pfh.hpp` | O(k^2) pair feature histogram | 未启动 | 与 FPFH 共享 helper/bench 输入，但文件级任务单独推进 |
| 7 | Moment of inertia reductions | `impl/moment_of_inertia_estimation.hpp` | covariance/projection/minmax reductions | 未启动 | 建立 scalar/RVV reduction 对拍，明确 Eigen solver 边界 |
| 8 | Organized edge detection | `impl/organized_edge_detection.hpp` | organized depth/RGB/normal edge masks 与 label writes | 未启动 | 先做 label-equivalence diagnostic，确认 label index 顺序 |

## 8. Closeout

- 输出文档位置：`doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md`。
- 第一轮文件候选筛选统计：`13 high / 40 mid / 84 low`，第二轮必查基线 `53`。
- 第二轮队列统计：建议 `8`，保留 `29`，暂缓或不推荐 `16`，补入 `low` 为 `0`。
- 第一条未完成主题：`impl/don.hpp` 的 pointwise normal difference 函数级 RVV 评估。
- 通用规则反哺：features 模块中 OMP wrapper、helper-only `src` 文件和 companion header 需要显式标注 `non-standalone` 或 shared-helper 边界；后续实施仍按文件推进，但 helper 文件必须依赖 caller-shaped 证据，不应因为 helper 位于热点内层就单独升入建议队列。
