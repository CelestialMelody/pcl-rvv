# Phase 010 结果：板卡证据与 production 边界

> 当前状态提示：本文是 Phase 010 的历史阶段结果。Phase 020 刷新了 gather 诊断，Phase 030 又执行并回滚 production direct probe（真实生产入口探针）。当前 EvidenceDecision 为 `rollback/no-production`，当前 board correctness 为 8 tests passed。

## 实际执行范围

本阶段补齐 board / target hardware（板卡 / 目标硬件）证据、Evidence Doctor（证据体检）和 EvidenceDecision（证据决策）边界。production 源码未修改，目标文件保持只读复核。

实际触碰范围：

- `test-rvv/registration/correspondence_rejection_poly/**`
- `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md`
- `tmp/rvv-work-logs/registration/correspondence_rejection_poly/**`

只读复核路径：

- `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`
- `registration/include/pcl/registration/correspondence_rejection_poly.h`

## 计划动作回填

| action | 状态 | 证据路径 | 结论 |
| --- | --- | --- | --- |
| A1 board summary 支撑 | done | `script/summarize_crpoly_board_repeated.py` | 可解析 run-labelled board logs，输出 `summary.md` 和 `evidence_manifest.json`。 |
| A2 Makefile board targets | done | `Makefile` | 新增 board smoke、edge repeated、acceptance repeated 和确认复跑变量。 |
| A3 board correctness smoke | done | `log/board/test_smoke/run_test.log` | Phase 010 时 board gtest 6 tests passed；同路径已在 Phase 020 刷新为 7 tests passed，并在 Phase 030 刷新为 8 tests passed。 |
| A4 edge repeated board | done | `log/board/edge_batch_repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | decision bucket 为 `weak_positive`，doctor Errors=0、Warnings=0、Suggestions=0。 |
| A5 acceptance repeated board | done | `log/board/acceptance_filter_repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | 初始 bucket 为 `neutral`，doctor Errors=0、Warnings=0、Suggestions=1。 |
| A6 acceptance confirmation rerun | done | `log/board/acceptance_filter_confirm/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | 确认 bucket 仍为 `neutral`，doctor Errors=0、Warnings=1、Suggestions=1；复跑预算已用完。 |
| A7 registry / freshness | done | `log/evidence_registry.json` | 已登记 board smoke、edge summary / manifest / doctor、acceptance 初始和确认 summary / manifest / doctor。 |
| A8 S10 文档 closeout | done | 本文件、evaluation、roadmap、matrix、Handoff | 当前结论保持 `diagnostic/no-production`；只允许进入 topic-local production-shaped gather 诊断。 |

## Board 结果

| candidate family | run_label | case | speedup_values | median | degradation_frequency | decision_bucket | doctor result | 处理 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `edge_length_batch` | `edge_batch_repeated` | 64K | 1.066x, 1.069x, 1.096x, 1.066x, 1.078x | 1.069x | 0/5 | `weak_positive` | Errors=0, Warnings=0, Suggestions=0 | 只支持下一阶段 production-shaped gather 诊断，不支持 production 修改。 |
| `edge_length_batch` | `edge_batch_repeated` | 256K | 1.069x, 1.084x, 1.057x, 1.027x, 1.029x | 1.057x | 0/5 | `weak_positive` | Errors=0, Warnings=0, Suggestions=0 | 256K 中位数刚过 weak-positive 阈值，后续必须把真实 correspondence index gather 成本纳入计时。 |
| `accept_rate_filter` | `acceptance_filter_repeated` | 64K | 1.065x, 1.071x, 1.057x, 1.065x, 1.062x | 1.065x | 0/5 | `neutral` | Errors=0, Warnings=0, Suggestions=1 | 64K 弱正向，但同 family 256K 接近阈值，整体不升级。 |
| `accept_rate_filter` | `acceptance_filter_repeated` | 256K | 1.002x, 1.024x, 1.035x, 1.009x, 1.004x | 1.009x | 0/5 | `neutral` | `near_threshold_ba` suggestion | 触发同边界确认复跑。 |
| `accept_rate_filter` | `acceptance_filter_confirm` | 64K | 1.059x, 1.076x, 1.064x, 1.063x, 1.062x | 1.063x | 0/5 | `neutral` | Errors=0, Warnings=1, Suggestions=1 | 64K 仍弱正向，但 family 结论由 256K 风险约束。 |
| `accept_rate_filter` | `acceptance_filter_confirm` | 256K | 1.012x, 1.023x, 1.043x, 1.018x, 0.996x | 1.018x | 1/5 | `neutral` | `ba_degradation_frequency` warning；`near_threshold_ba` suggestion | 复跑预算已用完；保持 no-production，后续只做可选 scalar-tail attribution。 |

`speedup = std_ms / rvv_ms`，大于 1 表示 RVV build 更快。当前 board 数字来自 test support candidate wrapper（测试支撑候选包装），不是 production direct（生产直连）证据。

## Evidence Doctor 结果

| report | Errors | Warnings | Suggestions | 处理 |
| --- | --- | --- | --- | --- |
| `log/board/edge_batch_repeated/evidence_doctor.md` | 0 | 0 | 0 | 可作为 `edge_length_batch` 的 repeated board diagnostic 证据。 |
| `log/board/acceptance_filter_repeated/evidence_doctor.md` | 0 | 0 | 1 | 256K `near_threshold_ba` 触发确认复跑。 |
| `log/board/acceptance_filter_confirm/evidence_doctor.md` | 0 | 1 | 1 | 256K 出现 1/5 退化且仍接近阈值；结论降级为 `neutral/no-production`，不再自动复跑。 |

## Evidence registry 状态

`log/evidence_registry.json` 已登记本阶段 board evidence：

- `log/board/test_smoke/run_test.log`
- `log/board/edge_batch_repeated/summary.md`
- `log/board/edge_batch_repeated/evidence_manifest.json`
- `log/board/edge_batch_repeated/evidence_doctor.md`
- `log/board/acceptance_filter_repeated/summary.md`
- `log/board/acceptance_filter_repeated/evidence_manifest.json`
- `log/board/acceptance_filter_repeated/evidence_doctor.md`
- `log/board/acceptance_filter_confirm/summary.md`
- `log/board/acceptance_filter_confirm/evidence_manifest.json`
- `log/board/acceptance_filter_confirm/evidence_doctor.md`

各 run 目录下的 `run_bench_*.log` 是 raw log（原始日志），默认本机保留，不进入提交边界。

## S10 EvidenceDecision

当前 EvidenceDecision：`diagnostic/no-production`。

分项判断：

- `edge_length_batch`：`weak_positive` board diagnostic。它证明预构造 squared distance 数组上的局部公式值得继续做 topic-local production-shaped gather 诊断；它不能证明真实 production gather、dispatch、fallback 或泛型点类型边界。
- `accept_rate_filter`：`neutral/no-production`。确认复跑出现 256K 退化频率 warning 和 near-threshold suggestion；当前不建议进入 production，也不作为下一阶段默认路线。
- `histogram_otsu_scalar`：继续标量。当前没有 profile 或 board evidence 指向这段是主热点。

production 决策：不修改 `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`，不进入 production integration loop。若继续当前 topic，默认下一阶段是 `020-production-shaped-gather-diagnostic`，仍只写 topic-local 诊断资产。

## 诊断证据链

| 证据层 | 当前结果 | 支持什么 | 不支持什么 |
| --- | --- | --- | --- |
| correctness | Phase 010 时 board smoke 和 QEMU Std/RVV gtest 均通过 6 tests；Phase 020 同路径刷新为 7 tests | guard、edge formula、accept rate、histogram / Otsu、固定 seed public entry 语义保持。 | 不证明 production RVV dispatch。 |
| QEMU path | QEMU smoke 可运行且 doctor pass | 构建、日志形状和 manifest 合同可用。 | QEMU timing 不作为性能结论。 |
| asm | bench RVV binary 中存在局部 RVV 指令 | test-only helper 被编译到 RVV bench binary。 | helper 内联，production hot symbol 归因未闭合。 |
| board performance | edge weak-positive；acceptance neutral | 目标硬件上预构造距离数组局部公式有弱正向诊断信号。 | 不证明真实 correspondence gather 后仍正向，也不证明完整函数端到端收益。 |
| negative / neutral evidence | acceptance 256K 接近阈值且确认复跑 1/5 退化 | accept-rate filter 不应进入 production。 | 不能单独归因于 scalar tail、温度、频率或输入分布。 |

## 继续 / 停止决策

Phase 010 完成。当前仍有 topic-local、未阻塞的下一动作：`020-production-shaped-gather-diagnostic`。它只评估真实 correspondence index 读点、PointXYZ AoS（结构数组）布局、staging（暂存）或 indexed load（索引加载）成本是否会吞掉 `edge_length_batch` 的弱正向收益。

停止条件没有命中 production-ready。继续条件只对 topic-local 诊断成立，不允许修改 production 源码。
