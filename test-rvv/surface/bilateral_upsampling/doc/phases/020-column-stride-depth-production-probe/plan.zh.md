# Phase 020: column-stride depth production probe

## 阶段意图和边界

本阶段把 phase 010 的 `column-stride-depth-direct` 候选推进到 production integration loop（生产接入闭环）。目标是在真实 `BilateralUpsampling<PointInT, PointOutT>::performProcessing` 中加入有界 RVV 分流，并重新收集 production direct（真实生产路径）正确性、反汇编、板卡性能和 Evidence Doctor（证据体检）证据。

validated_scope：`surface/include/pcl/surface/impl/bilateral_upsampling.hpp`；公开入口 `process` -> `performProcessing`；`PointXYZRGB` / `PointXYZRGBA` 输入输出组合；organized cloud（有序点云）；单精度 `x/y/z` AoS layout（结构数组布局）；直接 `r/g/b` 成员颜色访问；`window_size=3/4/5` 的 topic bench case。

unvalidated_scope：其它自定义 RGB 点类型、非 `PointXYZRGB/RGBA` 的输出整点语义、非 organized 输入、`process` 中矩阵求逆之外的调用方链路、真实传感器数据分布、PI5 后的最终采纳决定。

不可触碰路径：不修改公开 API，不修改 `surface/include/pcl/surface/bilateral_upsampling.h` 的类声明，不修改无关 registration / organized_fast_mesh 脏改，不回滚用户已有 diff。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| diagnostic phase 000 | staged-window-reduction 正确性通过但板卡负向，已拒绝接入 | `doc/phases/000-current-state-and-diagnostic-staged-window/result.zh.md` |
| diagnostic phase 010 | direct-depth 为 `1.01x/1.11x/1.26x`，Evidence Doctor `Errors=0` | `doc/phases/010-column-stride-depth-direct-probe/result.zh.md` |
| production source | 当前仍是单一标量 `performProcessing` | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` |
| explicit instantiation | 生产库预编译 `PointXYZRGB/RGBA` 输入输出组合 | `surface/src/bilateral_upsampling.cpp` |
| board availability | 用户说明板卡可用，topic 已有 `board_smoke` | `test-rvv/surface/bilateral_upsampling/board.mk` |

## 生产候选和 gate

| item | 冻结策略 |
| --- | --- |
| production helper | 抽出 `bilateralUpsamplingPerformProcessingStd` 保留原标量语义；新增 `bilateralUpsamplingPerformProcessingRVV` 只在 `__RVV10__` 下编译。 |
| dispatch | `performProcessing` 中先尝试 RVV，失败自然落回 Std。 |
| point type gate | 使用 `pcl::PointXYZRGB` / `pcl::PointXYZRGBA` exact gate 做 PI1 窄范围接入；其它模板实例 fallback。 |
| layout gate | 对输入/输出分别要求 `pcl::rvv::RVVXYZAoSFloatLayout`；RVV 只直接 stride load 输入 `z`，输出仍按成员写 `x/y/z/r/g/b`。 |
| runtime gate | 输入点数、width、height、窗口范围合法；小规模暂不强制 fallback，板卡结果决定是否补规模阈值。 |
| numeric boundary | 保留查表权重和输出反投影公式；RVV 改变窗口列内浮点规约顺序，production test 使用容差比较。 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| PI2 production patch | 修改 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` | 公开 API 不变，非 RVV 构建可编译并走 Std。 |
| PI3 production direct tests | 在 topic test/bench 中加入真实 PCL `BilateralUpsampling` public entry 对拍 | std/RVV QEMU 两侧通过，覆盖 `PointXYZRGB`、`PointXYZRGBA`、NaN holes、fallback/非 RVV 构建。 |
| PI4 asm | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | 反汇编中 production helper 可见 `vlse32/vmfeq/vmerge/vfmul/vfredusum`。 |
| PI4 board | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | 板卡生成 production direct compare summary。 |
| PI4 doctor | 生成 phase 020 manifest 并运行 Evidence Doctor | 无未解释 Error；Warning / Suggestion 写入 result。 |
| PI5 decision | 回填 result、matrix、roadmap、evaluation 和队列表 | 若生产证据支持接入，停在 `pending_user_confirmation_adopt_production`；若不支持，保留 patch 并等用户确认是否回滚。 |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-public 和 production-detail；phase 010 只作为进入本阶段的 diagnostic 输入。 |
| A/B boundary | public overload：真实 `process` / `performProcessing`；detail helper 用于反汇编归属。 |
| 当前决策问题 | RVV-vs-scalar，判断 direct-depth 生产分流是否比当前生产标量路径快且正确。 |
| diagnostic 是否可外推到 production | 不能直接外推；本阶段必须以 production direct 证据刷新结论。 |
| comparison-boundary / baseline mismatch 风险 | 有。生产入口会包含真实 PCL 点型、对象状态、矩阵和输出容器；phase 020 用 public entry bench 降低错配。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | phase 010 已满足本阶段进入条件；若 phase 020 production direct 小图仍 weak-positive，按板卡复跑预算或规模 gate 处理。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已有 adopted RVV family；若 production Std/RVV positive 且无 Evidence Doctor Error，可进入 PI5 用户确认，但采纳仍需用户明确确认。 |

## 板卡复跑预算和决策桶

先运行一次 `board_smoke`。若 production direct 三项中任一项落在 `0.97x..1.10x`，最多追加一次同边界复跑；若 bucket 不变则按该桶关闭，若方向摇摆则标为 `unstable` 并停在 PI5 用户判断。`positive >= 1.10x`，`weak-positive 1.03x..1.10x`，`neutral 0.97x..1.03x`，`negative < 0.97x`。

## 继续 / 停止条件

默认继续 PI2-PI5。只有以下情况停止：生产补丁需要修改公开 API 或扩大点类型范围；fallback gate 无法隔离；QEMU correctness 失败且无法同轮修复；反汇编无法归属；板卡不可达；Evidence Doctor Error 需要用户判断；PI5 到达最终采纳或回滚确认点。

## 文档更新清单

本阶段回填 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、`bilateral_upsampling-evaluation.zh.md`、`benchmark-and-evidence.zh.md` 和 surface 函数评估队列。只有 PI5 证据通过且用户确认采纳后，才创建或更新 `doc-rvv/surface/bilateral_upsampling-RVV.zh.md`。
