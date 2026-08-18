# PCL 全库 RVV 模块筛查报告

本文档记录 PCL 全库 RVV 优化的模块级筛查结论。模块筛查位于第一轮文件候选筛选之前，目标是确定哪些顶层模块进入后续文件候选筛选，哪些模块排除、暂缓或后置单独评估。

## 1. 筛查口径

- 筛查单位：PCL 仓库顶层模块目录。
- 源码后缀：`.h`、`.hpp`、`.c`、`.cc`、`.cpp`、`.cu`。
- 不统计非源码资产：脚本、文档、配置、二进制产物等。
- 已完成模块：`common`、`2d` 已有 RVV 工作基础，不纳入本轮 library-screening 主队列。
- 全局排除目录：`doc`、`doc-rvv`、`test`、`test-rvv`、`.ci`、`.dev`、`.git`、`.github`、`build`、`cmake`、`tmp`。

## 2. 模块评分模型

模块评分用于确定后续第一轮文件候选筛选顺序，不直接决定具体 RVV 实现。它是模块级半定量筛选，不是性能结论；真实收益必须在具体主题中通过专项测试、QEMU 正确性、反汇编和板卡 bench 闭环确认。

总分 = 计算收益（0-40） + 热度证据（0-25） + 可实现性（0-20） + 可验证性（0-15）。

| 分层 | 分数 | 含义 |
| ---- | ---- | ---- |
| 高优 | `>= 70` | 优先进入第一轮文件候选筛选和后续函数评估队列 |
| 中优 | `50-69` | 进入第一轮文件候选筛选，但通常排在高优模块之后 |
| 低优 | `< 50` | 暂不进入当前主线，后续有新证据时再评估 |

### 2.1 计算收益（0-40）

衡量模块中是否存在适合 RVV 的批量数值路径。主要看源码结构、算法类型和数据流形态。

| 分档 | 分数范围 | 判断锚点 |
| ---- | -------- | -------- |
| 很强 | `32-40` | 大量逐点 / 邻域 / 图像式 / residual / transform / 编解码循环；主路径通常随点数、像素数、correspondence 数或邻域数增长 |
| 较强 | `24-31` | 存在多个明确批量计算入口，但部分路径受模板、search、solver、状态机或不规则访存稀释 |
| 中等 | `16-23` | 有局部可向量化点，但模块主职责不完全是数值吞吐，或热点规模/覆盖面不稳定 |
| 较弱 | `8-15` | 主要是调度、IO 管理、后端封装、外存管理、GPU/CUDA 专用路径或小规模计算 |
| 很弱 | `0-7` | 基本不承载库主计算路径，或 RVV 只能覆盖极小辅助代码 |

### 2.2 热度证据（0-25）

衡量模块是否位于常见 PCL 使用链路中，以及是否已有测试、benchmark 或典型数据集入口可支撑后续验证。

| 分档 | 分数范围 | 判断锚点 |
| ---- | -------- | -------- |
| 很强 | `20-25` | 常用公开 API 或主流水线模块；有上游 test 和 benchmark，或已有 RVV 主题证明入口常用 |
| 较强 | `15-19` | 常见算法模块；有上游 test，benchmark 或典型数据集入口部分存在 |
| 中等 | `10-14` | 有测试覆盖，但真实使用热度或标准数据入口不够稳定 |
| 较弱 | `5-9` | 使用场景偏专门，测试或 benchmark 入口有限 |
| 很弱 | `0-4` | 主要是示例、工具、边缘模块或缺少可复核入口 |

### 2.3 可实现性（0-20）

衡量 RVV 是否可能以局部、可维护、低侵入方式落地。分数高不代表一定加速，只代表工程上更容易形成可验证实现。

| 分档 | 分数范围 | 判断锚点 |
| ---- | -------- | -------- |
| 很强 | `16-20` | 数据布局规整，主要循环可用 VL chunk、stride/gather、mask、规约或压缩表达；不需要改变公开 API |
| 较强 | `12-15` | 有清晰 RVV 点，但存在模板、AoS 字段、indices、organized 边界或少量复杂分支 |
| 中等 | `8-11` | RVV 点较分散，主成本可能被 search、sort、map、solver、状态机或随机采样稀释 |
| 较弱 | `4-7` | 主要路径依赖外部后端、GPU/CUDA、IO 调度、树结构不规则访问或复杂对象状态 |
| 很弱 | `0-3` | 需要重构公开 API / 数据结构或改变用户可见语义才可能向量化 |

