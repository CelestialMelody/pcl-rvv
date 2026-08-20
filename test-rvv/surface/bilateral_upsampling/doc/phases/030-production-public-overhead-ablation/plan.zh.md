# Phase 030: production public overhead ablation

## 阶段意图和边界

本阶段不修改 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp`，只在 topic-local test/bench 里做 public shell（公开入口外壳）开销消融。目标是判断 phase 020 的负向到底来自 RVV kernel 本身，还是来自 `BilateralUpsampling::process` 的对象构造、参数设置、stdout 打印、dispatch/fallback 和 public unprojection shell。

validated_scope：`test-rvv/surface/bilateral_upsampling/src/bench_bilateral_upsampling.cpp`；production public 运行形态；`PointXYZRGB` / `PointXYZRGBA`；organized grid；window_size=3/4/5；steady-state `process` 调用；stdout-silenced 和 setup-outside-timer 形态。

unvalidated_scope：改变 production 源码后的新行为、其它点类型、非 organized 输入、真实传感器数据、去掉 public shell 后的最终采纳决定。

不可触碰路径：不改 production 源码，不改公开 API，不改 phase 020 以前的负向证据，不回滚用户未确认的生产补丁。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| phase 020 production public probe | 公开入口为 `0.96x / 0.89x / 0.95x`，Evidence Doctor `Errors=3` | `doc/phases/020-column-stride-depth-production-probe/result.zh.md` |
| next action | 需要 public shell overhead ablation | `doc/bilateral_upsampling-evaluation.zh.md` |
| bench harness | 当前 benchmark 直接在 timed lambda 里构造 `BilateralUpsampling` | `src/bench_bilateral_upsampling.cpp` |

## 假设与候选

| candidate family | 假设 | 预期信号 | 风险 |
| --- | --- | --- | --- |
| steady-state public `process` | 只把对象构造和 setters 移出 timed lambda | 若收益明显，说明 current public probe 混入了 setup overhead | 不能单独证明 kernel 变快 |
| stdout-silenced public `process` | 在 benchmark 期间临时重定向 stdout 到 `/dev/null` | 若收益明显，说明 public `printf` shell 影响了 board 结果 | 只能作为诊断，不是最终 production 结论 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production public steady-state `process` | organized RGBD grid | `PointXYZRGB` / `PointXYZRGBA`, `RVVXYZAoSFloatLayout` | real public `process`, setup outside timer | existing public-entry correctness already passed | compare steady-state benchmark against phase 020 full public probe | done: `0.98x / 0.91x / 0.98x` | bench lambda still contains production helper | `Errors=3`、`Warnings=9`、`Suggestions=0` | attempted / negative | 等待用户确认是否回滚生产补丁；若要继续，只能开更深的 production-detail 消融。 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| bench ablation | 修改 `src/bench_bilateral_upsampling.cpp` | 新增 steady-state public shell ablation cases，setup 在 timed lambda 外完成 |
| asm refresh | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | 反汇编仍能归属到 production helper，且 bench case 名可对上 |
| board refresh | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | 产出 steady-state public shell 结果，和 phase 020 full public probe 可比较 |
| doctor refresh | 生成 phase 030 manifest 并运行 Evidence Doctor | 若 shell 开销减小导致结论变化，则回填 matrix / roadmap |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-public-ablation（生产公开入口消融） |
| A/B boundary | public overload；A/B 差别是 shell setup / stdout 是否计入 timed window |
| 当前决策问题 | RVV-vs-scalar，判断 phase 020 负向是否主要来自 public shell overhead |
| diagnostic 是否可外推到 production | 不能直接外推，只能指导下一步是否值得继续留着当前生产补丁 |
| comparison-boundary / baseline mismatch 风险 | 有；public shell 消融和 phase 020 不是同一计时边界 |
| weak / negative / neutral / unstable 时是否允许 bounded probe | 允许；本阶段只做诊断，不触碰 production 源码 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 暂不适用；当前先判断 shell overhead 是否值得继续做 production 取舍 |

## 板卡复跑预算和决策桶

本阶段预算只允许一次 board_smoke。最新 steady-state public shell 结果仍为负向，因此不再追加复跑；下一步只保留用户确认回滚或另起更深的 production-detail 消融。

## 继续 / 停止条件

默认继续完成 bench ablation、asm 和 board 证据。当前这轮已完成 board 证据且仍为负向，接下来只停在 PI5 / 用户确认点。

## 文档更新清单

本阶段回填 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、`bilateral_upsampling-evaluation.zh.md`、`benchmark-and-evidence.zh.md`、phase README 和 current handoff。
