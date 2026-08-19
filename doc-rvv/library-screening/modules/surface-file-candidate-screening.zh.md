# surface 模块 RVV 第一轮文件级筛选报告

本文档是 `surface/**` 的第一轮文件候选筛选重写版。旧文档只作为 previous baseline 和覆盖检查参考，不再把“循环数量 / 数学项数量”当作充分判据；本轮以当前源码为主，逐文件判断 `high / mid / low`。

## 1. 输入依据与范围

- 覆盖范围：`surface/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `346`，已判定 `346`（`346/346` 全覆盖）。
- 主线文件：`105` 个非第三方源码后缀文件。
- 第三方口径：`surface/include/pcl/surface/3rdparty/**` 与 `surface/src/3rdparty/**` 共 `241` 个文件，只登记覆盖，不纳入本轮候选主线。
- 其中 `opennurbs` `212` 个文件、`poisson4` `29` 个文件，均默认 仅登记覆盖。
- 路径显示：候选表和覆盖表中的 `impl/`、`on_nurbs/`、`vtk_smoothing/`、`src/3rdparty/` 文件使用模块内短路径；若短路径有歧义，则回退为 repo-relative 路径。

## 2. 第一轮筛选口径

- 第一轮只回答“文件中是否存在值得继续下钻的可 SIMD/RVV 片段”，不直接等同于实施队列。
- `high/mid` 必须能写出可复核源码事实：具体入口、函数 / loop / helper、trip count 来源、运算和访存形态、控制流 / 依赖 / 输出语义风险，以及测试或 bench 入口可行性。
- `low` 也必须一文件一行覆盖，不静默遗漏。
- 对 `surface/include/pcl/surface/3rdparty/opennurbs/**`、`surface/src/3rdparty/opennurbs/**`、`surface/include/pcl/surface/3rdparty/poisson4/**` 和 `surface/src/3rdparty/poisson4/**`，默认只登记覆盖；除非出现明确且可维护的 PCL-facing 热点证据，否则不纳入候选主线。
- 对 public `.h` / `.hpp`，如果真实循环在对应 `impl/*.hpp`、`src/*.cpp` 或第三方内核中，本轮按低优先级登记，不因为名字或注释里有数学词就升档。

## 3. 第一轮筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 源码文件总数 | 346 | `surface/**` 源码后缀文件。 |
| 主线文件总数 | 105 | 非第三方源码后缀文件。 |
| 第三方文件总数 | 241 | `opennurbs` 212 + `poisson4` 29，默认 仅登记覆盖。 |
| high | 11 | 二轮必查。 |
| mid | 20 | 二轮必查。 |
| low | 315 | 已覆盖但不进入二轮初始基线。 |
| high + mid | 31 | 第一轮候选基线。 |
| 候选占比 | 31/346 = 9.0% | high + mid / 源码文件总数。 |

## 4. 与旧 surface 第一轮文档相比的主要口径变化

| 调整类型 | 变化 |
| --- | --- |
| 判定单位 | 从“循环数量 / 数学项数量”回到入口、loop/helper、trip count、访存、语义风险和测试入口。 |
| 头文件口径 | 公开声明头、薄 wrapper、显式实例化和伴随 src 普遍降级，不再因为命中数学词就进入候选。 |
| 第三方口径 | `opennurbs` 和 `poisson4` 统一按 仅登记覆盖 处理。 |
| 候选基线 | 旧文档的 51 个 `high+mid` 缩到当前 31 个，主要因为部分 wrapper、solver 伴随文件和 search / qhull / state-machine 路径不再被当作独立 batch 候选。 |
| 新增证据 | `test/surface/*.cpp`、`test/surface/CMakeLists.txt` 里的 unit test 入口被用来校准可验证性，而不是靠文件关键词。 |

## 5. high 候选（11）

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| --- | --- | --- | --- | --- | --- |
| `impl/organized_fast_mesh.hpp` | OrganizedFastMesh::performReconstruction / reconstructPolygons / make*Mesh | input_->width / input_->height 与 triangle_pixel_size_rows_/columns_ | organized 点云行列遍历、validity mask、quad / triangle 输出 | shadowed-face 判定与可变 polygon 输出 | 直接 organized 网格遍历，几何谓词可转 mask |
| `impl/bilateral_upsampling.hpp` | BilateralUpsampling::performProcessing / computeDistances | input_->width * input_->height；窗口面积 (2*window_size_+1)^2；rgb 表 3*255+1 | organized 像素网格、预计算 exp 表、局部窗口乘加 / mask 累加 | 逐像素输出写、NaN fallback、窗口边界、RGB 与 depth 耦合 | 直接 organized 批量循环，trip count 清晰且没有外部 search / solver 屏障 |
| `impl/marching_cubes.hpp` | MarchingCubes::performReconstruction / createSurface | res_x_ * res_y_ * res_z_ | voxel corner load、cubeindex mask、edge / tri table 插值 | 查表分支与 push_back 可变输出 | 直接 3D 网格扫描，cell 工作规整且 RVV strip-mining 形态清晰 |
| `impl/marching_cubes_rbf.hpp` | MarchingCubesRBF::voxelizeData / kernel | N = input_->size()；2N x 2N dense build；res_x_ * res_y_ * res_z_ grid eval | pairwise kernel matrix 填充、voxel 三重循环、centers dense reduction | fullPivLu solve、cubic kernel 与大 working set | 点和 voxel 上的直接批量数学路径，但需解释 dense solve 阶段 |
| `impl/mls.hpp` | MovingLeastSquares::performProcessing / computeMLSPointNormal / performUpsampling / computeMLSSurface | indices_->size()、nn_indices.size()、num_neighbors、polynomial order | 逐点邻域算术、polynomial term 构造、weighted reduction | searchForNeighbors / radiusSearch 与 OpenMP merge 会稀释局部计算 | MLS surface 直接路径，批量循环大且已有专用 unit test |
| `on_nurbs/fitting_surface_pdm.cpp` | FittingSurface::assemble / assembleInterior / assembleBoundary / addInteriorRegularisation / addBoundaryRegularisation / updateSurf | interior.size()、boundary.size()、regularisation_resU/V、Order()、CVCount() | point / regularisation grid 上的 basis evaluation 与 row assembly | inverseMapping 与 solver 耦合、小 order 循环、curvature fallback | PCL-facing surface fit assembly 直接路径，已有 on_nurbs test harness |
| `on_nurbs/fitting_surface_tdm.cpp` | FittingSurfaceTDM::assemble / assembleBoundary / assembleInterior / add*Regularisation / updateSurf | boundary.size()、interior.size()、regularisation_resU/V、CVCount() | surface grid 上的 point residual 与 tangent / normal assembly | 3x solver layout 扩展与 tangent constraint 耦合 | surface fit assembly 直接路径，批量循环规整且入口可测 |
| `on_nurbs/fitting_curve_2d_pdm.cpp` | FittingCurve2dPDM::assemble / assembleInterior / addCageRegularisation / updateCurve / addPointConstraint | interior.size()、elements.size()-1、m_order、m_cv_count | 1D knot-span assembly、basis evaluation、逐 control point update | solver 与 inverse mapping、低 order wrapping 语义 | 直接 curve fitting 批量工作，不是 wrapper |
| `on_nurbs/fitting_curve_pdm.cpp` | FittingCurve::assemble / assembleInterior / addCageRegularisation / updateCurve / addPointConstraint | interior.size()、elements.size()-1、m_order、m_cv_count | curve basis evaluation、residual assembly 与简单 point update | solver 耦合与 wrap-around control point | primary curve-fitting 路径，批量循环规整且局部算术明确 |
| `on_nurbs/fitting_surface_im.cpp` | FittingSurfaceIM::computeMean / computeIndexBoundingBox / refine / initSurface / assemble / addPointConstraint | m_indices.size()、image width/height、KnotCount()/Order()、CVCount() | organized image indices、逐像素 parameter mapping、basis evaluation | 可选 inverse_mapping path、NaN 检查、solver 耦合 | organized image-style 批量路径，逐像素 assembly 直接可见 |
| `on_nurbs/triangulation.cpp` | Triangulation::createIndices / createVertices / convertSurface2PolygonMesh / convertTrimmedSurface2PolygonMesh / convertSurface2Vertices | resolution^2 与 cloud->size() | 规则 parameter grid，每个生成 vertex 一次 Evaluate | trimmed-surface inverseMapping 与可变 polygon 输出 | test_on_nurbs 覆盖的直接转换路径，grid-evaluation 形态清晰 |

## 6. mid 候选（20）

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| --- | --- | --- | --- | --- | --- |
| `impl/gp3.hpp` | GreedyProjectionTriangulation::reconstructPolygons / closeTriangle / connectPoint | indices_->size()、nnn_、fringe_queue_ size | KNN ring 上的 neighbor-angle scan 与 maskable comparison | 状态机、queue erase、search-dominated 逐点成本 | 有直接 public entry，但 search 与拓扑状态主导文件成本 |
| `impl/grid_projection.hpp` | GridProjection::reconstructPolygons / createSurfaceForCell / getVectorAtPoint | data_->size()、data_size_^3、pt_union_indices.size() | 规则 cell scan 与 weighted vector reduction | hash-map occupancy、union gather 与 cell-side branch | 存在规则 grid work，但 hash-map 与 staging 逻辑稀释独立 RVV 价值 |
| `impl/marching_cubes_hoppe.hpp` | MarchingCubesHoppe::voxelizeData | res_x_ * res_y_ * res_z_ | voxel traversal，每个 voxel 一次 nearestKSearch 与 normal-dot 计算 | KdTree search 主导；每 voxel gather 实际偏标量 | grid traversal 清晰，但 search step 使其低于最强 batch 路径 |
| `impl/concave_hull.hpp` | ConcaveHull::performReconstruction / reconstruct / getHullPointIndices | cloud_transformed.size()、edges size、polygon index span | centroid / covariance 与 qhull input array 前后的 staging loop | qhull 外部求解主导，edge walking 偏串行 | 第三方 hull solver 前后的 partial-preprocess，不是自包含 SIMD kernel |
| `impl/convex_hull.hpp` | ConvexHull::performReconstruction2D / performReconstruction3D / getHullPointIndices | indices_->size()、hull.size() | qhull 调用前后的 coordinate staging 与 polygon output copy | qhull 主导，projection choice 增加分支 | 仍值得二轮复核，但主 hull solver 在外部 |
| `impl/surfel_smoothing.hpp` | SurfelSmoothing::smoothCloudIteration / smoothPoint / computeSmoothedCloud / extractSalientFeaturesBetweenScales | interm_cloud_->size()、nn_indices.size()、cloud2->size() | 逐点 neighbor weights、residual accumulation 与 maskable update | 每轮 radiusSearch 加 convergence loop | 存在直接 batch math，但 iterative search / convergence 结构使其保持 mid |
| `impl/texture_mapping.hpp` | TextureMapping::mapTexture2MeshUV / mapTexture2Mesh / removeOccludedPoints / textureMeshwithMultipleCameras | tex_mesh.tex_polygons.size()、face / vertex count、input_cloud->size()、camera count | projection math、UV staging 与跨 face visibility mask | octree / kd-tree search、visibility state 与顺序敏感输出 | 有 RVV-friendly geometry，但 occlusion search 与 camera composition 主导 |
| `on_nurbs/closing_boundary.cpp` | commonBoundaryPoint1 / 2 / 3 / sampleUniform / sampleRandom / sampleFromBoundary / optimizeBoundary | nsteps、samples、boundary / interior list size、nurbs_list.size() | iterative boundary-point search 与 sample loop | 重复 inverseMapping 与 coupled boundary matching | 有用的 helper path，但 loop 辅助于主 fitting assembly |
| `on_nurbs/fitting_curve_2d.cpp` | FittingCurve2d::findElement / refine / assemble / inverseMapping / findClosestElementMidPoint | elements.size()、interior size、maxSteps、order | 1D curve basis evaluation 与 point residual assembly | iterative inverse mapping 与 small-order kernel | 存在直接 curve math，但 PDM variant 是更强 RVV target |
| `on_nurbs/fitting_curve_2d_apdm.cpp` | FittingCurve2dAPDM::assemble / addPointConstraint / addCageRegularisation / updateCurve | interior.size()、elements.size()-1、m_order、m_cv_count | curve basis staging 与 variant-specific weight | solver 耦合与 small knot-span loop | 与 PDM 同属 curve-fitting family，但该 variant 更偏辅助 |
| `on_nurbs/fitting_curve_2d_asdm.cpp` | FittingCurve2dASDM::assemble / addPointConstraint / addCageRegularisation / updateCurve | interior.size()、elements.size()-1、m_order、m_cv_count | curve basis staging 与 variant-specific residual term | solver 耦合与 low-order loop | 仍是有效 RVV 复核对象，但不是最直接 curve path |
| `on_nurbs/fitting_curve_2d_atdm.cpp` | FittingCurve2dATDM::assemble / addPointConstraint / addCageRegularisation / updateCurve | interior.size()、elements.size()-1、m_order、m_cv_count | 包含 tangent-aware term 的 curve assembly | inverse mapping 与 solver interaction | variant 文件有 RVV-relevant loop，但主要继承 PDM family 结构 |
| `on_nurbs/fitting_curve_2d_sdm.cpp` | FittingCurve2dSDM::assemble / addPointConstraint / addCageRegularisation / updateCurve | interior.size()、elements.size()-1、m_order、m_cv_count | curve residual staging 与 small basis loop | solver 耦合与 wrap-around control point | 有候选片段，但 loop structure 轻于 high-confidence 文件 |
| `on_nurbs/fitting_curve_2d_tdm.cpp` | FittingCurve2dTDM::assemble / addPointConstraint / addCageRegularisation / updateCurve | interior.size()、elements.size()-1、m_order、m_cv_count | 包含 tangent-aware term 的 curve fit | solver 耦合与 small-order basis loop | 值得二轮看，但不如 PDM surface 文件直接 |
| `on_nurbs/fitting_cylinder_pdm.cpp` | FittingCylinder::assemble / updateSurf / initNurbsPCACylinder | interior.size()、CVCount()、m_nurbs.m_cv_count | control-point grid 上的 surface assembly 与 PCA-driven initialization | specialized geometry setup 与 solver dependency | 存在直接 batch loop，但 specialized cylinder path 比顶层文件更窄 |
| `on_nurbs/fitting_sphere_pdm.cpp` | FittingSphere::assemble / updateSurf / initNurbsSphere | interior.size()、CVCount()、m_nurbs.m_cv_count | control-point grid 上的 surface assembly 与 sphere initialization | specialized geometry setup 与 solver dependency | 与 cylinder variant 同类：有效但不是最强独立 target |
| `on_nurbs/global_optimization_pdm.cpp` | GlobalOptimization::assemble / assembleInteriorPoints / assembleBoundaryPoints / assembleRegularisation / assembleClosingBoundaries / assembleCommonBoundaries / assembleCommonParams / updateSurf | m_nurbs.size()、per-surface interior/boundary/common size、CVCount() | multi-surface assembly loop 与重复 row-source staging | cross-surface coupling、inverseMapping 与 external solver state | 存在真实 batch work，但文件更像 multi-surface coordinator，不是干净 SIMD kernel |
| `on_nurbs/global_optimization_tdm.cpp` | GlobalOptimizationTDM::assemble / assembleInteriorPointsTD / assembleBoundaryPoints / assembleClosingBoundariesTD / assembleCommonParams / updateSurf | m_nurbs.size()、per-surface interior/boundary/common size、CVCount() | point list 上的 multi-surface tangent-aware assembly | cross-surface coupling 与 3x solver layout | 与 PDM variant 同属 coordinator profile，并额外有 tangent-state 成本 |
| `on_nurbs/nurbs_tools.cpp` | NurbsTools::computeMean / computeVariance / computeBoundingBox / pca / getClosestPoint | data.size()、CVCount() | linear scan、小 reduction 与 compact matrix assembly | helper 有用，但通常由更大的 fitting flow 调用 | helper math 真实存在，但属于 shared utility 而非 standalone hotspot |
| `on_nurbs/sequential_fitter.cpp` | compute_quadfit / compute_refinement / compute_boundary / compute_interior / compute / grow / PCL2ON | iteration count、boundary size、m_data size、resolution | FittingSurface 周围的 fit-loop orchestration 与 row/column staging | control-flow heavy 且依赖其他 on_nurbs component | 有用 entry point，但文件主要调度其他 fitting kernel |

## 7. 全量文件覆盖表（346/346）

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `3rdparty/opennurbs/examples_linking_pragmas.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_3dm.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_3dm_attributes.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_3dm_properties.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_3dm_settings.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_annotation.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_annotation2.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_arc.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_arccurve.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_archive.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_array.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_array_defs.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_base32.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_base64.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_beam.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_bezier.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_bitmap.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_bounding_box.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_box.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_brep.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_circle.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_color.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_compress.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_cone.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_crc.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_curve.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_curveonsurface.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_curveproxy.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_cylinder.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_defines.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_detail.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_dimstyle.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_dll_resource.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_ellipse.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_error.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_evaluate_nurbs.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_extensions.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_font.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_fpoint.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_fsp.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_fsp_defs.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_geometry.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_gl.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_group.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_hatch.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_hsort_template.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_instance.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_intersect.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_knot.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_layer.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_light.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_line.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_linecurve.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_linestyle.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_linetype.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_lookup.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_mapchan.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_material.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_math.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_matrix.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_memory.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_mesh.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_nurbscurve.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_nurbssurface.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_object.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_object_history.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_objref.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_offsetsurface.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_optimize.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_plane.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_planesurface.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_pluginlist.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_point.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_pointcloud.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_pointgeometry.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_pointgrid.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_polycurve.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_polyedgecurve.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_polyline.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_polylinecurve.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_qsort_template.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_rand.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_rendering.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_revsurface.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_rtree.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_sphere.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_string.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_sumsurface.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_surface.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_surfaceproxy.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_system.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_textlog.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_texture.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_texture_mapping.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_torus.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_unicode.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_userdata.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_uuid.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_version.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_viewport.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_workspace.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_xform.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/opennurbs/opennurbs_zlib.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/allocator.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/binary_node.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/bspline_data.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/bspline_data.hpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/factor.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/function_data.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/function_data.hpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/geometry.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/geometry.hpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/marching_cubes_poisson.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/mat.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/mat.hpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/multi_grid_octree_data.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/multi_grid_octree_data.hpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/octree_poisson.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/octree_poisson.hpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/poisson_exceptions.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/polynomial.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/polynomial.hpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/ppolynomial.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/ppolynomial.hpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/sparse_matrix.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/sparse_matrix.hpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/vector.h` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `3rdparty/poisson4/vector.hpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `bilateral_upsampling.h` | `low` | 否 | 公开声明头，真实循环在 impl/bilateral_upsampling.hpp | `impl/bilateral_upsampling.hpp` |
| `concave_hull.h` | `low` | 否 | 公开声明头，真实循环在 impl/concave_hull.hpp | `impl/concave_hull.hpp` |
| `convex_hull.h` | `low` | 否 | 公开声明头，真实循环在 impl/convex_hull.hpp | `impl/convex_hull.hpp` |
| `ear_clipping.h` | `low` | 否 | 公开声明头，真实循环在 src/ear_clipping.cpp | `src/ear_clipping.cpp` |
| `gp3.h` | `low` | 否 | 公开声明头，真实循环在 impl/gp3.hpp | `impl/gp3.hpp` |
| `grid_projection.h` | `low` | 否 | 公开声明头，真实循环在 impl/grid_projection.hpp | `impl/grid_projection.hpp` |
| `impl/bilateral_upsampling.hpp` | `high` | 是 | 直接 organized 批量循环，trip count 清晰且没有外部 search / solver 屏障 | `-` |
| `impl/concave_hull.hpp` | `mid` | 是 | 第三方 hull solver 前后的 partial-preprocess，不是自包含 SIMD kernel | `-` |
| `impl/convex_hull.hpp` | `mid` | 是 | 仍值得二轮复核，但主 hull solver 在外部 | `-` |
| `impl/gp3.hpp` | `mid` | 是 | 有直接 public entry，但 search 与拓扑状态主导文件成本 | `-` |
| `impl/grid_projection.hpp` | `mid` | 是 | 存在规则 grid work，但 hash-map 与 staging 逻辑稀释独立 RVV 价值 | `-` |
| `impl/marching_cubes.hpp` | `high` | 是 | 直接 3D 网格扫描，cell 工作规整且 RVV strip-mining 形态清晰 | `-` |
| `impl/marching_cubes_hoppe.hpp` | `mid` | 是 | grid traversal 清晰，但 search step 使其低于最强 batch 路径 | `-` |
| `impl/marching_cubes_rbf.hpp` | `high` | 是 | 点和 voxel 上的直接批量数学路径，但需解释 dense solve 阶段 | `-` |
| `impl/mls.hpp` | `high` | 是 | MLS surface 直接路径，批量循环大且已有专用 unit test | `-` |
| `impl/organized_fast_mesh.hpp` | `high` | 是 | 直接 organized 网格遍历，几何谓词可转 mask | `-` |
| `impl/poisson.hpp` | `low` | 否 | impl helper 未选为 standalone RVV target：poisson.hpp | `-` |
| `impl/processing.hpp` | `low` | 否 | MeshProcessing 基类 wrapper，真实工作由派生类完成 | `-` |
| `impl/reconstruction.hpp` | `low` | 否 | 抽象/公共基类实现片段，不承载独立 batch 热点 | `-` |
| `impl/surfel_smoothing.hpp` | `mid` | 是 | 存在直接 batch math，但 iterative search / convergence 结构使其保持 mid | `-` |
| `impl/texture_mapping.hpp` | `mid` | 是 | 有 RVV-friendly geometry，但 occlusion search 与 camera composition 主导 | `-` |
| `marching_cubes.h` | `low` | 否 | 公开声明头，真实循环在 impl/marching_cubes.hpp | `impl/marching_cubes.hpp` |
| `marching_cubes_hoppe.h` | `low` | 否 | 公开声明头，真实循环在 impl/marching_cubes_hoppe.hpp | `impl/marching_cubes_hoppe.hpp` |
| `marching_cubes_rbf.h` | `low` | 否 | 公开声明头，真实循环在 impl/marching_cubes_rbf.hpp | `impl/marching_cubes_rbf.hpp` |
| `mls.h` | `low` | 否 | 公开声明头，真实循环在 impl/mls.hpp | `impl/mls.hpp` |
| `on_nurbs/closing_boundary.h` | `low` | 否 | 公开声明 / 薄 wrapper (closing_boundary.h) | `-` |
| `on_nurbs/fitting_curve_2d.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_curve_2d.h) | `-` |
| `on_nurbs/fitting_curve_2d_apdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_curve_2d_apdm.h) | `-` |
| `on_nurbs/fitting_curve_2d_asdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_curve_2d_asdm.h) | `-` |
| `on_nurbs/fitting_curve_2d_atdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_curve_2d_atdm.h) | `-` |
| `on_nurbs/fitting_curve_2d_pdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_curve_2d_pdm.h) | `-` |
| `on_nurbs/fitting_curve_2d_sdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_curve_2d_sdm.h) | `-` |
| `on_nurbs/fitting_curve_2d_tdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_curve_2d_tdm.h) | `-` |
| `on_nurbs/fitting_curve_pdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_curve_pdm.h) | `-` |
| `on_nurbs/fitting_cylinder_pdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_cylinder_pdm.h) | `-` |
| `on_nurbs/fitting_sphere_pdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_sphere_pdm.h) | `-` |
| `on_nurbs/fitting_surface_im.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_surface_im.h) | `-` |
| `on_nurbs/fitting_surface_pdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_surface_pdm.h) | `-` |
| `on_nurbs/fitting_surface_tdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (fitting_surface_tdm.h) | `-` |
| `on_nurbs/global_optimization_pdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (global_optimization_pdm.h) | `-` |
| `on_nurbs/global_optimization_tdm.h` | `low` | 否 | 公开声明 / 薄 wrapper (global_optimization_tdm.h) | `-` |
| `on_nurbs/nurbs_data.h` | `low` | 否 | 公开声明 / 薄 wrapper (nurbs_data.h) | `-` |
| `on_nurbs/nurbs_solve.h` | `low` | 否 | 公开声明 / 薄 wrapper (nurbs_solve.h) | `-` |
| `on_nurbs/nurbs_tools.h` | `low` | 否 | 公开声明 / 薄 wrapper (nurbs_tools.h) | `-` |
| `on_nurbs/sequential_fitter.h` | `low` | 否 | 公开声明 / 薄 wrapper (sequential_fitter.h) | `-` |
| `on_nurbs/sparse_mat.h` | `low` | 否 | 公开声明 / 薄 wrapper (sparse_mat.h) | `-` |
| `on_nurbs/triangulation.h` | `low` | 否 | 公开声明 / 薄 wrapper (triangulation.h) | `-` |
| `organized_fast_mesh.h` | `low` | 否 | 公开声明头，真实循环在 impl/organized_fast_mesh.hpp | `impl/organized_fast_mesh.hpp` |
| `poisson.h` | `low` | 否 | 公开声明头，真实循环在 impl/poisson.hpp 且 third-party 执行体主导 | `impl/poisson.hpp` |
| `processing.h` | `low` | 否 | 公开声明头，真实循环在 impl/processing.hpp | `impl/processing.hpp` |
| `qhull.h` | `low` | 否 | 第三方/封装声明，真实循环不在本文件 | `-` |
| `reconstruction.h` | `low` | 否 | 抽象基类声明，真实循环分散在各实现文件 | `-` |
| `simplification_remove_unused_vertices.h` | `low` | 否 | 公开声明头，真实循环在 src/simplification_remove_unused_vertices.cpp | `src/simplification_remove_unused_vertices.cpp` |
| `surfel_smoothing.h` | `low` | 否 | 公开声明头，真实循环在 impl/surfel_smoothing.hpp | `impl/surfel_smoothing.hpp` |
| `texture_mapping.h` | `low` | 否 | 公开声明头，真实循环在 impl/texture_mapping.hpp | `impl/texture_mapping.hpp` |
| `vtk_smoothing/vtk.h` | `low` | 否 | 公开声明头，真实循环在 vtk_smoothing cpp | `-` |
| `vtk_smoothing/vtk_mesh_quadric_decimation.h` | `low` | 否 | 公开声明头，真实循环在 vtk_smoothing cpp | `src/vtk_smoothing/vtk_mesh_quadric_decimation.cpp` |
| `vtk_smoothing/vtk_mesh_smoothing_laplacian.h` | `low` | 否 | 公开声明头，真实循环在 vtk_smoothing cpp | `src/vtk_smoothing/vtk_mesh_smoothing_laplacian.cpp` |
| `vtk_smoothing/vtk_mesh_smoothing_windowed_sinc.h` | `low` | 否 | 公开声明头，真实循环在 vtk_smoothing cpp | `src/vtk_smoothing/vtk_mesh_smoothing_windowed_sinc.cpp` |
| `vtk_smoothing/vtk_mesh_subdivision.h` | `low` | 否 | 公开声明头，真实循环在 vtk_smoothing cpp | `src/vtk_smoothing/vtk_mesh_subdivision.cpp` |
| `vtk_smoothing/vtk_utils.h` | `low` | 否 | 公开声明头，真实循环在 src/vtk_smoothing/vtk_utils.cpp | `src/vtk_smoothing/vtk_utils.cpp` |
| `src/3rdparty/opennurbs/opennurbs_3dm_attributes.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_3dm_properties.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_3dm_settings.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_annotation.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_annotation2.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_arc.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_arccurve.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_archive.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_array.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_base32.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_base64.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_beam.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_bezier.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_beziervolume.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_bitmap.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_bounding_box.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_box.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_extrude.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_io.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_isvalid.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_region.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_tools.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_v2valid.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_circle.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_color.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_compress.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_cone.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_crc.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_curve.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_curveonsurface.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_curveproxy.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_cylinder.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_defines.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_detail.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_dimstyle.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_dll.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_ellipse.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_embedded_file.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_error.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_error_message.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_evaluate_nurbs.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_extensions.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_font.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_fsp.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_geometry.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_gl.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_group.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_hatch.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_instance.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_intersect.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_knot.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_layer.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_light.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_line.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_linecurve.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_linetype.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_lookup.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_material.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_math.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_matrix.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_memory.c` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_memory_util.c` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_mesh.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_mesh_ngon.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_mesh_tools.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_morph.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_nurbscurve.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_nurbssurface.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_nurbsvolume.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_object.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_object_history.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_objref.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_offsetsurface.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_optimize.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_plane.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_planesurface.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_pluginlist.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_point.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_pointcloud.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_pointgeometry.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_pointgrid.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_polycurve.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_polyedgecurve.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_polyline.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_polylinecurve.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_precompiledheader.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_rand.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_revsurface.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_rtree.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_sort.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_sphere.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_string.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_sum.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_sumsurface.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_surface.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_surfaceproxy.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_textlog.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_torus.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_unicode.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_userdata.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_uuid.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_viewport.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_workspace.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_wstring.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_xform.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_zlib.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/opennurbs/opennurbs_zlib_memory.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/poisson4/bspline_data.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/poisson4/factor.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/poisson4/geometry.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `src/3rdparty/poisson4/marching_cubes_poisson.cpp` | `low` | 否 | 第三方文件，仅登记覆盖 | `-` |
| `bilateral_upsampling.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/bilateral_upsampling.hpp` |
| `concave_hull.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/concave_hull.hpp` |
| `convex_hull.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/convex_hull.hpp` |
| `ear_clipping.cpp` | `low` | 否 | 串行 polygon triangulation，loop-carried state 与可变输出主导 | `-` |
| `gp3.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/gp3.hpp` |
| `grid_projection.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/grid_projection.hpp` |
| `marching_cubes.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/marching_cubes.hpp` |
| `marching_cubes_hoppe.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/marching_cubes_hoppe.hpp` |
| `marching_cubes_rbf.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/marching_cubes_rbf.hpp` |
| `mls.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/mls.hpp` |
| `on_nurbs/closing_boundary.cpp` | `mid` | 是 | 有用的 helper path，但 loop 辅助于主 fitting assembly | `-` |
| `on_nurbs/fitting_curve_2d.cpp` | `mid` | 是 | 存在直接 curve math，但 PDM variant 是更强 RVV target | `-` |
| `on_nurbs/fitting_curve_2d_apdm.cpp` | `mid` | 是 | 与 PDM 同属 curve-fitting family，但该 variant 更偏辅助 | `-` |
| `on_nurbs/fitting_curve_2d_asdm.cpp` | `mid` | 是 | 仍是有效 RVV 复核对象，但不是最直接 curve path | `-` |
| `on_nurbs/fitting_curve_2d_atdm.cpp` | `mid` | 是 | variant 文件有 RVV-relevant loop，但主要继承 PDM family 结构 | `-` |
| `on_nurbs/fitting_curve_2d_pdm.cpp` | `high` | 是 | 直接 curve fitting 批量工作，不是 wrapper | `-` |
| `on_nurbs/fitting_curve_2d_sdm.cpp` | `mid` | 是 | 有候选片段，但 loop structure 轻于 high-confidence 文件 | `-` |
| `on_nurbs/fitting_curve_2d_tdm.cpp` | `mid` | 是 | 值得二轮看，但不如 PDM surface 文件直接 | `-` |
| `on_nurbs/fitting_curve_pdm.cpp` | `high` | 是 | primary curve-fitting 路径，批量循环规整且局部算术明确 | `-` |
| `on_nurbs/fitting_cylinder_pdm.cpp` | `mid` | 是 | 存在直接 batch loop，但 specialized cylinder path 比顶层文件更窄 | `-` |
| `on_nurbs/fitting_sphere_pdm.cpp` | `mid` | 是 | 与 cylinder variant 同类：有效但不是最强独立 target | `-` |
| `on_nurbs/fitting_surface_im.cpp` | `high` | 是 | organized image-style 批量路径，逐像素 assembly 直接可见 | `-` |
| `on_nurbs/fitting_surface_pdm.cpp` | `high` | 是 | PCL-facing surface fit assembly 直接路径，已有 on_nurbs test harness | `-` |
| `on_nurbs/fitting_surface_tdm.cpp` | `high` | 是 | surface fit assembly 直接路径，批量循环规整且入口可测 | `-` |
| `on_nurbs/global_optimization_pdm.cpp` | `mid` | 是 | 存在真实 batch work，但文件更像 multi-surface coordinator，不是干净 SIMD kernel | `-` |
| `on_nurbs/global_optimization_tdm.cpp` | `mid` | 是 | 与 PDM variant 同属 coordinator profile，并额外有 tangent-state 成本 | `-` |
| `on_nurbs/nurbs_solve_eigen.cpp` | `low` | 否 | solver / sparse-map wrapper; main cost is external solver or container state | `-` |
| `on_nurbs/nurbs_solve_eigen_sparse.cpp` | `low` | 否 | solver / sparse-map wrapper; main cost is external solver or container state | `-` |
| `on_nurbs/nurbs_solve_umfpack.cpp` | `low` | 否 | solver / sparse-map wrapper; main cost is external solver or container state | `-` |
| `on_nurbs/nurbs_tools.cpp` | `mid` | 是 | helper math 真实存在，但属于 shared utility 而非 standalone hotspot | `-` |
| `on_nurbs/sequential_fitter.cpp` | `mid` | 是 | 有用 entry point，但文件主要调度其他 fitting kernel | `-` |
| `on_nurbs/sparse_mat.cpp` | `low` | 否 | solver / sparse-map wrapper; main cost is external solver or container state | `-` |
| `on_nurbs/triangulation.cpp` | `high` | 是 | test_on_nurbs 覆盖的直接转换路径，grid-evaluation 形态清晰 | `-` |
| `organized_fast_mesh.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/organized_fast_mesh.hpp` |
| `poisson.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/poisson.hpp` |
| `processing.cpp` | `low` | 否 | MeshProcessing wrapper；实际 batch loop 在派生类 | `impl/processing.hpp` |
| `simplification_remove_unused_vertices.cpp` | `low` | 否 | tail-compress copy/remap helper，不是 standalone RVV hotspot | `-` |
| `surfel_smoothing.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/surfel_smoothing.hpp` |
| `texture_mapping.cpp` | `low` | 否 | 显式实例化 / 薄入口；真实循环在配对 impl 头中 | `impl/texture_mapping.hpp` |
| `vtk_smoothing/vtk_mesh_quadric_decimation.cpp` | `low` | 否 | VTK wrapper / conversion path，主成本在 VTK 或格式搬运 | `-` |
| `vtk_smoothing/vtk_mesh_smoothing_laplacian.cpp` | `low` | 否 | VTK wrapper / conversion path，主成本在 VTK 或格式搬运 | `-` |
| `vtk_smoothing/vtk_mesh_smoothing_windowed_sinc.cpp` | `low` | 否 | VTK wrapper / conversion path，主成本在 VTK 或格式搬运 | `-` |
| `vtk_smoothing/vtk_mesh_subdivision.cpp` | `low` | 否 | VTK wrapper / conversion path，主成本在 VTK 或格式搬运 | `-` |
| `vtk_smoothing/vtk_utils.cpp` | `low` | 否 | VTK wrapper / conversion path，主成本在 VTK 或格式搬运 | `-` |

## 8. 二轮交接说明

- 第二轮函数评估队列应读取：`doc-rvv/library-screening/surface/surface-function-evaluation-queue.zh.md`。
- 本轮没有把 `low` 文件静默遗漏；如果后续源码变化、profile 或已完成主题证据显示漏判，可以在二轮里补入并说明来源。
- 本轮没有运行板卡 bench，也没有修改 production 源码。
- 如果需要补充到通用规则，当前最值得写入指令的是：第三方目录默认只做 仅登记覆盖，`high/mid` 必须写明入口、trip count、访存形态、语义风险和测试入口，不能再靠循环计数单独定级。
