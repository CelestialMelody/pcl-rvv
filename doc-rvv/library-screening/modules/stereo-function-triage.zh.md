# stereo 模块文件级筛查清单（全覆盖版，重评）

本版按统一口径重评：全文件覆盖、实现优先。双目模块口径：重点看代价体构建与窗口聚合循环。

## 1. 覆盖范围与口径

- 覆盖范围：`stereo/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `11`，已判定 `11`（`11/11` 全覆盖）。
- 目录拆分：`include` `5`，`src` `6`。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（1）

| file_path | 说明 |
| --- | --- |
| `stereo/src/stereo_matching.cpp` | 循环33处，双目匹配核心路径，建议优先优化 |

### 2.2 mid 候选（5）

| file_path | 说明 |
| --- | --- |
| `stereo/src/stereo_adaptive_cost_so.cpp` | 存在可向量化路径（循环20，分支6），建议次优先 |
| `stereo/src/stereo_block_based.cpp` | 存在可向量化路径（循环13，分支4），建议次优先 |
| `stereo/include/pcl/stereo/impl/disparity_map_converter.hpp` | 存在可向量化路径（循环4，分支5），建议次优先 |
| `stereo/src/digital_elevation_map.cpp` | 存在可向量化路径（循环4，分支5），建议次优先 |
| `stereo/include/pcl/stereo/stereo_matching.h` | 存在可向量化路径（循环0，分支5），建议次优先 |

## 3. 全量文件覆盖表（11/11）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `stereo/include/pcl/stereo/digital_elevation_map.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `stereo/include/pcl/stereo/disparity_map_converter.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `stereo/include/pcl/stereo/impl/disparity_map_converter.hpp` |
| `stereo/include/pcl/stereo/impl/disparity_map_converter.hpp` | `mid` | 是 | 存在可向量化路径（循环4，分支5），建议次优先 | `-` |
| `stereo/include/pcl/stereo/stereo_grabber.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `stereo/include/pcl/stereo/stereo_matching.h` | `mid` | 是 | 存在可向量化路径（循环0，分支5），建议次优先 | `-` |
| `stereo/src/digital_elevation_map.cpp` | `mid` | 是 | 存在可向量化路径（循环4，分支5），建议次优先 | `-` |
| `stereo/src/disparity_map_converter.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `stereo/src/stereo_adaptive_cost_so.cpp` | `mid` | 是 | 存在可向量化路径（循环20，分支6），建议次优先 | `-` |
| `stereo/src/stereo_block_based.cpp` | `mid` | 是 | 存在可向量化路径（循环13，分支4），建议次优先 | `-` |
| `stereo/src/stereo_grabber.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `stereo/src/stereo_matching.cpp` | `high` | 是 | 循环33处，双目匹配核心路径，建议优先优化 | `-` |

## 4. 简要统计

- 模块统计：high `1`，mid `5`，low `5`。
- 候选占比：`6/11 = 54.5%`。
- include 口径：候选 `2/5`。
- src 口径：候选 `4/6`。
