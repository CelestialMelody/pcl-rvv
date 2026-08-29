# Phase 020 Plan: accumulator-scatter-audit

## 阶段意图和边界

本阶段审计 `HoughSpace3D::vote()` / `voteInt()` / `findMaxima()` 这一段。
目标不是立刻承诺实现，而是先判断 scatter / interpolation / voter tracking 是否还有
可审查、可维护、值得继续的 RVV 空间。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| production direct 结果 | neutral，未支持采纳 | `test-rvv/recognition/hough_3d/log/board/repeated_phase010_production_direct/summary.md` |
| scatter 代码位置 | 明确 | `recognition/src/cg/hough_3d.cpp` |
| 生产 patch 状态 | 仅 attempted | `recognition/include/pcl/recognition/impl/cg/hough_3d.hpp` |

## Phase Scope 与扩展队列

- `validated_scope`：Hough accumulator scatter 与 interpolation。
- `unvalidated_scope`：其它 recognition topic、其它 public API。

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `accumulator-scatter-audit` | full 入口的瓶颈可能在 vote / voteInt，不在前半段 | `unordered_map`、稀疏写回、顺序敏感 | planned |

## 继续 / 停止条件

若 scatter / interpolation 没有明显可审查空间，当前 topic 可以暂停并整理结论。