### 2.4 可验证性（0-15）

衡量后续能否建立正确性和性能闭环。

| 分档 | 分数范围 | 判断锚点 |
| ---- | -------- | -------- |
| 很强 | `12-15` | 上游 test 覆盖充分，有 benchmark 或容易建立专项 bench，可构造 std/RVV 对拍、fallback case 和板卡性能闭环 |
| 较强 | `9-11` | 有上游 test，专项 bench 可建立，但数据集、参数或边界 case 需要补齐 |
| 中等 | `6-8` | 可测入口存在，但代表性数据、性能归因或上游覆盖不足 |
| 较弱 | `3-5` | 需要较多新测试或专门数据，结果不易归因到局部 RVV |
| 很弱 | `0-2` | 缺少稳定入口或验证成本明显高于潜在收益 |

### 2.5 复核机制

- 每个分项分数必须能对应上面的分档锚点；如果只能给经验判断，应在“依据”中说明不确定性。
- 模块分数只用于确定是否进入第一轮文件候选筛选；第一轮和第二轮可以推翻模块级预期。
- 第一轮文件候选筛选完成后，可以用 high/mid 占比、候选形态和漏筛情况反向修正模块级评分。
- 已完成主题的板卡结果、bench-only 回退和维护成本应反哺后续模块筛查；例如某类路径反复被 search / sort / Eigen / map 稀释，应下调同类模块的计算收益或可实现性。

## 3. 筛查候选模块

模块级盘点覆盖 26 个顶层模块：

`apps`、`benchmarks`、`cuda`、`examples`、`features`、`filters`、`geometry`、`gpu`、`io`、`kdtree`、`keypoints`、`ml`、`octree`、`outofcore`、`people`、`recognition`、`registration`、`sample_consensus`、`search`、`segmentation`、`simulation`、`stereo`、`surface`、`tools`、`tracking`、`visualization`

## 4. 排除模块

以下 5 个模块不进入当前 RVV 优化主线：

| 模块 | 计算收益 | 热度证据 | 可实现性 | 可验证性 | 总分 | 分层 | 处理 | 原因 |
| ---- | -------: | -------: | -------: | -------: | ---: | ---- | ---- | ---- |
| `apps` | 24 | 12 | 9 | 7 | 52 | 中 | 排除 | 应用代码较多，核心收益主要来自底层库模块 |
| `tools` | 18 | 10 | 12 | 12 | 52 | 中 | 排除 | 工具侧可验证性好，但性能收益依赖底层模块优化 |
| `benchmarks` | 8 | 6 | 16 | 15 | 45 | 低 | 排除 | 用于测评，不是主计算路径 |
| `examples` | 12 | 9 | 11 | 12 | 44 | 低 | 排除 | 教程性质代码多，收益不稳定 |
| `people` | 17 | 10 | 11 | 9 | 47 | 低 | 排除 | 模块规模较小，收益点不足以进入当前主线 |

## 5. 优化模块池

排除应用 / 工具 / 示例 / 基准 / people 后，优化模块池保留 21 个模块：

