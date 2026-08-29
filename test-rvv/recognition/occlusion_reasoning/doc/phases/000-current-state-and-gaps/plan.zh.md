# Phase 000 Plan: current-state-and-gaps

## 阶段意图和边界

本阶段启动 `recognition/include/pcl/recognition/hv/occlusion_reasoning.h` 的 RVV
函数级评估。阶段范围先收窄为公开 inline `filter()` 的 projection、bounds mask 和
keep-order compression。`getOccludedCloud()`、`ZBuffering`、`copyPointCloud` 成本和更复杂
depth gather 都放到后续 phase。

## 当前状态清单

| item | current state |
| --- | --- |
| source | 公开 inline `filter()` 与 `getOccludedCloud()` 都在头文件中；`ZBuffering` 在 impl 头中。 |
| topic assets | 目前刚建立 `test-rvv/recognition/occlusion_reasoning` scaffold。 |
| board availability | 用户已说明板卡可用；本 phase 不以“等待板卡”作为停止条件。 |
| production state | 尚未进入 production integration loop。 |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `projection-mask-compress-rvv` | model `x/y/z` projection + bounds mask + keep-order compression 能先吃掉大头 | 还没证明 scene depth compare 是否会稀释收益 | planned |
| `depth-gather-rvv` | scene depth finite check 和 threshold compare 可在第二阶段再向量化 | organized depth gather 的成本和 mask 复杂度未知 | deferred |
| `copyPointCloud` split | wrapper 成本可单独归因 | 当前优先级低，且会扩大计时边界 | deferred |

## 诊断到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper vs scalar reference |
| 当前决策问题 | RVV-vs-scalar |
| diagnostic 是否可外推到 production | unknown；只覆盖公开 inline `filter()` 核心 loop |
| comparison-boundary / baseline mismatch 风险 | yes；wrapper 和更复杂 lookup 还未闭合 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但必须先有 correctness、asm、board repeated |
| clean adoption 是否需要同一 production boundary A/B | yes |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `projection-mask-compress-rvv` | ordered scan over organized model cloud | `PointXYZ`, `float`, organized AoS | public inline `filter()` | red/green test against scalar reference | bench helper | planned 5-run board repeated | RVV math + mask + compress | planned | planned | write RED test first |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED test | `src/test_occlusion_reasoning.cpp` | RVV build 对 candidate path 编译或链接失败，说明 test 真在守门。 |
| reference helper | `include/impl/occlusion_reasoning_reference.hpp` | 标量 reference 与公开 inline 语义一致。 |
| candidate helper | `include/impl/occlusion_reasoning_candidates.hpp` | RVV path 命中 projection / mask / compress。 |
| bench harness | `src/bench_occlusion_reasoning.cpp` | board 可运行并输出 summary / checksum。 |
| asm | `make -C ... check_occlusion_rvv_asm` | 看到预期 RVV 指令。 |
| board repeated | 5-run budget | Evidence Doctor clean 或可解释。 |

## 板卡复跑预算和决策桶

默认 5-run。`median >= 1.20x` 且 `B/A < 1` 为 positive；`1.05x~1.20x` 为 weak_positive；
`0.95x~1.05x` 为 neutral；`< 0.95x` 为 negative；预算内摇摆为 unstable。

## 继续 / 停止条件

下一步默认进入 RED test + reference helper。只要还有未阻塞的 test/bench/board/asm 动作，
就不收口 topic。

## 文档更新清单

本阶段更新 README、evaluation、testing-overview、correctness-tests、benchmark-and-evidence、
optimization-evidence、optimization-roadmap、test-support-code-map、phase index、optimization matrix 和 plan。
当前不创建 `doc-rvv`。
