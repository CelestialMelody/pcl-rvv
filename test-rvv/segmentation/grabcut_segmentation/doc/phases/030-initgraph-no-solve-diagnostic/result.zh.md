# Phase 030: initGraph no-solve diagnostic result

## 阶段结论

Phase 030 完成了 `initGraph` no-solve diagnostic（不含求解诊断）。本阶段在 Phase 020 的
terminal weight（端点权重）公式后追加 lightweight terminal write sink（轻量端点写入消耗点），
用于观察 RVV GMM（高斯混合模型）候选在更接近 `GrabCut<PointT>::initGraph()` 的边界里是否仍有收益。

当前 EvidenceDecision（证据决策）是 `partial-production-candidate / PI1-plan-next`。5-run repeated
board（重复板卡测试）仍稳定正向，支持进入 PI1 production integration plan（生产接入计划）；但本阶段仍是
test-support helper（测试支撑 helper）边界，不包含真实 `setTerminalWeights`、n-link graph edge mutation
（图边写入）、trimap（三区域标记图）固定标签分支、`refineOnce` / `extract` public entry（公开入口）或
max-flow（最大流）求解，因此不能直接写成 production-ready（可直接接入生产）。

## 执行范围回填

| 计划动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED no-solve correctness | historical / not_recreated | 恢复本轮当前状态时，`src/test_grabcut.cpp` 和 `include/impl/grabcut_diagnostic_reference.hpp` 已经包含 Phase 030 helper/test。 | 当前没有可审查的 fresh RED 失败日志；不伪造 TDD（测试驱动开发）失败证据，后续以当前 green 和证据边界记录。 |
| GREEN helper | done | `include/impl/grabcut_diagnostic_reference.hpp` | `InitGraphNoSolveResult`、`computeTerminalWriteSink` 和 `computeInitGraphNoSolveReference/Candidate` 已存在；Std 构建走标量同构链路，RVV 构建复用 Phase 020/010 RVV GMM 子链路。 |
| correctness 对拍 | done | `make -C test-rvv/segmentation/grabcut_segmentation run_test_compare` | QEMU Std：4 tests passed；QEMU RVV：5 tests passed。`InitGraphNoSolveMatchesScalarReference` 在两侧通过。 |
| bench case | done | `src/bench_grabcut.cpp` 支持 `--case initgraph_no_solve` | QEMU smoke 能输出 `BENCH grabcut_component case=initgraph_no_solve`、checksum、误差预算和 `max_budget_ratio`。 |
| Evidence Doctor target | done | `make -C test-rvv/segmentation/grabcut_segmentation run_initgraph_repeated_evidence_doctor` | Makefile 已提供 Phase 030 single-run 和 repeated manifest / doctor target；默认 repeated dir 指向本阶段采用的 `repeated-board-20260827-144235`。 |
| QEMU smoke | done | `make -C test-rvv/segmentation/grabcut_segmentation run_bench_std BENCH_ARGS='--width 128 --height 96 --iterations 2 --warmup 1 --case initgraph_no_solve'`；RVV 同命令换 `run_bench_rvv` | QEMU 只证明 bench parser、日志形状和误差字段可用；QEMU timing 不作为性能结论。 |
| 单次板卡 smoke | done | `make -C test-rvv/segmentation/grabcut_segmentation run_board_bench_compare BENCH_ARGS='--case initgraph_no_solve --iterations 8 --warmup 2'`，随后 `fetch_board_logs run_initgraph_evidence_doctor` | 单次 doctor 为 `Errors=0, Warnings=1, Suggestions=0`，warning 是 `low_run_count`；按计划升级到 repeated board。 |
| 5-run repeated board | done | `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-manifest.json` | Milkv-Jupiter `640x480`、`iterations=8`、`warmup=2` 下 B/A 为 `1.5486, 1.5416, 1.5418, 1.5396, 1.5389`，B/A median（中位数）约 `1.5416x`。 |
| repeated Evidence Doctor | done | `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md` | `Errors=0, Warnings=0, Suggestions=0`。 |
| 反汇编归属 | done | `make -C test-rvv/segmentation/grabcut_segmentation dump_bench_rvv`，`build/asm/riscv/bench_grabcut_rvv.full.asm` | `computeTerminalWeightsCandidate` 调用 `computeGMMProbabilityCandidate`；后者符号范围内有 `vle32.v`、`vse32.v`、`vfmacc.vf/vv` 和 `vfmul`。`computeInitGraphNoSolveCandidate` 被内联 / 折叠，归属按 terminal/GMM helper 调用链记录。 |

## 诊断证据链

