# Phase 000 Result: Current State And Gaps

## 执行摘要

已创建 NDT test-only RVV diagnostic scaffold，并完成 QEMU correctness、QEMU smoke、反汇编、板卡 correctness smoke 和 5 次 board repeated bench。当前 staged derivative accumulation RVV 候选为稳定 negative，不建议接入 production。

## 变化范围

| 类别 | 路径 |
| --- | --- |
| test support | `include/ndt.h`、`include/impl/ndt_fixtures.hpp`、`include/impl/ndt_references.hpp`、`include/impl/ndt_candidates.hpp` |
| test / bench | `src/test_ndt.cpp`、`src/bench_ndt.cpp` |
| scripts | `script/generate_ndt_qemu_evidence_manifest.py`、`script/generate_ndt_board_repeated_summary.py` |
| topic docs | README、doc suite、phase plan/result/matrix、roadmap、evaluation |
| production | 未修改 |

## 结果

| evidence | command | result |
| --- | --- | --- |
| QEMU correctness | `make run_test_compare` | Std/RVV 各 4 TEST pass |
| QEMU smoke + asm | `make record_qemu_smoke_evidence_state` | Evidence Doctor Errors=0，Warnings=0；filtered asm 223 行 |
| Board correctness | `make run_board_test_smoke` | 4 TEST pass |
| Board repeated | `make run_board_bench_repeated` | hessian median 0.320x；gradient median 0.401x；5/5 below 1 |
| Board Evidence Doctor | `test-rvv/registration/ndt/log/board/derivative_accumulation_repeated/evidence_doctor.md` | Errors=2，Warnings=2 |

## 解释和边界

`ba_degradation_frequency` Errors 说明当前 RVV candidate 不能支撑 production-ready。`contract_mismatch` Warnings 是预期的 diagnostic 边界：baseline 是 scalar order，candidate 是 RVV reduction。该 mismatch 使本证据不能升级成 strict production A/B，但不影响“当前 test helper 形态稳定负向”的判断。

负向归因假设：跨样本 SoA staged buffer、标量 `exp` 预处理、36 个 hessian 元素的多遍 RVV reduction 和内存流量成本超过乘加收益。该归因尚未由 profile（性能剖析）或 ablation（消融对照）单独证明，不能写成唯一根因。

数学 helper 审计：`ndt.hpp` 里的 `std::log` / `std::exp(-0.5)` 位于 `computeTransformation` 的 Gaussian 常量初始化，通常每次 alignment（配准求解）只执行一次；`std::sin` / `std::cos` 位于 `computeAngleDerivatives`，每次 derivative pass（导数计算轮次）只处理 3 个角。逐 point-neighbor sample（点-邻域样本）反复执行的是 `updateDerivatives` / `updateHessian` 中的 double `std::exp`。当前 `common/include/pcl/common/impl/rvv_math.hpp` 已提供 `expf_RVV_f32m2`、`logf_RVV_f32m2` 和 `sincos_finite_domain_RVV_f32m2` 等 float helper，但没有可直接用于本 double 热路径的 `exp_RVV_f64*` 合同。因此“标量 `exp` 预处理”只能作为未消融的可能贡献因素；若后续 profile 证明该 double `exp` 是主瓶颈，应先按 `rvv-math-vectorization` 建立 double exp helper 的语义、误差、QEMU 和板卡证据，再回到 NDT caller smoke。

## 文档同步和下一步

已更新 README、evaluation、testing overview、correctness、benchmark/evidence、optimization evidence、code map、roadmap 和 matrix。`doc-rvv` production 长期主题文档不适用。

默认下一步：暂停在用户确认点，建议不把当前 RVV 优化实现接入源码。若用户要求继续，下一 phase 只建议做真实 public-entry profile，不改 production。
