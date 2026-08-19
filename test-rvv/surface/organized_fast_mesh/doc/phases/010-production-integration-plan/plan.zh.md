# Phase 010 Plan

## 阶段意图和边界

本阶段进入 PI1 production integration plan（生产接入计划），并在 gate 可控时继续 PI2-PI5。
生产接入候选只覆盖：

- `PointInT == pcl::PointXYZ`
- `triangle_pixel_size_rows_ == 1`
- `triangle_pixel_size_columns_ == 1`
- `store_shadowed_faces_ == true`
- organized cloud，work item count 足够大

不覆盖：

- `PointXYZRGB` / `PointXYZRGBA` / 用户自定义点类型；
- shadow edge 检查，即 `store_shadowed_faces_ == false`；
- 非 1 像素 step；
- 泛型 PointXYZ-like traits；
- `Scalar=double`，本类当前固定读取 float point fields。

## PI1 gate

| gate | decision | reason |
| --- | --- | --- |
| public API | keep | 不新增公开 API。 |
| exact point type | phase-local exception | 当前诊断只证明 `PointXYZ`；其它点型 fallback。 |
| fallback | required | 不满足 gate 时调用原标量实现。 |
| dispatch | allowed | 在 `make*Mesh` 入口顶部尝试 RVV，成功则 return。 |
| helper layout | internal namespace | header-only 模板，避免扩大 class protected API。 |
| doc-rvv | not_applicable yet | PI5 前不创建长期 production 文档。 |

## 实现动作

1. 在 `surface/include/pcl/surface/impl/organized_fast_mesh.hpp` 增加 `__RVV10__` helper。
2. 为 `makeQuadMesh` / `makeRightCutMesh` / `makeLeftCutMesh` / `makeAdaptiveCutMesh` 增加窄 gate 分流。
3. 在 test-rvv 增加 production-direct 测试和 bench case，确保真实 public entry 命中。
4. 跑 QEMU correctness、production bench smoke、反汇编、板卡 smoke 和 Evidence Doctor。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic -> production-public |
| A/B boundary | test helper -> public overload / production helper |
| 当前决策问题 | RVV-vs-scalar and fallback correctness |
| diagnostic 是否可外推到 production | only for scoped gate；生产补丁必须重新跑 public entry 证据 |
| comparison-boundary / baseline mismatch 风险 | yes；test helper 不等于真实 `OrganizedFastMesh` 对象状态 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes；仅限上述 gate，PI5 后停等用户确认 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | no；当前只有一个 candidate family，后续泛型 / shadow 扩展另开 phase |

## 板卡复跑预算

- 本阶段先做单次 production-direct board smoke。
- 若 production direct 仍正向，PI5 停在用户确认点，不自动采纳。
- 若结果方向与诊断相反或 Evidence Doctor 有 Error，停在 pending rollback / user judgment。
