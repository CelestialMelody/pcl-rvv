# fpfh 阶段索引

| phase | 状态 | 默认恢复动作 | 文档 |
| --- | --- | --- | --- |
| `000-current-state-and-caller-shaped-ablation` | done | 已完成；作为 baseline diagnostic（基线诊断）读取。 | `000-current-state-and-caller-shaped-ablation/plan.zh.md`, `000-current-state-and-caller-shaped-ablation/result.zh.md` |
| `001-weighted-spfh-33-diagnostic-candidate` | done | 已完成；test-only dense-row candidate（仅测试 dense row 候选）是 strong diagnostic positive（强诊断正向），但不作为生产采纳证据。 | `001-weighted-spfh-33-diagnostic-candidate/plan.zh.md`, `001-weighted-spfh-33-diagnostic-candidate/result.zh.md` |
| `002-weighted-spfh-33-production-probe` | done | 已完成 PI5 production probe（生产探针）：生产 detail 和 public board evidence 均为正向，当前生产补丁建议保留。 | `002-weighted-spfh-33-production-probe/plan.zh.md`, `002-weighted-spfh-33-production-probe/result.zh.md` |
| `003-spfh-pair-feature-batch-audit` | done | 已完成可行性审计；命中 stop condition（停止条件）：bin-stability 和 histogram scatter 语义未闭合，不建议同轮继续实现。 | `003-spfh-pair-feature-batch-audit/plan.zh.md`, `003-spfh-pair-feature-batch-audit/result.zh.md` |

当前停止状态：`topic_closeout_ready_for_topic_only_commit`。

本 topic 现在保留 `features/include/pcl/features/impl/fpfh.hpp` 中的有界 RVV production patch（生产补丁）：
`weightPointSPFHSignature` 在 `__RVV10__` 下先尝试 11+11+11 bin 的
`pcl::detail::weightFPFHSignature33RVV`，gate（验收条件）不满足时回到原标量路径。
用户已确认当前 topic 可以结束并进入提交流程；Phase 002 的 post-integration board data（接入后板卡数据）
满足采纳口径，正式 `doc-rvv` 文档已创建。本 topic 推荐使用 topic-only commit（只提交本主题相关产物），
不默认提交 raw logs（原始日志）或其它 topic 改动。

默认恢复动作：

1. 提交阶段默认只纳入 FPFH production patch、`test-rvv/features/fpfh` 测试/文档资产、正式
   `doc-rvv/features/fpfh-RVV.zh.md` 和 features 队列表更新。
2. 若继续 `spfh-pair-feature-batch`，先按 Phase 003 result 的恢复条件补 bin-stability（分箱稳定性）和 histogram scatter（直方图离散累加）语义证据；当前不建议直接实现。
3. generic point type（泛型点类型）、`Scalar=double`、OMP FPFH 和非默认 bin layout 仍在扩展队列中，不能由 Phase 002 结论外推关闭。
