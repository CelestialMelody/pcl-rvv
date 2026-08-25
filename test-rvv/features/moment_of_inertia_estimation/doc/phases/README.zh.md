# Moment of inertia estimation RVV phase index

本目录记录 `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` 的 RVV（RISC-V Vector，可变长度向量）优化阶段。阶段文档属于 topic-local test asset（主题本地测试资产），不代表 production（生产源码）已经接入。

| phase | status | scope | plan | result |
| --- | --- | --- | --- | --- |
| 000-current-state-and-reduction-diagnostic | completed | 建立函数级评估、测试脚手架、ordered indexed cloud（按 `indices_` 顺序遍历的点云）规约 diagnostic（诊断）候选和第一轮 QEMU/asm/board 证据 | `000-current-state-and-reduction-diagnostic/plan.zh.md` | `000-current-state-and-reduction-diagnostic/result.zh.md` |
| 010-projected-covariance-fusion-diagnostic | completed | 验证 `getProjectedCloud()` + projected covariance 是否可融合为不写临时点云的 RVV diagnostic helper | `010-projected-covariance-fusion-diagnostic/plan.zh.md` | `010-projected-covariance-fusion-diagnostic/result.zh.md` |
| 020-pi1-production-integration-plan | completed | 只写 PI1 production integration plan（生产接入计划），冻结 PI2 前的范围、fallback 和证据计划 | `020-pi1-production-integration-plan/plan.zh.md` | `020-pi1-production-integration-plan/result.zh.md` |
| 030-production-mean-aabb-pi2-pi5 | rolled-back | 接入 mean/AABB-only production probe（只接入质心和轴对齐包围盒的生产探针），public compute 证据为 neutral，已按用户确认回滚 | `030-production-mean-aabb-pi2-pi5/plan.zh.md` | `030-production-mean-aabb-pi2-pi5/result.zh.md` |
| 040-projected-covariance-production-probe | adopted | 接入 projected covariance fusion（投影协方差融合）production path，完成 public compute 板卡 repeated、Evidence Doctor 和正式 `doc-rvv` | `040-projected-covariance-production-probe/plan.zh.md` | `040-projected-covariance-production-probe/result.zh.md` |
| 050-point-type-expansion | completed | 继续补常见 `PointXYZ`-like typed scope 的 production-public 证据，验证同一 helper 在 PointXYZI / RGB / RGBA / RGBNormal 上仍保持正向收益 | `050-point-type-expansion/plan.zh.md` | `050-point-type-expansion/result.zh.md` |

默认恢复动作：从 `050-point-type-expansion/result.zh.md` 恢复。当前 production-public evidence（真实公开入口证据）为 `positive`，已采纳 projected covariance production patch；phase050 已把常见 PointXYZ-like typed scope 也验证为 positive。若后续还要继续，只能打开新的 custom point type / layout phase，不再默认延伸当前 phase。
