# sac_model_circle3d 函数级评估

## 范围和目标源码

本 topic 覆盖 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` 中 `SampleConsensusModelCircle3D<PointT>` 的三条逐点距离入口：

| 函数 | 标量职责 | 当前 RVV 状态 |
| --- | --- | --- |
| `getDistancesToModel` | 遍历 `indices_`，把点投影到圆平面，再计算点到圆周最近点的距离并写入 `std::vector<double>`。 | 未接 production；Phase 000 不覆盖。 |
| `selectWithinDistance` | 使用同一投影核，按平方距离阈值筛选 inliers，并写 `error_sqr_dists_`。 | 生产补丁已回滚；当前源码恢复为标量路径，production gate 不再保留。 |
| `countWithinDistance` | 使用同一投影核，只统计小于阈值的点数。 | Phase 000 test-only candidate 负向，不建议按当前 code shape 接 production。 |

## 标量路径重建

每个点先读取 `x/y/z`，构造 `P - C`。标量路径用 `lambda = -(P-C).dot(N) / N.dot(N)` 把点投影到圆所在平面，然后对 `P_proj - C` 做 `normalized()`，得到圆周点 `K = C + r * normalize(P_proj-C)`。`selectWithinDistance` 和 `countWithinDistance` 使用 `|P-K|^2 < threshold^2`，`getDistancesToModel` 写 `|P-K|`。

当前源码中 `getDistancesToModel` 的 `lambda` 符号写法与 count/select 不同。本阶段先把 count/select 作为同构链路（same-chain，按同一公式顺序对拍）诊断对象；`getDistancesToModel` 等 count/select 的投影核和退化点边界清楚后再评估 full-RVV（完整 RVV）sqrt / double store。

## 初步判断

| 问题 | 当前结论 |
| --- | --- |
| 是否值得继续 | 值得做 component ablation。circle2d、line、stick 证明 gather、mask / `vcompress` 和 full-RVV sqrt/store 可行，但 circle3d 的投影、除法和 normalize 是新边界。 |
| production 接入 | `selectWithinDistance` 的接入后证据为 negative / unstable，当前不建议采纳；`countWithinDistance` 当前不建议接入。 |
| 首阶段证据 | correctness、QEMU 路径、asm attribution（反汇编归属）、board component bench 和 Evidence Doctor。 |
| 主要风险 | double 标量语义与 RVV float 近似差异、投影点接近圆心的 normalize 退化、`N.dot(N)` 除法成本、`getDistancesToModel` 的公式符号差异。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `selectWithinDistance` | production public entry | 公开入口标量路径，作为 Phase 000 参考链路。 | correctness reference（正确性参考） | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` |
| `countWithinDistance` | production public entry | 公开入口标量路径，作为 Phase 000 参考链路。 | correctness reference | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` |
| `SampleConsensusModelCircle3DAccess` | test-only diagnostic | 暴露测试候选入口，不修改 production。 | component ablation / diagnostic | `test-rvv/sample_consensus/sac_model_circle3d/include/impl/sac_model_circle3d_candidates.hpp` |
| `test_sac_model_circle3d` | correctness test | 对拍 public path 和 projection candidate。 | correctness gate（正确性验收） | `test-rvv/sample_consensus/sac_model_circle3d/src/test_sac_model_circle3d.cpp` |
| Phase 000 plan | phase plan | 冻结首阶段范围、证据和停止条件。 | recovery pointer（恢复入口） | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/plan.zh.md` |
| Phase 000 result | phase result | 保存 current EvidenceDecision、board summary、doctor finding 和 doc suite audit。 | diagnostic closeout（诊断收尾） | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/result.zh.md` |
| projection repeated evidence manifest | evidence output summary | 记录有 warm-up 的 5-run board B/A、checksum 和 asm metadata。 | component ablation board summary | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-manifest.json` |
| projection Evidence Doctor | evidence output summary | 暴露 count 退化 Error，并确认 select 无 finding。 | EvidenceDecision gate | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-doctor.md` |

## 诊断证据链

Phase 000 correctness 证明 test-only candidate 与 public count/select 在当前样本和误差预算内一致；QEMU 只证明正确性和日志形状；反汇编证明 RVV 指令归属到 test-only helper；板卡性能只来自有 warm-up 的 5-run repeated board summary。

当前 EvidenceDecision（证据决策）：

- `countWithinDistance`：`rejected with evidence`。B/A mean 0.5713、median 0.5713、5/5 退化，Evidence Doctor 报 `ba_degradation_frequency` Error。
- `selectWithinDistance`：Phase 000 的 test-only candidate 为 `partial-production-candidate gate`，B/A mean 1.1294、median 1.1279、5/5 正向，Evidence Doctor 对 select 无 finding。

该诊断证据不能替代 production direct（真实生产路径证据）。后续 Phase 010 已按用户授权接入 production 并重跑 post-integration board：`PointXYZ` 10-run B/A mean 0.9748、median 0.9991、5/10 退化，Evidence Doctor 为 Errors=1、Warnings=1、Suggestions=0。Phase 020 还显示 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 三个扩展点型均负向。当前结论是 `rollback/no-production`；不创建 `doc-rvv/sample_consensus/sac_model_circle3d-RVV.zh.md`，因为没有 adopted production behavior（已采用生产行为）。
