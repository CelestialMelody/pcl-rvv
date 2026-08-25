# Phase 000 current-state and diagnostic plan

## 阶段意图和边界

本阶段要建立 `features/include/pcl/features/impl/shot.hpp` 的函数级 RVV 诊断入口，证明 SHOT352 / SHOT1344 public entry（公开入口）在固定 local reference frame（局部参考系）下可以做 Std/RVV correctness（正确性）对拍、反汇编导出和板卡 smoke（小型验证）。本阶段不修改 production（生产源码），不接入 `shot.hpp` 的 RVV dispatch（分流逻辑），也不把 QEMU timing（QEMU 计时）写成性能结论。

`validated_scope`：`SHOTEstimation<PointXYZ, Normal, SHOT352>` 和 `SHOTColorEstimation<PointXYZRGBA, Normal, SHOT1344>`；`Scalar=float`；合成 AoS（结构数组）输入；提供外部 reference frames；radius-search 邻域。

`unvalidated_scope`：自动 LRF 估计、上游 PCD benchmark、SHOT OMP、其它 PointXYZ-like / PointRGBA-like 泛型点型、`Scalar=double`、真实 production dispatch、LRF 失效之外的 fallback gate、专门 RVV helper。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| production source | `shot.hpp` 无 `__RVV10__` 分支；热点是 shape/color bin、插值、归一化和输出复制。 | `features/include/pcl/features/impl/shot.hpp` |
| upstream tests | `test_shot_estimation.cpp` 覆盖 SHOT / SHOT OMP、indices、search surface、LRF 组合和若干 descriptor 固定值。 | `test/features/test_shot_estimation.cpp` |
| upstream benchmark | Google Benchmark 使用 PCD 文件，覆盖 SHOT352 / SHOT1344 和 OMP variants。 | `benchmarks/features/shot.cpp` |
| topic scaffold | 新建自包含 test-rvv topic，当前尚未运行。 | `test-rvv/features/shot` |
| evidence registry | 尚未接入 registry；本阶段使用人工 freshness scan。 | `not_available` |

## 假设与候选族

| candidate family | hypothesis | risk |
| --- | --- | --- |
| public SHOT fixed-LRF diagnostic scaffold | 固定 LRF 可以让第一阶段聚焦 descriptor 计算，避免 LRF 自动估计遮蔽。 | 仍包含 radius search，不能单独证明 descriptor helper 收益。 |
| shape-bin dot helper RVV | normal dot + finite mask 是低风险向量化入口。 | histogram scatter 和三角函数可能主导。 |
| descriptor normalization / copy RVV | 352 / 1344 维顺序数组循环适合低风险批量化。 | `sqrt` 之后的除法只执行一次，收益可能小。 |
| interpolation code-shape diagnostic | 批量投影和距离计算可能降低邻域点循环成本。 | `acos` / `atan2` 和 scatter-like histogram 写入使语义与维护风险较高。 |

## 计划动作

| action | artifact / command | expected evidence | completion criterion |
| --- | --- | --- | --- |
| A1 scaffold | `Makefile`、`board.mk`、`include/shot.h`、`include/impl/shot_fixtures.hpp` | topic-local test support 可编译。 | `make -C test-rvv/features/shot run_test_compare` 能构建 Std/RVV。 |
| A2 correctness | `src/test_shot.cpp` | SHOT352 / SHOT1344 descriptor finite + unit norm，invalid LRF NaN fallback。 | Std 与 RVV 测试均通过。 |
| A3 bench wrapper | `src/bench_shot.cpp` | `Dataset:`、`Iterations:`、case timing 和 checksum 输出。 | bench binary 可构建；QEMU compare 不运行，板卡执行。 |
| A4 asm | `make -C test-rvv/features/shot dump_bench_rvv` | RVV binary 的指令列表。 | 记录是否能看到 RVV 指令；热点归属不足时降级。 |
| A5 board smoke | `make -C test-rvv/features/shot board_smoke` | 板卡测试和 Std/RVV bench compare。 | 板卡可用时执行；若 SSH/rsync/tool 失败，写真实 blocker。 |
| A6 docs | phase result、matrix、evaluation、roadmap | 可恢复的 evidence decision。 | 每个动作回填 done / partial / blocked / deferred。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断）。 |
| A/B boundary | public overload shape with test-owned synthetic input；尚未修改 production dispatch。 |
| 当前决策问题 | `RVV-vs-scalar` scaffold 可运行性；不是 production family selection。 |
| diagnostic 是否可外推到 production | unknown。固定 LRF 和合成输入只接近 descriptor 入口，不能覆盖真实 LRF、PCD 数据或 OMP。 |
| comparison-boundary / baseline mismatch 风险 | yes。Std/RVV 目前主要比较编译宏和已有库路径，尚无新 RVV family。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes, only after component evidence identifies a small helper with isolated correctness and fallback. |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes, if a new helper family is proposed after scaffold. |

## 板卡复跑预算和决策桶

本阶段先跑 `board_smoke` 单次板卡测试和 bench compare，目标是验证板卡闭环、日志格式和初始方向。若结果参与 EvidenceDecision，下一阶段必须建立 repeated summary（重复摘要）和 Evidence Doctor（证据体检）输入。默认决策桶：明显稳定 `>1.10x` 为 positive，`1.02x-1.10x` 为 weak_positive，接近 1 或方向反转为 neutral / unstable。

## 继续 / 停止条件

默认继续。只有命中以下条件才停止：板卡或工具不可达、QEMU/构建失败且无法修复、证据出现 correctness mismatch、dirty isolation 不安全，或继续需要修改 production / public API / 其它 topic。Phase 000 通过后，默认下一阶段是 `010-shape-bin-or-normalize-component-diagnostic`，在 shape-bin dot 与 descriptor normalize/copy 中选择更低风险候选。

## 文档更新清单

本阶段应更新 `README.zh.md`、`doc/shot-evaluation.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` 和本阶段 `result.zh.md`。`doc-rvv/features/shot-RVV.zh.md` 当前不适用。
