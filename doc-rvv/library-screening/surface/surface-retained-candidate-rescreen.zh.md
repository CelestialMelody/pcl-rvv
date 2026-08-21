# surface 模块保留候选复筛

本文档记录 `surface` 模块第三轮保留候选复筛。输入范围只包含
`doc-rvv/library-screening/surface/surface-function-evaluation-queue.zh.md`
中 `3.2 保留实施的候选文件` 的 17 个候选；不重新扩大到全 surface 模块，不补入 3.2 之外的新候选。

本轮遵守 `rvv-screening` 的保留候选复筛口径：只做源码阅读、静态分析、轻量入口确认和筛选文档；不修改
production 源码，不建立新的 `test-rvv` topic，不运行板卡 bench，不提交。筛选队列只授权后续进入函数级评估，
不能替代后续 `rvv-workflow` / `rvv-test` 的 correctness（正确性）、QEMU、反汇编、board（目标硬件）和
production integration（生产接入）证据闭环。

## 1. 输入依据与复筛原因

输入依据：

- `doc-rvv/library-screening/surface/surface-function-evaluation-queue.zh.md`
- `.agents/skills/rvv-screening/SKILL.md`
- `.agents/skills/rvv-screening/references/screening-criteria.md`
- `.agents/skills/rvv-screening/references/stage-and-queue-policy.md`
- `.agents/skills/rvv-screening/references/evidence-boundaries.md`
- `.agents/skills/rvv-screening/references/templates/retained-candidate-rescreen-template.md`
- `test-rvv/surface/organized_fast_mesh/doc/organized_fast_mesh-evaluation.zh.md`
- `test-rvv/surface/bilateral_upsampling/doc/bilateral_upsampling-evaluation.zh.md`
- `test-rvv/surface/marching_cubes/doc/marching_cubes-evaluation.zh.md`
- `test-rvv/surface/triangulation/doc/triangulation-evaluation.zh.md`
- `surface/include/pcl/surface/impl/*.hpp`、`surface/src/on_nurbs/*.cpp` 中对应 17 个候选源码

复筛原因是 surface 已完成四个建议队列主题。真实板卡结果和 no-production 回退原因已经改变保留候选的排序价值：

- `bilateral_upsampling` 和 `marching_cubes` 证明，公开入口中的大规模 organized window / grid scan 若能覆盖主成本，
  并且 fallback（回退）边界和 point type / layout gate（点型 / 布局门禁）清楚，可以形成 production direct 正向证据。
- `organized_fast_mesh` 和 `triangulation` 证明，局部规则写入、finite mask（有限值掩码）或参数网格 store 即使 correctness
  与反汇编成立，也可能被可变 polygon append（多边形追加）、`pcl::Vertices` 输出构造、`Evaluate` 或 public entry 外壳稀释到
  no-production。
- 已完成主题要求本轮下调 search、solver、state machine（状态机）、hash map、octree ray tracing、inverse mapping（反向映射）
  和 NURBS solve 主导的候选；只保留能提出可归因首阶段问题的候选。

## 2. 筛选统计

| 统计项 | 数量 / 结论 |
| --- | ---: |
| 3.2 保留实施候选输入总数 | 17 |
| 建议启动函数级评估 | 1 |
| 暂缓 / 不单独实施 | 16 |
| 重新纳入 3.2 之外候选 | 0 |
| 合并、删除或源码冲突项 | 0 |

当前建议启动路径分布：

| 默认评估路径 | 数量 | 说明 |
| --- | ---: | --- |
| `component ablation` | 1 | `marching_cubes_rbf` 先拆分 RBF matrix fill / solve / voxel eval，不直接承诺 production。 |

## 3. 已完成主题经验总结

### 3.1 已完成主题证据包

