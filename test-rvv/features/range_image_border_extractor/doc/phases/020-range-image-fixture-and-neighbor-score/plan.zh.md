# Phase 020: range-image fixture and neighbor-score diagnostic

## 阶段意图和边界

本阶段默认仍不修改 production。目标是补 Phase 000/010 未覆盖的 public-shaped source（公开入口形态来源）：构造可复核 `RangeImage` fixture 或等价 production-adjacent oracle，评估 `extractBorderScoreImages()` 与 `getNeighborDistanceChangeScore()` 的成本和数据分布，判断 score-update RVV 的局部收益是否会被前置 score 生成稀释。

## 当前状态

| evidence | state |
| --- | --- |
| single score image update | positive，median `2.560x` |
| four score image update | positive，median `2.210x` |
| RangeImage / LocalSurface score generation | not_yet_covered |
| production source | unchanged |

## 动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RangeImage fixture audit | 查找 repo 中 `RangeImage::createFromPointCloud` / NARF / range image 测试样例 | 确定最小 fixture 构造方式和链接库 |
| public-shaped oracle | 新增 test-only wrapper 或 subclass，不修改 production | 能提取四方向 score images 并与稳定 checksum 对齐 |
| component timing | 新增 bench case 区分 score generation、score update、generation+update | 可以解释 score-update 占比是否足以支撑 production probe |
| board repeated + Doctor | 5-run bounded budget | decision bucket、checksum、Doctor 结果回填 |

## 停止条件

若 RangeImage fixture 需要修改 production、链接依赖无法在当前 test-rvv harness 内闭合、板卡不可达、oracle 不能稳定复现 public semantics，或 Evidence Doctor 出现无法降级的 Error，本阶段转 `turn_stop_deferred` 并输出 Handoff。否则继续到 PI1 计划或转向 neighbor-distance / classification 候选。
