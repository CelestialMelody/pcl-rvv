# tracking 模块文件级筛查清单（全覆盖版，重评）

本版按当前统一标准重评：全文件覆盖、实现优先、允许推翻旧结论；`3rdparty/**` 仅登记覆盖，不纳入候选。

## 1. 覆盖范围与口径

- 覆盖范围：`tracking/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `30`，已判定 `30`（`30/30` 全覆盖）。
- 目录拆分：`include` `26`，`src` `4`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（2）

| file_path | 说明 |
| --- | --- |
| `tracking/include/pcl/tracking/impl/pyramidal_klt.hpp` | 循环27处，跟踪状态估计计算密集，建议优先优化 |
| `tracking/include/pcl/tracking/impl/particle_filter.hpp` | 循环21处，跟踪状态估计计算密集，建议优先优化 |

### 2.2 mid 候选（20）

| file_path | 说明 |
| --- | --- |
| `tracking/include/pcl/tracking/impl/kld_adaptive_particle_filter_omp.hpp` | 存在可向量化路径（循环6，数学项4），建议次优先 |
| `tracking/include/pcl/tracking/impl/particle_filter_omp.hpp` | 存在可向量化路径（循环6，数学项4），建议次优先 |
| `tracking/include/pcl/tracking/impl/tracking.hpp` | 存在可向量化路径（循环5，数学项74），建议次优先 |
| `tracking/include/pcl/tracking/impl/kld_adaptive_particle_filter.hpp` | 存在可向量化路径（循环4，数学项5），建议次优先 |
| `tracking/include/pcl/tracking/kld_adaptive_particle_filter.h` | 存在可向量化路径（循环3，数学项20），建议次优先 |
| `tracking/include/pcl/tracking/impl/nearest_pair_point_cloud_coherence.hpp` | 存在可向量化路径（循环3，数学项7），建议次优先 |
| `tracking/include/pcl/tracking/impl/approx_nearest_pair_point_cloud_coherence.hpp` | 存在可向量化路径（循环3，数学项6），建议次优先 |
| `tracking/include/pcl/tracking/impl/coherence.hpp` | 存在可向量化路径（循环1，数学项8），建议次优先 |
| `tracking/include/pcl/tracking/particle_filter.h` | 存在可向量化路径（循环0，数学项64），建议次优先 |
| `tracking/include/pcl/tracking/pyramidal_klt.h` | 存在可向量化路径（循环0，数学项42），建议次优先 |
| `tracking/include/pcl/tracking/hsv_color_coherence.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `tracking/include/pcl/tracking/tracking.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `tracking/include/pcl/tracking/coherence.h` | 存在可向量化路径（循环0，数学项20），建议次优先 |
| `tracking/include/pcl/tracking/kld_adaptive_particle_filter_omp.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `tracking/include/pcl/tracking/particle_filter_omp.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `tracking/include/pcl/tracking/distance_coherence.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `tracking/include/pcl/tracking/normal_coherence.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `tracking/include/pcl/tracking/nearest_pair_point_cloud_coherence.h` | 存在可向量化路径（循环0，数学项13），建议次优先 |
| `tracking/include/pcl/tracking/tracker.h` | 存在可向量化路径（循环0，数学项9），建议次优先 |
| `tracking/include/pcl/tracking/approx_nearest_pair_point_cloud_coherence.h` | 存在可向量化路径（循环0，数学项7），建议次优先 |

## 3. 全量文件覆盖表（30/30）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `tracking/include/pcl/tracking/approx_nearest_pair_point_cloud_coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项7），建议次优先 | `tracking/include/pcl/tracking/impl/approx_nearest_pair_point_cloud_coherence.hpp` |
| `tracking/include/pcl/tracking/coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项20），建议次优先 | `tracking/include/pcl/tracking/impl/coherence.hpp` |
| `tracking/include/pcl/tracking/distance_coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `tracking/include/pcl/tracking/impl/distance_coherence.hpp` |
| `tracking/include/pcl/tracking/hsv_color_coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `tracking/include/pcl/tracking/impl/hsv_color_coherence.hpp` |
| `tracking/include/pcl/tracking/impl/approx_nearest_pair_point_cloud_coherence.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项6），建议次优先 | `-` |
| `tracking/include/pcl/tracking/impl/coherence.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项8），建议次优先 | `-` |
| `tracking/include/pcl/tracking/impl/distance_coherence.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `tracking/include/pcl/tracking/impl/hsv_color_coherence.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `tracking/include/pcl/tracking/impl/kld_adaptive_particle_filter.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项5），建议次优先 | `-` |
| `tracking/include/pcl/tracking/impl/kld_adaptive_particle_filter_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项4），建议次优先 | `-` |
| `tracking/include/pcl/tracking/impl/nearest_pair_point_cloud_coherence.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项7），建议次优先 | `-` |
| `tracking/include/pcl/tracking/impl/normal_coherence.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `tracking/include/pcl/tracking/impl/particle_filter.hpp` | `high` | 是 | 循环21处，跟踪状态估计计算密集，建议优先优化 | `-` |
| `tracking/include/pcl/tracking/impl/particle_filter_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项4），建议次优先 | `-` |
| `tracking/include/pcl/tracking/impl/pyramidal_klt.hpp` | `high` | 是 | 循环27处，跟踪状态估计计算密集，建议优先优化 | `-` |
| `tracking/include/pcl/tracking/impl/tracker.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `tracking/include/pcl/tracking/impl/tracking.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项74），建议次优先 | `-` |
| `tracking/include/pcl/tracking/kld_adaptive_particle_filter.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项20），建议次优先 | `tracking/include/pcl/tracking/impl/kld_adaptive_particle_filter.hpp` |
| `tracking/include/pcl/tracking/kld_adaptive_particle_filter_omp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `tracking/include/pcl/tracking/impl/kld_adaptive_particle_filter_omp.hpp` |
| `tracking/include/pcl/tracking/nearest_pair_point_cloud_coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项13），建议次优先 | `tracking/include/pcl/tracking/impl/nearest_pair_point_cloud_coherence.hpp` |
| `tracking/include/pcl/tracking/normal_coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `tracking/include/pcl/tracking/impl/normal_coherence.hpp` |
| `tracking/include/pcl/tracking/particle_filter.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项64），建议次优先 | `tracking/include/pcl/tracking/impl/particle_filter.hpp` |
| `tracking/include/pcl/tracking/particle_filter_omp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `tracking/include/pcl/tracking/impl/particle_filter_omp.hpp` |
| `tracking/include/pcl/tracking/pyramidal_klt.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项42），建议次优先 | `tracking/include/pcl/tracking/impl/pyramidal_klt.hpp` |
| `tracking/include/pcl/tracking/tracker.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项9），建议次优先 | `tracking/include/pcl/tracking/impl/tracker.hpp` |
| `tracking/include/pcl/tracking/tracking.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `tracking/include/pcl/tracking/impl/tracking.hpp` |
| `tracking/src/coherence.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `tracking/src/kld_adaptive_particle_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `tracking/src/particle_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `tracking/src/tracking.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |

## 4. 简要统计

- 模块统计：high `2`，mid `20`，low `8`。
- 候选占比：`22/30 = 73.3%`。
- include 口径：候选 `22/26`。
- src 口径：候选 `0/4`。
- thirdparty 口径：`0` 文件已登记，候选 `0`。
