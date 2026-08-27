# Phase 010 Result: production-shaped buildGraph timing

## 实际执行范围

本阶段按 `plan.zh.md` 建立 buildGraph-shaped production-shaped diagnostic（生产形态图构建诊断）：新增 graph build helper、graph checksum correctness test、`buildgraph_potential_batch` bench label、buildgraph repeated board target 和 manifest metadata。生产源码未修改。

Std / RVV 两侧均经过同一类边界：合成 `PointXYZ` dense cloud、完整 indices、真实 `pcl::search::autoSelectMethod` / `nearestKSearch`、Boost directed graph edge creation、capacity / reverse edge maps 和 duplicate marker。两侧差异限定在 potential 计算：Std 使用 double scalar formula，RVV 使用 Phase 000 的 unary / binary batch helpers。

## 计划动作回填

| action | status | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| 写 RED 测试 | done | `make -C test-rvv/segmentation/min_cut_segmentation run_test_rvv` | 按预期因 `computeBuildGraphPotentialBatchStd/RVV` 缺失失败 |
| 实现 Std graph helper | done | `include/impl/min_cut_segmentation_components.hpp` | 复刻 KNN、graph edge creation、capacity checksum |
| 实现 RVV graph helper | done | `include/impl/min_cut_segmentation_components.hpp` | 复用 unary / binary RVV helper，再按同一边序写 Boost graph |
| 扩展 bench | done | `src/bench_min_cut_segmentation.cpp` | 新增 `buildgraph_potential_batch` label、`--neighbours` 参数和 checksum |
| 扩展 manifest wrapper | done | `script/generate_min_cut_board_evidence_manifest.py`、`Makefile` | 新增 buildgraph repeated 目录和 `production_shaped_diagnostic` metadata |
| 板卡复跑 | done | `log/board/buildgraph-repeated/summary.md` | 5-run median 1.02x，min 0.98x，decision bucket 为 neutral |
| Evidence Doctor | done | `log/board/buildgraph-repeated/evidence_doctor.md` | Errors=1，Suggestions=1；必须降级，不可作为 production speedup |

## 正确性和路径证据

| 证据层 | 命令 / 路径 | 结果 | 边界 |
| --- | --- | --- | --- |
| RED gate | `make -C test-rvv/segmentation/min_cut_segmentation run_test_rvv` | 编译失败，缺少 `computeBuildGraphPotentialBatchStd` / `computeBuildGraphPotentialBatchRVV` | 证明新增测试能抓到缺失 graph helper |
| QEMU correctness | `make -C test-rvv/segmentation/min_cut_segmentation run_test_compare` | Std / RVV 均通过 3 个 tests | 只证明 test-only helper；QEMU 不作为性能结论 |
| asm attribution | `make -C test-rvv/segmentation/min_cut_segmentation dump_bench_rvv` | RVV bench binary 仍包含 `vle32.v`、`vfmacc.vv`、`vfsqrt.v`、`vse32.v`、`vsetvli`，并在 buildGraph-shaped 路径调用 unary / binary RVV helper | 尚非 production hot symbol |

## 板卡性能证据

板卡为 `Milkv-Jupiter`，预算为 5 run，参数为 `--size 8192 --foreground 19 --neighbours 14 --iterations 3 --warmup 1 --case-filter buildgraph_potential_batch`。summary-only 证据路径：

- `log/board/buildgraph-repeated/summary.md`
- `log/board/buildgraph-repeated/evidence_manifest.json`
- `log/board/buildgraph-repeated/evidence_doctor.md`

| case | run count | median speedup | min | max | values | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `buildgraph_potential_batch` | 5 | 1.02x | 0.98x | 1.02x | 1.02x, 1.02x, 1.02x, 0.98x, 0.99x | neutral |

该结果说明 Phase 000 的 component speedup 被 KNN、Boost graph mutation 和 duplicate marker 成本基本稀释。真实 `extract()` 还会包含 max-flow solver，因此当前证据不支持 production patch。

## Evidence Doctor 结果

`make -C test-rvv/segmentation/min_cut_segmentation run_board_buildgraph_evidence_doctor`

结果：`Errors=1，Warnings=0，Suggestions=1`。

| severity | signal | observed pattern | 处理 |
| --- | --- | --- | --- |
| Error | `ba_degradation_frequency` | 5 run 中 2 run 低于 1.0：`1.02x, 1.02x, 1.02x, 0.98x, 0.99x` | 不能只用 median 1.02x 写成稳定加速；降级为 neutral |
| Suggestion | `near_threshold_ba` | median=1.02x，距离 1.0 不足 0.05 | 不进入 production patch；保留诊断证据 |

manifest 记录 binary hash：`0a3cd09900734450f405bd3b32d747aca0718fd18f2b92e71d26a0ebf705c257`。板卡日志出现远端 `Clock skew detected` 警告，但 5 轮均完成并拉回 summary / manifest / doctor；该警告不改变 Evidence Doctor 的降级结论。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper_buildgraph`，不是 public overload |
| 当前决策问题 | potential batch 在 search / graph 成本中是否仍有可见收益 |
| diagnostic 是否可外推到 production | 只能外推为“当前不建议进入生产接入”。它仍未覆盖真实 `extract()`、对象生命周期和 max-flow |
| comparison-boundary / baseline mismatch 风险 | 有。RVV binary path 使用 float `expf_RVV_f32m2`，production scalar 为 double `std::exp`；checksum 只在声明预算内对拍 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前为 neutral 且 Evidence Doctor 有 Error，不允许 production patch |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不进入 clean adoption；若未来重启，必须重新走 PI1 |

## Continue / Stop Decision

current_decision：`no-production / stop-after-diagnostic`。

stop_condition_hit：`not_recommended_to_continue_optimization`。原因是 buildGraph-shaped board evidence 为 neutral，且 Evidence Doctor 报告退化频率 Error；真实 production `extract()` 还会继续加入 max-flow solver 成本，预期更难保留收益。

next_phase_default：不进入 `020-production-integration-plan`。本 topic 可保留 Phase 000/010 诊断资产；若未来要重启，只建议先提出新的 bounded hypothesis（例如完全不同的 graph construction strategy），不能从当前 potential batch 直接接 production。
