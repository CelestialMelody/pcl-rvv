# PCL RVV 文件级筛查总索引（中高优模块，v3）

## 1. 说明

- 本文件为总索引/看板，不承载完整函数细节。
- 详细函数清单改为模块独立文件，位于 `doc-rvv/library-screening/modules/`。
- 当前口径：阶段 A（全文件全函数粗筛）已覆盖 16 个当前推进模块（`gpu` 暂缓）。

## 2. 模块索引（按评分顺序）


| rank | module             | priority | stageA_status | module_file                                      |
| ---- | ------------------ | -------- | ------------- | ------------------------------------------------ |
| 1    | `registration`     | high     | done          | `modules/registration-function-triage.zh.md`     |
| 2    | `surface`          | high     | done          | `modules/surface-function-triage.zh.md`          |
| 3    | `filters`          | high     | done          | `modules/filters-function-triage.zh.md`          |
| 4    | `io`               | high     | done          | `modules/io-function-triage.zh.md`               |
| 5    | `features`         | mid      | done          | `modules/features-function-triage.zh.md`         |
| 6    | `segmentation`     | mid      | done          | `modules/segmentation-function-triage.zh.md`     |
| 7    | `sample_consensus` | mid      | done          | `modules/sample_consensus-function-triage.zh.md` |
| 8    | `recognition`      | mid      | done          | `modules/recognition-function-triage.zh.md`      |
| 9    | `keypoints`        | mid      | done          | `modules/keypoints-function-triage.zh.md`        |
| 10   | `tracking`         | mid      | done          | `modules/tracking-function-triage.zh.md`         |
| 11   | `kdtree`           | mid      | done          | `modules/kdtree-function-triage.zh.md`           |
| 12   | `geometry`         | mid      | done          | `modules/geometry-function-triage.zh.md`         |
| 13   | `search`           | mid      | done          | `modules/search-function-triage.zh.md`           |
| 14   | `ml`               | mid      | done          | `modules/ml-function-triage.zh.md`               |
| 15   | `stereo`           | mid      | done          | `modules/stereo-function-triage.zh.md`           |
| 16   | `octree`           | mid      | done          | `modules/octree-function-triage.zh.md`           |

- 说明：`gpu` 模块已标记为“暂缓推进”，不在当前推进索引中（保留在模块评分文档中）。


## 3. 下一阶段

- 进入阶段 B：按实现队列对高/中优文件做函数级别深入评估（验证路径、风险细化、回退条件）。
