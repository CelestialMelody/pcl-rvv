# search 模块文件级筛查清单（全覆盖版，重评）

本版按数据结构专用口径重评：全文件覆盖、实现优先。数据结构查询模块口径：重点看树遍历分支与批量查询重排潜力。

## 1. 覆盖范围与口径

- 覆盖范围：`search/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `21`，已判定 `21`（`21/21` 全覆盖）。
- 目录拆分：`include` `15`，`src` `6`。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（3）

| file_path | 说明 |
| --- | --- |
| `search/include/pcl/search/kdtree_nanoflann.h` | 循环10处，查询/检索核心路径，建议优先优化 |
| `search/include/pcl/search/impl/search.hpp` | 循环6处，查询/检索核心路径，建议优先优化 |
| `search/include/pcl/search/search.h` | 循环4处，查询/检索核心路径，建议优先优化 |

### 2.2 mid 候选（8）

| file_path | 说明 |
| --- | --- |
| `search/include/pcl/search/impl/flann_search.hpp` | 存在可向量化路径（循环16，分支29），建议次优先 |
| `search/include/pcl/search/impl/brute_force.hpp` | 存在可向量化路径（循环14，分支35），建议次优先 |
| `search/include/pcl/search/impl/organized.hpp` | 存在可向量化路径（循环11，分支32），建议次优先 |
| `search/include/pcl/search/organized.h` | 存在可向量化路径（循环3，分支16），建议次优先 |
| `search/include/pcl/search/flann_search.h` | 存在可向量化路径（循环0，分支4），建议次优先 |
| `search/include/pcl/search/kdtree.h` | 存在可向量化路径（循环0，分支3），建议次优先 |
| `search/include/pcl/search/brute_force.h` | 存在可向量化路径（循环0，分支1），建议次优先 |
| `search/include/pcl/search/auto.h` | 存在可向量化路径（循环0，分支0），建议次优先 |

## 3. 全量文件覆盖表（21/21）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `search/include/pcl/search/auto.h` | `mid` | 是 | 存在可向量化路径（循环0，分支0），建议次优先 | `search/include/pcl/search/impl/auto.hpp` |
| `search/include/pcl/search/brute_force.h` | `mid` | 是 | 存在可向量化路径（循环0，分支1），建议次优先 | `search/include/pcl/search/impl/brute_force.hpp` |
| `search/include/pcl/search/flann_search.h` | `mid` | 是 | 存在可向量化路径（循环0，分支4），建议次优先 | `search/include/pcl/search/impl/flann_search.hpp` |
| `search/include/pcl/search/impl/auto.hpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `search/include/pcl/search/impl/brute_force.hpp` | `mid` | 是 | 存在可向量化路径（循环14，分支35），建议次优先 | `-` |
| `search/include/pcl/search/impl/flann_search.hpp` | `mid` | 是 | 存在可向量化路径（循环16，分支29），建议次优先 | `-` |
| `search/include/pcl/search/impl/kdtree.hpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `search/include/pcl/search/impl/organized.hpp` | `mid` | 是 | 存在可向量化路径（循环11，分支32），建议次优先 | `-` |
| `search/include/pcl/search/impl/search.hpp` | `high` | 是 | 循环6处，查询/检索核心路径，建议优先优化 | `-` |
| `search/include/pcl/search/kdtree.h` | `mid` | 是 | 存在可向量化路径（循环0，分支3），建议次优先 | `search/include/pcl/search/impl/kdtree.hpp` |
| `search/include/pcl/search/kdtree_nanoflann.h` | `high` | 是 | 循环10处，查询/检索核心路径，建议优先优化 | `-` |
| `search/include/pcl/search/octree.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `search/include/pcl/search/organized.h` | `mid` | 是 | 存在可向量化路径（循环3，分支16），建议次优先 | `search/include/pcl/search/impl/organized.hpp` |
| `search/include/pcl/search/pcl_search.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `search/include/pcl/search/search.h` | `high` | 是 | 循环4处，查询/检索核心路径，建议优先优化 | `search/include/pcl/search/impl/search.hpp` |
| `search/src/auto.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `search/src/brute_force.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `search/src/kdtree.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `search/src/octree.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `search/src/organized.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `search/src/search.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |

## 4. 简要统计

- 模块统计：high `3`，mid `8`，low `10`。
- 候选占比：`11/21 = 52.4%`。
- include 口径：候选 `11/15`。
- src 口径：候选 `0/6`。
