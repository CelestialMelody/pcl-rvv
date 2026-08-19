# Phase 000 Plan

## 阶段意图和边界

本阶段只证明 organized_fast_mesh 的规则扫描是否值得进入生产接入闭环。
范围冻结为 `PointXYZ`、`float`、organized cloud、`triangle_pixel_size=1`、`storeShadowedFaces(true)`。

## 当前状态清单

- 生产源码：未修改。
- 测试支撑：新建。
- QEMU / board：未跑。
- Evidence Doctor：未跑。
- 目标文件：`surface/include/pcl/surface/impl/organized_fast_mesh.hpp`

## 假设与候选族

| candidate family | hypothesis |
| --- | --- |
| finite-cache scan | 将 `isFinite` 结果缓存起来可减少重复检查 |
| adaptive diagonal preference | adaptive cut 的 z 差值可批量计算 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| finite-cache scan | ordered-cloud-pair | PointXYZ / float / organized | test-only candidate | reference compare | bench compare | pending | pending | pending | tentative | 跑 QEMU correctness |
| adaptive diagonal preference | ordered-cloud-pair | PointXYZ / float / organized | test-only candidate | reference compare | bench compare | pending | pending | pending | tentative | 跑 QEMU correctness |

## 实现和测试动作

1. 跑 `run_test_compare`。
2. 跑 `run_bench_compare`。
3. 生成 `generate_vec_report`。
4. 在板卡上跑 `run_board_test` 和单次 `run_board_bench_compare`，作为首阶段诊断信号。
5. 写 Evidence Doctor 和 registry。

## 阶段完成条件

- correctness 对拍通过。
- bench 和 board summary 有首阶段诊断结果；production gate 前再补 repeated summary。
- Evidence Doctor 没有阻塞错误。
- 再决定是否进入生产接入闭环。

## 继续 / 停止条件

- 继续：板卡可用且 bucket 稳定。
- 停止：用户要求停在诊断边界，或证据显示收益不足。
