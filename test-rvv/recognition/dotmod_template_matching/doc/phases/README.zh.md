# DOTMOD Template Matching Phase Index

| phase | status | plan | result | default recovery |
| --- | --- | --- | --- | --- |
| `000-current-state-and-submap-direct-window` | completed | `000-current-state-and-submap-direct-window/plan.zh.md` | `000-current-state-and-submap-direct-window/result.zh.md` | 作为 historical diagnostic evidence（历史诊断证据）读取 |
| `010-full-detecttemplates-shaped-diagnostic` | completed | `010-full-detecttemplates-shaped-diagnostic/plan.zh.md` | `010-full-detecttemplates-shaped-diagnostic/result.zh.md` | 作为 production probe（生产探针）前置证据读取 |
| `020-production-integration-direct-window` | completed | `020-production-integration-direct-window/plan.zh.md` | `020-production-integration-direct-window/result.zh.md` | 读取后进入 Phase 030 当前证据 |
| `030-response-buffer-reuse` | completed | `030-response-buffer-reuse/plan.zh.md` | `030-response-buffer-reuse/result.zh.md` | 当前默认恢复入口 |

## 默认恢复入口

当前默认恢复入口是 `ready_for_review`。Phase 030 后，当前 production direct（真实生产路径）
summary 为 `log/board/repeated_phase020_production_direct/summary.md`；路径名沿用 Phase 020
target，内容代表 Phase 030 response-buffer-reuse 后的当前证据。

如果下一轮要继续当前 topic，必须先说明新的扩大范围，例如真实 workload profile、更多模板尺寸、
threshold 输出 staging 或 standalone `QuantizedMap::getSubMap()`。没有这些新输入时，本 topic
没有授权内的高优先级未阻塞优化动作。
