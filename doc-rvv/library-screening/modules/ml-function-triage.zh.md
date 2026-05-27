# ml 模块文件级筛查清单（全覆盖版，重评）

本版按统一口径重评：全文件覆盖、实现优先。机器学习模块口径：重点看核函数/聚类/CRF 的批量计算路径。

## 1. 覆盖范围与口径

- 覆盖范围：`ml/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `42`，已判定 `42`（`42/42` 全覆盖）。
- 目录拆分：`include` `34`，`src` `8`。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（4）

| file_path | 说明 |
| --- | --- |
| `ml/src/svm.cpp` | 循环189处，学习/推断核心路径，建议优先优化 |
| `ml/src/permutohedral.cpp` | 循环62处，学习/推断核心路径，建议优先优化 |
| `ml/src/svm_wrapper.cpp` | 循环34处，学习/推断核心路径，建议优先优化 |
| `ml/include/pcl/ml/impl/dt/decision_tree_trainer.hpp` | 循环8处，学习/推断核心路径，建议优先优化 |

### 2.2 mid 候选（12）

| file_path | 说明 |
| --- | --- |
| `ml/src/densecrf.cpp` | 存在可向量化路径（循环17，分支6），建议次优先 |
| `ml/include/pcl/ml/impl/ferns/fern_trainer.hpp` | 存在可向量化路径（循环16，分支3），建议次优先 |
| `ml/src/kmeans.cpp` | 存在可向量化路径（循环13，分支13），建议次优先 |
| `ml/include/pcl/ml/impl/ferns/fern_evaluator.hpp` | 存在可向量化路径（循环9，分支0），建议次优先 |
| `ml/include/pcl/ml/regression_variance_stats_estimator.h` | 存在可向量化路径（循环8，分支1），建议次优先 |
| `ml/src/pairwise_potential.cpp` | 存在可向量化路径（循环7，分支2），建议次优先 |
| `ml/include/pcl/ml/impl/dt/decision_tree_evaluator.hpp` | 存在可向量化路径（循环7，分支0），建议次优先 |
| `ml/include/pcl/ml/ferns/fern.h` | 存在可向量化路径（循环6，分支0），建议次优先 |
| `ml/include/pcl/ml/svm_wrapper.h` | 存在可向量化路径（循环3，分支21），建议次优先 |
| `ml/include/pcl/ml/impl/dt/decision_forest_evaluator.hpp` | 存在可向量化路径（循环3，分支0），建议次优先 |
| `ml/include/pcl/ml/impl/kmeans.hpp` | 存在可向量化路径（循环2，分支10），建议次优先 |
| `ml/include/pcl/ml/kmeans.h` | 存在可向量化路径（循环1，分支2），建议次优先 |

## 3. 全量文件覆盖表（42/42）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `ml/include/pcl/ml/branch_estimator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/densecrf.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/dt/decision_forest.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/dt/decision_forest_evaluator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/dt/decision_forest_trainer.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/dt/decision_tree.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/dt/decision_tree_data_provider.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/dt/decision_tree_evaluator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/dt/decision_tree_trainer.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/feature_handler.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/ferns/fern.h` | `mid` | 是 | 存在可向量化路径（循环6，分支0），建议次优先 | `-` |
| `ml/include/pcl/ml/ferns/fern_evaluator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/ferns/fern_trainer.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/impl/dt/decision_forest_evaluator.hpp` | `mid` | 是 | 存在可向量化路径（循环3，分支0），建议次优先 | `-` |
| `ml/include/pcl/ml/impl/dt/decision_forest_trainer.hpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/impl/dt/decision_tree_evaluator.hpp` | `mid` | 是 | 存在可向量化路径（循环7，分支0），建议次优先 | `-` |
| `ml/include/pcl/ml/impl/dt/decision_tree_trainer.hpp` | `high` | 是 | 循环8处，学习/推断核心路径，建议优先优化 | `-` |
| `ml/include/pcl/ml/impl/ferns/fern_evaluator.hpp` | `mid` | 是 | 存在可向量化路径（循环9，分支0），建议次优先 | `-` |
| `ml/include/pcl/ml/impl/ferns/fern_trainer.hpp` | `mid` | 是 | 存在可向量化路径（循环16，分支3），建议次优先 | `-` |
| `ml/include/pcl/ml/impl/kmeans.hpp` | `mid` | 是 | 存在可向量化路径（循环2，分支10），建议次优先 | `-` |
| `ml/include/pcl/ml/impl/svm/svm_wrapper.hpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/kmeans.h` | `mid` | 是 | 存在可向量化路径（循环1，分支2），建议次优先 | `ml/include/pcl/ml/impl/kmeans.hpp` |
| `ml/include/pcl/ml/multi_channel_2d_comparison_feature.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/multi_channel_2d_comparison_feature_handler.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/multi_channel_2d_data_set.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/multiple_data_2d_example_index.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/pairwise_potential.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/permutohedral.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/point_xy_32f.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/point_xy_32i.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/regression_variance_stats_estimator.h` | `mid` | 是 | 存在可向量化路径（循环8，分支1），建议次优先 | `-` |
| `ml/include/pcl/ml/stats_estimator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/svm.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/include/pcl/ml/svm_wrapper.h` | `mid` | 是 | 存在可向量化路径（循环3，分支21），建议次优先 | `-` |
| `ml/src/densecrf.cpp` | `mid` | 是 | 存在可向量化路径（循环17，分支6），建议次优先 | `-` |
| `ml/src/kmeans.cpp` | `mid` | 是 | 存在可向量化路径（循环13，分支13），建议次优先 | `-` |
| `ml/src/pairwise_potential.cpp` | `mid` | 是 | 存在可向量化路径（循环7，分支2），建议次优先 | `-` |
| `ml/src/permutohedral.cpp` | `high` | 是 | 循环62处，学习/推断核心路径，建议优先优化 | `-` |
| `ml/src/point_xy_32f.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/src/point_xy_32i.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ml/src/svm.cpp` | `high` | 是 | 循环189处，学习/推断核心路径，建议优先优化 | `-` |
| `ml/src/svm_wrapper.cpp` | `high` | 是 | 循环34处，学习/推断核心路径，建议优先优化 | `-` |

## 4. 简要统计

- 模块统计：high `4`，mid `12`，low `26`。
- 候选占比：`16/42 = 38.1%`。
- include 口径：候选 `10/34`。
- src 口径：候选 `6/8`。