| 主题 | 主文件 | 函数评估队列原始定位 | 实际覆盖范围 | 目标硬件结论 | 正确性证据 | 反汇编证据 | 生产接入状态 | 回退 / 暂缓原因 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `organized_fast_mesh` | `impl/organized_fast_mesh.hpp` | organized mesh reconstruction direct path | finite cache、adaptive diagonal preference、public mesh reconstruction probe | diagnostic 曾正向；production public 后 quad `1.00x`、right `0.99x`、left `0.99x`、adaptive `0.89x` | QEMU / board correctness 曾保持 checksum 一致 | test-only / production helper 曾可归属 RVV finite/adaptive helper | no-production / production patch 已回滚 | RVV 只覆盖 finite mask 和 adaptive z 差值，polygon append、分支和顺序输出仍是标量主成本。 |
| `bilateral_upsampling` | `impl/bilateral_upsampling.hpp` | organized RGBD window accumulation direct path | RGB/RGBA exact family、`Scalar=float`、AoS float layout 的 production color-gather helper | phase 073 public `1.26x/1.21x/1.18x/1.23x/1.22x`；steady `1.31x/1.21x/1.25x/1.21x/1.21x` | QEMU Std/RVV 13/13；public-entry RGB/RGBA correctness | color-gather helper 中 `vlse8`、`vzext`、`vluxei16`、`vfabs`、`vmflt`、`vfredusum` 可见 | adopted production behavior | 旧 staged-window、old exact-gate、nan-mask helper 负向或被 superseded；当前只外推到已验证 exact family。 |
| `marching_cubes` | `impl/marching_cubes.hpp` | active-cell / edge interpolation direct path | generic `RVVXYZAoSFloatLayout<PointNT>` active-cell prepass 接入；edge interpolation 不接入 | generic repeated：`PointXYZ=3.873x`、`PointXYZI=3.618x`、`PointXYZRGB=3.601x`、`PointXYZRGBA=3.639x` | QEMU Std/RVV 各 6 tests；board repeated checksum match | generic production path 有 RVV load / mask / prepass 指令归属 | adopted generic production | edge interpolation repeated 约 `0.992x/1.024x/1.049x` 且 Evidence Doctor 有退化 Error；Phase 060 RVV-vs-RVV median `1.007x` 为 neutral。 |
| `triangulation` | `on_nurbs/triangulation.cpp` | on_nurbs regular parameter grid / surface conversion | test-only `param_grid_rvv_store`，真实 OpenNURBS direct bench 未能推进 | repeated board：`tri_param_grid_512` median `0.978x`，`tri_surface_eval_256` median `0.999x`，两者均 3/5 退化 | QEMU correctness 和 board correctness 通过 | `vsetvli`、`vid.v`、`vfcvt.f.xu.v`、`vfmacc.vf`、`vsse32.v` 可归属 test helper | no-production | 参数网格写入不能穿透 `Evaluate` / output 构造；当前 RISC-V 安装库缺少 on_nurbs direct 符号。 |

### 3.2 可复用模式与失败边界

| 模式标签 | 来自哪些已完成主题 | 成立条件 | 失败 / 回退边界 | 对后续候选的影响 |
| --- | --- | --- | --- | --- |
| `organized window direct accumulation` | `bilateral_upsampling` | 公开入口主成本是 organized window 内同构 load / table lookup / finite mask / reduction，输出写回保持标量且成本较小 | staging 额外成本、未覆盖 infinity、point type / layout 未验证 | 只提升能覆盖真实窗口主成本的候选；不提升 NURBS solver 或 search-heavy fitting。 |
| `active-cell grid prepass` | `marching_cubes` | 批量判断 active cell，减少后续标量 surface emission 的无效工作，且 production gate 清楚 | 只优化 edge interpolation 或表查尾段时收益 neutral / weak | 支持 `marching_cubes_rbf` 做 voxel pipeline component ablation，但必须先量化 solve / voxel eval / surface emit 占比。 |
| `variable-output dilution` | `organized_fast_mesh`、`triangulation`、`marching_cubes` edge candidate | RVV 局部片段之后仍有可变 polygon / vertices / triangle append，或 public entry 主要成本在输出构造 | correctness 和 asm 成立也不能说明 production 价值 | mesh / polygon 构造类候选默认暂缓，除非首阶段覆盖 public entry 主成本。 |
| `search and solver dilution` | `organized_fast_mesh` 回退原因、`triangulation` no-production、filters / registration 已完成经验 | 局部公式前后由 nearestKSearch / radiusSearch / solver / inverseMapping / hash map / state queue 主导 | 局部 helper microbench 或参数写入不代表完整入口 | `marching_cubes_hoppe`、`mls`、`gp3`、`grid_projection`、`surfel_smoothing`、`texture_mapping` 和 on_nurbs fitting 默认暂缓。 |
| `diagnostic must match production boundary` | 四个已完成 surface 主题 | production public / production-shaped diagnostic 与 test-only helper 边界一致，且 Evidence Doctor 无关键退化 | helper 正向、public negative 或 near-threshold 时不接生产 | 保留候选即使启动函数级评估，也必须先写清首阶段证据问题，不能直接写 production patch。 |

