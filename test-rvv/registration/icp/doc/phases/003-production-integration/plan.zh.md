# Phase 003：production integration

## 目标

把 Phase 001/002 中稳定正向的 full-cloud transformCloud RVV 实现接入
`registration/include/pcl/registration/impl/icp.hpp`，并补齐 production direct correctness、
fallback gate、generic point layout gate、board repeated benchmark 和 asm attribution。

## 计划动作

| action | 完成判据 | 状态 |
| --- | --- | --- |
| production dispatch | `transformCloud` 在 `__RVV10__` + `Scalar=float` + layout/offset/size gate 满足时走 RVV。 | done |
| semantic preservation | RVV helper 只写原标量路径会写的字段，不额外执行 `output = input`。 | done |
| production direct tests | 覆盖 `PointXYZ`、`PointNormal`、`PointXYZI`、`PointXYZINormal`、small input、`Scalar=double`、in-place。 | done |
| QEMU correctness | Std/RVV 构建均通过。 | done |
| board correctness | 板卡 RVV gtest 通过。 | done |
| production repeated bench | 板卡 5-run repeated summary 生成并记录 registry。 | done |
| asm attribution | bench RVV binary 中 production `transformCloud` 符号归因闭合。 | done |
| Evidence Doctor | Errors=0；warning 必须解释或降级。 | done |

## 复跑预算

默认 5-run。若 median 接近 1.0、出现跨方向、Doctor Error 或 decision bucket 摇摆，再追加一次同边界 5-run；
当前结果全部明显正向，不扩大预算。
