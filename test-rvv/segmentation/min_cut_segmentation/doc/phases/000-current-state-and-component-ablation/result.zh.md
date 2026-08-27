# Phase 000 Result: current state and component ablation

## 实际执行范围

本阶段建立了 `MinCutSegmentation<PointT>` potential loop 的 test-only component diagnostic（测试专用组件诊断）：把 unary foreground min-distance（前景最小距离）和 binary distance / exp weight（二元距离权重）拆成 Std / RVV helper，对拍、反汇编和板卡 repeated benchmark 均已闭环。生产源码未修改。

## 计划动作回填

| action | status | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| RED correctness | done | `make -C test-rvv/segmentation/min_cut_segmentation run_test_rvv` | 首次按预期因 RVV / Std helper 未定义失败 |
| GREEN correctness | done | `make -C test-rvv/segmentation/min_cut_segmentation run_test_compare` | Std / RVV 均通过 2 个测试 |
| bench build smoke | done | `make -C test-rvv/segmentation/min_cut_segmentation dump_bench_rvv` | RVV bench binary 编译并生成反汇编摘录 |
| board repeated | done | `make -C test-rvv/segmentation/min_cut_segmentation run_board_min_cut_repeated` | 5-run board summary、manifest 和 Evidence Doctor 生成 |
| result 回填 | done | 本文件、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、`min_cut_segmentation-evaluation.zh.md` | 当前默认进入 Phase 010 |

## 正确性和路径证据

| 证据层 | 命令 / 路径 | 结果 | 边界 |
| --- | --- | --- | --- |
| QEMU correctness | `make -C test-rvv/segmentation/min_cut_segmentation run_test_compare` | Std / RVV 均通过 | 只证明 test-only helper；不证明 production dispatch |
| asm attribution | `make -C test-rvv/segmentation/min_cut_segmentation dump_bench_rvv` | `computeUnaryPotentialsRVV` / `computeBinaryPotentialsRVV` 附近出现 `vle32.v`、`vfmacc.vv`、`vfsqrt.v`、`vse32.v`、`vsetvli` | 尚非 production hot symbol |
| board access | `make -C test-rvv/segmentation/min_cut_segmentation check_board_ssh` | pass | 用户说明板卡可用，本阶段实测可部署和回收日志 |

## 板卡性能证据

板卡为 `Milkv-Jupiter`，预算为 5 run，参数为 `--size 65536 --foreground 19 --iterations 8 --warmup 2`。summary-only 证据路径：

- `log/board/repeated/summary.md`
- `log/board/repeated/evidence_manifest.json`
- `log/board/repeated/evidence_doctor.md`

| case | run count | median speedup | min | max | values | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `unary_min_distance` | 5 | 2.08x | 2.07x | 2.13x | 2.08x, 2.07x, 2.08x, 2.13x, 2.11x | positive |
| `binary_exp_weight` | 5 | 2.44x | 2.36x | 2.49x | 2.36x, 2.48x, 2.38x, 2.44x, 2.49x | positive |

## Evidence Doctor 结果

`make -C test-rvv/segmentation/min_cut_segmentation run_board_evidence_doctor`

结果：`Errors=0，Warnings=0，Suggestions=0`。manifest 记录 binary hash：`d531e6d32fa71bfd04ed73586e0168b9d7e51ec28bc79497235a1113aef8c0d6`。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `diagnostic` |
| A/B boundary | `test_helper_component`；Std / RVV 两侧为 component helper |
| 当前决策问题 | potential loop 是否有足够 component 上界 |
| diagnostic 是否可外推到 production | 不能直接外推。当前计时排除了 `nearestKSearch`、Boost graph mutation、`edge_marker_` duplicate checks 和 max-flow |
| comparison-boundary / baseline mismatch 风险 | 有。binary RVV 使用 float `expf_RVV_f32m2`，生产标量是 double `std::exp` |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive；但只能支持进入 production-shaped diagnostic，不支持直接改 production |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。Phase 010 要先确认 search / graph 稀释后的收益 |

## Continue / Stop Decision

current_decision：`diagnostic-positive / continue`。

stop_condition_hit：`none`。板卡可用，Evidence Doctor 无 finding，且两个 component 都为 positive。

next_phase_default：`010-production-shaped-buildgraph-timing`。本阶段只证明 component 上界，下一阶段必须把 KNN、Boost graph 写入和 edge duplicate check 放回计时边界。