## 4. 筛选口径修正 / 复筛变化理由

本轮对 3.2 保留候选作如下口径修正：

1. 对 `marching_cubes_rbf` 保留一个窄的函数级评估入口：它与已采纳的 `marching_cubes` 共享 public reconstruction / surface emission
   边界，且 `voxelizeData()` 中存在 dense matrix fill 和 voxel grid eval 两类可拆分循环；但 `fullPivLu().solve()` 是硬边界，
   因此只能先做 component ablation。
2. 对 `marching_cubes_hoppe` 降级：每个 voxel 调 `nearestKSearch`，与 `marching_cubes` active-cell prepass 的连续 grid scan 模式不同。
3. 对 `mls`、`gp3`、`grid_projection`、`surfel_smoothing`、`texture_mapping` 降级：这些文件里的 RVV 片段真实存在，但主流程被
   nearest / radius search、fringe queue、sort / erase、hash map、octree ray tracing、camera selection 或迭代状态稀释。
4. 对 on_nurbs fitting family 整体降级：surface / curve assembly loop 会写 `NurbsSolve` 矩阵，局部 basis / regularisation 规整；
   但 `inverseMapping`、`assembleClosestPoints`、`m_solver.solve()` 和控制点更新才构成完整拟合入口。`triangulation` 已证明
   on_nurbs 中单纯规则参数写入不能自然转成 production 价值。
5. `diagnostic`、`production-shaped diagnostic`、`component ablation` 和 `profile prerequisite` 只作为后续 topic 内首阶段证据路径；
   本复筛只输出 `建议启动函数级评估` 与 `暂缓 / 不单独实施`。

## 5. 保留实施候选逐项复筛

### 5.1 建议启动函数级评估

