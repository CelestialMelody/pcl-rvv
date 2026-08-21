# OpenNI2 Grabber RVV Phase Index

本文是 `test-rvv/io/openni2_grabber` 的阶段恢复入口。当前 topic 来自 `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` 中 `src/openni2_grabber.cpp` 的 OpenNI frame-to-cloud（帧到点云）队列项。

| phase | status | plan | result | 默认恢复动作 |
| --- | --- | --- | --- | --- |
| 000-current-state-and-diagnostic-scaffold | completed | `000-current-state-and-diagnostic-scaffold/plan.zh.md` | `000-current-state-and-diagnostic-scaffold/result.zh.md` | 已关闭 production-shaped diagnostic；`PointXYZ` depth path 为 partial production candidate。 |
| 010-production-integration-plan | completed / blocked_on_production_authorization | `010-production-integration-plan/plan.zh.md` | `010-production-integration-plan/result.zh.md` | 需要用户明确授权修改 production 源码后，才能进入 PI2 production patch。 |
| 020-production-depth-connection | adopted / completed | `020-production-depth-connection/plan.zh.md` | `020-production-depth-connection/result.zh.md` | production detail helper 已接入并完成板卡重测；用户已确认有收益即可采纳。 |
| 030-rgb-point-type-diagnostic | completed / diagnostic-only | `030-rgb-point-type-diagnostic/plan.zh.md` | `030-rgb-point-type-diagnostic/result.zh.md` | 已补 `PointXYZRGB` / `PointXYZRGBA` 泛型点型诊断 correctness；不扩大 production scope。 |

## 文档归属

阶段计划和阶段结果归属在 `doc/phases/`；跨阶段候选归属在 `doc/optimization-roadmap.zh.md`；函数级评估归属在 `doc/openni2_grabber-evaluation.zh.md`。Phase 020 进入 production integration loop（生产接入闭环）并获得用户采纳确认后，长期主题文档归属到 `doc-rvv/io/openni2_grabber-RVV.zh.md`；该文档当前记录 adopted production behavior（已采纳生产行为）状态。
