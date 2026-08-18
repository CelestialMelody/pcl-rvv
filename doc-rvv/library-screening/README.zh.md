# PCL RVV 筛选文档索引

本目录记录 PCL 库 RVV 优化进入具体主题前的筛选材料。当前筛选链路分为模块筛查、第一轮文件候选筛选、第二轮函数评估队列和保留候选复筛。

## 1. 当前入口

- 阶段化 workflow：`doc-rvv/library-screening/module-optimization-workflow.zh.md`
- 全库模块筛查报告：`doc-rvv/library-screening/module-screening.zh.md`
- 第一轮模块文档目录：`doc-rvv/library-screening/modules/`
- 第一轮模板：`doc-rvv/library-screening/modules/_module-file-candidate-screening-template.zh.md`
- 函数评估队列模板：`.agents/skills/rvv-screening/references/templates/function-evaluation-queue-template.md`
- 保留候选复筛模板：`.agents/skills/rvv-screening/references/templates/retained-candidate-rescreen-template.md`

## 2. 模块筛查口径

模块筛查先确定哪些顶层模块进入后续第一轮文件候选筛选。当前结论：

- 筛查候选模块：26 个顶层模块；
- 优化模块池：21 个模块；
- 当前主推进模块：16 个模块；
- 暂缓主线模块：`gpu`、`cuda`、`outofcore`、`simulation`、`visualization`；
- 排除模块：`apps`、`tools`、`benchmarks`、`examples`、`people`。

详细评分和处理理由见 `doc-rvv/library-screening/module-screening.zh.md`。

## 3. 当前推进模块

当前第一轮筛选覆盖 16 个模块，`gpu` 因目录结构差异暂缓主线推进。

| 顺序 | 模块 | 第一轮文档 | 第二轮 / 复筛状态 |
| ---: | ---- | ---------- | ----------------- |
| 1 | `registration` | `modules/registration-file-candidate-screening.zh.md` | `registration/registration-function-evaluation-queue.zh.md` |
| 2 | `surface` | `modules/surface-file-candidate-screening.zh.md` | 待二轮 |
| 3 | `filters` | `modules/filters-file-candidate-screening.zh.md` | `filters/filters-function-evaluation-queue.zh.md`，`filters/filters-retained-candidate-rescreen.zh.md` |
| 4 | `io` | `modules/io-file-candidate-screening.zh.md` | 待二轮 |
| 5 | `features` | `modules/features-file-candidate-screening.zh.md` | 待二轮 |
| 6 | `segmentation` | `modules/segmentation-file-candidate-screening.zh.md` | 待二轮 |
| 7 | `sample_consensus` | `modules/sample_consensus-file-candidate-screening.zh.md` | 已有部分 RVV 主题，后续按模块情况复筛 |
| 8 | `recognition` | `modules/recognition-file-candidate-screening.zh.md` | 待二轮 |
| 9 | `keypoints` | `modules/keypoints-file-candidate-screening.zh.md` | 待二轮 |
| 10 | `tracking` | `modules/tracking-file-candidate-screening.zh.md` | 待二轮 |
| 11 | `kdtree` | `modules/kdtree-file-candidate-screening.zh.md` | 待二轮 |
| 12 | `geometry` | `modules/geometry-file-candidate-screening.zh.md` | 待二轮 |
| 13 | `search` | `modules/search-file-candidate-screening.zh.md` | 待二轮 |
| 14 | `ml` | `modules/ml-file-candidate-screening.zh.md` | 待二轮 |
| 15 | `stereo` | `modules/stereo-file-candidate-screening.zh.md` | 待二轮 |
| 16 | `octree` | `modules/octree-file-candidate-screening.zh.md` | 待二轮 |

## 4. 第一轮筛选口径

第一轮是文件级粗筛，输出 `high/mid/low`：

- `high/mid` 是第二轮必须复核并交代去向的初始候选基线；
- `low` 是已覆盖但不进入二轮初始基线，不是永久排除；
- 第一轮不直接决定最终 RVV 实施队列。

## 5. 第二轮筛选口径

第二轮按实施动作和证据置信度组织：

- 建议进行 RVV 优化的文件；
- 保留实施的候选文件；
- 暂缓或不推荐考虑 RVV 优化的文件。

第二轮重点判断 RVV 是否覆盖公开入口主成本、是否能建立正确性和性能闭环、是否值得维护。
