# approximate_progressive_morphological_filter 阶段索引

## 当前默认恢复入口

当前 topic 暂停在 `050-tail-vector-filter-probe` 之后。`030` 完成 production patch（生产补丁）和 `PointXYZ` production public board（真实公开入口板卡）证据；`040` 已把接入后板卡收益扩展到 `PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA`；`050` 尝试进一步向量化 tail 阶段剩余比较 / 输出压缩，但相对 040 adopted RVV baseline（已采纳 RVV 基线）整体为 neutral（中性），已回退。

当前 production 结论：保留 040 已采纳实现。提交 commit 仍需用户单独授权。

## 阶段列表

| phase | status | plan | result | next action |
| --- | --- | --- | --- | --- |
| `000-current-state-and-component-plan` | complete | `doc/phases/000-current-state-and-component-plan/plan.zh.md` | `doc/phases/000-current-state-and-component-plan/result.zh.md` | 进入完整链路生产形态诊断 |
| `010-production-shaped-full-diagnostic` | complete | `doc/phases/010-production-shaped-full-diagnostic/plan.zh.md` | `doc/phases/010-production-shaped-full-diagnostic/result.zh.md` | 进入 PI1 生产接入计划 |
| `020-production-integration-plan` | superseded-by-030 | `doc/phases/020-production-integration-plan/plan.zh.md` | `doc/phases/020-production-integration-plan/result.zh.md` | 用户后续授权已解除 PI2 前置 blocker |
| `030-production-patch-and-direct-evidence` | adopted | `doc/phases/030-production-patch-and-direct-evidence/plan.zh.md` | `doc/phases/030-production-patch-and-direct-evidence/result.zh.md` | 进入 040 点型扩展 |
| `040-point-type-production-expansion` | adopted within measured point types | `doc/phases/040-point-type-production-expansion/plan.zh.md` | `doc/phases/040-point-type-production-expansion/result.zh.md` | 进入 050 tail 实现族探针 |
| `050-tail-vector-filter-probe` | rejected and reverted | `doc/phases/050-tail-vector-filter-probe/plan.zh.md` | `doc/phases/050-tail-vector-filter-probe/result.zh.md` | 当前无值得自动推进的生产优化方向；暂停等待审查 / 用户选择 |

## 文档归属

phase plan/result 保存阶段探索和继续 / 停止判断；`doc/optimization-roadmap.zh.md` 保存跨阶段候选搜索空间；`doc/approximate_progressive_morphological_filter-evaluation.zh.md` 保存函数级评估、诊断证据链和 adopted production（已采用生产）证据链。`doc-rvv/segmentation/approximate_progressive_morphological_filter-RVV.zh.md` 保存当前已采用 production 行为、fallback 和接入后的板卡证据。
