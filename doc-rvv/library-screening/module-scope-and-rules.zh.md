# PCL 全库 RVV 筛查范围与规则（v2）

## 1. 口径拆分

本轮明确区分两类口径：

- 筛查候选模块：用于模块级盘点和评分，覆盖 26 个顶层模块。
- 优化模块池：用于文件级台账与函数级首批清单，排除应用/工具/示例/基准与非主干模块后，保留 21 个模块。

## 2. 通用规则

- 以仓库顶层模块目录为单位开展筛查。
- 仅统计源码文件后缀：`.h`、`.hpp`、`.c`、`.cc`、`.cpp`、`.cu`。
- 文件级台账以“文件路径”为主键，一文件一行。
- 不统计非源码资产（脚本、文档、配置、二进制产物等）。

## 3. 全局排除目录

- 按要求排除：`common`、`2d`、`doc`、`doc-rvv`、`test`、`test-rvv`、`.ci`、`.dev`。
- 额外排除非业务目录：`.git`、`.github`、`build`、`cmake`、`tmp`。

## 4. 筛查候选模块（26）

`apps`、`benchmarks`、`cuda`、`examples`、`features`、`filters`、`geometry`、`gpu`、`io`、`kdtree`、`keypoints`、`ml`、`octree`、`outofcore`、`people`、`recognition`、`registration`、`sample_consensus`、`search`、`segmentation`、`simulation`、`stereo`、`surface`、`tools`、`tracking`、`visualization`

## 5. 优化模块池（21）

优化模块池在筛查候选模块基础上明确排除以下 5 个模块：

- `apps`
- `tools`
- `benchmarks`
- `examples`
- `people`

优化模块池最终为：

`cuda`、`features`、`filters`、`geometry`、`gpu`、`io`、`kdtree`、`keypoints`、`ml`、`octree`、`outofcore`、`recognition`、`registration`、`sample_consensus`、`search`、`segmentation`、`simulation`、`stereo`、`surface`、`tracking`、`visualization`

## 6. 优化模块池源码文件数量统计

| module | source_file_count |
|---|---:|
| cuda | 65 |
| features | 137 |
| filters | 109 |
| geometry | 17 |
| gpu | 226 |
| io | 151 |
| kdtree | 6 |
| keypoints | 36 |
| ml | 42 |
| octree | 27 |
| outofcore | 45 |
| recognition | 70 |
| registration | 145 |
| sample_consensus | 70 |
| search | 21 |
| segmentation | 76 |
| simulation | 18 |
| stereo | 11 |
| surface | 346 |
| tracking | 30 |
| visualization | 66 |

优化模块池源码文件总数：1714。

## 7. 执行顺序口径（重启版）

文档与执行统一采用以下顺序：

1. 模块级判断（范围与优先级）
2. 文件级候选统计（全文件不漏，形成候选文件池）
3. 函数级筛选统计（仅在候选文件内做函数层深入筛选）

补充说明：

- `gpu` 因目录结构与常规模块差异较大，暂时移出主优化队列，后置单独评估。
