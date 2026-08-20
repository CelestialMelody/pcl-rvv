# Phase 040: production detail helper-only ablation

## 阶段意图和边界

Phase 030 已证明 steady-state public shell（稳态公开入口外壳）消融仍为 `0.98x / 0.91x / 0.98x`，不能解释掉 production public（公开生产入口）负向。本阶段继续把计时边界切到更细：只比较 production detail helper（生产内部 helper）本体，排除 `process()` 的 `initCompute`、organized 检查、`projection_matrix_.inverse()`、stdout 打印和 `computeDistances()`。

validated_scope：`PointXYZRGB` / `PointXYZRGBA` exact gate（精确点型门控）、organized RGBD grid、window_size=3/4/5、预计算 `val_exp_depth_matrix` / `val_exp_rgb_vector`、预计算 `unprojection_matrix`、当前生产源码中 `pcl::bilateralUpsamplingPerformProcessingStd` 与 `pcl::bilateralUpsamplingPerformProcessingRVV` 的 helper-only 计时。

unvalidated_scope：改变 production 源码后的新行为、public API、其它点类型、非 organized 输入、真实传感器数据、是否最终采纳或回滚当前 production patch。

不可触碰路径：本阶段只修改 topic-local bench / phase 文档 / manifest；不修改 production 源码，不回滚用户未确认的生产补丁。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| phase 020 production public probe | `0.96x / 0.89x / 0.95x`，Evidence Doctor `Errors=3` | `doc/phases/020-column-stride-depth-production-probe/result.zh.md` |
| phase 030 steady-state public shell | `0.98x / 0.91x / 0.98x`，Evidence Doctor `Errors=3` | `doc/phases/030-production-public-overhead-ablation/result.zh.md` |
| candidate | 当前 production helper 使用 strided depth load、finite mask merge 和 vector reduction | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` |

## 假设与候选

| candidate family | 假设 | 预期信号 | 风险 |
| --- | --- | --- | --- |
| production detail helper-only | 如果 public 负向主要来自 `process()` shell 或 `computeDistances()`，helper-only 应接近 phase 010 direct-depth 诊断的正向 | helper-only 明显高于 public / steady-state public | helper-only 仍不能替代 production public adoption evidence |
| production detail helper-only negative | 如果 helper 本体也负向或接近 1.0x，说明当前 production RVV helper 自身不足以采纳 | helper-only `<= 1.0x` 或只有 near-threshold | 需要另找 helper 内部新候选，而不是继续修 public shell |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production detail helper-only | organized RGBD grid | `PointXYZRGB` / `PointXYZRGBA`, `RVVXYZAoSFloatLayout` | direct call to production helper, precomputed tables/unprojection | existing public-entry correctness + helper checksum compare | new helper-only bench cases | pending | production helper symbol should contain `vlse32/vmfeq/vmerge/vfmul/vfredusum` | pending | planned | add helper-only cases, run QEMU / asm / board / doctor |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| bench helper-only cases | 修改 `src/bench_bilateral_upsampling.cpp` | 新增三条 `production detail helper` case；Std/RVV 构建能输出 checksum 和误差统计。 |
| QEMU correctness / smoke | `make -C test-rvv/surface/bilateral_upsampling run_test_compare`；`make -C ... run_bench_rvv BENCH_ARGS='1 0'` | correctness 仍为 Std/RVV 9/9；QEMU bench 只看新增 case 输出形状，不采信 timing。 |
| asm refresh | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | helper-only case 仍能归属到当前 production helper 的 RVV 指令。 |
| board refresh | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | 产出 helper-only Std/RVV 三项结果。 |
| doctor refresh | 生成 phase 040 manifest 并运行 Evidence Doctor | 回填 `Errors / Warnings / Suggestions`，并决定是否继续找 helper 内部新候选。 |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-detail-ablation（生产内部 helper 消融） |
| A/B boundary | production detail helper；预计算 tables/unprojection，不含 public `process()` shell |
| 当前决策问题 | implementation-shape / RVV-vs-scalar：当前 production helper 本体是否有收益 |
| diagnostic 是否可外推到 production | 不能直接外推到 public adoption；只能解释 public 负向来自 helper 本体还是 shell。 |
| comparison-boundary / baseline mismatch 风险 | 有；本阶段刻意排除了 `process()` shell 和 `computeDistances()`，因此只用于归因。 |
| weak / negative / neutral / unstable 时是否允许 bounded probe | 允许继续 helper-internal component ablation；不允许据此采纳当前 patch。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前不适用；若 helper-only 正向但 public 仍负向，需要新 production public candidate 再过 PI2-PI5。 |

## 板卡复跑预算和决策桶

本阶段预算只允许一次 `board_smoke`。决策桶沿用：`positive >= 1.10x`，`weak-positive 1.03x..1.10x`，`neutral 0.97x..1.03x`，`negative < 0.97x`。若 helper-only 三项仍负向或中项负向，默认进入 helper-internal component ablation；若三项明显正向，再判断 public shell / computeDistances 是否值得形成新 production candidate。

## 继续 / 停止条件

默认继续完成 bench、QEMU、asm、board 和 Evidence Doctor。只有 bench 编译失败且同轮无法修复、board 不可达、Evidence Doctor Error 暴露证据合同无法修复，或需要用户对回滚 / 保留 production patch 作最终判断时，才停止。

## 文档更新清单

本阶段回填 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、`bilateral_upsampling-evaluation.zh.md`、`benchmark-and-evidence.zh.md`、phase README 和 current handoff。
