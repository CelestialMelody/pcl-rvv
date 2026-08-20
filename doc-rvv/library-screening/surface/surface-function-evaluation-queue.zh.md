# surface 模块 RVV 第二轮函数评估队列

本文档把 `doc-rvv/library-screening/modules/surface-file-candidate-screening.zh.md` 中的 31 个 `high/mid` 候选，按公开入口、函数族、主成本覆盖类型、测试可行性和维护风险，下钻成 surface 模块的第二轮函数评估队列。

本轮只做筛选，不重跑第一轮，不修改 production 源码，不建立 `test-rvv` topic，不跑板卡 bench，不提交。

## 1. 输入依据

- 第一轮文件候选筛选：`doc-rvv/library-screening/modules/surface-file-candidate-screening.zh.md`
- 筛选标准：`.agents/skills/rvv-screening/references/screening-criteria.md`
- 阶段与队列规则：`.agents/skills/rvv-screening/references/stage-and-queue-policy.md`
- 证据边界：`.agents/skills/rvv-screening/references/evidence-boundaries.md`
- 函数评估队列模板：`.agents/skills/rvv-screening/references/templates/function-evaluation-queue-template.md`
- surface 测试入口：`test/surface/CMakeLists.txt`、`test/surface/test_*.cpp`
- surface 示例 / 工具入口：`examples/surface/*.cpp`、`tools/*.cpp`
- 路径显示：候选表沿用第一轮短路径，省略 `surface/include/pcl/surface/` 与 `surface/src/` 公共前缀。

## 2. 二轮筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 第一轮源码文件总数 | 346 | `surface/**` 的源码后缀文件。 |
| 第一轮 high | 11 | 二轮初始候选基线。 |
| 第一轮 mid | 20 | 二轮初始候选基线。 |
| 第一轮 low | 315 | 本轮未发现必须补入的明显漏判项。 |
| 第二轮初始候选基线 | 31 | high + mid 去重后数量。 |
| 新增补充候选 | 0 | 本轮未从 low 或候选外新增。 |
| 第二轮候选总数 | 31 | 仍为初始基线。 |
| 建议进行 RVV 优化的文件 | 4 | 公开入口主路径较清楚。 |
| 保留实施的候选文件 | 17 | 有明确 RVV 片段，但主成本仍需后续评估。 |
| 暂缓或不推荐考虑 RVV 优化的文件 | 10 | 外部 solver、qhull、协调器或专门辅助路径主导。 |
| 源码冲突 / 删除 / 合并 | 0 | 未发现需要删改候选的源码冲突。 |

## 3. 文件级变化理由

本轮没有重做第一轮，而是把 31 个 `high/mid` 候选逐项落到三类结果。标准很简单：能直接覆盖公开入口主成本、且 loop 形态稳定的，进建议队列；有真实 RVV 片段但 search / solver / 状态机 仍会稀释收益的，留在保留队列；主成本主要在外部库、协调器、专门辅助流程里的，暂缓或不推荐独立做 RVV 主题。

本轮未发现需要从 `low` 补入的文件。也就是说，当前二轮队列仍以第一轮 31 个候选为边界。

### 3.1 建议进行 RVV 优化的文件

| 文件 | 公开入口 / 函数族 | 主成本覆盖类型 | 测试 / bench 可行性 | 去向理由 |
| --- | --- | --- | --- | --- |
| `impl/organized_fast_mesh.hpp` | `performReconstruction`、`reconstructPolygons`、`make*Mesh` | `direct-main-path` | `test/surface/test_organized_fast_mesh.cpp` | organized 行列扫描、valid / shadow mask 和可变 polygon 输出都在本文件内，入口直接、规则性最好；production public probe 已做且不建议接入，生产补丁已回滚。 |
| `impl/bilateral_upsampling.hpp` | `process`、`performProcessing`、`computeDistances` | `direct-main-path` | `tools/bilateral_upsampling.cpp` | organized 像素网格 + 有界窗口累加，算术密度高，边界和 NaN fallback 都可局部验证；phase 020 已完成 production public probe，当前公开入口不建议接入，等待 PI5 用户确认是否回滚。 |
| `impl/marching_cubes.hpp` | `performReconstruction`、`createSurface` | `direct-main-path` | `test/surface/test_marching_cubes.cpp`、`tools/marching_cubes_reconstruction.cpp` | 3D voxel 扫描、edge table / tri table 插值和输出生成是标准 batch 路径。 |
| `on_nurbs/triangulation.cpp` | `createIndices`、`createVertices`、`convertSurface2PolygonMesh`、`convertSurface2Vertices` | `direct-main-path` | `test/surface/test_on_nurbs.cpp`、`examples/surface/example_nurbs_fitting_surface.cpp` | 规则网格生成和 `Evaluate` 扫描很清楚，和 surface on_nurbs 示例 / 测试直接对得上。 |

