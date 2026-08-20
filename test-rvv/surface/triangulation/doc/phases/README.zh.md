# triangulation RVV 阶段索引

本文只维护 `test-rvv/surface/triangulation` 的阶段恢复入口。阶段计划和结果保存接入前诊断（pre-production diagnostic，生产接入前的测试专用诊断）事实；没有用户确认采纳的 production patch（生产补丁）前，不创建 `doc-rvv/surface/triangulation-RVV.zh.md`。

| phase | 状态 | 作用 | 默认恢复动作 |
| --- | --- | --- | --- |
| `000-current-state-and-grid-sampling-diagnostic` | no-production_current_candidate / turn_stop_deferred | 建立函数级评估、测试支撑结构、规则网格参数写入 RVV candidate（候选实现）和未裁剪 surface 采样 bench。 | 板卡恢复后已补 5-run repeated；当前 candidate 不进入 production。用户已说明忽略 on_nurbs 依赖相关方向；剩余不依赖该符号链的 `createIndices` 小对象输出不建议作为 RVV phase。 |

当前默认恢复入口：`000-current-state-and-grid-sampling-diagnostic/result.zh.md` 的停止结论。若用户以后重新授权 on_nurbs / OpenNURBS 依赖链路或明确要求做非 RVV 标量消融，再创建新 phase。
