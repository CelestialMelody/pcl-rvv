# Phase Index

| phase | status | plan | result | default recovery |
| --- | --- | --- | --- | --- |
| 000-current-state-and-score-accumulation | completed / historical-positive superseded | `000-current-state-and-score-accumulation/plan.zh.md` | `000-current-state-and-score-accumulation/result.zh.md` | 旧 positive 只作 historical evidence；当前 truth 以 Phase 010/020 复跑为准 |
| 010-threshold-scan-and-detection-order | completed / attempted-negative | `010-threshold-scan-and-detection-order/plan.zh.md` | `010-threshold-scan-and-detection-order/result.zh.md` | 保守 scan RVV 5/5 退化，不进入 production |
| 020-accumulation-bench-boundary-ablation | completed / attempted-negative | `020-accumulation-bench-boundary-ablation/plan.zh.md` | `020-accumulation-bench-boundary-ablation/result.zh.md` | accumulation-only 隔离复跑仍 5/5 退化，默认继续 Phase 030 energy map generation |
| 030-energy-map-generation | completed / attempted-negative | `030-energy-map-generation/plan.zh.md` | `030-energy-map-generation/result.zh.md` | energy map generation median `0.954x`，5/5 退化，默认继续 Phase 040 linearized map copy |
| 040-linearized-map-copy-ablation | completed / attempted-positive / partial-production-candidate | `040-linearized-map-copy-ablation/plan.zh.md` | `040-linearized-map-copy-ablation/result.zh.md` | linearized map copy median `2.160x`，0/5 退化；继续 full-chain timing 和 production eligibility audit |
| 050-full-chain-timing-and-production-eligibility | completed / attempted-positive / production-ready-for-PI1-plan | `050-full-chain-timing-and-production-eligibility/plan.zh.md` | `050-full-chain-timing-and-production-eligibility/result.zh.md` | full-chain total median `1.565x`，linearized copy median `1.790x`，均 0/5 退化且 Doctor clean；进入 PI1 计划 |
| 060-pi1-production-integration-plan | completed / authorized-by-current-prompt | `060-pi1-production-integration-plan/plan.zh.md` | `060-pi1-production-integration-plan/result.zh.md` | PI1 冻结默认宏下 `matchTemplates` / `detectTemplates` linearized copy 的 production patch 范围；用户本轮允许按接入后板卡收益采纳 |
| 070-production-integration-loop | completed / adopted-production-behavior | `070-production-integration-loop/plan.zh.md` | `070-production-integration-loop/result.zh.md` | production-public board median：`matchTemplates 1.138x`、`detectTemplates 1.129x`，均 0/5 退化，Doctor 0/0/0；正式 `doc-rvv` 已创建 |
| 080-semi-scale-production-direct-probe | completed / adopted-production-behavior | `080-semi-scale-production-direct-probe/plan.zh.md` | `080-semi-scale-production-direct-probe/result.zh.md` | semi-scale production-public board median：`detectTemplatesSemiScaleInvariant 1.136x`，0/5 退化，Doctor 0/0/0；正式 `doc-rvv` 已刷新 |

## 默认恢复入口

当前默认恢复入口是 final verification（最终验证）：运行 Phase 070 / 080 correctness、反汇编、evidence freshness 和 `git diff --check`。若这些检查通过，当前 topic 可停在 ready-for-review。剩余可单独新开 phase 的方向是 `LINEMOD_USE_SEPARATE_ENERGY_MAPS`，但它依赖非默认编译宏和四套 map 语义，当前 adopted production 范围不覆盖。
