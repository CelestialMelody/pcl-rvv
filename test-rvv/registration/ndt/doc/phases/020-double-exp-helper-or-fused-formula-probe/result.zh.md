# Phase 020 Result: Double Exp Helper Or Fused Formula Probe

## 执行摘要

本阶段实现了 topic-local double `exp` RVV prototype（主题本地 double exp 原型），只用于 `test-rvv/registration/ndt/include/impl/ndt_candidates.hpp` 的诊断候选。QEMU correctness（QEMU 正确性）通过，说明有限域近似在当前 NDT caller-shaped sample（按调用方形态构造的样本）上没有破坏 score、gradient 和 hessian 的现有容差；但 5 次 board repeated（板卡重复性能测试）仍稳定 negative，因此 scalar double `std::exp` 不是 phase 000 负向结果的单点主因。

不建议把本 prototype 抽到 `common/include/pcl/common/impl/rvv_math.hpp`，也不建议接入 `registration/include/pcl/registration/impl/ndt.hpp`。

## 变化范围

| 类别 | 路径 |
| --- | --- |
| topic-local candidate | `include/impl/ndt_candidates.hpp` |
| board target / registry | `Makefile` |
| evidence output | `log/board/double_exp_probe_repeated/summary.md`、`evidence_doctor.md` |
| production | 未修改 |
| common math helper | 未修改 |

## 语义和数学 helper 审计

本阶段 helper 是 finite-domain fast approximation（有限输入域快速近似），不是 strict libm replacement（严格 libm 替换）。输入只来自 NDT Gaussian 权重公式 `x = -gauss_d2 * q / 2`，本阶段按 `[-32, 0]` clamp（夹取）处理域外值。它没有覆盖 NaN、Inf、overflow、underflow、subnormal、signed zero 等完整 libm 语义，也没有数学专项参数脚本或公共 caller 白名单。

因此它只能回答“去掉当前 staged candidate 里的 scalar `std::exp` loop 后，板卡 decision bucket 是否改变”。结果显示没有改变；所以暂不建议为 NDT 单独推进公共 double `exp_RVV_f64*` helper。

## 结果

| evidence | command | result |
| --- | --- | --- |
| QEMU correctness | `make run_test_compare` | Std/RVV 各 4 TEST pass |
| QEMU smoke / asm | `make record_qemu_smoke_evidence_state` | RVV bench 可运行；Evidence Doctor clean |
| board correctness | `make run_board_test_smoke` | 4 TEST pass |
| board repeated | `make run_board_bench_ndt_double_exp_repeated` | hessian median 0.321x；gradient median 0.405x；两者 5/5 below 1 |
| Board Evidence Doctor | `log/board/double_exp_probe_repeated/evidence_doctor.md` | Errors=2，Warnings=3，Suggestions=1 |

## 解释和边界

与 phase 000 对比，hessian 从 0.320x 到 0.321x，gradient 从 0.401x 到 0.405x，只是噪声级变化，decision bucket 仍是 `negative`。这说明 phase 000 的负向不能主要归因于 scalar double `std::exp`；更可能的主因仍是 staged buffer（暂存缓冲）、多遍 hessian reduction（海森矩阵多遍规约）、寄存器压力和内存流量。

public-entry align case 在本 target 中约 1.007x、`neutral`，但 production 源码没有 NDT RVV patch；它只能继续作为 profile cross-check（剖析交叉检查），不能作为 production speedup。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | pre-production diagnostic |
| A/B boundary | test helper with topic-local double exp prototype |
| 当前决策问题 | RVV-vs-scalar implementation-shape；是否值得继续公共 double exp helper |
| diagnostic 是否可外推到 production | no；仍排除真实邻域搜索、point derivative、line search、solver 和 production dispatch |
| comparison-boundary / baseline mismatch 风险 | yes；baseline 是 scalar order，candidate 是 RVV reduction + topic-local exp prototype |
| negative 时是否允许 bounded production probe | 不建议；除非另有 fused formula 正向证据和用户明确授权 |
| clean adoption 是否需要 production boundary A/B | yes；当前没有 production patch，也没有 production-detail A/B |

## 决策

`double exp helper audit` 状态改为 `attempted/rejected for NDT production`。本 topic 不建议继续投入公共 double exp helper；如果未来别的 caller 也需要 double `exp`，应另走 `rvv-math-vectorization` 数学专项目录、参数脚本、same-chain（同构链路）对拍、特殊值合同和下游 smoke。

NDT 若继续探索，下一阶段默认方向应切到 fused per-sample formula（融合逐样本公式）或减少 staging / reduction 次数的 RVV-vs-RVV A/B；若不继续，则当前 topic 保持 `bench-only/no-production`。
