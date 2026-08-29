# Phase Index

| phase | status | plan | result | default recovery |
| --- | --- | --- | --- | --- |
| `000-current-state-and-filter-diagnostic` | historical | `000-current-state-and-filter-diagnostic/plan.zh.md` | `000-current-state-and-filter-diagnostic/result.zh.md` | 仅作历史 diagnostic baseline；已被 production direct 证据取代 |
| `010-production-integration` | completed | `010-production-integration/plan.zh.md` | `010-production-integration/result.zh.md` | 复核 `ZBuffering::filter(indices)`、`doc-rvv` 和 current-handoff |
| `020-inline-filter-production-integration` | completed | `020-inline-filter-production-integration/plan.zh.md` | `020-inline-filter-production-integration/result.zh.md` | 复核 public inline filter/getOccludedCloud、`doc-rvv` 和 current-handoff |

## 默认恢复入口

当前默认恢复入口是 Phase 020 result：先复核 public inline board summary、Evidence Doctor、
`doc-rvv` 和 current-handoff，再决定是否停在当前 adopted 结果，或另开新的扩展 phase。
Phase 010 仍保留为历史已采纳的 production direct 基线，Phase 000 只保留为历史诊断基线。
