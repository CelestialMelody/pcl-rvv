# Phase 020 Plan: production-shaped-color-coder-precheck

## 阶段意图和边界

本阶段把 phase 010 的 component_ablation（组件消融）正向信号推进到 production-shaped diagnostic（生产形态诊断）：使用真实 PCL 点类型和更接近 `ColorCoding<PointT>` 调用形态的测试 / bench，判断 synthetic `ColorPoint` 结果是否值得进入后续 PI1 production integration plan（生产接入计划）。

本阶段不修改 `io/include/pcl/compression/color_coding.h`，不创建 production patch（生产补丁），不声称 production adoption（生产采纳）。若需要改 production，必须另起 PI1 并读取 `rvv-implementation` 的 dispatch / fallback / point-load-store 规则。

## 当前状态清单

| item | state | path |
| --- | --- | --- |
| phase 010 board summary | encode/default/decode component evidence positive or weak-positive | `log/board/component_repeat_5/summary.md` |
| Evidence Doctor | `Errors=0, Warnings=5, Suggestions=0`，Warnings 全部集中在 `encode_average_*` | `log/board/component_repeat_5/evidence_doctor.md` |
| correctness | Std/RVV QEMU 对拍通过 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| asm | candidate binary contains expected RVV instructions | `build/asm/riscv/bench_color_coding_rvv.asm` |
| production source | untouched | `io/include/pcl/compression/color_coding.h` |
| doc suite | partial | README / evaluation / phase docs exist; testing / benchmark / code-map role docs still deferred |

## Phase Scope 与扩展队列

| field | value |
| --- | --- |
| validated_scope | planned: production-shaped diagnostic for `pcl::PointXYZRGBA` or equivalent RGBA point layout; indexed leaf encode and contiguous decode/default |
| unvalidated_scope | full `OctreePointCloudCompression` public entry, entropy coder, point coder interaction, real stream IO, other point types, `Scalar` variations, non-RGBA layouts |
| point_type_expansion_queue | after this phase: decide whether `PointXYZRGBA` precheck supports PI1; do not generalize to all `PointT` |
| phase_closeout_boundary | close only production-shaped diagnostic precheck; no production dispatch or adoption |

## 实现和测试动作

| action | artifact | command | completion |
| --- | --- | --- | --- |
| add production-shaped smoke | `src/test_color_coding.cpp` / support header | `make -C test-rvv/io/color_coding run_test_compare` | real PCL point type RGBA offset and `ColorCoding`-like byte vectors match current reference |
| add production-shaped bench case | `src/bench_color_coding.cpp` | QEMU smoke + board repeated | labels distinguish component vs production-shaped cases; checksum stable |
| update manifest metadata | `script/generate_color_coding_evidence_manifest.py` | board repeated target | evidence role becomes production-shaped diagnostic for new labels, while phase 010 component labels remain diagnostic |
| doc-suite essentials | `doc/testing-overview.zh.md`, `doc/benchmark-and-evidence.zh.md`, `doc/test-support-code-map.zh.md` or merged sections | path-scoped git status | reviewer can locate targets, labels, scripts and summary paths without chat context |
| rerun evidence | QEMU + asm + board + Doctor + registry | `run_test_compare`, `dump_bench_rvv`, `run_board_color_coding_repeated`, `check_evidence_freshness` | no Doctor Error; warnings explained or evidence downgraded |

## Evidence Doctor 和 registry 规则

本阶段若覆盖 `component_repeat_5` 输出，必须更新 registry run label 和 doc refs 到 phase 020；若同时保留 phase 010 component truth，应使用新 output dir 或明确写 historical/current 分工。任何 Doctor Error 阻塞 production-shaped conclusion。Warnings 必须说明是否只影响 `encode_average`、某个 point type / layout，或会改变 PI1 判断。

## 板卡复跑预算和决策桶

预算为 5 次 repeated run。decision bucket 沿用 phase 010：`positive >= 1.08x`，`weak-positive 1.02x-1.08x`，`neutral 0.98x-1.02x`，`negative < 0.98x`，方向摇摆则 `unstable`。如果 production-shaped labels 与 component labels 方向冲突，优先降级为 `implementation-shape needs audit`，不要进入 production patch。

## Diagnostic 到 production mismatch audit

| question | planned answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | production-shaped helper / test helper |
| 当前决策问题 | 是否值得进入 PI1，而不是是否 clean adopt |
| diagnostic 是否可外推到 production | unknown；本阶段要检查真实点类型和 `ColorCoding` 状态边界，但仍不包含完整 compression public entry |
| comparison-boundary / baseline mismatch 风险 | yes；bench wrapper 与 public compression caller 仍不同 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | only if issue is clearly wrapper-specific and PI1 can isolate fallback / dispatch; otherwise defer |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 继续 / 停止条件

若 production-shaped diagnostic 仍为 positive 或 weak-positive 且 Doctor 无 Error，下一阶段默认是 PI1 production integration plan（只写生产接入计划并冻结范围），仍不能自动进入 production patch。若 production-shaped 结果为 neutral / negative / unstable，下一阶段改为 no-production / implementation-shape review，并写清是否保留 component diagnostic 资产。

`next_phase_default`: execute this phase, starting with production-shaped smoke and doc-suite essentials.
