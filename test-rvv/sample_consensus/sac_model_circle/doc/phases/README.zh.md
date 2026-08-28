# sac_model_circle phase index

| phase | 状态 | 入口 | 结果 |
| --- | --- | --- | --- |
| `000-circle-select-distance-production` | completed; production adopted by Phase 040 | `000-circle-select-distance-production/plan.zh.md` | `000-circle-select-distance-production/result.zh.md` |
| `020-circle-getdistances-ablation` | completed; diagnostic negative | `020-circle-getdistances-ablation/plan.zh.md` | `020-circle-getdistances-ablation/result.zh.md` |
| `030-structure-parity-doc-suite` | completed; doc suite adopted | `030-structure-parity-doc-suite/plan.zh.md` | `030-structure-parity-doc-suite/result.zh.md` |
| `040-production-closeout-and-identity-frontier` | completed; production closeout adopted; identity rejected | `040-production-closeout-and-identity-frontier/plan.zh.md` | `040-production-closeout-and-identity-frontier/result.zh.md` |
| `050-getdistances-vfsqrt-full-rvv` | completed; diagnostic positive | `050-getdistances-vfsqrt-full-rvv/plan.zh.md` | `050-getdistances-vfsqrt-full-rvv/result.zh.md` |
| `060-getdistances-production-probe` | completed; production direct positive | `060-getdistances-production-probe/plan.zh.md` | `060-getdistances-production-probe/result.zh.md` |
| `070-getdistances-production-closeout` | completed; getDistances adopted | `070-getdistances-production-closeout/plan.zh.md` | `070-getdistances-production-closeout/result.zh.md` |
| `080-select-compressed-error-tail` | completed; detail A/B positive | `080-select-compressed-error-tail/plan.zh.md` | `080-select-compressed-error-tail/result.zh.md` |
| `090-select-error-tail-production-probe` | completed; production direct positive and adopted | `090-select-error-tail-production-probe/plan.zh.md` | `090-select-error-tail-production-probe/result.zh.md` |

默认恢复动作：先读取 Phase 000 / 020 / 030 / 040 / 050 / 060 / 070 / 080 / 090 result、对应 manifest 和 Evidence Doctor。
当前 `selectWithinDistance` / `countWithinDistance` gather-style RVV 已按用户确认采纳；`getDistancesToModel`
Phase 050 full-RVV diagnostic（完整 RVV 诊断）为 positive，Phase 060 production probe（生产探针）
接入后 production public Std/RVV 也为 positive-stable；Phase 070 已按用户确认采纳为 adopted
production behavior（已采用生产行为）；Phase 090 `selectWithinDistance` full-RVV error tail（完整 RVV 误差写回尾段）
接入后 production public Std/RVV 为 positive-stable，并按用户确认采纳。长期 `doc-rvv` 已刷新为最终采用数据。
identity-index strided load 当前实现族已被 strict A/B 拒绝。

文档归属：`README.zh.md` 是 topic navigation（主题导航），`doc/sac_model_circle-evaluation.zh.md`
是函数级评估和 Traceability Map（可追踪性地图）主归属，测试、bench（性能测试）、优化证据和代码地图
分别归到 `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、
`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md` 和
`doc/test-support-code-map.zh.md`。