### 3.2 保留实施的候选文件

| 文件 | 公开入口 / 函数族 | 主成本覆盖类型 | 测试 / bench 可行性 | 去向理由 |
| --- | --- | --- | --- | --- |
| `impl/marching_cubes_rbf.hpp` | `voxelizeData`、`kernel` | `partial-preprocess` | `test/surface/test_marching_cubes.cpp`、`tools/marching_cubes_reconstruction.cpp` | `2N x 2N` matrix fill 和 voxel eval 规整，但 `fullPivLu` solve 仍是大块标量成本，先保留。 |
| `impl/marching_cubes_hoppe.hpp` | `voxelizeData` | `diagnostic` | `test/surface/test_marching_cubes.cpp`、`tools/marching_cubes_reconstruction.cpp` | 每个 voxel 都要 `nearestKSearch`，search 稀释了 RVV 片段，先等诊断或消融结果。 |
| `impl/mls.hpp` | `performProcessing`、`computeMLSPointNormal`、`performUpsampling`、`computeMLSSurface` | `diagnostic` | `test/surface/test_moving_least_squares.cpp`、`tools/mls_smoothing.cpp` | 逐点批量工作很真，但邻域搜索、迭代投影和 OpenMP 合并都要先拆开看。 |
| `impl/gp3.hpp` | `reconstructPolygons`、`closeTriangle`、`connectPoint` | `diagnostic` | `test/surface/test_gp3.cpp`、`tools/gp3_surface.cpp` | KNN、状态机、fringe queue 和 erase / sort 太重，RVV 片段存在但不够独立。 |
| `impl/grid_projection.hpp` | `reconstructPolygons`、`createSurfaceForCell`、`getVectorAtPoint` | `diagnostic` | `test/surface/test_grid_projection.cpp` | grid work 真实存在，但 hash map、union gather 和 cell staging 稀释了独立收益。 |
| `impl/surfel_smoothing.hpp` | `smoothCloudIteration`、`smoothPoint`、`computeSmoothedCloud`、`extractSalientFeaturesBetweenScales` | `diagnostic` | 无单独 surface test，和 `tools/mls_smoothing.cpp` 场景相近 | 迭代 smooth + radiusSearch 结构里有 batch math，但要先证明 search 不是主成本。 |
| `impl/texture_mapping.hpp` | `mapTexture2MeshUV`、`removeOccludedPoints`、`textureMeshwithMultipleCameras` | `diagnostic` | `test/surface/test_gp3.cpp` 中有相关调用路径；也有示例可复用 | projection math 可以向量化，但 octree / camera composition / occlusion state 太重。 |
| `on_nurbs/fitting_surface_im.cpp` | `assemble`、`addPointConstraint`、`addCage*Regularisation`、`updateSurf` | `partial-preprocess` | `test/surface/test_on_nurbs.cpp`、`examples/surface/example_nurbs_fitting_surface.cpp` | organized image-style 路径清楚，但 inverseMapping 和 solver 仍会吃掉大头。 |
| `on_nurbs/fitting_surface_pdm.cpp` | `assemble`、`assembleInterior`、`assembleBoundary`、`add*Regularisation`、`updateSurf` | `partial-preprocess` | `test/surface/test_on_nurbs.cpp`、`examples/surface/example_nurbs_fitting_surface.cpp` | 主体是 NURBS 装配和更新，RVV 片段真实，但 solver 与 inverseMapping 仍是边界。 |
| `on_nurbs/fitting_surface_tdm.cpp` | `assemble`、`assembleInterior`、`assembleBoundary`、`add*Regularisation`、`updateSurf` | `partial-preprocess` | `BUILD_surface_on_nurbs` 下的 `test_on_nurbs.cpp` / `example_nurbs_fitting_surface.cpp` | 和 PDM 同族，tangent 项更重，仍适合先保留做函数级评估。 |
| `on_nurbs/fitting_curve_pdm.cpp` | `assemble`、`assembleInterior`、`addCageRegularisation`、`updateCurve` | `partial-preprocess` | `test/surface/test_on_nurbs.cpp`、`examples/surface/example_nurbs_fitting_closed_curve3d.cpp` | curve assembly 很规整，但 inverseMapping 和余弦 / knot 细节需要先评估收益。 |
| `on_nurbs/fitting_curve_2d_pdm.cpp` | `assemble`、`assembleInterior`、`addPointConstraint`、`addCageRegularisation` | `partial-preprocess` | `examples/surface/example_nurbs_fitting_closed_curve.cpp` | 2D curve batch 逻辑清楚，但 PDM 族仍被投影和 solver 稀释。 |
| `on_nurbs/fitting_curve_2d_apdm.cpp` | `assemble`、`assembleInterior`、`addPointConstraint`、`addCageRegularisation` | `partial-preprocess` | `examples/surface/example_nurbs_fitting_closed_curve.cpp`、`example_nurbs_fitting_surface.cpp` | 和 PDM 同族，局部权重和 regularisation 可看，但还不是最直接的建议主题。 |
| `on_nurbs/fitting_curve_2d_asdm.cpp` | `assemble`、`assembleInterior`、`assembleClosestPoints`、`addCageRegularisation` | `partial-preprocess` | `examples/surface/example_nurbs_fitting_surface.cpp` | closest-points 和 adaptive 权重增加了维护风险，先保留不抢建议位。 |
| `on_nurbs/fitting_curve_2d_atdm.cpp` | `assemble`、`assembleInterior`、`assembleClosestPoints`、`addCageRegularisation` | `partial-preprocess` | `examples/surface/example_nurbs_fitting_surface.cpp` | tangent-aware 变体仍有明确 batch 片段，但更适合和 PDM 族一起复核。 |
| `on_nurbs/fitting_curve_2d_sdm.cpp` | `assemble`、`assembleInterior`、`addPointConstraint`、`addCageRegularisation` | `partial-preprocess` | `examples/surface/example_nurbs_fitting_closed_curve.cpp` | 仍是 curve family 的真入口，但与 PDM 主线相比更像变体候选。 |
| `on_nurbs/fitting_curve_2d_tdm.cpp` | `assemble`、`assembleInterior`、`addPointConstraint`、`addCageRegularisation` | `partial-preprocess` | `examples/surface/example_nurbs_fitting_closed_curve.cpp` | 2D tangent-aware 路径明确，可留给后续复筛，不必现在进建议队列。 |