| 证据层 | 当前事实 | 不能证明的范围 |
| --- | --- | --- |
| correctness | `GrabCutDiagnosticReference.InitGraphNoSolveMatchesScalarReference` 覆盖 terminal weights 和 sink checksum。 | 不覆盖真实 graph node id 分配、n-link edge mutation、trimap 固定标签分支或 solver 状态机。 |
| 数值预算 | repeated manifest 记录 `max_abs_error=128.0`、`max_rel_error=0.003508725`、`max_budget_ratio=0.004656613`。 | `max_abs_error` 是 sink checksum 的累计量差异，必须结合相对 / 绝对组合预算解释；它不是单点 terminal cost 的绝对误差。 |
| QEMU | Std/RVV correctness 通过；bench smoke 输出 shape 可解析。 | QEMU timing 不进入性能排序。 |
| 反汇编 | Phase 030 通过 terminal/GMM 调用链命中 RVV math helper；GMM 符号内可归属 RVV FMA、load 和 store。 | 仍不是 production binary 的 public dispatch 或 production detail helper 证据。 |
| 板卡 repeated | 本阶段采用 `repeated-board-20260827-144235`，5-run 全部 B/A > 1，median 约 `1.5416x`。 | 只是 synthetic BGR samples（合成 BGR 样本）的 production-shaped diagnostic，不证明完整 `extract/refineOnce` wall time。 |
| Evidence Doctor | repeated doctor 为 `Errors=0, Warnings=0, Suggestions=0`。 | Doctor 只检查 manifest 中表达的诊断边界；仍需 reviewer 复核源码、文档和 production 接入计划。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic / component_ablation`。 |
| A/B boundary | `test_support_helper`。baseline 是 topic-local scalar reference，candidate 是 topic-local RVV helper。 |
| 当前决策问题 | Phase 020 的 terminal weight 正向信号在加入 terminal write sink 后是否仍成立，并是否足以进入 PI1 计划。 |
| diagnostic 是否可外推到 production | 只能外推为 partial-production-candidate。公式链路和 unknown trimap terminal write 接近 production，但真实 graph mutation、trimap 固定标签、public object state 和 max-flow 都未计入。 |
| comparison-boundary / baseline mismatch 风险 | 存在。当前是不同 build 的 helper-level Std/RVV 对比，不是同一 production boundary 内的 RVV-vs-RVV family selection，也不是 public overload。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 repeated bucket 是 positive；若后续 production direct 反转，不能回头把本诊断写成 no-production，只能说明 diagnostic 到 production 被真实 graph / solver 边界稀释。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。PI1 只能冻结计划；production patch、production direct tests、board repeated 和 PI5 用户确认缺一不可。 |

## Evidence Doctor 和证据新鲜度

主证据为：

- `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/030-initgraph-no-solve-diagnostic/repeated-board-20260827-144235/`

`doc/phases/030-initgraph-no-solve-diagnostic/repeated-board-20260827-144045/` 是本阶段同日早先产生的
historical evidence（历史证据）目录；当前 result、matrix 和 roadmap 以 `repeated-board-20260827-144235`
为主证据。`log/board/run_bench_std.log` 和 `log/board/run_bench_rvv.log` 会被后续板卡命令覆盖，只能代表最后一次
board smoke（板卡小型验证）。

板卡命令多次报告远端 `script/rvv-board-run.mk` modification time（修改时间）超前导致的 clock skew warning
（时钟偏斜警告）。该 warning 不改变当前 checksum、B/A bucket 或 Evidence Doctor 分级；若进入 production evidence，
PI4 应重新记录环境和二进制身份。

## 阶段反思和矩阵更新

| candidate family | 更新 | 下一步 |
| --- | --- | --- |
| `initGraph` no-solve probe | `attempted / positive diagnostic`。加入 terminal write sink 后，5-run repeated 仍为正向。 | 创建 PI1 production integration plan，冻结 public entry、fallback、点类型 / layout、真实 graph mutation 和 production evidence。 |
| terminal-weight public-shaped component | Phase 020 的 `~1.54x` 正向信号得到 Phase 030 支持。 | 作为 PI1 公式候选输入，但不能直接接 production。 |
| organized n-link RVV exp candidate | 仍不作为 PI1 主触发项。 | 只有 PI1 计划选择覆盖 n-link edge mutation 时，才作为单独 fallback / no-production 边界处理。 |
| max-flow solver | 继续保持 `rejected with evidence`。 | State-heavy active set、parent/orphan 和 map/deque mutation 不进入本 topic 首轮 RVV 接入。 |

## Continue / stop decision

`continue_stop_decision=continue_to_PI1_plan`。Phase 030 没有命中板卡、correctness、asm 或 Evidence Doctor 停止条件；
但继续到 PI2 production patch（生产补丁）会修改 production 源码，因此需要先完成 PI1 文档计划并在 PI1 result 中停在
`PI2-blocked-on-user-authorization`，等待用户明确授权是否进入 production integration loop。
