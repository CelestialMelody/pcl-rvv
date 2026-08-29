# Phase Index

| phase | status | plan | result | default recovery |
| --- | --- | --- | --- | --- |
| `000-current-state-and-gaps` | completed | `000-current-state-and-gaps/plan.zh.md` | `000-current-state-and-gaps/result.zh.md` | `010-cluster-growth-diagnostic` |
| `010-cluster-growth-diagnostic` | completed | `010-cluster-growth-diagnostic/plan.zh.md` | `010-cluster-growth-diagnostic/result.zh.md` | `closeout / production doc refresh` |
| `020-cluster-growth-production-probe` | completed | `020-cluster-growth-production-probe/plan.zh.md` | `020-cluster-growth-production-probe/result.zh.md` | `closeout / production doc refresh` |

## 默认恢复入口

当前默认恢复入口是 Phase 020 的收尾结果。若要继续扩展，下一轮应进入新的
production-direct growth phase；若不继续，则直接做 closeout / production doc refresh，
而不是回到 Phase 000。
