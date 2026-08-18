# ml 模块 RVV 第一轮文件级筛选报告

本文档记录 `ml` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 覆盖范围：`ml/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `42`，已判定 `42`（`42/42` 全覆盖）。
- 目录拆分：`include` `34`，`src` `8`。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `ml/include/pcl/ml/`，`src` 文件以 `src/` 开头显示。

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
| 源码文件总数 | 42 | `ml/**` 源码文件。 |
| 已判定文件数 | 42 | `42/42` |
| high | 4 | 二轮必查 |
| mid | 12 | 二轮必查 |
| low | 26 | 已覆盖但不进入二轮初始基线 |
| high + mid | 16 | 第一轮候选基线 |
| 候选占比 | 16/42 = 38.1% | high + mid / 源码文件总数 |

## 4. high 候选（4）

| 文件 | 第一轮证据 |
| --- | --- |
| `src/svm.cpp` | 循环189处，学习/推断核心路径，建议优先优化 |
| `src/permutohedral.cpp` | 循环62处，学习/推断核心路径，建议优先优化 |
| `src/svm_wrapper.cpp` | 循环34处，学习/推断核心路径，建议优先优化 |
| `impl/dt/decision_tree_trainer.hpp` | 循环8处，学习/推断核心路径，建议优先优化 |

## 5. mid 候选（12）

| 文件 | 第一轮证据 |
| --- | --- |
| `src/densecrf.cpp` | 存在可向量化路径（循环17，分支6），建议次优先 |
| `impl/ferns/fern_trainer.hpp` | 存在可向量化路径（循环16，分支3），建议次优先 |
| `src/kmeans.cpp` | 存在可向量化路径（循环13，分支13），建议次优先 |
| `impl/ferns/fern_evaluator.hpp` | 存在可向量化路径（循环9，分支0），建议次优先 |
| `regression_variance_stats_estimator.h` | 存在可向量化路径（循环8，分支1），建议次优先 |
| `src/pairwise_potential.cpp` | 存在可向量化路径（循环7，分支2），建议次优先 |
| `impl/dt/decision_tree_evaluator.hpp` | 存在可向量化路径（循环7，分支0），建议次优先 |
| `ferns/fern.h` | 存在可向量化路径（循环6，分支0），建议次优先 |
| `svm_wrapper.h` | 存在可向量化路径（循环3，分支21），建议次优先 |
| `impl/dt/decision_forest_evaluator.hpp` | 存在可向量化路径（循环3，分支0），建议次优先 |
| `impl/kmeans.hpp` | 存在可向量化路径（循环2，分支10），建议次优先 |
| `kmeans.h` | 存在可向量化路径（循环1，分支2），建议次优先 |

## 6. 全量文件覆盖表（42/42）

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `branch_estimator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `densecrf.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `dt/decision_forest.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `dt/decision_forest_evaluator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `dt/decision_forest_trainer.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `dt/decision_tree.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `dt/decision_tree_data_provider.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `dt/decision_tree_evaluator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `dt/decision_tree_trainer.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `feature_handler.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ferns/fern.h` | `mid` | 是 | 存在可向量化路径（循环6，分支0），建议次优先 | `-` |
| `ferns/fern_evaluator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `ferns/fern_trainer.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `impl/dt/decision_forest_evaluator.hpp` | `mid` | 是 | 存在可向量化路径（循环3，分支0），建议次优先 | `-` |
| `impl/dt/decision_forest_trainer.hpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `impl/dt/decision_tree_evaluator.hpp` | `mid` | 是 | 存在可向量化路径（循环7，分支0），建议次优先 | `-` |
| `impl/dt/decision_tree_trainer.hpp` | `high` | 是 | 循环8处，学习/推断核心路径，建议优先优化 | `-` |
| `impl/ferns/fern_evaluator.hpp` | `mid` | 是 | 存在可向量化路径（循环9，分支0），建议次优先 | `-` |
| `impl/ferns/fern_trainer.hpp` | `mid` | 是 | 存在可向量化路径（循环16，分支3），建议次优先 | `-` |
| `impl/kmeans.hpp` | `mid` | 是 | 存在可向量化路径（循环2，分支10），建议次优先 | `-` |
| `impl/svm/svm_wrapper.hpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `kmeans.h` | `mid` | 是 | 存在可向量化路径（循环1，分支2），建议次优先 | `impl/kmeans.hpp` |
| `multi_channel_2d_comparison_feature.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `multi_channel_2d_comparison_feature_handler.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `multi_channel_2d_data_set.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `multiple_data_2d_example_index.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `pairwise_potential.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `permutohedral.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `point_xy_32f.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `point_xy_32i.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `regression_variance_stats_estimator.h` | `mid` | 是 | 存在可向量化路径（循环8，分支1），建议次优先 | `-` |
| `stats_estimator.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `svm.h` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `svm_wrapper.h` | `mid` | 是 | 存在可向量化路径（循环3，分支21），建议次优先 | `-` |
| `src/densecrf.cpp` | `mid` | 是 | 存在可向量化路径（循环17，分支6），建议次优先 | `-` |
| `src/kmeans.cpp` | `mid` | 是 | 存在可向量化路径（循环13，分支13），建议次优先 | `-` |
| `src/pairwise_potential.cpp` | `mid` | 是 | 存在可向量化路径（循环7，分支2），建议次优先 | `-` |
| `src/permutohedral.cpp` | `high` | 是 | 循环62处，学习/推断核心路径，建议优先优化 | `-` |
| `src/point_xy_32f.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `src/point_xy_32i.cpp` | `low` | 否 | 以声明/接口封装为主，暂不纳入本轮候选 | `-` |
| `src/svm.cpp` | `high` | 是 | 循环189处，学习/推断核心路径，建议优先优化 | `-` |
| `src/svm_wrapper.cpp` | `high` | 是 | 循环34处，学习/推断核心路径，建议优先优化 | `-` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/ml/ml-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
