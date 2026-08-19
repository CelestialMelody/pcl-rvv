# Phase 020 Plan: Double Exp Helper Or Fused Formula Probe

## 阶段意图和边界

本阶段验证一个窄问题：phase 000 的 staged derivative accumulation（分阶段暂存后的导数累加）负向结果，是否主要受 scalar double `std::exp` 预处理拖累。

本阶段只在 `test-rvv/registration/ndt` 下实现 topic-local prototype（主题本地原型），不修改 `common/include/pcl/common/impl/rvv_math.hpp`，不修改 `registration/include/pcl/registration/impl/ndt.hpp`，不接入 production。

## 语义合同

目标 helper 是 finite-domain fast approximation（有限输入域快速近似），不是 strict libm replacement（严格 libm 替换）。

- 输入来自 NDT Gaussian 权重：`x = -gauss_d2 * q / 2`。
- 正常 NDT 中 `q = x_trans^T c_inv x_trans` 应非负，`gauss_d2 > 0`，所以主要输入域是 `x <= 0`。
- 本阶段原型先覆盖有限区间 `[-32, 0]`，超出范围按 clamp（夹取）处理，用于诊断是否值得继续，不作为 production 语义。
- 误差门槛先以 caller 结果为主：score、gradient、hessian 相对当前标量 reference 的差异必须落在现有测试容差内；若失败，先不跑性能结论。

## 当前证据输入

phase 010 gprof 显示 `updateDerivatives` 自耗时约 52.63%，`updateHessian` 约 27.63%。这支持继续评估 double `exp` 或 fused formula，但 phase 000 的 staged RVV candidate 在板卡上 hessian 0.320x、gradient 0.401x，说明只做规约和乘加不够。

## 实现动作

1. 在 `include/impl/ndt_candidates.hpp` 增加 topic-local double `exp` RVV helper，使用 `x = n*ln2 + r`、`r in [-ln2/2, ln2/2]`、多项式核函数和 `2^n` bit reconstruction（位重构）。
2. 在现有 RVV candidate 的 `q` 计算后直接用 vector helper 得到 `exp(q_arg)`，避免 `q_buf` store + scalar `std::exp` loop。
3. 保留 invalid weight mask（无效权重掩码）语义：`gauss_d2 * exp_value` 必须在 `[0, 1]` 内才写入 `scaled_weight` 并参与 score。
4. 不调整 gradient / hessian 后续 staged reduction，确保本阶段只消融 `exp` 预处理。

## 测试和证据

| evidence | command | 完成条件 |
| --- | --- | --- |
| QEMU correctness | `make run_test_compare` | Std/RVV 各 4 TEST pass |
| QEMU smoke / asm | `make record_qemu_smoke_evidence_state` | 生成 RVV 指令和 clean 或解释后的 Evidence Doctor |
| board repeated | `make run_board_bench_repeated` | 与 phase 000 同 case 比较 decision bucket 是否改变 |
| evidence freshness | `make evidence_status` | registry fresh |

## 决策

- 若 board repeated 转为 stable positive，下一 phase 才考虑把 double helper 抽到数学专项目录，并按 `rvv-math-vectorization` 补完整专项测试和文档。
- 若仍是 negative / neutral，说明 scalar `exp` 不是主要单点瓶颈，下一步优先 fused formula / 减少 staging，或停止 NDT 当前候选。
- 若 correctness 失败，记录为 helper 语义不满足 NDT caller budget，不跑性能结论。
