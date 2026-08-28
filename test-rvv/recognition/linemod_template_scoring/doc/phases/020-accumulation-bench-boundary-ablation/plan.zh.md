# Phase 020 Plan: accumulation-bench-boundary-ablation

## 阶段意图和边界

本阶段只诊断 Phase 000 与 Phase 010 对 score accumulation（分数累加）方向相反的问题。目标是建立一个编译期隔离的 accumulation-only bench（只编译累加计时路径的性能测试），复核旧 Phase 000 的 positive 是否来自旧 bench 边界，还是当前 RVV 累加 helper 在板卡上已经稳定退化。本阶段不修改 `recognition/src/linemod.cpp`，不进入 production integration loop（生产接入闭环），也不把 diagnostic（诊断）结果直接外推为 production no-go。

## 当前状态清单

| item | current state |
| --- | --- |
| Phase 000 historical run | accumulation-only 旧 bench 输出 5-run positive，median `1.404x`，0/5 退化 |
| Phase 010 current run | 同一 topic 新 bench 输出三项计时；accumulation median `0.842x`，scan median `0.601x`，combined median `0.879x`，三项均 5/5 退化 |
| correctness | Std/RVV gtest 已覆盖 score accumulation 和 scan same-chain（同构链路）语义 |
| asm | 当前 RVV bench 反汇编包含 accumulation 与 scan 相关 RVV 指令 |
| open question | accumulation 方向反转可能来自 bench 二进制形态、代码布局、inlining（内联）、cache/warm-up 交互或 Phase 000 旧 Std 侧异常偏慢 |

## 假设与候选族

| hypothesis | expected signal | evidence needed |
| --- | --- | --- |
| bench-shape / code-layout mismatch | 编译期隔离 accumulation-only 后 speedup 接近 Phase 000 旧正向，或至少明显不同于 Phase 010 三项 bench | accumulation-only 专用 binary 的 QEMU log-shape smoke、asm、5-run board repeated、Evidence Doctor |
| helper truly negative on current toolchain / board state | 编译期隔离后仍 5/5 退化，median 仍低于 `0.97x` | 同上，并把 Phase 000 旧 summary 降级为 historical baseline |
| Phase 000 old Std side outlier | 新隔离 run 的 Std median 接近 Phase 010 的 `194 us` 而不是旧 run 的 `316 us` | 新 summary 的逐轮 Std/RVV 值和 doctor finding |

## 优化矩阵

| candidate | scope | correctness | bench | asm | board | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| compile-time accumulation-only bench | `u8` score maps -> `u16` score sums, same helper, no scan code in timed binary | reuse `run_test_compare` | planned dedicated target | planned dump | planned 5-run budget, board available | planned | planned |

## 实现和测试动作

| action | artifact | expected evidence | completion |
| --- | --- | --- | --- |
| RED：验证编译期开关尚未隔离输出 | `src/bench_linemod_template_scoring.cpp` via `EXTRA_CXXFLAGS=-DLINEMOD_SCORE_BENCH_ACCUMULATION_ONLY` | 现状仍输出 scan/combined，脚本断言失败 | failure observed before edit |
| 实现编译期开关 | `src/bench_linemod_template_scoring.cpp`、Makefile target | accumulation-only binary 只输出 accumulation timing 和旧 checksum 字段；默认 binary 保持三项输出 | QEMU smoke pass |
| 增加 Phase 020 evidence target | `Makefile` | `collect_accumulation_ablation_repeated_board`、manifest、doctor、registry、freshness check 可复用 wrapper | target exists and runs |
| 板卡 repeated | 5-run budget | 判断 positive / weak / neutral / negative / unstable | summary + doctor |

## Evidence Doctor 和 Registry

正式性能结论只引用 `doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-summary.md` 及同目录 manifest / doctor。若 Evidence Doctor 出 Error 且 checksum 一致，先解释为负向/异常证据，不把 Error 静默改成 pass。registry 使用 `phase020-accumulation-ablation-repeated-20260828` run label 登记。

## 阶段完成条件

- QEMU 只用于 log-shape smoke（日志形状冒烟）和 correctness，不使用 QEMU timing。
- 板卡 5-run 完成且 checksum 一致。
- result 回填 Phase 000 historical、Phase 010 current、Phase 020 isolated 三者关系。
- 若 Phase 020 仍为 negative，roadmap 默认转向 energy map / linearized map 或 profile，不建议直接 production patch score accumulation。

## 板卡复跑预算和决策桶

默认 5-run，`median speedup >= 1.05` 为 positive，`1.00-1.05` 为 weak-positive，`0.97-1.00` 为 neutral，`<0.97` 为 negative；若方向跨越 1 或 Evidence Doctor 指出长尾且影响桶，最多再做一次同边界确认复跑。当前会话确认板卡可用，所以不因“需要板卡”停止。

## 继续 / 停止条件

若 Phase 020 闭合且还有未阻塞的 energy map、linearized map 或 profile action，继续下一 phase；只有生产接入需要用户确认、板卡/工具不可用、Evidence Doctor Error 无法解释、dirty isolation 不安全或当前授权内无未阻塞动作时停止。

## 文档更新清单

更新 Phase 020 result、Phase README、optimization matrix、optimization roadmap、evaluation、README 和筛选队列。Phase 000 result 必须把旧 positive 标成 historical，并说明当前证据发生方向反转；不得继续把旧数值写成 current truth。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper / bench binary shape ablation |
| 当前决策问题 | implementation-shape 与 RVV-vs-scalar 边界复核 |
| diagnostic 是否可外推到 production | unknown；该 phase 只判断 bench 边界是否影响 accumulation helper 方向 |
| comparison-boundary / baseline mismatch 风险 | yes；Phase 000、Phase 010、Phase 020 可能不是同一 binary shape，需要分层解释 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但条件是后续 profile 或 production-shaped direct 证明累加仍是热点且同边界不退化 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若进入 production，需要 production direct Std/RVV repeated；若存在多个 RVV family，需要同边界 RVV-vs-RVV A/B |
