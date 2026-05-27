# geometry 模块文件级筛查清单（全覆盖版，重评）

本版按数据结构专用口径重评：全文件覆盖、实现优先。数据结构与几何拓扑模块口径：重点看拓扑遍历、邻接访问与分支重排潜力。

## 1. 覆盖范围与口径

- 覆盖范围：`geometry/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `17`，已判定 `17`（`17/17` 全覆盖）。
- 目录拆分：`include` `17`，`src` `0`。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（1）

| file_path | 说明 |
| --- | --- |
| `geometry/include/pcl/geometry/mesh_base.h` | 循环29处，几何拓扑/邻接核心路径，建议优先优化 |

### 2.2 mid 候选（4）

| file_path | 说明 |
| --- | --- |
| `geometry/include/pcl/geometry/impl/polygon_operations.hpp` | 存在可向量化路径（循环12，分支17），建议次优先 |
| `geometry/include/pcl/geometry/mesh_io.h` | 存在可向量化路径（循环6，分支18），建议次优先 |
| `geometry/include/pcl/geometry/mesh_conversion.h` | 存在可向量化路径（循环5，分支1），建议次优先 |
| `geometry/include/pcl/geometry/polygon_operations.h` | 存在可向量化路径（循环0，分支4），建议次优先 |

## 3. 全量文件覆盖表（17/17）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `geometry/include/pcl/geometry/boost.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `geometry/include/pcl/geometry/get_boundary.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `geometry/include/pcl/geometry/impl/polygon_operations.hpp` | `mid` | 是 | 存在可向量化路径（循环12，分支17），建议次优先 | `-` |
| `geometry/include/pcl/geometry/line_iterator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `geometry/include/pcl/geometry/mesh_base.h` | `high` | 是 | 循环29处，几何拓扑/邻接核心路径，建议优先优化 | `-` |
| `geometry/include/pcl/geometry/mesh_circulators.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `geometry/include/pcl/geometry/mesh_conversion.h` | `mid` | 是 | 存在可向量化路径（循环5，分支1），建议次优先 | `-` |
| `geometry/include/pcl/geometry/mesh_elements.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `geometry/include/pcl/geometry/mesh_indices.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `geometry/include/pcl/geometry/mesh_io.h` | `mid` | 是 | 存在可向量化路径（循环6，分支18），建议次优先 | `-` |
| `geometry/include/pcl/geometry/mesh_traits.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `geometry/include/pcl/geometry/organized_index_iterator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `geometry/include/pcl/geometry/planar_polygon.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `geometry/include/pcl/geometry/polygon_mesh.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `geometry/include/pcl/geometry/polygon_operations.h` | `mid` | 是 | 存在可向量化路径（循环0，分支4），建议次优先 | `geometry/include/pcl/geometry/impl/polygon_operations.hpp` |
| `geometry/include/pcl/geometry/quad_mesh.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `geometry/include/pcl/geometry/triangle_mesh.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |

## 4. 简要统计

- 模块统计：high `1`，mid `4`，low `12`。
- 候选占比：`5/17 = 29.4%`。
- include 口径：候选 `5/17`。
- src 口径：候选 `0/0`。