### 3.3 暂缓或不推荐考虑 RVV 优化的文件

| 文件 | 原始去向 / 合并到主题 | 主成本覆盖类型 | 测试 / bench 可行性 | 暂缓或不推荐原因 |
| --- | --- | --- | --- | --- |
| `impl/concave_hull.hpp` | qhull 前后预处理 / 输出整理 | `non-standalone` | `test/surface/test_concave_hull.cpp` | 主要成本在 qhull，源码里 RVV 片段只是前后 staging。 |
| `impl/convex_hull.hpp` | qhull 前后预处理 / 输出整理 | `non-standalone` | `test/surface/test_convex_hull.cpp` | 也是外部 qhull 主导，文件本身更像包装和输出转换。 |
| `on_nurbs/closing_boundary.cpp` | on_nurbs 边界闭合辅助 | `non-standalone` | `BUILD_surface_on_nurbs` 下的 `test_on_nurbs.cpp` | 重复 inverseMapping 和 boundary matching，属于辅助流程，不宜单独立项。 |
| `on_nurbs/global_optimization_pdm.cpp` | 多 surface 协调器 | `non-standalone` | `BUILD_surface_on_nurbs` 下的 `test_on_nurbs.cpp` | 交叉 surface 组合、common boundary 和 inverseMapping 让它更像 orchestrator。 |
| `on_nurbs/global_optimization_tdm.cpp` | 多 surface 协调器 | `non-standalone` | `BUILD_surface_on_nurbs` 下的 `test_on_nurbs.cpp` | 同样是多 surface 编排，tangent 版还更复杂，先不单独做 RVV 主题。 |
| `on_nurbs/nurbs_tools.cpp` | on_nurbs 公用工具 | `non-standalone` | 被 `test_on_nurbs.cpp` 和各 example 间接复用 | 辅助函数可用，但不承载独立热点。 |
| `on_nurbs/sequential_fitter.cpp` | on_nurbs 顺序拟合协调器 | `non-standalone` | `BUILD_surface_on_nurbs` 下的 `test_on_nurbs.cpp`、`example_nurbs_fitting_surface.cpp` | 主要做多阶段调度和外部 fitting 组合，RVV 机会不独立。 |
| `on_nurbs/fitting_curve_2d.cpp` | 2D curve 基类 / 通用入口 | `non-standalone` | `examples/surface/example_nurbs_fitting_curve2d.cpp` | 该文件有批量工作，但 PDM 族已覆盖更明确的主线。 |
| `on_nurbs/fitting_cylinder_pdm.cpp` | 专门几何初始化 + assembly | `non-standalone` | `BUILD_surface_on_nurbs` 下的 `test_on_nurbs.cpp` | 主题过窄，更多是特定几何的初始化和 solver 包装。 |
| `on_nurbs/fitting_sphere_pdm.cpp` | 专门几何初始化 + assembly | `non-standalone` | `BUILD_surface_on_nurbs` 下的 `test_on_nurbs.cpp` | 同上，窄主题不适合作为独立 RVV 首波。 |

