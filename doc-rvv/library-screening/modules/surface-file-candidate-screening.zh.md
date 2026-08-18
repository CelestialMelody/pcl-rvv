# surface 模块 RVV 第一轮文件级筛选报告

本文档记录 `surface` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 覆盖范围：`surface/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `346`，已判定 `346`（`346/346` 全覆盖）。
- 目录拆分：`include` `190`，`src` `156`。
- 第三方口径：`3rdparty/**` 文件 `241`，候选 `0`（仅登记，不纳入优化队列）。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `surface/include/pcl/surface/`，`src` 文件以 `src/` 开头显示。

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
| 源码文件总数 | 346 | `surface/**` 源码文件，第三方实现仅登记覆盖，不纳入候选主线。 |
| 已判定文件数 | 346 | `346/346` |
| high | 11 | 二轮必查 |
| mid | 40 | 二轮必查 |
| low | 295 | 已覆盖但不进入二轮初始基线 |
| high + mid | 51 | 第一轮候选基线 |
| 候选占比 | 51/346 = 14.7% | high + mid / 源码文件总数 |

## 4. high 候选（11）

| 文件 | 第一轮证据 |
| --- | --- |
| `src/on_nurbs/fitting_surface_pdm.cpp` | 循环52处，数学/几何计算密集，建议优先优化 |
| `impl/gp3.hpp` | 循环41处，数学/几何计算密集，建议优先优化 |
| `src/on_nurbs/fitting_curve_2d_apdm.cpp` | 循环35处，数学/几何计算密集，建议优先优化 |
| `src/on_nurbs/global_optimization_tdm.cpp` | 循环34处，数学/几何计算密集，建议优先优化 |
| `impl/texture_mapping.hpp` | 循环30处，数学/几何计算密集，建议优先优化 |
| `impl/grid_projection.hpp` | 循环29处，数学/几何计算密集，建议优先优化 |
| `src/on_nurbs/fitting_curve_2d_pdm.cpp` | 循环27处，数学/几何计算密集，建议优先优化 |
| `src/on_nurbs/fitting_cylinder_pdm.cpp` | 循环26处，数学/几何计算密集，建议优先优化 |
| `src/on_nurbs/triangulation.cpp` | 循环25处，数学/几何计算密集，建议优先优化 |
| `impl/mls.hpp` | 循环24处，数学/几何计算密集，建议优先优化 |
| `src/vtk_smoothing/vtk_utils.cpp` | 循环10处，数学/几何计算密集，建议优先优化 |

## 5. mid 候选（40）

| 文件 | 第一轮证据 |
| --- | --- |
| `src/on_nurbs/global_optimization_pdm.cpp` | 存在可向量化路径（循环23，数学项112），建议次优先 |
| `src/on_nurbs/fitting_curve_2d.cpp` | 存在可向量化路径（循环23，数学项104），建议次优先 |
| `src/on_nurbs/fitting_sphere_pdm.cpp` | 存在可向量化路径（循环23，数学项55），建议次优先 |
| `src/on_nurbs/fitting_surface_im.cpp` | 存在可向量化路径（循环23，数学项52），建议次优先 |
| `src/on_nurbs/closing_boundary.cpp` | 存在可向量化路径（循环18，数学项103），建议次优先 |
| `src/on_nurbs/nurbs_tools.cpp` | 存在可向量化路径（循环16，数学项57），建议次优先 |
| `src/on_nurbs/fitting_curve_pdm.cpp` | 存在可向量化路径（循环15，数学项49），建议次优先 |
| `src/on_nurbs/nurbs_solve_umfpack.cpp` | 存在可向量化路径（循环14，数学项5），建议次优先 |
| `impl/surfel_smoothing.hpp` | 存在可向量化路径（循环12，数学项21），建议次优先 |
| `src/on_nurbs/sequential_fitter.cpp` | 存在可向量化路径（循环9，数学项90），建议次优先 |
| `src/on_nurbs/fitting_curve_2d_asdm.cpp` | 存在可向量化路径（循环9，数学项40），建议次优先 |
| `impl/bilateral_upsampling.hpp` | 存在可向量化路径（循环9，数学项13），建议次优先 |
| `src/on_nurbs/fitting_surface_tdm.cpp` | 存在可向量化路径（循环9，数学项12），建议次优先 |
| `impl/concave_hull.hpp` | 存在可向量化路径（循环9，数学项11），建议次优先 |
| `impl/organized_fast_mesh.hpp` | 存在可向量化路径（循环9，数学项1），建议次优先 |
| `src/on_nurbs/sparse_mat.cpp` | 存在可向量化路径（循环9，数学项0），建议次优先 |
| `impl/poisson.hpp` | 存在可向量化路径（循环8，数学项38），建议次优先 |
| `src/on_nurbs/fitting_curve_2d_atdm.cpp` | 存在可向量化路径（循环8，数学项26），建议次优先 |
| `impl/marching_cubes.hpp` | 存在可向量化路径（循环8，数学项24），建议次优先 |
| `src/on_nurbs/fitting_curve_2d_sdm.cpp` | 存在可向量化路径（循环8，数学项22），建议次优先 |
| `impl/convex_hull.hpp` | 存在可向量化路径（循环8，数学项20），建议次优先 |
| `impl/marching_cubes_rbf.hpp` | 存在可向量化路径（循环7，数学项18），建议次优先 |
| `src/on_nurbs/nurbs_solve_eigen.cpp` | 存在可向量化路径（循环6，数学项14），建议次优先 |
| `src/on_nurbs/fitting_curve_2d_tdm.cpp` | 存在可向量化路径（循环6，数学项10），建议次优先 |
| `src/ear_clipping.cpp` | 存在可向量化路径（循环5，数学项26），建议次优先 |
| `src/simplification_remove_unused_vertices.cpp` | 存在可向量化路径（循环5，数学项2），建议次优先 |
| `src/on_nurbs/nurbs_solve_eigen_sparse.cpp` | 存在可向量化路径（循环3，数学项44），建议次优先 |
| `impl/marching_cubes_hoppe.hpp` | 存在可向量化路径（循环3，数学项10），建议次优先 |
| `mls.h` | 存在可向量化路径（循环2，数学项75），建议次优先 |
| `grid_projection.h` | 存在可向量化路径（循环2，数学项70），建议次优先 |
| `organized_fast_mesh.h` | 存在可向量化路径（循环1，数学项52），建议次优先 |
| `gp3.h` | 存在可向量化路径（循环0，数学项47），建议次优先 |
| `texture_mapping.h` | 存在可向量化路径（循环0，数学项40），建议次优先 |
| `marching_cubes.h` | 存在可向量化路径（循环0，数学项13），建议次优先 |
| `bilateral_upsampling.h` | 存在可向量化路径（循环0，数学项12），建议次优先 |
| `marching_cubes_hoppe.h` | 存在可向量化路径（循环0，数学项10），建议次优先 |
| `poisson.h` | 存在可向量化路径（循环0，数学项9），建议次优先 |
| `convex_hull.h` | 存在可向量化路径（循环0，数学项8），建议次优先 |
| `processing.h` | 存在可向量化路径（循环0，数学项7），建议次优先 |
| `reconstruction.h` | 存在可向量化路径（循环0，数学项5），建议次优先 |

## 6. 全量文件覆盖表（346/346）

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `3rdparty/opennurbs/examples_linking_pragmas.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_3dm.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_3dm_attributes.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_3dm_properties.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_3dm_settings.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_annotation.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_annotation2.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_arc.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_arccurve.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_archive.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_array.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_array_defs.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_base32.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_base64.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_beam.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_bezier.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_bitmap.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_bounding_box.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_box.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_brep.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_circle.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_color.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_compress.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_cone.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_crc.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_curve.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_curveonsurface.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_curveproxy.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_cylinder.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_defines.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_detail.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_dimstyle.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_dll_resource.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_ellipse.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_error.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_evaluate_nurbs.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_extensions.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_font.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_fpoint.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_fsp.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_fsp_defs.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_geometry.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_gl.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_group.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_hatch.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_hsort_template.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_instance.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_intersect.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_knot.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_layer.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_light.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_line.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_linecurve.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_linestyle.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_linetype.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_lookup.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_mapchan.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_material.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_math.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_matrix.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_memory.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_mesh.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_nurbscurve.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_nurbssurface.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_object.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_object_history.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_objref.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_offsetsurface.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_optimize.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_plane.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_planesurface.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_pluginlist.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_point.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_pointcloud.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_pointgeometry.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_pointgrid.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_polycurve.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_polyedgecurve.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_polyline.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_polylinecurve.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_qsort_template.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_rand.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_rendering.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_revsurface.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_rtree.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_sphere.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_string.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_sumsurface.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_surface.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_surfaceproxy.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_system.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_textlog.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_texture.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_texture_mapping.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_torus.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_unicode.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_userdata.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_uuid.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_version.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_viewport.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_workspace.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_xform.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/opennurbs/opennurbs_zlib.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/allocator.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/binary_node.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/bspline_data.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/bspline_data.hpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/factor.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/function_data.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/function_data.hpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/geometry.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/geometry.hpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/marching_cubes_poisson.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/mat.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/mat.hpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/multi_grid_octree_data.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/multi_grid_octree_data.hpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/octree_poisson.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/octree_poisson.hpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/poisson_exceptions.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/polynomial.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/polynomial.hpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/ppolynomial.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/ppolynomial.hpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/sparse_matrix.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/sparse_matrix.hpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/vector.h` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `3rdparty/poisson4/vector.hpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `bilateral_upsampling.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项12），建议次优先 | `impl/bilateral_upsampling.hpp` |
| `concave_hull.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/concave_hull.hpp` |
| `convex_hull.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项8），建议次优先 | `impl/convex_hull.hpp` |
| `ear_clipping.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `gp3.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项47），建议次优先 | `impl/gp3.hpp` |
| `grid_projection.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项70），建议次优先 | `impl/grid_projection.hpp` |
| `impl/bilateral_upsampling.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项13），建议次优先 | `-` |
| `impl/concave_hull.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项11），建议次优先 | `-` |
| `impl/convex_hull.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项20），建议次优先 | `-` |
| `impl/gp3.hpp` | `high` | 是 | 循环41处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/grid_projection.hpp` | `high` | 是 | 循环29处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/marching_cubes.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项24），建议次优先 | `-` |
| `impl/marching_cubes_hoppe.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项10），建议次优先 | `-` |
| `impl/marching_cubes_rbf.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项18），建议次优先 | `-` |
| `impl/mls.hpp` | `high` | 是 | 循环24处，数学/几何计算密集，建议优先优化 | `-` |
| `impl/organized_fast_mesh.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项1），建议次优先 | `-` |
| `impl/poisson.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项38），建议次优先 | `-` |
| `impl/processing.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/reconstruction.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/surfel_smoothing.hpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项21），建议次优先 | `-` |
| `impl/texture_mapping.hpp` | `high` | 是 | 循环30处，数学/几何计算密集，建议优先优化 | `-` |
| `marching_cubes.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项13），建议次优先 | `impl/marching_cubes.hpp` |
| `marching_cubes_hoppe.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项10），建议次优先 | `impl/marching_cubes_hoppe.hpp` |
| `marching_cubes_rbf.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/marching_cubes_rbf.hpp` |
| `mls.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项75），建议次优先 | `impl/mls.hpp` |
| `on_nurbs/closing_boundary.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_curve_2d.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_curve_2d_apdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_curve_2d_asdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_curve_2d_atdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_curve_2d_pdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_curve_2d_sdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_curve_2d_tdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_curve_pdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_cylinder_pdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_sphere_pdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_surface_im.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_surface_pdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/fitting_surface_tdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/global_optimization_pdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/global_optimization_tdm.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/nurbs_data.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/nurbs_solve.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/nurbs_tools.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/sequential_fitter.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/sparse_mat.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `on_nurbs/triangulation.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `organized_fast_mesh.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项52），建议次优先 | `impl/organized_fast_mesh.hpp` |
| `poisson.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项9），建议次优先 | `impl/poisson.hpp` |
| `processing.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项7），建议次优先 | `impl/processing.hpp` |
| `qhull.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `reconstruction.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项5），建议次优先 | `impl/reconstruction.hpp` |
| `simplification_remove_unused_vertices.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `surfel_smoothing.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/surfel_smoothing.hpp` |
| `texture_mapping.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项40），建议次优先 | `impl/texture_mapping.hpp` |
| `vtk_smoothing/vtk.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `vtk_smoothing/vtk_mesh_quadric_decimation.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `vtk_smoothing/vtk_mesh_smoothing_laplacian.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `vtk_smoothing/vtk_mesh_smoothing_windowed_sinc.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `vtk_smoothing/vtk_mesh_subdivision.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `vtk_smoothing/vtk_utils.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/3rdparty/opennurbs/opennurbs_3dm_attributes.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_3dm_properties.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_3dm_settings.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_annotation.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_annotation2.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_arc.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_arccurve.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_archive.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_array.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_base32.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_base64.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_beam.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_bezier.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_beziervolume.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_bitmap.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_bounding_box.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_box.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_extrude.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_io.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_isvalid.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_region.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_tools.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_brep_v2valid.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_circle.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_color.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_compress.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_cone.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_crc.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_curve.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_curveonsurface.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_curveproxy.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_cylinder.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_defines.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_detail.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_dimstyle.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_dll.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_ellipse.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_embedded_file.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_error.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_error_message.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_evaluate_nurbs.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_extensions.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_font.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_fsp.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_geometry.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_gl.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_group.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_hatch.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_instance.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_intersect.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_knot.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_layer.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_light.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_line.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_linecurve.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_linetype.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_lookup.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_material.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_math.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_matrix.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_memory.c` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_memory_util.c` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_mesh.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_mesh_ngon.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_mesh_tools.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_morph.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_nurbscurve.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_nurbssurface.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_nurbsvolume.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_object.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_object_history.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_objref.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_offsetsurface.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_optimize.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_plane.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_planesurface.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_pluginlist.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_point.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_pointcloud.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_pointgeometry.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_pointgrid.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_polycurve.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_polyedgecurve.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_polyline.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_polylinecurve.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_precompiledheader.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_rand.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_revsurface.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_rtree.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_sort.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_sphere.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_string.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_sum.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_sumsurface.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_surface.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_surfaceproxy.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_textlog.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_torus.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_unicode.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_userdata.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_uuid.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_viewport.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_workspace.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_wstring.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_xform.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_zlib.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/opennurbs/opennurbs_zlib_memory.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/poisson4/bspline_data.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/poisson4/factor.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/poisson4/geometry.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/3rdparty/poisson4/marching_cubes_poisson.cpp` | `low` | 否 | 第三方外部实现（3rdparty），按口径仅登记覆盖，不纳入本轮候选。 | `-` |
| `src/bilateral_upsampling.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/concave_hull.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/convex_hull.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/ear_clipping.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项26），建议次优先 | `-` |
| `src/gp3.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/grid_projection.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/marching_cubes.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/marching_cubes_hoppe.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/marching_cubes_rbf.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/mls.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/on_nurbs/closing_boundary.cpp` | `mid` | 是 | 存在可向量化路径（循环18，数学项103），建议次优先 | `-` |
| `src/on_nurbs/fitting_curve_2d.cpp` | `mid` | 是 | 存在可向量化路径（循环23，数学项104），建议次优先 | `-` |
| `src/on_nurbs/fitting_curve_2d_apdm.cpp` | `high` | 是 | 循环35处，数学/几何计算密集，建议优先优化 | `-` |
| `src/on_nurbs/fitting_curve_2d_asdm.cpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项40），建议次优先 | `-` |
| `src/on_nurbs/fitting_curve_2d_atdm.cpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项26），建议次优先 | `-` |
| `src/on_nurbs/fitting_curve_2d_pdm.cpp` | `high` | 是 | 循环27处，数学/几何计算密集，建议优先优化 | `-` |
| `src/on_nurbs/fitting_curve_2d_sdm.cpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项22），建议次优先 | `-` |
| `src/on_nurbs/fitting_curve_2d_tdm.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项10），建议次优先 | `-` |
| `src/on_nurbs/fitting_curve_pdm.cpp` | `mid` | 是 | 存在可向量化路径（循环15，数学项49），建议次优先 | `-` |
| `src/on_nurbs/fitting_cylinder_pdm.cpp` | `high` | 是 | 循环26处，数学/几何计算密集，建议优先优化 | `-` |
| `src/on_nurbs/fitting_sphere_pdm.cpp` | `mid` | 是 | 存在可向量化路径（循环23，数学项55），建议次优先 | `-` |
| `src/on_nurbs/fitting_surface_im.cpp` | `mid` | 是 | 存在可向量化路径（循环23，数学项52），建议次优先 | `-` |
| `src/on_nurbs/fitting_surface_pdm.cpp` | `high` | 是 | 循环52处，数学/几何计算密集，建议优先优化 | `-` |
| `src/on_nurbs/fitting_surface_tdm.cpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项12），建议次优先 | `-` |
| `src/on_nurbs/global_optimization_pdm.cpp` | `mid` | 是 | 存在可向量化路径（循环23，数学项112），建议次优先 | `-` |
| `src/on_nurbs/global_optimization_tdm.cpp` | `high` | 是 | 循环34处，数学/几何计算密集，建议优先优化 | `-` |
| `src/on_nurbs/nurbs_solve_eigen.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项14），建议次优先 | `-` |
| `src/on_nurbs/nurbs_solve_eigen_sparse.cpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项44），建议次优先 | `-` |
| `src/on_nurbs/nurbs_solve_umfpack.cpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项5），建议次优先 | `-` |
| `src/on_nurbs/nurbs_tools.cpp` | `mid` | 是 | 存在可向量化路径（循环16，数学项57），建议次优先 | `-` |
| `src/on_nurbs/sequential_fitter.cpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项90），建议次优先 | `-` |
| `src/on_nurbs/sparse_mat.cpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项0），建议次优先 | `-` |
| `src/on_nurbs/triangulation.cpp` | `high` | 是 | 循环25处，数学/几何计算密集，建议优先优化 | `-` |
| `src/organized_fast_mesh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/poisson.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/processing.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/simplification_remove_unused_vertices.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项2），建议次优先 | `-` |
| `src/surfel_smoothing.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/texture_mapping.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/vtk_smoothing/vtk_mesh_quadric_decimation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/vtk_smoothing/vtk_mesh_smoothing_laplacian.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/vtk_smoothing/vtk_mesh_smoothing_windowed_sinc.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/vtk_smoothing/vtk_mesh_subdivision.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/vtk_smoothing/vtk_utils.cpp` | `high` | 是 | 循环10处，数学/几何计算密集，建议优先优化 | `-` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/surface/surface-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
