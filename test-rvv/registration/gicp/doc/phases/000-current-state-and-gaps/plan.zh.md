# Phase 000: Current State And Gaps

## 阶段意图和边界

本阶段回答 `gicp.hpp` 是否值得建议进入 production integration loop（生产接入闭环）。范围限定为接入生产前诊断：residual / Mahalanobis dense-row 累加和 covariance post-KNN 协方差局部组件。不可触碰路径是 production 源码、public API、真实 GICP dispatch、KdTree search、correspondence search、3x3 SVD、Newton eigensolver 和 BFGS solver。

## 当前状态清单

| area | state | evidence |
| --- | --- | --- |
| 源码候选 | `gicp` 是保留复筛候选 | `doc-rvv/library-screening/registration/registration-retained-candidate-rescreen.zh.md` |
| production | 未修改 | `registration/include/pcl/registration/impl/gicp.hpp` |
| test scaffold | 本阶段新建 topic-local scaffold | `test-rvv/registration/gicp/` |
| phase plan order | partial | scaffold 已先于 plan 创建；result 和 Handoff 必须暴露该流程偏差 |

## 假设与候选族

| candidate family | hypothesis | risk |
| --- | --- | --- |
| residual dense-row RVV reduction | correspondence 后 residual 批量扫描足够大，RVV 可以降低 `M*d` 和 dCost 累加成本 | production 中 PointCloud + indices 访存和 optimizer 调用频率可能稀释 |
| covariance post-KNN RVV reduction | KNN 后 k=20 的 mean / covariance 累加可以受益 | 每点 k 太小，KNN 和 SVD 可能主导 |

## 优化矩阵

见 `doc/phases/optimization-matrix.zh.md`。

## 实现和测试动作

| action | artifact / command | expected evidence | completion |
| --- | --- | --- | --- |
| 创建 topic scaffold | `include/`、`src/`、`script/`、`doc/` | 可编译测试资产 | done / partial |
| QEMU correctness | `make run_test_compare` | Std/RVV gtest pass | required |
| QEMU smoke + asm | `make run_qemu_smoke_evidence_doctor` | RVV bench 可运行、asm 有 RVV 指令、doctor 输出 | required |
| board smoke | `make run_board_test_smoke` | 板卡 RVV test pass | required |
| board repeated | `make run_board_bench_repeated` | 5-run summary、manifest、doctor、registry | required |
| 写 result | `doc/phases/000-current-state-and-gaps/result.zh.md` | action 状态和 EvidenceDecision | required |

## Evidence Doctor 和 Registry

板卡 repeated summary 必须生成 `log/board/component_diagnostic_repeated/summary.md`、`evidence_manifest.json` 和 `evidence_doctor.md`。QEMU smoke 若只用 Markdown summary，Evidence Doctor 只能作为轻量 reviewer aid。

## 板卡复跑预算和决策桶

默认 5 runs。若 Evidence Doctor Error 或 decision bucket 跨方向摇摆，最多追加一次同边界复跑；否则不扩大预算。bucket 规则见 `doc/benchmark-and-evidence.zh.md`。

## 继续 / 停止条件

若 residual 组件板卡 repeated 为 positive 且 correctness / asm / doctor 无阻塞，默认停止在用户确认点，询问是否建议将 RVV 优化实现接入源码。该停止命中 production 接入需用户授权。若 residual 和 covariance 均 weak / neutral / negative，本阶段进入 no-production diagnostic closeout，不建议接 production。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar 是否足以建议 PI1 |
| diagnostic 是否可外推到 production | unknown，需要 public entry profile 和 production direct 证据 |
| comparison-boundary / baseline mismatch 风险 | yes，预展开 dense arrays 不等同 PointCloud + indices |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 默认 no，除非用户明确要求 |
| clean adoption 是否需要 production boundary RVV-vs-RVV detail A/B | yes |
