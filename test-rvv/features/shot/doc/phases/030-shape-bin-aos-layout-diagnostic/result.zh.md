# Phase 030 result: shape-bin AoS layout diagnostic

## 结果摘要

本阶段完成 `pcl::Normal` AoS（array of structures，结构数组）连续布局的 shape-bin component diagnostic（组件诊断）。新增的 `computeShapeBinDistanceAoSRVV` 使用 RVV stride load（跨步加载）读取 `normal_x/y/z` 字段，并复刻 Phase 020 的 finite normal（有限法线）、clamp（夹紧）和 `double` bin 输出语义。production（生产源码）仍未修改。

板卡三次 targeted run 中，`shape_bin_aos_component` 稳定为 positive：1.87x、1.90x、1.76x。与 SoA 版本的 2.38x-2.47x 相比，AoS stride load 成本可见，但仍保留明显收益。

## 计划动作回填

| action | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| D1 red test | done | `make -C test-rvv/features/shot run_test_compare` | 缺少 AoS helper 时按预期编译失败：`computeShapeBinDistanceAoSScalar` / `computeShapeBinDistanceAoSRVV` 不存在。 |
| D2 helper implementation | done | `include/impl/shot_shape_bin.hpp` | 新增连续 `pcl::Normal` AoS 标量参考与 RVV candidate；非 RVV 构建回到标量 helper。 |
| D3 component bench | done | `src/bench_shot.cpp`、`script/generate_shot_evidence_manifest.py` | 新增 `shape_bin_aos_component`，checksum 与 timing 可解析。 |
| D4 asm | done | `make -C test-rvv/features/shot dump_bench_rvv` | 反汇编中可见 `vlse32.v`、`vfwcvt.f.f.v`、`vfmacc.vf` 和 `vse64.v`，归属到 test-only AoS helper / bench callsite。 |
| D5 board evidence | done | 3 次 `run_board_bench_compare fetch_board_logs BENCH_ARGS='--case-filter shape_bin_aos_component'` | AoS component 稳定大于 1.10x，决策桶为 positive。 |
| D6 docs | done | 本 result、matrix、roadmap、evaluation、README、队列表 | 文档同步为 partial-production-candidate；下一步仍是 test-only indexed gather 诊断或显式生产授权。 |

## Correctness 与 asm 证据

`make -C test-rvv/features/shot run_test_compare` 通过 Std / RVV 两侧 7 个测试。新增 AoS 测试用连续 `pcl::PointCloud<pcl::Normal>` 构造 257 个 normal，并覆盖 NaN 与 clamp 边界。

`make -C test-rvv/features/shot dump_bench_rvv` 通过，`bench_shot_rvv.asm` 中有 `vlse32.v` 跨步加载、`vfwcvt.f.f.v` float-to-double widen（浮点扩宽）、`vfmacc.vf` 融合乘加和 `vse64.v` double store。该证据只证明 test-only AoS helper 的机器码形态，不证明 production `createBinDistanceShape` 已命中 RVV。

## 板卡证据

| run label | Std avg | RVV avg | speedup |
| --- | --- | --- | --- |
| `phase030_aos_component_smoke` | 1.6200 ms | 0.8663 ms | 1.87x |
| `phase030_aos_component_rerun1` | 1.6271 ms | 0.8578 ms | 1.90x |
| `phase030_aos_component_rerun2` | 1.5904 ms | 0.9035 ms | 1.76x |

证据路径保留在本机生成目录：

- `test-rvv/features/shot/log/board/phase030_aos_component_smoke/`
- `test-rvv/features/shot/log/board/phase030_aos_component_rerun1/`
- `test-rvv/features/shot/log/board/phase030_aos_component_rerun2/`

这些 raw logs（原始日志）按 summary-only（只提交摘要）策略默认不提交。

## Evidence Doctor 解释

三次 targeted run 的 Evidence Doctor（证据体检）均为 `comparisons=1`、`Errors=0`、`Warnings=1`、`Suggestions=0`。唯一 Warning 是 `low_run_count`，因为每个 run-labelled 目录仍按单次 compare 解析。当前阶段用 1 次 smoke + 2 次 rerun 解释稳定性，因此可作为 diagnostic positive；它仍不是 production performance（生产性能）证据。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component diagnostic（组件诊断）。 |
| A/B boundary | test helper / contiguous AoS component bench。 |
| 当前决策问题 | implementation-shape and RVV-vs-scalar component A/B。 |
| diagnostic 是否可外推到 production | 不能直接外推。连续 AoS 已比 SoA 更接近 production，但 production 使用 `indices` 指向邻域 normal，可能是任意顺序和非连续访问。 |
| comparison-boundary / baseline mismatch 风险 | 存在。当前计时边界不包含 `indices` gather、`nan_counter`、`PCL_WARN` 和 estimator protected state。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段实际为 positive，但仍需要 indexed gather / side-effect audit，或用户显式授权 PI1。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。 |

## EvidenceDecision

`shape_bin_aos_component` 的当前阶段决策是 `partial-production-candidate`。它证明跨步读取 `pcl::Normal` 连续 AoS 后仍有稳定收益，但 production 接入前还需要回答 arbitrary `indices` gather 和 warning side effect 是否会抵消收益。

## 继续 / 停止决定

`continue_stop_decision`：继续。当前没有板卡、工具、correctness、dirty isolation 或证据矛盾 blocker。

`stop_condition_hit`：none。

`next_phase_default`：`040-shape-bin-indexed-gather-diagnostic`。
