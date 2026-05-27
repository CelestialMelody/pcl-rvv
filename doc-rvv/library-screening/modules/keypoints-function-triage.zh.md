# keypoints 模块文件级筛查清单（全覆盖版，重评）

本版按当前统一标准重评：全文件覆盖、实现优先、允许推翻旧结论；`3rdparty/**` 仅登记覆盖，不纳入候选。

## 1. 覆盖范围与口径

- 覆盖范围：`keypoints/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `36`，已判定 `36`（`36/36` 全覆盖）。
- 目录拆分：`include` `25`，`src` `11`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（4）

| file_path | 说明 |
| --- | --- |
| `keypoints/src/narf_keypoint.cpp` | 循环48处，关键点响应计算密集，建议优先优化 |
| `keypoints/src/brisk_2d.cpp` | 循环34处，关键点响应计算密集，建议优先优化 |
| `keypoints/include/pcl/keypoints/impl/harris_3d.hpp` | 循环13处，关键点响应计算密集，建议优先优化 |
| `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` | 循环11处，关键点响应计算密集，建议优先优化 |

### 2.2 mid 候选（20）

| file_path | 说明 |
| --- | --- |
| `keypoints/src/agast_2d.cpp` | 存在可向量化路径（循环20，数学项45），建议次优先 |
| `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` | 存在可向量化路径（循环14，数学项16），建议次优先 |
| `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` | 存在可向量化路径（循环14，数学项3），建议次优先 |
| `keypoints/include/pcl/keypoints/impl/smoothed_surfaces_keypoint.hpp` | 存在可向量化路径（循环11，数学项5），建议次优先 |
| `keypoints/include/pcl/keypoints/impl/harris_6d.hpp` | 存在可向量化路径（循环10，数学项88），建议次优先 |
| `keypoints/include/pcl/keypoints/impl/susan.hpp` | 存在可向量化路径（循环7，数学项71），建议次优先 |
| `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` | 存在可向量化路径（循环7，数学项10），建议次优先 |
| `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` | 存在可向量化路径（循环7，数学项0），建议次优先 |
| `keypoints/include/pcl/keypoints/agast_2d.h` | 存在可向量化路径（循环0，数学项94），建议次优先 |
| `keypoints/include/pcl/keypoints/brisk_2d.h` | 存在可向量化路径（循环0，数学项41），建议次优先 |
| `keypoints/include/pcl/keypoints/susan.h` | 存在可向量化路径（循环0，数学项29），建议次优先 |
| `keypoints/include/pcl/keypoints/harris_3d.h` | 存在可向量化路径（循环0，数学项27），建议次优先 |
| `keypoints/include/pcl/keypoints/harris_2d.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `keypoints/include/pcl/keypoints/harris_6d.h` | 存在可向量化路径（循环0，数学项20），建议次优先 |
| `keypoints/include/pcl/keypoints/trajkovic_3d.h` | 存在可向量化路径（循环0，数学项19），建议次优先 |
| `keypoints/include/pcl/keypoints/iss_3d.h` | 存在可向量化路径（循环0，数学项16），建议次优先 |
| `keypoints/include/pcl/keypoints/sift_keypoint.h` | 存在可向量化路径（循环0，数学项15），建议次优先 |
| `keypoints/include/pcl/keypoints/trajkovic_2d.h` | 存在可向量化路径（循环0，数学项13），建议次优先 |
| `keypoints/include/pcl/keypoints/smoothed_surfaces_keypoint.h` | 存在可向量化路径（循环0，数学项10），建议次优先 |
| `keypoints/include/pcl/keypoints/keypoint.h` | 存在可向量化路径（循环0，数学项8），建议次优先 |

## 3. 全量文件覆盖表（36/36）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `keypoints/include/pcl/keypoints/agast_2d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项94），建议次优先 | `keypoints/include/pcl/keypoints/impl/agast_2d.hpp` |
| `keypoints/include/pcl/keypoints/brisk_2d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项41），建议次优先 | `keypoints/include/pcl/keypoints/impl/brisk_2d.hpp` |
| `keypoints/include/pcl/keypoints/harris_2d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| `keypoints/include/pcl/keypoints/harris_3d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项27），建议次优先 | `keypoints/include/pcl/keypoints/impl/harris_3d.hpp` |
| `keypoints/include/pcl/keypoints/harris_6d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项20），建议次优先 | `keypoints/include/pcl/keypoints/impl/harris_6d.hpp` |
| `keypoints/include/pcl/keypoints/impl/agast_2d.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `keypoints/include/pcl/keypoints/impl/brisk_2d.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项3），建议次优先 | `-` |
| `keypoints/include/pcl/keypoints/impl/harris_3d.hpp` | `high` | 是 | 循环13处，关键点响应计算密集，建议优先优化 | `-` |
| `keypoints/include/pcl/keypoints/impl/harris_6d.hpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项88），建议次优先 | `-` |
| `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` | `mid` | 是 | 存在可向量化路径（循环14，数学项16），建议次优先 | `-` |
| `keypoints/include/pcl/keypoints/impl/keypoint.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` | `high` | 是 | 循环11处，关键点响应计算密集，建议优先优化 | `-` |
| `keypoints/include/pcl/keypoints/impl/smoothed_surfaces_keypoint.hpp` | `mid` | 是 | 存在可向量化路径（循环11，数学项5），建议次优先 | `-` |
| `keypoints/include/pcl/keypoints/impl/susan.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项71），建议次优先 | `-` |
| `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项0），建议次优先 | `-` |
| `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项10），建议次优先 | `-` |
| `keypoints/include/pcl/keypoints/iss_3d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项16），建议次优先 | `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` |
| `keypoints/include/pcl/keypoints/keypoint.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项8），建议次优先 | `keypoints/include/pcl/keypoints/impl/keypoint.hpp` |
| `keypoints/include/pcl/keypoints/narf_keypoint.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `keypoints/include/pcl/keypoints/sift_keypoint.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项15），建议次优先 | `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` |
| `keypoints/include/pcl/keypoints/smoothed_surfaces_keypoint.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项10），建议次优先 | `keypoints/include/pcl/keypoints/impl/smoothed_surfaces_keypoint.hpp` |
| `keypoints/include/pcl/keypoints/susan.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项29），建议次优先 | `keypoints/include/pcl/keypoints/impl/susan.hpp` |
| `keypoints/include/pcl/keypoints/trajkovic_2d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项13），建议次优先 | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` |
| `keypoints/include/pcl/keypoints/trajkovic_3d.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项19），建议次优先 | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` |
| `keypoints/src/agast_2d.cpp` | `mid` | 是 | 存在可向量化路径（循环20，数学项45），建议次优先 | `-` |
| `keypoints/src/brisk_2d.cpp` | `high` | 是 | 循环34处，关键点响应计算密集，建议优先优化 | `-` |
| `keypoints/src/harris_3d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `keypoints/src/harris_6d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `keypoints/src/iss_3d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `keypoints/src/narf_keypoint.cpp` | `high` | 是 | 循环48处，关键点响应计算密集，建议优先优化 | `-` |
| `keypoints/src/sift_keypoint.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `keypoints/src/smoothed_surfaces_keypoint.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `keypoints/src/susan.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `keypoints/src/trajkovic_2d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `keypoints/src/trajkovic_3d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |

## 4. 简要统计

- 模块统计：high `4`，mid `20`，low `12`。
- 候选占比：`24/36 = 66.7%`。
- include 口径：候选 `21/25`。
- src 口径：候选 `3/11`。
- thirdparty 口径：`0` 文件已登记，候选 `0`。
