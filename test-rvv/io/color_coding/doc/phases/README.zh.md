# color_coding phase index

| phase | status | plan | result | default recovery |
| --- | --- | --- | --- | --- |
| `000-current-state-and-diagnostic-scaffold` | completed / historical board numbers | `000-current-state-and-diagnostic-scaffold/plan.zh.md` | `000-current-state-and-diagnostic-scaffold/result.zh.md` | phase 010 replaced current board truth |
| `010-bench-stabilization-and-leaf-size-sweep` | completed | `010-bench-stabilization-and-leaf-size-sweep/plan.zh.md` | `010-bench-stabilization-and-leaf-size-sweep/result.zh.md` | phase 020 production-shaped precheck |
| `020-production-shaped-color-coder-precheck` | completed / partial-production-candidate | `020-production-shaped-color-coder-precheck/plan.zh.md` | `020-production-shaped-color-coder-precheck/result.zh.md` | phase 030 PI1 encode/default plan |
| `030-pi1-encode-default-production-integration-plan` | planned / user checkpoint | `030-pi1-encode-default-production-integration-plan/plan.zh.md` | `030-pi1-encode-default-production-integration-plan/result.zh.md` | wait for production patch authorization |
| `040-structure-parity-doc-suite` | completed / turn-stop at production authorization boundary | `040-structure-parity-doc-suite/plan.zh.md` | `040-structure-parity-doc-suite/result.zh.md` | wait for production patch authorization |
| `050-decode-implementation-shape-audit` | completed / staged-store rejected | `050-decode-implementation-shape-audit/plan.zh.md` | `050-decode-implementation-shape-audit/result.zh.md` | wait for production patch authorization |
| `060-pi2-production-patch-and-direct-evidence` | completed / historical PI5 checkpoint | `060-pi2-production-patch-and-direct-evidence/plan.zh.md` | `060-pi2-production-patch-and-direct-evidence/result.zh.md` | superseded by phase 070 partial rollback and phase 080 full rollback |
| `070-pi5-partial-rollback-default-only` | completed / historical no-adoption evidence | `070-pi5-partial-rollback-default-only/plan.zh.md` | `070-pi5-partial-rollback-default-only/result.zh.md` | superseded by phase 080 full rollback |
| `080-full-rollback-no-adoption-closeout` | completed / no production RVV adopted | `080-full-rollback-no-adoption-closeout/plan.zh.md` | `080-full-rollback-no-adoption-closeout/result.zh.md` | no next phase |

当前默认恢复入口：phase 080 已完成完整回滚，`color_coding.h` 当前没有 production RVV 分流。Phase 070 default-only 板卡证据为 median `1.0035x`、min `0.9945x`，Evidence Doctor `Errors=1`，作为 no-adoption 决策依据。当前没有建议继续推进的 RVV candidate；如未来重开，需要新的 profile 或新的 implementation family phase plan。