## 4. 执行清单 / 状态表

### 4.1 建议进行 RVV 优化的文件

| 顺序 | 主题 | 主文件 | 当前状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- |
| 1 | organized mesh reconstruction | `impl/organized_fast_mesh.hpp` | 已完成 / no-production / 已提交 | diagnostic 曾正向，但 public path 的 production evidence 负向；生产补丁已回滚，当前不建议继续扩大接入；topic 资产提交为 `7bfead613`。 |
| 2 | bilateral upsampling | `impl/bilateral_upsampling.hpp` | 已完成函数级评估和 production public probe，等待用户确认是否回滚生产补丁 | staged-window-reduction 诊断为负向（`0.91x/0.94x/0.91x`）且不建议接入；phase 010 direct-depth 诊断为 `1.04x/1.17x/1.30x`、Evidence Doctor `Errors=0`；phase 020 production public 结果为 `0.97x/0.89x/0.95x`、Evidence Doctor `Errors=3`，当前不建议接入。 |
| 3 | marching cubes | `impl/marching_cubes.hpp` | 建议进入函数级评估 | 先评估 voxel 扫描和 edge/tri table 路径。 |
| 4 | on_nurbs triangulation | `on_nurbs/triangulation.cpp` | 函数级评估已建档；当前 candidate 不进入 production；按当前指令暂停继续推进 | `test-rvv/surface/triangulation` 已完成 `param_grid_rvv_store` 接入前诊断：QEMU / board correctness 和 asm 通过；5-run board repeated 为 `tri_param_grid_512` median `0.978x`、`tri_surface_eval_256` median `0.999x`，两者均 3/5 退化，Evidence Doctor 均 `Errors=1`。用户已说明忽略 on_nurbs 依赖相关方向；剩余不依赖该符号链的 `createIndices` 输出构造只适合非 RVV 标量消融，不建议作为 RVV next phase。 |

### 4.2 保留实施的候选文件

这些文件保留后续函数级评估路径，但本轮不进建议队列。后续要回答的第一组问题是：RVV 片段到底占不占主成本，还是只是前置装配、诊断或辅助算子。

### 4.3 暂缓或不推荐考虑 RVV 优化的文件

这些文件当前更适合作为主主题的伴随上下文、辅助实现或后续复核对象，不单独开 RVV 主题。
