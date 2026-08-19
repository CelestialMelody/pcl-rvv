# NDT Optimization Roadmap

## 当前边界

当前 topic 已完成 staged derivative accumulation（分阶段暂存后的导数累加）诊断和真实 public-entry profile（公开入口性能剖析）。EvidenceDecision 仍为 `bench-only/no-production`，不建议把当前 staged RVV 候选接入 production；但 public-entry gprof 显示 `updateDerivatives` / `updateHessian` 是主热点，因此仍有继续探索 double `exp` / fused formula 的价值。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| staged derivative accumulation RVV | 当前源码 shape scan | `updateDerivatives` 数学核 | 跨样本规约 | 多遍 buffer / reduction 成本 | correctness、asm、board repeated、doctor | rejected | none |
| public-entry profile | phase 反思 | 真实 `NormalDistributionsTransform::align` | 找到真实热点占比 | 已完成 3-run board repeated 和 gprof probe；端到端 speedup neutral | board profile、function attribution | adopted | none |
| fused per-sample formula RVV | 负向归因 + phase 010/020 evidence | 减少 staged buffer 的局部公式 | 降低内存流量 | 仍受导数规约成本和寄存器压力影响 | RVV-vs-RVV A/B、board repeated | adopted as next search direction if continuing | next phase |
| double exp RVV math helper audit | 用户反馈 + 当前源码 math scan + phase 010 gprof | `updateDerivatives` / `updateHessian` 中的 double `std::exp` | 去除逐样本标量 exp 成本 | topic-local prototype correctness pass，但 board 仍 negative | QEMU correctness、board repeated、Evidence Doctor | attempted/rejected for NDT production | none |
| compiler auto-vectorization report | workflow 可选诊断 | `ndt.hpp` 局部公式 | 判断手写 RVV 是否必要 | 只说明编译器行为，不是性能证据 | `generate_vec_report`、asm attribution | deferred | optional-vectorization-report |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| staged derivative accumulation RVV | 5 次 board repeated 稳定 negative；hessian 0.320x，gradient 0.401x | 只有在新设计减少 staged buffer / reduction 次数后才可重开 |
| direct reuse of existing float RVV math helpers | NDT 热路径使用 double `std::exp`；当前 `rvv_math.hpp` 的 `expf_RVV_f32m2` / `logf_RVV_f32m2` / `sincos_finite_domain_RVV_f32m2` 不能直接替换 double 公式 | 已由 profile 证明导数核是主热点；下一步需要完成 double helper 的 math-vectorization phase |
| topic-local double exp RVV prototype | phase 020 correctness 通过，但 board repeated 仍 hessian 0.321x、gradient 0.405x；没有改变 negative decision bucket | 只有其它 caller 也需要 double `exp`，或 NDT 已有 fused formula 正向证据后，才另走数学专项 |
| production integration loop | 当前 diagnostic 负向，且没有 production-shaped public-entry positive | 用户明确要求做 bounded production probe，并先完成 profile / fallback / dispatch 计划 |

## 默认恢复动作

`next_phase_default`: 若继续优化，进入 `030-fused-formula-or-reduction-staging-probe`。phase 020 已排除“只替换 scalar double exp 就能转正”的假设；下一步只剩减少 staged buffer / reduction 次数的实现形态。若不继续，保持 `bench-only/no-production`。
