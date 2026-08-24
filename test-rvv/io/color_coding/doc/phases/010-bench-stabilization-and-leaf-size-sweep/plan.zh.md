# Phase 010 Plan: bench-stabilization-and-leaf-size-sweep

## 阶段意图和边界

本阶段只修正 component bench（组件性能测试）形状，不扩大到 production（生产源码）。目标是用更长计时边界和 leaf-size sweep（叶大小扫描）复核 phase 000 的证据：`encode_points` 与 `setDefaultColor` 是否稳定正向，`decodePoints` 是否真有收益，`encodeAverageOfPoints` 是否只是短 case outlier（异常偏离）。

## 当前状态清单

| item | state | path |
| --- | --- | --- |
| correctness | Std/RVV QEMU 对拍通过 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| board summary | 5-run component summary generated | `log/board/component_repeat_5/summary.md` |
| Evidence Doctor | `Errors=1, Warnings=2`，decode 退化频率是阻塞 clean conclusion 的 Error | `log/board/component_repeat_5/evidence_doctor.md` |
| registry | fresh | `log/evidence_registry.json` |

## Phase Scope 与扩展队列

| field | value |
| --- | --- |
| validated_scope | diagnostic helper；synthetic `ColorPoint`；leaf sizes 31/257/1024/4096；longer timing batch |
| unvalidated_scope | production caller、真实 leaf-size distribution、entropy coder、full compression public entry |
| point_type_expansion_queue | production-shaped `pcl::PointXYZRGBA` probe remains deferred until phase 010 stabilizes component evidence |
| phase_closeout_boundary | 只关闭 component bench stability；不关闭 production adoption |

## 实现和测试动作

| action | artifact | command | completion |
| --- | --- | --- | --- |
| 扩展 bench case | `src/bench_color_coding.cpp` | build / board repeated | cases 覆盖 encode average / encode points / decode 的 `31/257/1024/4096`，default color 覆盖 `4096/16384` |
| 增加 batch repeat | `src/bench_color_coding.cpp` | board repeated | 每次 iteration 内重复足够多批次，降低 0.001ms 级别噪声 |
| 更新 manifest metadata | `script/generate_color_coding_evidence_manifest.py` | `run_board_color_coding_repeated` | case size 解析正确 |
| rerun correctness / bench | QEMU + board | `run_test_compare`、`dump_bench_rvv`、`run_board_color_coding_repeated` | QEMU pass；board summary / Doctor fresh |

## Evidence Doctor 和 registry 规则

新 board repeated 覆盖旧 `component_repeat_5` summary。若 result direction、decision bucket 或 Error / Warning 数量变化，phase 000 数值降级为 historical evidence（历史证据），phase 010 summary 成为 current truth。

## 板卡复跑预算和决策桶

预算仍为 5 次 repeated run。decision bucket：`positive >= 1.08x`，`weak-positive 1.02x-1.08x`，`neutral 0.98x-1.02x`，`negative < 0.98x`，若方向摇摆则 `unstable`。若 decode 仍有 degradation-frequency Error，decode candidate 记为 rejected/deferred，不能进入 production-shaped probe。

## Diagnostic 到 production mismatch audit

本阶段仍是 `component_ablation` evidence role，A/B boundary 是 `test helper`。任何 positive 只能支持下一阶段 production-shaped diagnostic（生产形态诊断）计划，不能支持 production adoption（生产采纳）。

## 继续 / 停止条件

若 phase 010 稳定支持 encode/default 正向且 Doctor 无阻塞 Error，下一阶段默认是 `020-production-shaped-color-coder-precheck`。若 Doctor Error 仍存在或收益退化，下一阶段改为 no-production / targeted implementation-shape review，并写清恢复条件。
