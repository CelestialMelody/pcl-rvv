# recognition 模块 RVV 第一轮文件级筛选报告

本文档记录 `recognition` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 覆盖范围：`recognition/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `70`，已判定 `70`（`70/70` 全覆盖）。
- 目录拆分：`include` `53`，`src` `17`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `recognition/include/pcl/recognition/`，`src` 文件以 `src/` 开头显示。

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
| 源码文件总数 | 70 | `recognition/**` 源码文件，第三方实现仅登记覆盖，不纳入候选主线。 |
| 已判定文件数 | 70 | `70/70` |
| high | 5 | 二轮必查 |
| mid | 27 | 二轮必查 |
| low | 38 | 已覆盖但不进入二轮初始基线 |
| high + mid | 32 | 第一轮候选基线 |
| 候选占比 | 32/70 = 45.7% | high + mid / 源码文件总数 |

## 4. high 候选（5）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/implicit_shape_model.hpp` | 循环107处，识别/匹配计算密集，建议优先优化 |
| `src/linemod.cpp` | 循环86处，识别/匹配计算密集，建议优先优化 |
| `surface_normal_modality.h` | 循环59处，识别/匹配计算密集，建议优先优化 |
| `color_gradient_modality.h` | 循环38处，识别/匹配计算密集，建议优先优化 |
| `src/face_detection/rf_face_detector_trainer.cpp` | 循环19处，识别/匹配计算密集，建议优先优化 |

## 5. mid 候选（27）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/linemod/line_rgbd.hpp` | 存在可向量化路径（循环37，数学项37），建议次优先 |
| `impl/hv/hv_go.hpp` | 存在可向量化路径（循环34，数学项35），建议次优先 |
| `src/face_detection/face_detector_data_provider.cpp` | 存在可向量化路径（循环26，数学项19），建议次优先 |
| `src/ransac_based/obj_rec_ransac.cpp` | 存在可向量化路径（循环23，数学项31），建议次优先 |
| `color_modality.h` | 存在可向量化路径（循环22，数学项37），建议次优先 |
| `src/ransac_based/orr_octree.cpp` | 存在可向量化路径（循环21，数学项16），建议次优先 |
| `face_detection/rf_face_utils.h` | 存在可向量化路径（循环18，数学项73），建议次优先 |
| `color_gradient_dot_modality.h` | 存在可向量化路径（循环16，数学项30），建议次优先 |
| `face_detection/face_common.h` | 存在可向量化路径（循环14，数学项12），建议次优先 |
| `impl/hv/hv_papazov.hpp` | 存在可向量化路径（循环12，数学项7），建议次优先 |
| `src/dotmod.cpp` | 存在可向量化路径（循环12，数学项4），建议次优先 |
| `impl/ransac_based/simple_octree.hpp` | 存在可向量化路径（循环11，数学项23），建议次优先 |
| `src/cg/hough_3d.cpp` | 存在可向量化路径（循环10，数学项26），建议次优先 |
| `src/ransac_based/orr_octree_zprojection.cpp` | 存在可向量化路径（循环10，数学项3），建议次优先 |
| `hv/hv_go.h` | 存在可向量化路径（循环9，数学项12），建议次优先 |
| `impl/hv/occlusion_reasoning.hpp` | 存在可向量化路径（循环8，数学项8），建议次优先 |
| `impl/cg/hough_3d.hpp` | 存在可向量化路径（循环7，数学项62），建议次优先 |
| `crh_alignment.h` | 存在可向量化路径（循环7，数学项40），建议次优先 |
| `impl/hv/greedy_verification.hpp` | 存在可向量化路径（循环7，数学项5），建议次优先 |
| `face_detection/rf_face_detector_trainer.h` | 存在可向量化路径（循环6，数学项22），建议次优先 |
| `src/quantizable_modality.cpp` | 存在可向量化路径（循环6，数学项1），建议次优先 |
| `impl/cg/geometric_consistency.hpp` | 存在可向量化路径（循环5，数学项25），建议次优先 |
| `linemod.h` | 存在可向量化路径（循环4，数学项24），建议次优先 |
| `src/ransac_based/model_library.cpp` | 存在可向量化路径（循环4，数学项5），建议次优先 |
| `hv/greedy_verification.h` | 存在可向量化路径（循环3，数学项13），建议次优先 |
| `ransac_based/trimmed_icp.h` | 存在可向量化路径（循环3，数学项11），建议次优先 |
| `implicit_shape_model.h` | 存在可向量化路径（循环0，数学项73），建议次优先 |

## 6. 全量文件覆盖表（70/70）

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `cg/correspondence_grouping.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `cg/geometric_consistency.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `cg/hough_3d.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `color_gradient_dot_modality.h` | `mid` | 是 | 存在可向量化路径（循环16，数学项30），建议次优先 | `-` |
| `color_gradient_modality.h` | `high` | 是 | 循环38处，识别/匹配计算密集，建议优先优化 | `-` |
| `color_modality.h` | `mid` | 是 | 存在可向量化路径（循环22，数学项37），建议次优先 | `-` |
| `crh_alignment.h` | `mid` | 是 | 存在可向量化路径（循环7，数学项40），建议次优先 | `-` |
| `dense_quantized_multi_mod_template.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `distance_map.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `dot_modality.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `dotmod.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `face_detection/face_common.h` | `mid` | 是 | 存在可向量化路径（循环14，数学项12），建议次优先 | `-` |
| `face_detection/face_detector_data_provider.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `face_detection/rf_face_detector_trainer.h` | `mid` | 是 | 存在可向量化路径（循环6，数学项22），建议次优先 | `-` |
| `face_detection/rf_face_utils.h` | `mid` | 是 | 存在可向量化路径（循环18，数学项73），建议次优先 | `-` |
| `hv/greedy_verification.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项13），建议次优先 | `-` |
| `hv/hv_go.h` | `mid` | 是 | 存在可向量化路径（循环9，数学项12），建议次优先 | `-` |
| `hv/hv_papazov.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `hv/hypotheses_verification.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `hv/occlusion_reasoning.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/cg/correspondence_grouping.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/cg/geometric_consistency.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项25），建议次优先 | `-` |
| `impl/cg/hough_3d.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项62），建议次优先 | `-` |
| `impl/hv/greedy_verification.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项5），建议次优先 | `-` |
| `impl/hv/hv_go.hpp` | `mid` | 是 | 存在可向量化路径（循环34，数学项35），建议次优先 | `-` |
| `impl/hv/hv_papazov.hpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项7），建议次优先 | `-` |
| `impl/hv/occlusion_reasoning.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项8），建议次优先 | `-` |
| `impl/implicit_shape_model.hpp` | `high` | 是 | 循环107处，识别/匹配计算密集，建议优先优化 | `-` |
| `impl/linemod/line_rgbd.hpp` | `mid` | 是 | 存在可向量化路径（循环37，数学项37），建议次优先 | `-` |
| `impl/ransac_based/simple_octree.hpp` | `mid` | 是 | 存在可向量化路径（循环11，数学项23），建议次优先 | `-` |
| `impl/ransac_based/voxel_structure.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `implicit_shape_model.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项73），建议次优先 | `impl/implicit_shape_model.hpp` |
| `linemod.h` | `mid` | 是 | 存在可向量化路径（循环4，数学项24），建议次优先 | `-` |
| `linemod/line_rgbd.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `mask_map.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `point_types.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `quantizable_modality.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `quantized_map.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ransac_based/auxiliary.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ransac_based/bvh.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ransac_based/hypothesis.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ransac_based/model_library.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ransac_based/obj_rec_ransac.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ransac_based/orr_graph.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ransac_based/orr_octree.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ransac_based/orr_octree_zprojection.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ransac_based/rigid_transform_space.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ransac_based/simple_octree.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ransac_based/trimmed_icp.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项11），建议次优先 | `-` |
| `ransac_based/voxel_structure.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `region_xy.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sparse_quantized_multi_mod_template.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `surface_normal_modality.h` | `high` | 是 | 循环59处，识别/匹配计算密集，建议优先优化 | `-` |
| `src/cg/geometric_consistency.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/cg/hough_3d.cpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项26），建议次优先 | `-` |
| `src/dotmod.cpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项4），建议次优先 | `-` |
| `src/face_detection/face_detector_data_provider.cpp` | `mid` | 是 | 存在可向量化路径（循环26，数学项19），建议次优先 | `-` |
| `src/face_detection/rf_face_detector_trainer.cpp` | `high` | 是 | 循环19处，识别/匹配计算密集，建议优先优化 | `-` |
| `src/hv/greedy_verification.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/hv/hv_go.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/hv/hv_papazov.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/hv/occlusion_reasoning.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/implicit_shape_model.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/linemod.cpp` | `high` | 是 | 循环86处，识别/匹配计算密集，建议优先优化 | `-` |
| `src/mask_map.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/quantizable_modality.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项1），建议次优先 | `-` |
| `src/ransac_based/model_library.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项5），建议次优先 | `-` |
| `src/ransac_based/obj_rec_ransac.cpp` | `mid` | 是 | 存在可向量化路径（循环23，数学项31），建议次优先 | `-` |
| `src/ransac_based/orr_octree.cpp` | `mid` | 是 | 存在可向量化路径（循环21，数学项16），建议次优先 | `-` |
| `src/ransac_based/orr_octree_zprojection.cpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项3），建议次优先 | `-` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
