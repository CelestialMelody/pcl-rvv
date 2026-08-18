# RVV 模块优化阶段化工作流

本文档说明 PCL 模块从全库筛查进入 RVV 实施的通用推进方式。当前工作流分为模块筛查、第一轮文件候选筛选、第二轮函数评估队列、逐主题实施和二轮保留候选复筛。

## 1. 阶段定义

### 阶段 0：全库模块筛查

输入：

- PCL 顶层模块目录；
- 已完成或已有基础的 RVV 模块状态；
- 模块级计算密度、热点证据、可实现性和可验证性。

输出：

```text
doc-rvv/library-screening/module-screening.zh.md
```

阶段 0 用于确定哪些模块进入后续第一轮文件候选筛选，哪些模块排除、暂缓或后置单独评估。该阶段不进入具体文件和函数。

当前结论：

- 当前主推进模块：`registration`、`surface`、`filters`、`io`、`features`、`segmentation`、`sample_consensus`、`recognition`、`keypoints`、`tracking`、`kdtree`、`geometry`、`search`、`ml`、`stereo`、`octree`；
- `gpu` 暂缓主线，后置单独评估；
- `cuda`、`outofcore`、`simulation`、`visualization` 暂缓；
- `apps`、`tools`、`benchmarks`、`examples`、`people` 排除出当前优化主线。

### 阶段 A：第一轮文件候选筛选

输入：

- `doc-rvv/library-screening/module-screening.zh.md`；
- 目标模块源码；
- 上游 test / benchmark 入口；
- 已有同类 RVV 主题经验；
- `doc-rvv/library-screening/modules/_module-file-candidate-screening-template.zh.md`。

输出：

```text
doc-rvv/library-screening/modules/<module>-file-candidate-screening.zh.md
```

阶段 A 是文件候选粗筛，只回答“文件中是否存在值得第二轮继续下钻的可 SIMD/RVV 片段”。它生成 `high/mid/low` 文件级基线，但不直接决定最终 RVV 实施队列。

- `high/mid` 是第二轮必须复核并交代去向的初始候选基线；
- `low` 是已覆盖但未进入二轮初始基线，不是永久排除；
- 如果第二轮发现 `low` 或未列出文件存在明显漏判，可以补入并说明证据。

### 阶段 B：第二轮函数评估队列

输入：

- `doc-rvv/library-screening/modules/<module>-file-candidate-screening.zh.md`；
- 当前源码；
- 上游 test / benchmark；
- 已有同类 RVV 经验。

输出：

```text
doc-rvv/library-screening/<module>/<module>-function-evaluation-queue.zh.md
```

阶段 B 是模块函数评估队列，不是重新全模块筛库。它必须把第一轮 `high/mid` 候选逐项交代去向，并按三类组织结论：

- 建议进行 RVV 优化的文件；
- 保留实施的候选文件；
- 暂缓或不推荐考虑 RVV 优化的文件。

第二轮重点判断 RVV 是否覆盖公开入口主成本、是否能验证、是否能维护。文件有大循环不等于值得直接进入建议优化队列。

### 阶段 C：逐主题 RVV 优化

当函数评估队列或二轮保留候选复筛文档中仍有未完成建议主题时：

- 按执行清单和状态表选择下一主题；
- 建立或复查函数级评估文档；
- 完成 RVV 实现、专项 test/bench、QEMU、反汇编、板卡验证和主题文档；
- closeout 后同步主题评估、主题文档、工作日志和模块状态表。

该阶段不要重新做模块级候选选择，除非筛选文档与当前源码存在明确冲突。

### 阶段 D：二轮保留候选复筛

触发条件：

- 建议进行 RVV 优化的文件队列已经完成或没有明确下一主题；
- 已完成主题暴露出收益弱、diagnostic / bench-only 路径回退、验证成本高或筛选口径需要修正；
- 需要用真实实现和板卡结果反哺保留候选排序。

输出：

```text
doc-rvv/library-screening/<module>/<module>-second-pass-retained-candidate-rescreen.zh.md
```

复筛必须基于已完成主题证据，而不是重复函数评估队列理由。复筛后形成 `建议启动函数级评估` 与 `暂缓 / 不单独实施` 两类主分组；diagnostic / bench-only 是后续 topic 内证据路径，不是模块复筛固定队列。随后回到阶段 C。

## 2. 第一轮筛选判断标准

第一轮主要看文件内是否存在可 SIMD/RVV 的局部证据：

- 循环规模：随点数、像素数、邻域数、correspondence 数、bin 数或文件大小线性或更高增长；
- 算术密度：乘加、距离、统计、几何谓词、权重、规约、矩阵 / 向量小公式或数学函数；
- 访存模式：连续、固定 stride、AoS 字段、organized 行列、indices gather、临时数组；
- 分支复杂度：简单谓词 / mask 优先，深分支、状态机、随机采样、map/hash、search 或 solver 主导降级；
- 语义风险：非结合规约、阈值边界、NaN/Inf、输出顺序、indices、字段复制、用户自定义点类型；
- 可验证性：std/RVV 对拍、fallback case、边界 case、QEMU 和板卡验证是否可建立。

第一轮不承诺公开入口主成本覆盖，也不承诺生产分流。

## 3. 第二轮筛选判断标准

第二轮必须从文件下钻到公开入口、函数族、loop/helper 和输出语义，重点判断：

- RVV 是否覆盖公开入口主成本或输出生成主路径；
- 数据布局是否能由 VL chunk、stride/gather load、segment load/store、mask、`vcompress`、规约或 FMA 自然表达；
- 输出顺序、NaN/Inf、indices、removed_indices、keep_organized、字段复制、in-place、安全别名等语义是否可验证；
- 是否能建立专项 test/bench、QEMU 正确性、反汇编证据和板卡真实性能闭环；
- 主成本是否被 search、sort、map、heap、random、Eigen solver、lattice、冲突累加、邻域不规则访问或整点字段复制稀释。

建议优化文件通常应覆盖 `direct-main-path`。只覆盖前置预处理、尾段压缩或诊断子公式的文件，应谨慎进入建议队列，除非入口极常用、实现很小且验证闭环清晰。

## 4. 性能分层口径

真实性能结论必须来自板卡或目标硬件。QEMU 只用于构建、正确性、日志格式和指令路径证据。

建议分层：

- `>= 2.0x`：强候选，可优先接入生产路径；
- `1.2x ~ 2.0x`：中高候选，需确认覆盖面和维护成本；
- `1.05x ~ 1.2x`：弱收益候选，只在入口常用、实现小且风险低时接入生产；
- `< 1.05x` 或 bench-diagnosis 不成立：不接入生产，记录尝试和回退原因。

弱收益或回退案例必须反哺后续复筛。
