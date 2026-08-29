# Optimization Roadmap

## 当前边界

当前 topic 从 `trajkovic_2d.hpp` 的 response grid 生产接入开始。Phase 000 只覆盖 `PointXYZI`、
默认 intensity accessor、3x3 window 和 organized full-cloud public `compute()`。NMS 保留标量。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| response-grid-rvv | 当前源码固定邻域 stencil | EIGHT_CORNERS response grid | 减少逐点算术和 8-corner 临时 vector 成本 | 已通过标量复核控制 NMS 浮点细差 | correctness、asm、board repeated、Evidence Doctor | adopted for narrow scope | closed in `000-current-state-and-response-grid-production-probe` |
| nms-rvv-or-filter | 阶段反思候选 | second threshold 和 occupancy 前筛选 | 可能减少 NMS 标量工作 | 排序、occupancy、输出顺序风险高 | profile 证明采纳后 NMS 成为主瓶颈，且 public output 可稳定对拍 | not_now | only if adopted response-grid path becomes NMS-bound |
| point-type-expansion | 泛型点类型策略 | `PointXYZINormal` 或其它 single-float intensity 点型 | 扩大覆盖范围 | intensity accessor 特化和字段布局不同 | traits / fallback test、asm、board | deferred | new phase only after user chooses wider point-type scope |
| larger-window-support | 当前源码支持奇数 window | `window_size > 3` | 扩大覆盖范围 | stride offset 变化，收益不明 | correctness + board per window | deferred | 只有用户需要或 workload 常用时恢复 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | EIGHT_CORNERS response grid 已采纳，FOUR_CORNERS 保持标量 | production-public 板卡结果在两个规模上正向，fallback control 近 1x 且 checksum match | 后续只在扩大点型、window 或 NMS 范围时需要新证据 | closed |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| FOUR_CORNERS RVV 化 | public output / NMS 敏感，board fallback-control 近 1x；本轮不值得增加生产路径 | workload 明确依赖 FOUR_CORNERS，且新 phase 能证明 output 稳定与板卡收益 |
| NMS RVV 化 | 输出顺序和 occupancy 语义敏感；EIGHT_CORNERS production end-to-end 已明显正向 | response-grid 已采纳但后续 profile 显示 NMS 主导 |
| 泛型 RGB intensity accessor | RGB 灰度 accessor 不是单 float intensity load，当前 phase 不外推 | 后续读取泛型点类型策略并补 RGB 语义对拍 |