| 模块 | 源码文件数 | 计算收益 | 热度证据 | 可实现性 | 可验证性 | 总分 | 分层 | 当前处理 |
| ---- | ---------: | -------: | -------: | -------: | -------: | ---: | ---- | -------- |
| `registration` | 145 | 33 | 19 | 13 | 12 | 77 | 高 | 进入第一轮，已进入第二轮 |
| `surface` | 346 | 34 | 20 | 12 | 11 | 77 | 高 | 进入第一轮 |
| `filters` | 109 | 31 | 20 | 14 | 11 | 76 | 高 | 进入第一轮，已完成函数评估队列和保留候选复筛 |
| `io` | 151 | 30 | 21 | 13 | 11 | 75 | 高 | 进入第一轮 |
| `gpu` | 226 | 32 | 18 | 11 | 10 | 71 | 高 | 暂缓主线，后置单独评估 |
| `features` | 137 | 29 | 18 | 13 | 9 | 69 | 中 | 进入第一轮 |
| `segmentation` | 76 | 28 | 16 | 12 | 11 | 67 | 中 | 进入第一轮 |
| `sample_consensus` | 70 | 27 | 16 | 12 | 10 | 65 | 中 | 进入第一轮，已有部分 RVV 主题 |
| `recognition` | 70 | 26 | 15 | 11 | 9 | 61 | 中 | 进入第一轮 |
| `keypoints` | 36 | 24 | 14 | 12 | 9 | 59 | 中 | 进入第一轮 |
| `tracking` | 30 | 23 | 13 | 11 | 10 | 57 | 中 | 进入第一轮 |
| `kdtree` | 6 | 18 | 13 | 14 | 11 | 56 | 中 | 进入第一轮 |
| `geometry` | 17 | 20 | 12 | 13 | 10 | 55 | 中 | 进入第一轮 |
| `search` | 21 | 19 | 12 | 13 | 10 | 54 | 中 | 进入第一轮 |
| `ml` | 42 | 22 | 11 | 10 | 10 | 53 | 中 | 进入第一轮 |
| `stereo` | 11 | 21 | 11 | 11 | 9 | 52 | 中 | 进入第一轮 |
| `octree` | 27 | 20 | 12 | 10 | 9 | 51 | 中 | 进入第一轮 |
| `outofcore` | 45 | 15 | 9 | 10 | 8 | 42 | 低 | 暂缓 |
| `simulation` | 18 | 16 | 8 | 10 | 8 | 42 | 低 | 暂缓 |
| `visualization` | 66 | 14 | 10 | 8 | 8 | 40 | 低 | 暂缓 |
| `cuda` | 65 | 10 | 11 | 6 | 7 | 34 | 低 | 暂缓 |

优化模块池源码文件总数：`1714`。

## 6. 当前主推进模块

当前主线推进 16 个模块：4 个高优模块 + 12 个中优模块。`gpu` 虽为高优评分，但目录结构和 CPU/RVV 入口差异较大，后置单独评估；低优模块暂缓。

| 顺序 | 模块 | 分数 | 分层 | 第一轮文档 |
| ---: | ---- | ---: | ---- | ---------- |
| 1 | `registration` | 77 | 高 | `modules/registration-file-candidate-screening.zh.md` |
| 2 | `surface` | 77 | 高 | `modules/surface-file-candidate-screening.zh.md` |
| 3 | `filters` | 76 | 高 | `modules/filters-file-candidate-screening.zh.md` |
| 4 | `io` | 75 | 高 | `modules/io-file-candidate-screening.zh.md` |
| 5 | `features` | 69 | 中 | `modules/features-file-candidate-screening.zh.md` |
| 6 | `segmentation` | 67 | 中 | `modules/segmentation-file-candidate-screening.zh.md` |
| 7 | `sample_consensus` | 65 | 中 | `modules/sample_consensus-file-candidate-screening.zh.md` |
| 8 | `recognition` | 61 | 中 | `modules/recognition-file-candidate-screening.zh.md` |
| 9 | `keypoints` | 59 | 中 | `modules/keypoints-file-candidate-screening.zh.md` |
| 10 | `tracking` | 57 | 中 | `modules/tracking-file-candidate-screening.zh.md` |
| 11 | `kdtree` | 56 | 中 | `modules/kdtree-file-candidate-screening.zh.md` |
| 12 | `geometry` | 55 | 中 | `modules/geometry-file-candidate-screening.zh.md` |
| 13 | `search` | 54 | 中 | `modules/search-file-candidate-screening.zh.md` |
| 14 | `ml` | 53 | 中 | `modules/ml-file-candidate-screening.zh.md` |
| 15 | `stereo` | 52 | 中 | `modules/stereo-file-candidate-screening.zh.md` |
| 16 | `octree` | 51 | 中 | `modules/octree-file-candidate-screening.zh.md` |

## 7. 后续衔接

模块筛查之后的流程为：

1. 第一轮文件候选筛选：对主推进模块生成 `doc-rvv/library-screening/modules/<module>-file-candidate-screening.zh.md`。
2. 第二轮函数评估队列：把第一轮 high/mid 初始候选基线转成 `建议进行 RVV 优化的文件`、`保留实施的候选文件`、`暂缓或不推荐考虑 RVV 优化的文件`。
3. 逐主题 RVV 优化：按函数评估队列或保留候选复筛执行清单推进函数级评估、实现、测试、QEMU、反汇编、板卡闭环和主题文档。
