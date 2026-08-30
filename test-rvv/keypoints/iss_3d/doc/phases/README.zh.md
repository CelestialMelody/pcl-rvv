# ISS 3D RVV Phase Index

## 当前恢复入口

默认从 `010-production-integration/result.zh.md` 恢复。Phase 000 已证明
`ISSKeypoint3D::getScatterMatrix` 中的 scatter matrix（散布矩阵）局部核在
diagnostic/component ablation（诊断 / 组件消融）边界为 positive；Phase 010 证明
同一优化进入真实 public `compute()` 后只有 neutral 弱正向，不建议采纳为长期 production 行为。

## 阶段列表

| phase | 状态 | plan | result | 默认下一步 |
| --- | --- | --- | --- | --- |
| 000-current-state-and-scatter-diagnostic | completed_positive | `000-current-state-and-scatter-diagnostic/plan.zh.md` | `000-current-state-and-scatter-diagnostic/result.zh.md` | 已进入 Phase 010。 |
| 010-production-integration | completed_neutral_no_production | `010-production-integration/plan.zh.md` | `010-production-integration/result.zh.md` | production patch 已移除；当前进入 no-production closeout 和提交候选。 |

## 文档归属

- phase plan / result：保存阶段计划、实际命令、证据解释、继续 / 停止判断。
- `optimization-matrix.zh.md`：保存候选 family、row source、点型、测试、板卡、反汇编和决策状态。
- `../optimization-roadmap.zh.md`：保存跨阶段可继续搜索空间和默认恢复队列。
- topic-local evaluation 和 doc suite：当前已补齐；production 长期 `doc-rvv` 由于未采纳判为 not_applicable。
