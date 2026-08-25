# Phase 020 result: shape-bin component diagnostic

## 结果摘要

本阶段按计划完成 `createBinDistanceShape` 的 test-only SoA（structure of arrays，按字段拆开的数组）组件诊断。`computeShapeBinDistanceRVV` 与标量参考在 finite normal（有限法线）、NaN fallback（非法值回退）、clamp（夹紧）和 `double` bin 输出上对齐；production（生产源码）`features/include/pcl/features/impl/shot.hpp` 未修改。

板卡三次 all-case run 中，`shape_bin_component` 稳定为 positive：2.47x、2.38x、2.38x。这个结果说明 dot + clamp 算法本身值得继续，但仍不能直接接入 production，因为真实源码使用 `pcl::Normal` AoS（array of structures，结构数组）、邻域 indices（索引）读取、estimator protected state（受保护对象状态）和 `PCL_WARN` 的 NaN 计数副作用。

## 计划动作回填

| action | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| C1 red test | done | `make -C test-rvv/features/shot run_test_compare` | 缺少 shape-bin helper 时曾得到预期编译失败，证明测试会捕捉未实现候选。 |
| C2 helper implementation | done | `include/impl/shot_shape_bin.hpp`、`include/shot.h` | 新增标量参考与 RVV 候选；非 RVV 构建自动回到标量 helper。 |
| C3 component bench | done | `src/bench_shot.cpp`、`script/generate_shot_evidence_manifest.py` | 新增 `shape_bin_component`，checksum 与 timing 均可被 manifest 解析。 |
| C4 asm | done | `make -C test-rvv/features/shot dump_bench_rvv` | RVV bench 反汇编生成成功；本阶段归因到 test-only helper / bench callsite，不归因到 production helper。 |
| C5 board evidence | done | `board_smoke` + 2 次 `run_board_bench_compare fetch_board_logs` | `shape_bin_component` 三次 run 稳定大于 1.10x；public entry 仍在约 1x 附近。 |
| C6 docs | done | 本 result、matrix、roadmap、evaluation、README、队列表 | 文档同步为 partial-production-candidate，但 production probe 需要显式授权。 |

## Correctness 与 asm 证据

`make -C test-rvv/features/shot run_test_compare` 通过 Std / RVV 两侧 6 个测试。新增测试覆盖 257 个 normal 样本，其中包含 NaN、超过 clamp 上界和低于 clamp 下界的输入。

`make -C test-rvv/features/shot dump_bench_rvv` 生成 RVV bench 反汇编。它证明 RVV binary（RVV 二进制）包含当前 helper 所需的向量 load、widen、FMA（fused multiply-add，融合乘加）、clamp 和 `double` store 形态；因为 production 源码未修改，asm 证据只归属于 test-only component。

## 板卡证据

| run label | public SHOT352 | public SHOT1344 | normalize 352 | normalize 1344 | shape-bin |
| --- | --- | --- | --- | --- | --- |
| `phase020_all_case_smoke` | 1.00x | 1.00x | 1.59x | 1.49x | 2.47x |
| `phase020_all_case_rerun1` | 0.99x | 1.01x | 1.63x | 1.52x | 2.38x |
| `phase020_all_case_rerun2` | 0.99x | 1.01x | 1.59x | 1.52x | 2.38x |

证据路径保留在本机生成目录：

- `test-rvv/features/shot/log/board/phase020_all_case_smoke/`
- `test-rvv/features/shot/log/board/phase020_all_case_rerun1/`
- `test-rvv/features/shot/log/board/phase020_all_case_rerun2/`

这些 raw logs（原始日志）按 summary-only（只提交摘要）策略默认不提交。文档只引用 run label 和关键数值。

## Evidence Doctor 解释

Phase 020 的 manifest 已包含 5 个 comparison（对比项）。三次 run 的 Evidence Doctor（证据体检）结果分别为：

| run label | doctor result | 处理 |
| --- | --- | --- |
| `phase020_all_case_smoke` | Errors=2，Warnings=5，Suggestions=0 | Error 来自 public fixed-LRF 入口退化频率；不用于 production 结论。 |
| `phase020_all_case_rerun1` | Errors=1，Warnings=5，Suggestions=1 | Error 来自 `public_shot352_fixed_lrf`；Suggestion 提醒 `public_shot1344_fixed_lrf` 接近阈值。 |
| `phase020_all_case_rerun2` | Errors=1，Warnings=5，Suggestions=1 | 同 rerun1。 |

所有 Warning 都包含 `low_run_count`，因为当前 manifest 每个 run label 仍按单次 compare 解析。阶段计划允许用 1 次 smoke + 2 次 rerun 的 run-labelled 目录做人工稳定性解释；因此 component 结果可作为 diagnostic positive（诊断正向），但不能写成 production performance（生产性能）或 clean adoption（干净采纳）。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component diagnostic（组件诊断）。 |
| A/B boundary | test helper / SoA component bench。 |
| 当前决策问题 | implementation-shape and RVV-vs-scalar component A/B。 |
| diagnostic 是否可外推到 production | 不能直接外推。SoA 连续数组没有覆盖 production 的 `pcl::Normal` AoS、indices gather、对象状态和 warning side effect。 |
| comparison-boundary / baseline mismatch 风险 | 存在。component 计时边界只包含 dot + clamp + store，不包含真实 `createBinDistanceShape` 的 normal cloud 访问和 NaN 计数。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不适用；实际为 positive，但仍需要 AoS/layout audit 和用户生产授权。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。当前没有 production patch，也没有同一 production boundary 的 RVV-vs-RVV detail A/B。 |

## EvidenceDecision

`shape_bin_component` 的当前阶段决策是 `partial-production-candidate`：组件数学链路有稳定板卡收益，值得继续检查更接近 production 的 AoS / index layout（结构数组 / 索引布局）。它不是 `production-ready`，也不授权修改 `features/include/pcl/features/impl/shot.hpp`。

## 继续 / 停止决定

`continue_stop_decision`：继续。当前没有板卡、工具、correctness、dirty isolation 或证据矛盾 blocker。由于 production 源码修改仍需显式授权，下一阶段默认是 test-only `030-shape-bin-aos-layout-diagnostic`，用 `pcl::Normal` AoS 连续布局检查 SoA 正向是否能靠近 production 数据形态。

`stop_condition_hit`：none。

`next_phase_default`：`030-shape-bin-aos-layout-diagnostic`。
