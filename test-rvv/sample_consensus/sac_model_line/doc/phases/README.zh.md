# sac_model_line phase index

## 当前 phase

| phase | 状态 | 入口 | 结果 |
| --- | --- | --- | --- |
| `000-line-count-diagnostic` | completed / partial-production-candidate | `000-line-count-diagnostic/plan.zh.md` | `000-line-count-diagnostic/result.zh.md` |
| `010-line-select-diagnostic` | completed / partial-production-candidate | `010-line-select-diagnostic/plan.zh.md` | `010-line-select-diagnostic/result.zh.md` |
| `020-line-get-distances-diagnostic` | completed / negative diagnostic | `020-line-get-distances-diagnostic/plan.zh.md` | `020-line-get-distances-diagnostic/result.zh.md` |
| `030-line-get-distances-vfsqrt-diagnostic` | completed / partial-production-candidate | `030-line-get-distances-vfsqrt-diagnostic/plan.zh.md` | `030-line-get-distances-vfsqrt-diagnostic/result.zh.md` |
| `040-line-production-integration` | completed / adopted production behavior | `040-line-production-integration/plan.zh.md` | `040-line-production-integration/result.zh.md` |
| `050-line-get-distances-vse64-store` | completed / adopted production behavior | `050-line-get-distances-vse64-store/plan.zh.md` | `050-line-get-distances-vse64-store/result.zh.md` |
| `060-line-select-vse64-compressed-store` | completed / adopted production behavior | `060-line-select-vse64-compressed-store/plan.zh.md` | `060-line-select-vse64-compressed-store/result.zh.md` |
| `070-line-identity-index-strided-load` | completed / rejected with evidence | `070-line-identity-index-strided-load/plan.zh.md` | `070-line-identity-index-strided-load/result.zh.md` |

## 默认恢复入口

默认恢复动作是 ready-for-review after Phase 070 rejection：production patch（生产补丁）已在工作区，`countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel` 的接入后 public Std/RVV board repeated（公开入口标量 / RVV 板卡重复测试）均为 positive-stable。Phase 050 已把 `getDistancesToModelRVV` 改成 `vfwcvt + vse64` direct double store（直接 double 写回）；Phase 060 已把 `selectWithinDistanceRVV` 的 compressed error double store（压缩误差 double 写回）也改成 `vfwcvt + vse64`。Phase 070 尝试 identity-index strided load（恒等索引跨步加载）后，strict RVV-vs-RVV A/B（同一生产边界内 RVV 实现族严格对比）没有满足三入口采纳条件，候选已拒绝并回退到 Phase 060 gather-only load family（只使用离散加载的实现族）。当前同边界性能搜索暂无新的 high-priority unblocked candidate（高优先级未阻塞候选）。

## 文档归属

阶段事实、doctor 结果和 matrix 更新归属本目录；函数级取舍归属 `../sac_model_line-evaluation.zh.md`；跨阶段候选归属 `../optimization-roadmap.zh.md`。production 长期主题文档是 `doc-rvv/sample_consensus/sac_model_line-RVV.zh.md`，当前使用 Phase 060 接入后板卡数据。