| 主题 | 关键入口 | 主成本覆盖类型 | 默认评估路径 / 首阶段证据问题 | 匹配的已验证模式 | 主要风险 | 推荐理由 | 证据来源 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `marching_cubes_rbf` | `MarchingCubesRBF<PointNT>::voxelizeData()`；后续经 `MarchingCubes<PointNT>::performReconstruction()` / `createSurface()` | `partial-preprocess` -> 可验证为 production-value | `component ablation`：先拆 `2N x 2N` RBF matrix fill、`fullPivLu().solve()`、voxel eval 和 base `createSurface()` 占比；只有 voxel eval 或 active-cell 预筛能穿透完整入口时才进入 production-shaped diagnostic | `marching_cubes` active-cell grid prepass；generic AoS float layout gate；edge interpolation neutral 作为失败边界 | Eigen solve 可能主导；kernel 对 centers 的内层循环可能访存和 `std::sqrt` / `std::exp` 成本高；RBF 参数和点数规模会影响 matrix / grid 主成本比例 | 这是 17 个保留候选中唯一同时满足“已有同族 production 入口”“无 per-voxel nearestKSearch”“可把首阶段问题拆清”的候选。启动它不代表接 production，只是先回答 RBF voxel pipeline 是否还有独立 RVV 价值。 | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp`；`test-rvv/surface/marching_cubes/doc/marching_cubes-evaluation.zh.md`；`test/surface/test_marching_cubes.cpp` |

### 5.2 暂缓 / 不单独实施

| 主题 | 主成本覆盖类型 | 暂缓原因 | 重新考虑条件 | 证据来源 |
| --- | --- | --- | --- | --- |
| `marching_cubes_hoppe` | `diagnostic` | `voxelizeData()` 三重 voxel loop 每个 voxel 调 `tree_->nearestKSearch`；局部 signed distance formula 不能代表入口主成本。 | 有 profile 证明 search 可忽略，或能复用已物化 nearest-neighbor / normal 数据形成 production-shaped diagnostic。 | `surface/include/pcl/surface/impl/marching_cubes_hoppe.hpp`；`marching_cubes` active-cell 与 search-dilution 模式 |
| `mls` | `diagnostic` | `performProcessing()` 和 `performUpsampling()` 围绕 `searchForNeighbors`、`nearestKSearch`、MLS polynomial surface、OpenMP 合并和 upsampling 状态；局部 basis / weighted sum 片段不独立。 | 真实 profile 指向 `computeMLSSurface()` 的某个规约 / basis loop，并能隔离 search 和 solver / projection。 | `surface/include/pcl/surface/impl/mls.hpp`；search and solver dilution |
| `gp3` | `diagnostic` | `reconstructPolygons()` 以 KNN、fringe queue、状态迁移、angle sort、erase 和可变 polygon output 为主；RVV 只能覆盖候选内小公式。 | 固定 K、大规模、可归因 workload 证明 angle / triangle predicate loop 主导，且输出状态保持标量可接受。 | `surface/include/pcl/surface/impl/gp3.hpp`；variable-output dilution |
| `grid_projection` | `diagnostic` | 主流程构建 `cell_hash_map_`、union gather、`computeMeanAndCovarianceMatrix`、recursive intersection 和 cell staging；`getVectorAtPoint()` 的距离权重 loop 被 hash / recursion 稀释。 | 有数据集 profile 证明 `getVectorAtPoint()` 或 cell projection 占主成本，并能固定 pt union 形态做 component ablation。 | `surface/include/pcl/surface/impl/grid_projection.hpp` |
| `surfel_smoothing` | `diagnostic` | 每点 `radiusSearch` 后才进入 normal / position 加权累加，`smoothPoint()` 还有迭代收敛状态；search 和迭代控制流主导。 | 已有 radiusSearch 结果或 profile 证明邻域加权累加占主成本，再按 production-shaped diagnostic 恢复。 | `surface/include/pcl/surface/impl/surfel_smoothing.hpp` |
| `texture_mapping` | `diagnostic` | projection math 可批处理，但 `removeOccludedPoints()` / `textureMeshwithMultipleCameras()` 由 octree ray tracing、camera composition、visibility state 和 mesh face output 主导。 | 固定 camera / mesh case profile 指向 projection 或 UV write loop，且 octree / visibility 已隔离。 | `surface/include/pcl/surface/impl/texture_mapping.hpp` |
| `fitting_surface_im` | `partial-preprocess` | image-style data loop 和 cage regularisation 规整，但完整入口包含 optional `inverseMapping()`、`NurbsSolve::solve()` 和 z-only control point update；`triangulation` 的参数网格诊断已显示 on_nurbs 局部写入容易被后续求值 / solver 稀释。 | 上游 image fitting profile 证明 matrix assembly rows 占主成本，且能把 solver 固定为同一 A/B 边界。 | `surface/src/on_nurbs/fitting_surface_im.cpp`；`test-rvv/surface/triangulation/doc/triangulation-evaluation.zh.md` |
| `fitting_surface_pdm` | `partial-preprocess` | `assembleInterior()` / `assembleBoundary()` 中每点先 inverse mapping，再写 `NurbsSolve` matrix；后续 `m_solver.solve()` 和 `updateSurf()` 是完整拟合闭环。 | profile 指向 `addPointConstraint()` / regularisation matrix fill，并能证明 solver 不主导。 | `surface/src/on_nurbs/fitting_surface_pdm.cpp` |
| `fitting_surface_tdm` | `partial-preprocess` | 与 PDM 同族，tangent constraint 增加 normal / tangent 项和三通道 matrix 写入；收益更容易被 inverse mapping 和 solve 稀释。 | TDM-specific dataset 显示 tangent assembly 是主成本，并有可对拍数值预算。 | `surface/src/on_nurbs/fitting_surface_tdm.cpp` |
| `fitting_curve_pdm` | `partial-preprocess` | 3D curve assembly loop 规整，但每点反向映射和 `NurbsSolve` 仍在主路径；单独更新 control point 是小尾段。 | 曲线 fitting profile 证明 `addPointConstraint()` 或 cage regularisation 占主成本。 | `surface/src/on_nurbs/fitting_curve_pdm.cpp` |
| `fitting_curve_2d_pdm` | `partial-preprocess` | 2D PDM assembly 比 surface 更窄，但仍有 inverse mapping、solver 和 control point update；输入来源主要是 example，生产价值未证明。 | 稳定 2D closed-curve workload + profile 指向 assembly rows。 | `surface/src/on_nurbs/fitting_curve_2d_pdm.cpp` |
| `fitting_curve_2d_apdm` | `partial-preprocess` | adaptive PDM 增加 closest-points sampling、concavity regularisation 和多次 iteration；比 PDM 更像 solver / control-flow topic。 | APDM dataset 证明 closest-points 或 assembly 成为热点，并能隔离 iteration 控制。 | `surface/src/on_nurbs/fitting_curve_2d_apdm.cpp` |
| `fitting_curve_2d_asdm` | `partial-preprocess` | ASDM 在 APDM 上叠加 normal / tangent / rho / closest-points 逻辑；首阶段问题比基础 PDM 更难归因。 | 先由 PDM/APDM 证明同族 assembly RVV 有价值，再恢复本变体。 | `surface/src/on_nurbs/fitting_curve_2d_asdm.cpp` |
| `fitting_curve_2d_atdm` | `partial-preprocess` | ATDM 是 APDM/TDM 组合变体，closest-points 与 tangent-aware matrix fill 混杂；不适合作为首个独立 topic。 | PDM/APDM/TDM 任一基础路径有正向 production-shaped diagnostic 后，再按变体恢复。 | `surface/src/on_nurbs/fitting_curve_2d_atdm.cpp` |
| `fitting_curve_2d_sdm` | `partial-preprocess` | SDM 有 normal / tangent 项，但仍受 inverse mapping、solver 和迭代 fitting 边界限制；比基础 PDM 缺少优先级。 | 基础 2D PDM profile 已证明 assembly 主导，且 SDM 特有项占比明确。 | `surface/src/on_nurbs/fitting_curve_2d_sdm.cpp` |
| `fitting_curve_2d_tdm` | `partial-preprocess` | TDM tangent-aware matrix fill 真实存在，但完整入口仍是反向映射 + solver + update；当前没有证据优先于基础曲线 family。 | 有 tangent-aware workload profile，或基础 curve PDM 已完成正向函数级评估。 | `surface/src/on_nurbs/fitting_curve_2d_tdm.cpp` |

## 6. 新的执行清单 / 状态表

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 依据模式 / 证据来源 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- | --- |
| 1 | `marching_cubes_rbf` | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` | `voxelizeData()` 中 RBF matrix fill / solve / voxel eval 的 component ablation；先不改 production | `marching_cubes` active-cell grid prepass strong-positive；RBF 同族 public reconstruction；`fullPivLu().solve()` 是硬风险 | 建议启动函数级评估 | 第一条建议启动的未完成 retained 主题。S2 首问：RBF voxel eval 或 active-cell 预筛是否占完整入口主成本，还是被 matrix solve / kernel math 稀释。 |
| 2 | retained remainder | 其余 16 个 3.2 文件 | 无默认独立 topic | search / solver / state machine / variable output / on_nurbs fitting failure boundaries | 暂缓 / 不单独实施 | 等 profile、dataset、子主题完成或用户明确要求恢复；恢复时必须先提出可隔离、可归因、可板卡验证的首阶段问题。 |

本轮未发现需要补充到 `rvv-screening`、`rvv-test`、`rvv-implementation` 或 `rvv-documentation` skill 的通用规则。现有 retained-candidate rescreen 规则已经覆盖本轮边界：筛选文档只能选择队列和首阶段证据问题，不能直接承诺 production 接入或目标硬件收益。
