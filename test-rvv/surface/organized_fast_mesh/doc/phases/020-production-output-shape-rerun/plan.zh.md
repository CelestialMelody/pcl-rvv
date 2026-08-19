# Phase 020：production output shape rerun

## 阶段意图和边界

本阶段验证上一轮 production direct（生产直连）负向结果的主要归因之一：RVV helper 的 polygon 输出写回形态是否偏离原标量实现。范围仍冻结为 `PointXYZ`、`float`、organized cloud、`triangle_pixel_size=1`、`storeShadowedFaces(true)`、`TRIANGLE_ADAPTIVE_CUT` 公开入口。

本阶段不扩大到泛型点类型、`PointXYZRGB`、shadow edge（阴影边）检查、非 1 步长、indices/correspondences 或其它 triangulation type 的 RVV 接入。

## 当前状态清单

| item | status |
| --- | --- |
| production patch | 已接入 adaptive-cut RVV gate，但 board public path 为负向 |
| correctness | QEMU / board public path correctness 曾通过，checksum 一致 |
| board evidence | `log/board/analyze_bench_compare.log` 显示 adaptive-cut `0.89x`，right/left 约 `0.99x`，quad 约 `1.00x` |
| Evidence Doctor | `log/board/evidence_doctor.md` 为 Errors=3，主要是 public right/left/adaptive 退化频率 |
| suspected root cause | RVV helper 使用 `push_back`，原标量路径使用预分配 `resize` + `idx` 原地写回 |

## 假设与候选族

| candidate family | hypothesis | risk |
| --- | --- | --- |
| production-output-shape | 恢复原标量的预分配 + `idx` 写回模型，减少 RVV helper 额外容器成本 | 若仍负向，说明 finite/adaptive RVV 片段太小或 staging 内存流量过高 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production-output-shape | ordered-cloud-pair | `PointXYZ` / `float` / organized | `OrganizedFastMesh::reconstruct` adaptive-cut | `run_test_compare` | public-path bench smoke | pending | existing RVV asm dump + refreshed if needed | pending | tentative |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public |
| A/B boundary | public overload |
| 当前决策问题 | implementation-shape and RVV-vs-scalar |
| diagnostic 是否可外推到 production | no；本阶段只接受 public path 的 correctness 和 board timing |
| comparison-boundary / baseline mismatch 风险 | low；Std/RVV 都走同一个公开入口，差异来自 `__RVV10__` gate |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，本阶段 probe 已限于同构输出修正；若仍负向则停止生产接入探索 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | no；当前不是选择多个 RVV family，而是验证当前 family 是否能超过标量 |

## 板卡复跑预算和决策桶

默认先跑 QEMU correctness 和窄 QEMU bench smoke（只看日志形状，不作为性能结论）。板卡可用时跑一次 `board_smoke`，每个 case 使用当前 bench 默认 20 次 iteration、5 次 warmup。若 adaptive-cut < `1.03x` 或 Evidence Doctor 仍有退化 Errors，本阶段判为 `negative` 或 `neutral`，不继续微调 production patch。

## 文档更新清单

本阶段更新 `organized_fast_mesh-evaluation.zh.md`、`optimization-roadmap.zh.md`、phase index 和本阶段 result，记录 production direct 负向原因和是否还建议继续该 topic。
