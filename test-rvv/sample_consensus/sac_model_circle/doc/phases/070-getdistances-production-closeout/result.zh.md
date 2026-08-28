# Phase 070 Result: getDistances production closeout

## 执行范围

本阶段在用户确认“板卡有收益且接入后测试通过即可采纳”的前提下，将 Phase 060 的 `getDistancesToModel` full-RVV production probe 收口为 adopted production behavior。production 源码没有新增修改；本阶段只刷新 topic-local docs、正式 `doc-rvv`、队列表和 Handoff。

## 动作回填

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| production `doc-rvv` closeout | done | `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md` 已把 `getDistancesToModel` 写成 adopted，并使用 Phase 060 接入后 board median `1.4737x`。 |
| topic-local 文档同步 | done | README、evaluation、optimization evidence、roadmap、matrix、benchmark/evidence、code map 和 phase index 已从 PI5 pending 刷新为 Phase 070 adopted。 |
| Handoff 同步 | done | `tmp/rvv-work-logs/sample_consensus/sac_model_circle/current-handoff/` 已更新为 `S11_getDistances_production_closeout_completed`。 |
| 队列表同步 | done | `doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md` 已把 circle getDistances 标为 production adopted。 |

## 证据结论

Phase 060 是本阶段采用依据。接入后的 public Std/RVV board repeated B/A 为 `1.4697, 1.4737, 1.4962, 1.4755, 1.4687`，median `1.4737x`，min/max `1.4687x / 1.4962x`，`B/A < 1` 为 0/5。Evidence Doctor 为 Errors=0，Warnings=0，Suggestions=0。

`getDistancesToModelRVV` 使用 RVV gather x/y、平方距离、`vfsqrt.v`、abs、`vfwcvt.f.f.v` 和 `vse64.v` dense double write-back（密集 double 写回）。Std/RVV raw checksum 不要求逐 bit 相同；正确性由 public / Standard / direct RVV `1e-6` tolerance gtest 覆盖。

## Optimization matrix 更新

`getDistances vfsqrt full-RVV production probe` 已从 `PI5 pending user confirmation` 更新为 `adopted production behavior`。Phase 020 的旧 `RVV sqr + scalar sqrt/store` candidate 仍保持 rejected；Phase 040 identity strided-load 仍保持 rejected。

## 后续方向判断

当前同边界 `getDistancesToModel` 没有新的高优先级未阻塞优化候选。可想到的后续方向包括：

- `selectWithinDistance` 命中点 exact error 仍有 `vcompress` 后标量 `sqrt` tail，但它只对 inlier 生效，且当前 select board median 已有 `1.6702x`。要继续优化需要设计新的压缩后 full-RVV distance write-back family，并做 RVV-vs-RVV detail A/B；这会改变已采纳 select family 的实现族选择，建议作为新 phase / 新用户确认范围，而不是本次 getDistances closeout 的默认续作。
- 更多 PointXYZ-like 点型 dedicated board、`Scalar=double`、自定义 layout 和 `circle3d` 投影核均超出当前证据边界，需要新 scope。

continue_stop_decision：当前 topic 内 production closeout 已完成，roadmap 和 matrix 没有授权且未阻塞的 high-priority next action。本轮停在 ready_for_review / submit boundary。
