# features 模块文件级筛查清单（全覆盖版，重评）

本版按与 `registration/surface/filters/io` 相同标准重评：全文件覆盖、实现优先、允许推翻旧结论；`3rdparty/**` 仅登记覆盖，不纳入候选。

## 1. 覆盖范围与口径

- 覆盖范围：`features/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `137`，已判定 `137`（`137/137` 全覆盖）。
- 目录拆分：`include` `97`，`src` `40`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（8）

| file_path | 说明 |
| --- | --- |
| `features/include/pcl/features/impl/our_cvfh.hpp` | 循环39处，特征描述子计算密集，建议优先优化 |
| `features/src/range_image_border_extractor.cpp` | 循环34处，特征描述子计算密集，建议优先优化 |
| `features/include/pcl/features/impl/integral_image_normal.hpp` | 循环28处，特征描述子计算密集，建议优先优化 |
| `features/include/pcl/features/impl/rops_estimation.hpp` | 循环25处，特征描述子计算密集，建议优先优化 |
| `features/include/pcl/features/impl/shot.hpp` | 循环16处，特征描述子计算密集，建议优先优化 |
| `features/include/pcl/features/impl/gfpfh.hpp` | 循环15处，特征描述子计算密集，建议优先优化 |
| `features/include/pcl/features/impl/multiscale_feature_persistence.hpp` | 循环12处，特征描述子计算密集，建议优先优化 |
| `features/include/pcl/features/impl/board.hpp` | 循环9处，特征描述子计算密集，建议优先优化 |

### 2.2 mid 候选（85）

| file_path | 说明 |
| --- | --- |
| `features/include/pcl/features/impl/esf.hpp` | 存在可向量化路径（循环29，数学项25），建议次优先 |
| `features/include/pcl/features/impl/brisk_2d.hpp` | 存在可向量化路径（循环24，数学项24），建议次优先 |
| `features/src/narf.cpp` | 存在可向量化路径（循环22，数学项95），建议次优先 |
| `features/include/pcl/features/impl/fpfh.hpp` | 存在可向量化路径（循环14，数学项32），建议次优先 |
| `features/include/pcl/features/impl/statistical_multiscale_interest_region_extraction.hpp` | 存在可向量化路径（循环14，数学项5），建议次优先 |
| `features/include/pcl/features/impl/organized_edge_detection.hpp` | 存在可向量化路径（循环14，数学项0），建议次优先 |
| `features/include/pcl/features/impl/integral_image2D.hpp` | 存在可向量化路径（循环12，数学项1），建议次优先 |
| `features/include/pcl/features/impl/normal_based_signature.hpp` | 存在可向量化路径（循环11，数学项38），建议次优先 |
| `features/include/pcl/features/impl/shot_omp.hpp` | 存在可向量化路径（循环10，数学项28），建议次优先 |
| `features/include/pcl/features/impl/pfh.hpp` | 存在可向量化路径（循环9，数学项15），建议次优先 |
| `features/include/pcl/features/impl/gasd.hpp` | 存在可向量化路径（循环8，数学项58），建议次优先 |
| `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` | 存在可向量化路径（循环8，数学项56），建议次优先 |
| `features/include/pcl/features/impl/rsd.hpp` | 存在可向量化路径（循环8，数学项49），建议次优先 |
| `features/include/pcl/features/impl/range_image_border_extractor.hpp` | 存在可向量化路径（循环8，数学项29），建议次优先 |
| `features/include/pcl/features/impl/cvfh.hpp` | 存在可向量化路径（循环8，数学项22），建议次优先 |
| `features/include/pcl/features/impl/intensity_spin.hpp` | 存在可向量化路径（循环8，数学项16），建议次优先 |
| `features/include/pcl/features/impl/pfhrgb.hpp` | 存在可向量化路径（循环8，数学项11），建议次优先 |
| `features/include/pcl/features/impl/usc.hpp` | 存在可向量化路径（循环7，数学项46），建议次优先 |
| `features/include/pcl/features/impl/vfh.hpp` | 存在可向量化路径（循环7，数学项32），建议次优先 |
| `features/include/pcl/features/impl/3dsc.hpp` | 存在可向量化路径（循环6，数学项70），建议次优先 |
| `features/include/pcl/features/impl/shot_lrf.hpp` | 存在可向量化路径（循环6，数学项29），建议次优先 |
| `features/include/pcl/features/impl/linear_least_squares_normal.hpp` | 存在可向量化路径（循环6，数学项21），建议次优先 |
| `features/include/pcl/features/impl/fpfh_omp.hpp` | 存在可向量化路径（循环6，数学项9），建议次优先 |
| `features/include/pcl/features/impl/intensity_gradient.hpp` | 存在可向量化路径（循环5，数学项32），建议次优先 |
| `features/include/pcl/features/from_meshes.h` | 存在可向量化路径（循环5，数学项30），建议次优先 |
| `features/include/pcl/features/impl/crh.hpp` | 存在可向量化路径（循环5，数学项14），建议次优先 |
| `features/include/pcl/features/impl/spin_image.hpp` | 存在可向量化路径（循环4，数学项35），建议次优先 |
| `features/include/pcl/features/impl/rift.hpp` | 存在可向量化路径（循环4，数学项24），建议次优先 |
| `features/include/pcl/features/impl/ppfrgb.hpp` | 存在可向量化路径（循环4，数学项18），建议次优先 |
| `features/include/pcl/features/impl/principal_curvatures.hpp` | 存在可向量化路径（循环4，数学项17），建议次优先 |
| `features/include/pcl/features/impl/boundary.hpp` | 存在可向量化路径（循环4，数学项12），建议次优先 |
| `features/include/pcl/features/impl/grsd.hpp` | 存在可向量化路径（循环4，数学项4），建议次优先 |
| `features/include/pcl/features/impl/moment_invariants.hpp` | 存在可向量化路径（循环4，数学项3），建议次优先 |
| `features/include/pcl/features/our_cvfh.h` | 存在可向量化路径（循环2，数学项72），建议次优先 |
| `features/include/pcl/features/shot_omp.h` | 存在可向量化路径（循环2，数学项38），建议次优先 |
| `features/include/pcl/features/impl/normal_3d.hpp` | 存在可向量化路径（循环2，数学项23），建议次优先 |
| `features/include/pcl/features/impl/normal_3d_omp.hpp` | 存在可向量化路径（循环2，数学项19），建议次优先 |
| `features/include/pcl/features/impl/ppf.hpp` | 存在可向量化路径（循环2，数学项18），建议次优先 |
| `features/include/pcl/features/impl/cppf.hpp` | 存在可向量化路径（循环2，数学项16），建议次优先 |
| `features/include/pcl/features/esf.h` | 存在可向量化路径（循环2，数学项15），建议次优先 |
| `features/include/pcl/features/impl/flare.hpp` | 存在可向量化路径（循环2，数学项14），建议次优先 |
| `features/include/pcl/features/impl/narf.hpp` | 存在可向量化路径（循环2，数学项11），建议次优先 |
| `features/include/pcl/features/normal_3d.h` | 存在可向量化路径（循环1，数学项110），建议次优先 |
| `features/include/pcl/features/rsd.h` | 存在可向量化路径（循环1，数学项52），建议次优先 |
| `features/include/pcl/features/vfh.h` | 存在可向量化路径（循环1，数学项52），建议次优先 |
| `features/include/pcl/features/cvfh.h` | 存在可向量化路径（循环1，数学项51），建议次优先 |
| `features/include/pcl/features/impl/feature.hpp` | 存在可向量化路径（循环1，数学项30），建议次优先 |
| `features/include/pcl/features/spin_image.h` | 存在可向量化路径（循环1，数学项29），建议次优先 |
| `features/include/pcl/features/boundary.h` | 存在可向量化路径（循环1，数学项28），建议次优先 |
| `features/include/pcl/features/principal_curvatures.h` | 存在可向量化路径（循环1，数学项19），建议次优先 |
| `features/include/pcl/features/normal_3d_omp.h` | 存在可向量化路径（循环1，数学项0），建议次优先 |
| `features/include/pcl/features/fpfh.h` | 存在可向量化路径（循环0，数学项92），建议次优先 |
| `features/include/pcl/features/shot.h` | 存在可向量化路径（循环0，数学项91），建议次优先 |
| `features/include/pcl/features/moment_of_inertia_estimation.h` | 存在可向量化路径（循环0，数学项77），建议次优先 |
| `features/include/pcl/features/integral_image_normal.h` | 存在可向量化路径（循环0，数学项64），建议次优先 |
| `features/include/pcl/features/narf.h` | 存在可向量化路径（循环0，数学项63），建议次优先 |
| `features/include/pcl/features/gasd.h` | 存在可向量化路径（循环0，数学项61），建议次优先 |
| `features/include/pcl/features/feature.h` | 存在可向量化路径（循环0，数学项56），建议次优先 |
| `features/include/pcl/features/board.h` | 存在可向量化路径（循环0，数学项47），建议次优先 |
| `features/include/pcl/features/pfh.h` | 存在可向量化路径（循环0，数学项41），建议次优先 |
| `features/src/pfh.cpp` | 存在可向量化路径（循环0，数学项38），建议次优先 |
| `features/include/pcl/features/rops_estimation.h` | 存在可向量化路径（循环0，数学项31），建议次优先 |
| `features/include/pcl/features/rift.h` | 存在可向量化路径（循环0，数学项30），建议次优先 |
| `features/include/pcl/features/3dsc.h` | 存在可向量化路径（循环0，数学项28），建议次优先 |
| `features/include/pcl/features/fpfh_omp.h` | 存在可向量化路径（循环0，数学项28），建议次优先 |
| `features/include/pcl/features/grsd.h` | 存在可向量化路径（循环0，数学项26），建议次优先 |
| `features/include/pcl/features/moment_invariants.h` | 存在可向量化路径（循环0，数学项23），建议次优先 |
| `features/include/pcl/features/flare.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `features/include/pcl/features/intensity_spin.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `features/include/pcl/features/multiscale_feature_persistence.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `features/include/pcl/features/integral_image2D.h` | 存在可向量化路径（循环0，数学项19），建议次优先 |
| `features/include/pcl/features/usc.h` | 存在可向量化路径（循环0，数学项19），建议次优先 |
| `features/include/pcl/features/crh.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `features/include/pcl/features/gfpfh.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `features/include/pcl/features/range_image_border_extractor.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `features/include/pcl/features/shot_lrf.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `features/include/pcl/features/shot_lrf_omp.h` | 存在可向量化路径（循环0，数学项16），建议次优先 |
| `features/include/pcl/features/intensity_gradient.h` | 存在可向量化路径（循环0，数学项15），建议次优先 |
| `features/include/pcl/features/linear_least_squares_normal.h` | 存在可向量化路径（循环0，数学项15），建议次优先 |
| `features/include/pcl/features/pfhrgb.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `features/include/pcl/features/organized_edge_detection.h` | 存在可向量化路径（循环0，数学项13），建议次优先 |
| `features/include/pcl/features/cppf.h` | 存在可向量化路径（循环0，数学项11），建议次优先 |
| `features/include/pcl/features/normal_based_signature.h` | 存在可向量化路径（循环0，数学项11），建议次优先 |
| `features/include/pcl/features/ppf.h` | 存在可向量化路径（循环0，数学项9），建议次优先 |
| `features/include/pcl/features/ppfrgb.h` | 存在可向量化路径（循环0，数学项9），建议次优先 |

## 3. 全量文件覆盖表（137/137）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `features/include/pcl/features/3dsc.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项28），建议次优先 | `features/include/pcl/features/impl/3dsc.hpp` |
| `features/include/pcl/features/board.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项47），建议次优先 | `features/include/pcl/features/impl/board.hpp` |
| `features/include/pcl/features/boundary.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项28），建议次优先 | `features/include/pcl/features/impl/boundary.hpp` |
| `features/include/pcl/features/brisk_2d.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `features/include/pcl/features/impl/brisk_2d.hpp` |
| `features/include/pcl/features/cppf.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项11），建议次优先 | `features/include/pcl/features/impl/cppf.hpp` |
| `features/include/pcl/features/crh.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `features/include/pcl/features/impl/crh.hpp` |
| `features/include/pcl/features/cvfh.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项51），建议次优先 | `features/include/pcl/features/impl/cvfh.hpp` |
| `features/include/pcl/features/don.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `features/include/pcl/features/impl/don.hpp` |
| `features/include/pcl/features/esf.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项15），建议次优先 | `features/include/pcl/features/impl/esf.hpp` |
| `features/include/pcl/features/feature.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项56），建议次优先 | `features/include/pcl/features/impl/feature.hpp` |
| `features/include/pcl/features/flare.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `features/include/pcl/features/impl/flare.hpp` |
| `features/include/pcl/features/fpfh.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项92），建议次优先 | `features/include/pcl/features/impl/fpfh.hpp` |
| `features/include/pcl/features/fpfh_omp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项28），建议次优先 | `features/include/pcl/features/impl/fpfh_omp.hpp` |
| `features/include/pcl/features/from_meshes.h` | `mid` | 是 | 存在可向量化路径（循环5，数学项30），建议次优先 | `-` |
| `features/include/pcl/features/gasd.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项61），建议次优先 | `features/include/pcl/features/impl/gasd.hpp` |
| `features/include/pcl/features/gfpfh.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `features/include/pcl/features/impl/gfpfh.hpp` |
| `features/include/pcl/features/grsd.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项26），建议次优先 | `features/include/pcl/features/impl/grsd.hpp` |
| `features/include/pcl/features/impl/3dsc.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项70），建议次优先 | `-` |
| `features/include/pcl/features/impl/board.hpp` | `high` | 是 | 循环9处，特征描述子计算密集，建议优先优化 | `-` |
| `features/include/pcl/features/impl/boundary.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项12），建议次优先 | `-` |
| `features/include/pcl/features/impl/brisk_2d.hpp` | `mid` | 是 | 存在可向量化路径（循环24，数学项24），建议次优先 | `-` |
| `features/include/pcl/features/impl/cppf.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项16），建议次优先 | `-` |
| `features/include/pcl/features/impl/crh.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项14），建议次优先 | `-` |
| `features/include/pcl/features/impl/cvfh.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项22），建议次优先 | `-` |
| `features/include/pcl/features/impl/don.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/include/pcl/features/impl/esf.hpp` | `mid` | 是 | 存在可向量化路径（循环29，数学项25），建议次优先 | `-` |
| `features/include/pcl/features/impl/feature.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项30），建议次优先 | `-` |
| `features/include/pcl/features/impl/flare.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项14），建议次优先 | `-` |
| `features/include/pcl/features/impl/fpfh.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项32），建议次优先 | `-` |
| `features/include/pcl/features/impl/fpfh_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项9），建议次优先 | `-` |
| `features/include/pcl/features/impl/gasd.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项58），建议次优先 | `-` |
| `features/include/pcl/features/impl/gfpfh.hpp` | `high` | 是 | 循环15处，特征描述子计算密集，建议优先优化 | `-` |
| `features/include/pcl/features/impl/grsd.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项4），建议次优先 | `-` |
| `features/include/pcl/features/impl/integral_image2D.hpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项1），建议次优先 | `-` |
| `features/include/pcl/features/impl/integral_image_normal.hpp` | `high` | 是 | 循环28处，特征描述子计算密集，建议优先优化 | `-` |
| `features/include/pcl/features/impl/intensity_gradient.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项32），建议次优先 | `-` |
| `features/include/pcl/features/impl/intensity_spin.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项16），建议次优先 | `-` |
| `features/include/pcl/features/impl/linear_least_squares_normal.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项21），建议次优先 | `-` |
| `features/include/pcl/features/impl/moment_invariants.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项3），建议次优先 | `-` |
| `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项56），建议次优先 | `-` |
| `features/include/pcl/features/impl/multiscale_feature_persistence.hpp` | `high` | 是 | 循环12处，特征描述子计算密集，建议优先优化 | `-` |
| `features/include/pcl/features/impl/narf.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项11），建议次优先 | `-` |
| `features/include/pcl/features/impl/normal_3d.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项23），建议次优先 | `-` |
| `features/include/pcl/features/impl/normal_3d_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项19），建议次优先 | `-` |
| `features/include/pcl/features/impl/normal_based_signature.hpp` | `mid` | 是 | 存在可向量化路径（循环11，数学项38），建议次优先 | `-` |
| `features/include/pcl/features/impl/organized_edge_detection.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项0），建议次优先 | `-` |
| `features/include/pcl/features/impl/our_cvfh.hpp` | `high` | 是 | 循环39处，特征描述子计算密集，建议优先优化 | `-` |
| `features/include/pcl/features/impl/pfh.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项15），建议次优先 | `-` |
| `features/include/pcl/features/impl/pfhrgb.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项11），建议次优先 | `-` |
| `features/include/pcl/features/impl/ppf.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项18），建议次优先 | `-` |
| `features/include/pcl/features/impl/ppfrgb.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项18），建议次优先 | `-` |
| `features/include/pcl/features/impl/principal_curvatures.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项17），建议次优先 | `-` |
| `features/include/pcl/features/impl/range_image_border_extractor.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项29），建议次优先 | `-` |
| `features/include/pcl/features/impl/rift.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项24），建议次优先 | `-` |
| `features/include/pcl/features/impl/rops_estimation.hpp` | `high` | 是 | 循环25处，特征描述子计算密集，建议优先优化 | `-` |
| `features/include/pcl/features/impl/rsd.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项49），建议次优先 | `-` |
| `features/include/pcl/features/impl/shot.hpp` | `high` | 是 | 循环16处，特征描述子计算密集，建议优先优化 | `-` |
| `features/include/pcl/features/impl/shot_lrf.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项29），建议次优先 | `-` |
| `features/include/pcl/features/impl/shot_lrf_omp.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/include/pcl/features/impl/shot_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项28），建议次优先 | `-` |
| `features/include/pcl/features/impl/spin_image.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项35），建议次优先 | `-` |
| `features/include/pcl/features/impl/statistical_multiscale_interest_region_extraction.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项5），建议次优先 | `-` |
| `features/include/pcl/features/impl/usc.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项46），建议次优先 | `-` |
| `features/include/pcl/features/impl/vfh.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项32），建议次优先 | `-` |
| `features/include/pcl/features/integral_image2D.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项19），建议次优先 | `features/include/pcl/features/impl/integral_image2D.hpp` |
| `features/include/pcl/features/integral_image_normal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项64），建议次优先 | `features/include/pcl/features/impl/integral_image_normal.hpp` |
| `features/include/pcl/features/intensity_gradient.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项15），建议次优先 | `features/include/pcl/features/impl/intensity_gradient.hpp` |
| `features/include/pcl/features/intensity_spin.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `features/include/pcl/features/impl/intensity_spin.hpp` |
| `features/include/pcl/features/linear_least_squares_normal.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项15），建议次优先 | `features/include/pcl/features/impl/linear_least_squares_normal.hpp` |
| `features/include/pcl/features/moment_invariants.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项23），建议次优先 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| `features/include/pcl/features/moment_of_inertia_estimation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项77），建议次优先 | `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` |
| `features/include/pcl/features/multiscale_feature_persistence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `features/include/pcl/features/impl/multiscale_feature_persistence.hpp` |
| `features/include/pcl/features/narf.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项63），建议次优先 | `features/include/pcl/features/impl/narf.hpp` |
| `features/include/pcl/features/narf_descriptor.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/include/pcl/features/normal_3d.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项110），建议次优先 | `features/include/pcl/features/impl/normal_3d.hpp` |
| `features/include/pcl/features/normal_3d_omp.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项0），建议次优先 | `features/include/pcl/features/impl/normal_3d_omp.hpp` |
| `features/include/pcl/features/normal_based_signature.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项11），建议次优先 | `features/include/pcl/features/impl/normal_based_signature.hpp` |
| `features/include/pcl/features/organized_edge_detection.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项13），建议次优先 | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| `features/include/pcl/features/our_cvfh.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项72），建议次优先 | `features/include/pcl/features/impl/our_cvfh.hpp` |
| `features/include/pcl/features/pfh.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项41），建议次优先 | `features/include/pcl/features/impl/pfh.hpp` |
| `features/include/pcl/features/pfh_tools.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/include/pcl/features/pfhrgb.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `features/include/pcl/features/impl/pfhrgb.hpp` |
| `features/include/pcl/features/ppf.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项9），建议次优先 | `features/include/pcl/features/impl/ppf.hpp` |
| `features/include/pcl/features/ppfrgb.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项9），建议次优先 | `features/include/pcl/features/impl/ppfrgb.hpp` |
| `features/include/pcl/features/principal_curvatures.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项19），建议次优先 | `features/include/pcl/features/impl/principal_curvatures.hpp` |
| `features/include/pcl/features/range_image_border_extractor.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `features/include/pcl/features/impl/range_image_border_extractor.hpp` |
| `features/include/pcl/features/rift.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项30），建议次优先 | `features/include/pcl/features/impl/rift.hpp` |
| `features/include/pcl/features/rops_estimation.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项31），建议次优先 | `features/include/pcl/features/impl/rops_estimation.hpp` |
| `features/include/pcl/features/rsd.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项52），建议次优先 | `features/include/pcl/features/impl/rsd.hpp` |
| `features/include/pcl/features/shot.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项91），建议次优先 | `features/include/pcl/features/impl/shot.hpp` |
| `features/include/pcl/features/shot_lrf.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `features/include/pcl/features/impl/shot_lrf.hpp` |
| `features/include/pcl/features/shot_lrf_omp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项16），建议次优先 | `features/include/pcl/features/impl/shot_lrf_omp.hpp` |
| `features/include/pcl/features/shot_omp.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项38），建议次优先 | `features/include/pcl/features/impl/shot_omp.hpp` |
| `features/include/pcl/features/spin_image.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项29），建议次优先 | `features/include/pcl/features/impl/spin_image.hpp` |
| `features/include/pcl/features/statistical_multiscale_interest_region_extraction.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `features/include/pcl/features/impl/statistical_multiscale_interest_region_extraction.hpp` |
| `features/include/pcl/features/usc.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项19），建议次优先 | `features/include/pcl/features/impl/usc.hpp` |
| `features/include/pcl/features/vfh.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项52），建议次优先 | `features/include/pcl/features/impl/vfh.hpp` |
| `features/src/3dsc.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/board.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/boundary.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/brisk_2d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/cppf.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/crh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/cvfh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/don.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/esf.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/flare.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/fpfh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/from_meshes.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/gasd.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/gfpfh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/grsd.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/integral_image_normal.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/intensity_gradient.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/intensity_spin.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/linear_least_squares_normal.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/moment_invariants.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/moment_of_inertia_estimation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/multiscale_feature_persistence.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/narf.cpp` | `mid` | 是 | 存在可向量化路径（循环22，数学项95），建议次优先 | `-` |
| `features/src/normal_3d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/normal_based_signature.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/organized_edge_detection.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/our_cvfh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/pfh.cpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项38），建议次优先 | `-` |
| `features/src/ppf.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/principal_curvatures.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/range_image_border_extractor.cpp` | `high` | 是 | 循环34处，特征描述子计算密集，建议优先优化 | `-` |
| `features/src/rift.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/rops_estimation.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/rsd.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/shot.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/shot_lrf.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/spin_image.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/statistical_multiscale_interest_region_extraction.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/usc.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `features/src/vfh.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |

## 4. 简要统计

- 模块统计：high `8`，mid `85`，low `44`。
- 候选占比：`93/137 = 67.9%`。
- include 口径：候选 `90/97`。
- src 口径：候选 `3/40`。
- thirdparty 口径：`0` 文件已登记，候选 `0`。
