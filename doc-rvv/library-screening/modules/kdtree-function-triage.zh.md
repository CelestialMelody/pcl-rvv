# kdtree 模块文件级筛查清单（全覆盖版，重评）

本版按数据结构模块专用口径重评：全文件覆盖、实现优先，并额外关注树遍历分支、查询批量化潜力与访存规整性。

## 1. 覆盖范围与口径

- 覆盖范围：`kdtree/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `6`，已判定 `6`（`6/6` 全覆盖）。
- 目录拆分：`include` `5`，`src` `1`。
- 数据结构口径：查询树结构模块中，允许分支存在；重点看批量查询路径是否可重排/分块来提升并行度。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（2）

| file_path | 说明 |
| --- | --- |
| `kdtree/include/pcl/kdtree/impl/kdtree_flann.hpp` | 循环6处，树查询/索引核心路径，建议优先优化 |
| `kdtree/include/pcl/kdtree/kdtree.h` | 循环0处，树查询/索引核心路径，建议优先优化 |

### 2.2 mid 候选（3）

| file_path | 说明 |
| --- | --- |
| `kdtree/include/pcl/kdtree/impl/io.hpp` | 存在可向量化路径（循环2，分支0），建议次优先 |
| `kdtree/include/pcl/kdtree/kdtree_flann.h` | 存在可向量化路径（循环0，分支8），建议次优先 |
| `kdtree/include/pcl/kdtree/io.h` | 存在可向量化路径（循环0，分支0），建议次优先 |

## 3. 全量文件覆盖表（6/6）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `kdtree/include/pcl/kdtree/impl/io.hpp` | `mid` | 是 | 存在可向量化路径（循环2，分支0），建议次优先 | `-` |
| `kdtree/include/pcl/kdtree/impl/kdtree_flann.hpp` | `high` | 是 | 循环6处，树查询/索引核心路径，建议优先优化 | `-` |
| `kdtree/include/pcl/kdtree/io.h` | `mid` | 是 | 存在可向量化路径（循环0，分支0），建议次优先 | `kdtree/include/pcl/kdtree/impl/io.hpp` |
| `kdtree/include/pcl/kdtree/kdtree.h` | `high` | 是 | 循环0处，树查询/索引核心路径，建议优先优化 | `-` |
| `kdtree/include/pcl/kdtree/kdtree_flann.h` | `mid` | 是 | 存在可向量化路径（循环0，分支8），建议次优先 | `kdtree/include/pcl/kdtree/impl/kdtree_flann.hpp` |
| `kdtree/src/kdtree_flann.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |

## 4. 简要统计

- 模块统计：high `2`，mid `3`，low `1`。
- 候选占比：`5/6 = 83.3%`。
- include 口径：候选 `5/5`。
- src 口径：候选 `0/1`。
