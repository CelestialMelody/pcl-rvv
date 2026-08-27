# Phase 010 Plan: production-shaped buildGraph timing

## 阶段意图和边界

本阶段建立 production-shaped diagnostic（生产形态诊断）：在 `test-rvv/segmentation/min_cut_segmentation/` 内复刻 `buildGraph()` 的主要结构，包括 search setup、`nearestKSearch`、Boost graph vertex / edge mutation、`edge_marker` duplicate check、source/sink edge 和 binary neighbor edge。Std / RVV 两侧只替换 potential 计算路径，用来判断 Phase 000 的 component 收益是否能穿透 search / graph 稀释。

本阶段仍不修改 `segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp`，不调用 production protected `buildGraph()`，也不证明 public `extract()` 已命中 RVV。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| component correctness | Std / RVV QEMU 均通过 2 个 tests | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| component board | unary median 2.08x、binary median 2.44x，均 positive | `log/board/repeated/summary.md` |
| Evidence Doctor | Errors=0，Warnings=0，Suggestions=0 | `log/board/repeated/evidence_doctor.md` |
| production 状态 | 未修改 production；当前仍只允许 topic-local 测试资产和文档 | `git status --short --untracked-files=all -- test-rvv/segmentation/min_cut_segmentation` |

## validated_scope / unvalidated_scope

validated_scope（本阶段准备证明）：

- row source：public-like full input cloud 与完整 indices。
- point type / Scalar / layout：`pcl::PointXYZ` / float xyz / AoS（结构数组）。
- entry shape（入口形态）：test-only production-shaped graph build helper。
- graph boundary：KNN、Boost directed graph edge creation、reverse edge map、capacity map 和 duplicate marker。
- scale：默认板卡规模 `8192` 点、`foreground=19`、`neighbours=14`、`iterations=3`、`warmup=1`。

unvalidated_scope（本阶段不证明）：

- production public dispatch、`initCompute()`、`extract()` 和 max-flow solver（最大流求解器）。
- 模板 `PointT` 泛型 traits、非 `PointXYZ` 点类型、indices 子集、非 dense 输入。
- binary production 的 double `std::exp` 完全语义替换；RVV 侧仍是 float finite-domain approximation（有限域近似）。

## 本阶段优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `buildgraph-potential-batch` | full input rows + KNN edge rows | `PointXYZ` / float xyz / dense AoS | test-only production-shaped graph build helper | 新增 graph checksum same-chain test | 新增 `buildgraph_potential_batch` bench label | 5-run repeated board summary，positive / neutral / negative 分桶 | `dump_bench_rvv` 仍检查 helper RVV 指令 | 生成 buildgraph diagnostic manifest 并跑 Evidence Doctor | planned |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 写 RED 测试 | `src/test_min_cut_segmentation.cpp` | 新增 graph build diagnostic 测试先失败，失败原因是缺少 graph helper |
| 实现 Std graph helper | `include/impl/min_cut_segmentation_components.hpp` | 复刻 KNN、graph edge creation、capacity checksum |
| 实现 RVV graph helper | `include/impl/min_cut_segmentation_components.hpp` | unary / binary weights 批量计算，再按 production edge 顺序写回 graph |
| 扩展 bench | `src/bench_min_cut_segmentation.cpp` | 新增 `buildgraph_potential_batch` label、CLI 参数和 checksum |
| 扩展 manifest wrapper | `script/generate_min_cut_board_evidence_manifest.py`、`Makefile` | 新增 `buildgraph` repeated 目录、summary / manifest / doctor |
| 板卡复跑 | `log/board/buildgraph-repeated/` | 5-run repeated summary + Evidence Doctor 生成 |
| 更新 result / roadmap / matrix | phase docs | result 回填 correctness、asm、board、doctor 和继续 / 停止决策 |

## 板卡复跑预算和决策桶

默认执行 5-run repeated board，参数：

```text
--size 8192 --foreground 19 --neighbours 14 --iterations 3 --warmup 1 --case-filter buildgraph_potential_batch
```

decision bucket（决策桶）：

- `positive`：median speedup >= 1.10，min >= 1.00，checksum 在已声明数值预算内，Evidence Doctor 无 Error。
- `weak-positive`：median 1.03 到 1.10，且维护成本低；只能支持 PI1 讨论。
- `neutral`：0.97 到 1.03；不支持直接进入 production patch，可考虑更小边界或 no-production closeout。
- `negative`：低于 0.97；不接 production。
- `unstable`：5 run 内方向反复或 Evidence Doctor warning 无法解释；降级为诊断证据。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic` |
| A/B boundary | `test_helper_buildgraph`，不是 public overload |
| 当前决策问题 | potential batch 在 search / graph 成本中是否仍有可见收益 |
| diagnostic 是否可外推到 production | 只能外推为是否值得写 PI1。真实 production 仍需 public dispatch、fallback、泛型点类型和 production direct evidence |
| comparison-boundary / baseline mismatch 风险 | 有。helper 复刻主要逻辑，但不经过真实对象生命周期和 max-flow |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | neutral / negative 时默认不接 production；weak-positive 只允许写 PI1 风险计划 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。production patch 后必须有 production public 或 production detail 证据 |

## 继续 / 停止条件

默认继续执行 RED-GREEN、QEMU correctness、asm、板卡 5-run 和 Evidence Doctor。合法停止条件：helper 需要修改 production / public API；板卡或工具不可达；Evidence Doctor Error 无法修复；buildgraph diagnostic 变为 neutral / negative 并且没有更小、有界且低维护成本的候选；或者进入 PI1 后需要用户明确确认生产接入范围。

next_phase_default：若本阶段 positive / weak-positive，进入 `020-production-integration-plan`；若 neutral / negative，进入 no-production diagnostic closeout 或仅保留 component evidence。
