# 050 tail-vector-filter-probe 结果

## 当前结论

本阶段已完成 `tail-vector-filter-compress` 窄 production probe（生产探针）。050 候选在 `thresholdGroundRVV` 中尝试把尾段剩余的 `diff < height_threshold` 比较和输出保序压缩也改成 RVV mask（掩码）+ `vcompress`，但 5-run 板卡同边界 A/B 显示它相对 040 已采纳 RVV 基线整体为 neutral（中性）：Std/RVV 仍为 positive（正向），但只有 `PointXYZ` dense 约达到 1.03x 相对收益，其余 label 都是持平或小幅波动。

按本阶段计划，050 生产源码改动已回退；当前 production（生产源码）保留 040 已采纳形态：grid z-min 使用 RVV，window-open 保持标量，threshold tail 使用 RVV indexed xyz gather（按索引离散加载）和 row/col 计算，`Zf(row,col)` lookup（查表）、最终阈值比较与 `push_back` 保持标量。

## 计划动作回填

| action | status | 证据 | 说明 |
| --- | --- | --- | --- |
| 实现 tail vector filter probe | done then reverted | `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` | 候选曾引入 `diffs` / `kept_indices` staging buffer，并用 `vcompress` 压缩通过索引；板卡 A/B 后撤回。 |
| correctness（正确性） | done | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare` | 候选和回退后均通过；最终状态 Std/RVV 各 14/14 passed。 |
| asm attribution（反汇编归属） | done | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter check_production_rvv_asm` | 候选和回退后均通过；最终状态可定位 `apmfExtractRVV` 和关键 RVV 指令。 |
| board A/B（板卡同边界对比） | done | `log/board/tail-vector-filter-probe-v1/summary.md` | 先跑 3-run，因接近阈值扩到 5-run；Evidence Doctor 重新生成后 clean。 |
| Evidence Doctor（证据体检） | done | `log/board/tail-vector-filter-probe-v1/evidence_doctor.md` | 5-run comparisons=8，Errors=0 / Warnings=0 / Suggestions=0。 |
| 回退 050 局部改动 | done | production diff + final gates | 只撤回 050 的 `thresholdGroundRVV` 改动，保留 030/040 已采纳生产优化。 |

## 5-run 板卡结果

050 作为新的 RVV 实现族选择，不能只看 Std/RVV speedup（标量 / RVV 加速比）；下表同时给出 040 adopted baseline（已采纳基线）和 050 candidate（候选）在 RVV 侧的同边界对比。

| Benchmark Item | 040 RVV median ms | 050 RVV median ms | 050 RVV vs 040 RVV | 040 speedup | 050 speedup | decision |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `apmf production public dense` | 60.6953 | 58.9103 | 1.030x faster | 1.48x | 1.53x | positive but isolated |
| `apmf production public non-dense` | 84.1152 | 82.3346 | 1.022x faster | 1.28x | 1.30x | neutral |
| `apmf production public PointXYZI dense` | 62.3148 | 62.8638 | 0.991x faster | 1.52x | 1.51x | neutral |
| `apmf production public PointXYZI non-dense` | 82.5741 | 80.8188 | 1.022x faster | 1.42x | 1.46x | neutral |
| `apmf production public PointXYZRGB dense` | 63.6101 | 63.2819 | 1.005x faster | 1.50x | 1.49x | neutral |
| `apmf production public PointXYZRGB non-dense` | 82.5997 | 80.9996 | 1.020x faster | 1.43x | 1.45x | neutral |
| `apmf production public PointXYZRGBA dense` | 63.2009 | 62.8845 | 1.005x faster | 1.51x | 1.51x | neutral |
| `apmf production public PointXYZRGBA non-dense` | 82.6646 | 80.8704 | 1.022x faster | 1.43x | 1.46x | neutral |

## Diagnostic-To-Production Mismatch Audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-public` 用于 Std/RVV 当前收益；`production-detail` A/B 用于 050 是否优于 040。 |
| A/B boundary | 同一 `bench_apmf_production` wrapper、同一 synthetic production public 输入、同一板卡 run budget；对比 `point-type-production-repeated-v1` 与 `tail-vector-filter-probe-v1`。 |
| 当前决策问题 | `RVV-family-selection`，判断 050 tail 比较/压缩组织是否优于 040 adopted RVV。 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；050 直接跑 production public。 |
| comparison-boundary / baseline mismatch 风险 | 低到中。两批 run label、输入和 wrapper 一致，但 binary hash 不同且不是单一二进制内的 runtime switch；因此只作为 production detail A/B，不写成严格微架构归因。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段就是 bounded probe；5-run 后中性，按计划回退。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要；本阶段已补 A/B，结果不支持 clean adoption。 |

## Evidence Doctor 与 rerun budget

- `log/board/tail-vector-filter-probe-v1/summary.md`：5-run，iterations=8，warmup=2，8 个 production public label 的 Std/RVV speedup 均为 positive。
- `log/board/tail-vector-filter-probe-v1/evidence_doctor.md`：Errors=0 / Warnings=0 / Suggestions=0。
- 3-run 时 Evidence Doctor 提示 `low_run_count` warning，因此按计划扩到 5-run；5-run 后 warning 消失。
- 板卡运行出现远端 Makefile clock skew warning（时间戳偏斜警告），但命令成功、日志完整、checksum 一致，未影响本阶段 Evidence Doctor 结果。

## Optimization Matrix 更新

| candidate family | correctness | asm | board evidence | doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- |
| `tail-vector-filter-compress` | final code after revert Std/RVV 14/14 passed | final code passed | 050 相对 040 整体 neutral，仅一个 label 约 1.03x | clean | rejected / reverted | none |

## Continue / Stop Decision

continue_stop_decision：`turn_stop_deferred with stop_condition_hit`。

停止原因：当前 topic 内已尝试并关闭主要高价值候选。040 已采纳的 production path 在 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 上都有接入后板卡收益；window-open RVV 历史证据中性 / 负向；050 tail 剩余比较/压缩探针相对已采纳 RVV 基线中性并已回退。继续扩大到 OpenMP 多线程、其它 `PCL_XYZ_POINT_TYPES` 或真实应用数据分布属于更宽验证 / profiling 范围，不是当前证据下值得自动推进的生产优化方向。

next_phase_default：暂停并等待 reviewer / 用户判断；若要继续，建议另开明确范围的 profiling 或点型覆盖扩展阶段。
