# DON phase index

| phase | 状态 | 默认恢复动作 |
| --- | --- | --- |
| `000-current-state-and-diagnostic-plan` | partial-production-candidate | 已完成 test-only RVV candidate、QEMU correctness、asm、board smoke 和 Evidence Doctor；结果见 `000-current-state-and-diagnostic-plan/result.zh.md` |
| `010-diagnostic-repeated-board` | partial-production-candidate | 已完成 5-run repeated board，decision bucket 为 `weak_positive`；结果见 `010-diagnostic-repeated-board/result.zh.md` |
| `020-pi1-production-integration-plan` | completed | 用户已要求继续推进；PI1 范围冻结后进入 `030-production-direct-probe` |
| `030-production-direct-probe` | superseded-by-rollback | production-public repeated board 为 `negative`，Evidence Doctor 有 1 个 Error；用户已在 phase 040 确认回滚 |
| `040-rollback-closeout` | rolled_back_no_production | 已回滚 DON production RVV helper / dispatch；保留 `PCLBase<PointInT>::initCompute()` 独立正确性修复 |
| `050-production-detail-ablation` | stop_no_worthwhile_production_direction | 已完成 finite-mask / sqrt / curvature-store 消融；没有保持 production 语义且值得继续推进的 RVV production 方向 |
