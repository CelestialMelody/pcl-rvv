# octree 模块文件级筛查清单（全覆盖版，重评）

本版按数据结构专用口径重评：全文件覆盖、实现优先。八叉树模块口径：重点看节点遍历、邻接访问与查询批量化。

## 1. 覆盖范围与口径

- 覆盖范围：`octree/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `27`，已判定 `27`（`27/27` 全覆盖）。
- 目录拆分：`include` `26`，`src` `1`。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（3）

| file_path | 说明 |
| --- | --- |
| `octree/include/pcl/octree/impl/octree_search.hpp` | 循环12处，树遍历/查询核心路径，建议优先优化 |
| `octree/include/pcl/octree/impl/octree2buf_base.hpp` | 循环7处，树遍历/查询核心路径，建议优先优化 |
| `octree/include/pcl/octree/octree_search.h` | 循环0处，树遍历/查询核心路径，建议优先优化 |

### 2.2 mid 候选（10）

| file_path | 说明 |
| --- | --- |
| `octree/include/pcl/octree/impl/octree_pointcloud_adjacency.hpp` | 存在可向量化路径（循环9，分支17），建议次优先 |
| `octree/include/pcl/octree/impl/octree_pointcloud.hpp` | 存在可向量化路径（循环7，分支26），建议次优先 |
| `octree/include/pcl/octree/octree2buf_base.h` | 存在可向量化路径（循环7，分支26），建议次优先 |
| `octree/include/pcl/octree/impl/octree_iterator.hpp` | 存在可向量化路径（循环5，分支19），建议次优先 |
| `octree/include/pcl/octree/impl/octree_base.hpp` | 存在可向量化路径（循环3，分支29），建议次优先 |
| `octree/include/pcl/octree/octree_base.h` | 存在可向量化路径（循环2，分支11），建议次优先 |
| `octree/include/pcl/octree/octree_iterator.h` | 存在可向量化路径（循环1，分支16），建议次优先 |
| `octree/include/pcl/octree/octree_pointcloud.h` | 存在可向量化路径（循环0，分支12），建议次优先 |
| `octree/include/pcl/octree/octree_pointcloud_adjacency.h` | 存在可向量化路径（循环0，分支3），建议次优先 |
| `octree/include/pcl/octree/octree_pointcloud_voxelcentroid.h` | 存在可向量化路径（循环0，分支3），建议次优先 |

## 3. 全量文件覆盖表（27/27）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `octree/include/pcl/octree/impl/octree2buf_base.hpp` | `high` | 是 | 循环7处，树遍历/查询核心路径，建议优先优化 | `-` |
| `octree/include/pcl/octree/impl/octree_base.hpp` | `mid` | 是 | 存在可向量化路径（循环3，分支29），建议次优先 | `-` |
| `octree/include/pcl/octree/impl/octree_iterator.hpp` | `mid` | 是 | 存在可向量化路径（循环5，分支19），建议次优先 | `-` |
| `octree/include/pcl/octree/impl/octree_pointcloud.hpp` | `mid` | 是 | 存在可向量化路径（循环7，分支26），建议次优先 | `-` |
| `octree/include/pcl/octree/impl/octree_pointcloud_adjacency.hpp` | `mid` | 是 | 存在可向量化路径（循环9，分支17），建议次优先 | `-` |
| `octree/include/pcl/octree/impl/octree_pointcloud_voxelcentroid.hpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/impl/octree_search.hpp` | `high` | 是 | 循环12处，树遍历/查询核心路径，建议优先优化 | `-` |
| `octree/include/pcl/octree/octree.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree2buf_base.h` | `mid` | 是 | 存在可向量化路径（循环7，分支26），建议次优先 | `octree/include/pcl/octree/impl/octree2buf_base.hpp` |
| `octree/include/pcl/octree/octree_base.h` | `mid` | 是 | 存在可向量化路径（循环2，分支11），建议次优先 | `octree/include/pcl/octree/impl/octree_base.hpp` |
| `octree/include/pcl/octree/octree_container.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree_impl.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree_iterator.h` | `mid` | 是 | 存在可向量化路径（循环1，分支16），建议次优先 | `octree/include/pcl/octree/impl/octree_iterator.hpp` |
| `octree/include/pcl/octree/octree_key.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree_node_pool.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree_nodes.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree_pointcloud.h` | `mid` | 是 | 存在可向量化路径（循环0，分支12），建议次优先 | `octree/include/pcl/octree/impl/octree_pointcloud.hpp` |
| `octree/include/pcl/octree/octree_pointcloud_adjacency.h` | `mid` | 是 | 存在可向量化路径（循环0，分支3），建议次优先 | `octree/include/pcl/octree/impl/octree_pointcloud_adjacency.hpp` |
| `octree/include/pcl/octree/octree_pointcloud_adjacency_container.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree_pointcloud_changedetector.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree_pointcloud_density.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree_pointcloud_occupancy.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree_pointcloud_pointvector.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree_pointcloud_singlepoint.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `octree/include/pcl/octree/octree_pointcloud_voxelcentroid.h` | `mid` | 是 | 存在可向量化路径（循环0，分支3），建议次优先 | `octree/include/pcl/octree/impl/octree_pointcloud_voxelcentroid.hpp` |
| `octree/include/pcl/octree/octree_search.h` | `high` | 是 | 循环0处，树遍历/查询核心路径，建议优先优化 | `octree/include/pcl/octree/impl/octree_search.hpp` |
| `octree/src/octree_inst.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |

## 4. 简要统计

- 模块统计：high `3`，mid `10`，low `14`。
- 候选占比：`13/27 = 48.1%`。
- include 口径：候选 `13/26`。
- src 口径：候选 `0/1`。
