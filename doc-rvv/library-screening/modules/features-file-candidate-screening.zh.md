# features 模块 RVV 第一轮文件级筛选报告

本文档记录 `features` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 覆盖范围：`features/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `137`，已判定 `137`（`137/137` 全覆盖）。
- 目录拆分：`include` `97`，`src` `40`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `features/include/pcl/features/`，`src` 文件以 `src/` 开头显示。

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
| 源码文件总数 | 137 | `features/**` 源码文件，第三方实现仅登记覆盖，不纳入候选主线。 |
| 已判定文件数 | 137 | `137/137` |
| high | 8 | 二轮必查 |
| mid | 85 | 二轮必查 |
| low | 44 | 已覆盖但不进入二轮初始基线 |
| high + mid | 93 | 第一轮候选基线 |
| 候选占比 | 93/137 = 67.9% | high + mid / 源码文件总数 |

## 4. high 候选（8）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/our_cvfh.hpp` | 循环39处，特征描述子计算密集，建议优先优化 |
| `src/range_image_border_extractor.cpp` | 循环34处，特征描述子计算密集，建议优先优化 |
| `impl/integral_image_normal.hpp` | 循环28处，特征描述子计算密集，建议优先优化 |
| `impl/rops_estimation.hpp` | 循环25处，特征描述子计算密集，建议优先优化 |
| `impl/shot.hpp` | 循环16处，特征描述子计算密集，建议优先优化 |
| `impl/gfpfh.hpp` | 循环15处，特征描述子计算密集，建议优先优化 |
| `impl/multiscale_feature_persistence.hpp` | 循环12处，特征描述子计算密集，建议优先优化 |
| `impl/board.hpp` | 循环9处，特征描述子计算密集，建议优先优化 |

## 5. mid 候选（85）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/esf.hpp` | 存在可向量化路径（循环29，数学项25），建议次优先 |
| `impl/brisk_2d.hpp` | 存在可向量化路径（循环24，数学项24），建议次优先 |
| `src/narf.cpp` | 存在可向量化路径（循环22，数学项95），建议次优先 |
| `impl/fpfh.hpp` | 存在可向量化路径（循环14，数学项32），建议次优先 |
| `impl/statistical_multiscale_interest_region_extraction.hpp` | 存在可向量化路径（循环14，数学项5），建议次优先 |
| `impl/organized_edge_detection.hpp` | 存在可向量化路径（循环14，数学项0），建议次优先 |
| `impl/integral_image2D.hpp` | 存在可向量化路径（循环12，数学项1），建议次优先 |
| `impl/normal_based_signature.hpp` | 存在可向量化路径（循环11，数学项38），建议次优先 |
| `impl/shot_omp.hpp` | 存在可向量化路径（循环10，数学项28），建议次优先 |
| `impl/pfh.hpp` | 存在可向量化路径（循环9，数学项15），建议次优先 |
| `impl/gasd.hpp` | 存在可向量化路径（循环8，数学项58），建议次优先 |
| `impl/moment_of_inertia_estimation.hpp` | 存在可向量化路径（循环8，数学项56），建议次优先 |
| `impl/rsd.hpp` | 存在可向量化路径（循环8，数学项49），建议次优先 |
| `impl/range_image_border_extractor.hpp` | 存在可向量化路径（循环8，数学项29），建议次优先 |
| `impl/cvfh.hpp` | 存在可向量化路径（循环8，数学项22），建议次优先 |
| `impl/intensity_spin.hpp` | 存在可向量化路径（循环8，数学项16），建议次优先 |
| `impl/pfhrgb.hpp` | 存在可向量化路径（循环8，数学项11），建议次优先 |
| `impl/usc.hpp` | 存在可向量化路径（循环7，数学项46），建议次优先 |
| `impl/vfh.hpp` | 存在可向量化路径（循环7，数学项32），建议次优先 |
| `impl/3dsc.hpp` | 存在可向量化路径（循环6，数学项70），建议次优先 |
| `impl/shot_lrf.hpp` | 存在可向量化路径（循环6，数学项29），建议次优先 |
| `impl/linear_least_squares_normal.hpp` | 存在可向量化路径（循环6，数学项21），建议次优先 |
| `impl/fpfh_omp.hpp` | 存在可向量化路径（循环6，数学项9），建议次优先 |
| `impl/intensity_gradient.hpp` | 存在可向量化路径（循环5，数学项32），建议次优先 |
| `from_meshes.h` | 存在可向量化路径（循环5，数学项30），建议次优先 |
| `impl/crh.hpp` | 存在可向量化路径（循环5，数学项14），建议次优先 |
| `impl/spin_image.hpp` | 存在可向量化路径（循环4，数学项35），建议次优先 |
| `impl/rift.hpp` | 存在可向量化路径（循环4，数学项24），建议次优先 |
| `impl/ppfrgb.hpp` | 存在可向量化路径（循环4，数学项18），建议次优先 |
| `impl/principal_curvatures.hpp` | 存在可向量化路径（循环4，数学项17），建议次优先 |
| `impl/boundary.hpp` | 存在可向量化路径（循环4，数学项12），建议次优先 |
| `impl/grsd.hpp` | 存在可向量化路径（循环4，数学项4），建议次优先 |
| `impl/moment_invariants.hpp` | 存在可向量化路径（循环4，数学项3），建议次优先 |
| `our_cvfh.h` | 存在可向量化路径（循环2，数学项72），建议次优先 |
| `shot_omp.h` | 存在可向量化路径（循环2，数学项38），建议次优先 |
| `impl/normal_3d.hpp` | 存在可向量化路径（循环2，数学项23），建议次优先 |
| `impl/normal_3d_omp.hpp` | 存在可向量化路径（循环2，数学项19），建议次优先 |
| `impl/ppf.hpp` | 存在可向量化路径（循环2，数学项18），建议次优先 |
| `impl/cppf.hpp` | 存在可向量化路径（循环2，数学项16），建议次优先 |
| `esf.h` | 存在可向量化路径（循环2，数学项15），建议次优先 |
| `impl/flare.hpp` | 存在可向量化路径（循环2，数学项14），建议次优先 |
| `impl/narf.hpp` | 存在可向量化路径（循环2，数学项11），建议次优先 |
| `normal_3d.h` | 存在可向量化路径（循环1，数学项110），建议次优先 |
| `rsd.h` | 存在可向量化路径（循环1，数学项52），建议次优先 |
| `vfh.h` | 存在可向量化路径（循环1，数学项52），建议次优先 |
| `cvfh.h` | 存在可向量化路径（循环1，数学项51），建议次优先 |
| `impl/feature.hpp` | 存在可向量化路径（循环1，数学项30），建议次优先 |
| `spin_image.h` | 存在可向量化路径（循环1，数学项29），建议次优先 |
| `boundary.h` | 存在可向量化路径（循环1，数学项28），建议次优先 |
| `principal_curvatures.h` | 存在可向量化路径（循环1，数学项19），建议次优先 |
| `normal_3d_omp.h` | 存在可向量化路径（循环1，数学项0），建议次优先 |
| `fpfh.h` | 存在可向量化路径（循环0，数学项92），建议次优先 |
| `shot.h` | 存在可向量化路径（循环0，数学项91），建议次优先 |
| `moment_of_inertia_estimation.h` | 存在可向量化路径（循环0，数学项77），建议次优先 |
| `integral_image_normal.h` | 存在可向量化路径（循环0，数学项64），建议次优先 |
| `narf.h` | 存在可向量化路径（循环0，数学项63），建议次优先 |
| `gasd.h` | 存在可向量化路径（循环0，数学项61），建议次优先 |
| `feature.h` | 存在可向量化路径（循环0，数学项56），建议次优先 |
| `board.h` | 存在可向量化路径（循环0，数学项47），建议次优先 |
| `pfh.h` | 存在可向量化路径（循环0，数学项41），建议次优先 |
| `src/pfh.cpp` | 存在可向量化路径（循环0，数学项38），建议次优先 |
| `rops_estimation.h` | 存在可向量化路径（循环0，数学项31），建议次优先 |
| `rift.h` | 存在可向量化路径（循环0，数学项30），建议次优先 |
| `3dsc.h` | 存在可向量化路径（循环0，数学项28），建议次优先 |
| `fpfh_omp.h` | 存在可向量化路径（循环0，数学项28），建议次优先 |
| `grsd.h` | 存在可向量化路径（循环0，数学项26），建议次优先 |
| `moment_invariants.h` | 存在可向量化路径（循环0，数学项23），建议次优先 |
| `flare.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `intensity_spin.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `multiscale_feature_persistence.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `integral_image2D.h` | 存在可向量化路径（循环0，数学项19），建议次优先 |
| `usc.h` | 存在可向量化路径（循环0，数学项19），建议次优先 |
| `crh.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `gfpfh.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `range_image_border_extractor.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `shot_lrf.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `shot_lrf_omp.h` | 存在可向量化路径（循环0，数学项16），建议次优先 |
| `intensity_gradient.h` | 存在可向量化路径（循环0，数学项15），建议次优先 |
| `linear_least_squares_normal.h` | 存在可向量化路径（循环0，数学项15），建议次优先 |
| `pfhrgb.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `organized_edge_detection.h` | 存在可向量化路径（循环0，数学项13），建议次优先 |
| `cppf.h` | 存在可向量化路径（循环0，数学项11），建议次优先 |
| `normal_based_signature.h` | 存在可向量化路径（循环0，数学项11），建议次优先 |
| `ppf.h` | 存在可向量化路径（循环0，数学项9），建议次优先 |
| `ppfrgb.h` | 存在可向量化路径（循环0，数学项9），建议次优先 |

## 6. 全量文件覆盖表（137/137）

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `3dsc.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项28），建议次优先 | `impl/3dsc.hpp` |
| `board.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项47），建议次优先 | `impl/board.hpp` |
| `boundary.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项28），建议次优先 | `impl/boundary.hpp` |
| `brisk_2d.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/brisk_2d.hpp` |
| `cppf.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项11），建议次优先 | `impl/cppf.hpp` |
| `crh.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `impl/crh.hpp` |
| `cvfh.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项51），建议次优先 | `impl/cvfh.hpp` |
| `don.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/don.hpp` |
| `esf.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项15），建议次优先 | `impl/esf.hpp` |
| `feature.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项56），建议次优先 | `impl/feature.hpp` |
| `flare.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `impl/flare.hpp` |
| `fpfh.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项92），建议次优先 | `impl/fpfh.hpp` |
| `fpfh_omp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项28），建议次优先 | `impl/fpfh_omp.hpp` |
| `from_meshes.h` | `mid` | 是 | 存在可向量化路径（循环5，数学项30），建议次优先 | `-` |
| `gasd.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项61），建议次优先 | `impl/gasd.hpp` |
| `gfpfh.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `impl/gfpfh.hpp` |
| `grsd.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项26），建议次优先 | `impl/grsd.hpp` |
| `impl/3dsc.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项70），建议次优先 | `-` |
| `impl/board.hpp` | `high` | 是 | 循环9处，特征描述子计算密集，建议优先优化 | `-` |
| `impl/boundary.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项12），建议次优先 | `-` |
| `impl/brisk_2d.hpp` | `mid` | 是 | 存在可向量化路径（循环24，数学项24），建议次优先 | `-` |
| `impl/cppf.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项16），建议次优先 | `-` |
| `impl/crh.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项14），建议次优先 | `-` |
| `impl/cvfh.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项22），建议次优先 | `-` |
| `impl/don.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/esf.hpp` | `mid` | 是 | 存在可向量化路径（循环29，数学项25），建议次优先 | `-` |
| `impl/feature.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项30），建议次优先 | `-` |
| `impl/flare.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项14），建议次优先 | `-` |
| `impl/fpfh.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项32），建议次优先 | `-` |
| `impl/fpfh_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项9），建议次优先 | `-` |
| `impl/gasd.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项58），建议次优先 | `-` |
| `impl/gfpfh.hpp` | `high` | 是 | 循环15处，特征描述子计算密集，建议优先优化 | `-` |
| `impl/grsd.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项4），建议次优先 | `-` |
| `impl/integral_image2D.hpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项1），建议次优先 | `-` |
| `impl/integral_image_normal.hpp` | `high` | 是 | 循环28处，特征描述子计算密集，建议优先优化 | `-` |
| `impl/intensity_gradient.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项32），建议次优先 | `-` |
| `impl/intensity_spin.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项16），建议次优先 | `-` |
| `impl/linear_least_squares_normal.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项21），建议次优先 | `-` |
| `impl/moment_invariants.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项3），建议次优先 | `-` |
| `impl/moment_of_inertia_estimation.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项56），建议次优先 | `-` |
| `impl/multiscale_feature_persistence.hpp` | `high` | 是 | 循环12处，特征描述子计算密集，建议优先优化 | `-` |
| `impl/narf.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项11），建议次优先 | `-` |
| `impl/normal_3d.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项23），建议次优先 | `-` |
| `impl/normal_3d_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项19），建议次优先 | `-` |
| `impl/normal_based_signature.hpp` | `mid` | 是 | 存在可向量化路径（循环11，数学项38），建议次优先 | `-` |
| `impl/organized_edge_detection.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项0），建议次优先 | `-` |
| `impl/our_cvfh.hpp` | `high` | 是 | 循环39处，特征描述子计算密集，建议优先优化 | `-` |
| `impl/pfh.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项15），建议次优先 | `-` |
| `impl/pfhrgb.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项11），建议次优先 | `-` |
| `impl/ppf.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项18），建议次优先 | `-` |
| `impl/ppfrgb.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项18），建议次优先 | `-` |
| `impl/principal_curvatures.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项17），建议次优先 | `-` |
| `impl/range_image_border_extractor.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项29），建议次优先 | `-` |
| `impl/rift.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项24），建议次优先 | `-` |
| `impl/rops_estimation.hpp` | `high` | 是 | 循环25处，特征描述子计算密集，建议优先优化 | `-` |
| `impl/rsd.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项49），建议次优先 | `-` |
| `impl/shot.hpp` | `high` | 是 | 循环16处，特征描述子计算密集，建议优先优化 | `-` |
| `impl/shot_lrf.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项29），建议次优先 | `-` |
| `impl/shot_lrf_omp.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/shot_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项28），建议次优先 | `-` |
| `impl/spin_image.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项35），建议次优先 | `-` |
| `impl/statistical_multiscale_interest_region_extraction.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项5），建议次优先 | `-` |
| `impl/usc.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项46），建议次优先 | `-` |
| `impl/vfh.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项32），建议次优先 | `-` |
| `integral_image2D.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项19），建议次优先 | `impl/integral_image2D.hpp` |
| `integral_image_normal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项64），建议次优先 | `impl/integral_image_normal.hpp` |
| `intensity_gradient.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项15），建议次优先 | `impl/intensity_gradient.hpp` |
| `intensity_spin.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `impl/intensity_spin.hpp` |
| `linear_least_squares_normal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项15），建议次优先 | `impl/linear_least_squares_normal.hpp` |
| `moment_invariants.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项23），建议次优先 | `impl/moment_invariants.hpp` |
| `moment_of_inertia_estimation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项77），建议次优先 | `impl/moment_of_inertia_estimation.hpp` |
| `multiscale_feature_persistence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `impl/multiscale_feature_persistence.hpp` |
| `narf.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项63），建议次优先 | `impl/narf.hpp` |
| `narf_descriptor.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `normal_3d.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项110），建议次优先 | `impl/normal_3d.hpp` |
| `normal_3d_omp.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项0），建议次优先 | `impl/normal_3d_omp.hpp` |
| `normal_based_signature.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项11），建议次优先 | `impl/normal_based_signature.hpp` |
| `organized_edge_detection.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项13），建议次优先 | `impl/organized_edge_detection.hpp` |
| `our_cvfh.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项72），建议次优先 | `impl/our_cvfh.hpp` |
| `pfh.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项41），建议次优先 | `impl/pfh.hpp` |
| `pfh_tools.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `pfhrgb.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `impl/pfhrgb.hpp` |
| `ppf.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项9），建议次优先 | `impl/ppf.hpp` |
| `ppfrgb.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项9），建议次优先 | `impl/ppfrgb.hpp` |
| `principal_curvatures.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项19），建议次优先 | `impl/principal_curvatures.hpp` |
| `range_image_border_extractor.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `impl/range_image_border_extractor.hpp` |
| `rift.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项30），建议次优先 | `impl/rift.hpp` |
| `rops_estimation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项31），建议次优先 | `impl/rops_estimation.hpp` |
| `rsd.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项52），建议次优先 | `impl/rsd.hpp` |
| `shot.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项91），建议次优先 | `impl/shot.hpp` |
| `shot_lrf.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `impl/shot_lrf.hpp` |
| `shot_lrf_omp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项16），建议次优先 | `impl/shot_lrf_omp.hpp` |
| `shot_omp.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项38），建议次优先 | `impl/shot_omp.hpp` |
| `spin_image.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项29），建议次优先 | `impl/spin_image.hpp` |
| `statistical_multiscale_interest_region_extraction.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/statistical_multiscale_interest_region_extraction.hpp` |
| `usc.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项19），建议次优先 | `impl/usc.hpp` |
| `vfh.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项52），建议次优先 | `impl/vfh.hpp` |
| `src/3dsc.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/board.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/boundary.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/brisk_2d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/cppf.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/crh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/cvfh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/don.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/esf.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/flare.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/fpfh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/from_meshes.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/gasd.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/gfpfh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/grsd.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/integral_image_normal.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/intensity_gradient.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/intensity_spin.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/linear_least_squares_normal.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/moment_invariants.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/moment_of_inertia_estimation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/multiscale_feature_persistence.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/narf.cpp` | `mid` | 是 | 存在可向量化路径（循环22，数学项95），建议次优先 | `-` |
| `src/normal_3d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/normal_based_signature.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/organized_edge_detection.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/our_cvfh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/pfh.cpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项38），建议次优先 | `-` |
| `src/ppf.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/principal_curvatures.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/range_image_border_extractor.cpp` | `high` | 是 | 循环34处，特征描述子计算密集，建议优先优化 | `-` |
| `src/rift.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/rops_estimation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/rsd.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/shot.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/shot_lrf.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/spin_image.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/statistical_multiscale_interest_region_extraction.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/usc.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/vfh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
