# Phase 010 Plan: PI1 Production Integration Plan

## 阶段意图和边界

本阶段只形成 PI1 production integration plan（生产接入计划），不修改 `features/include/pcl/features/impl/integral_image_normal.hpp`。Phase 000 已证明 map-prep diagnostic helper 在板卡上稳定正向；PI1 要回答它能否作为有界 production probe（生产探针）进入 PI2-PI5。

## 已读取的生产接入规则

| source | 对本阶段的约束 |
| --- | --- |
| `.agents/skills/rvv-implementation/SKILL.md` | 公开入口不改 API；production 接入后应有清晰的 Std fallback 和 RVV short-circuit。 |
| `.agents/skills/rvv-implementation/references/implementation-patterns.md` | 原标量主体不能被藏在同一个公开入口后半段；需要命名清楚的 `*_Std` / `*_RVV` 或等价 helper。 |
| `.agents/skills/rvv-implementation/references/fallback-and-dispatch.md` | gate（准入条件）需要覆盖非 RVV 构建、小规模、非 dense、indexed / subset、布局和语义风险。 |
| `.agents/skills/rvv-implementation/references/point-load-store.md` | 标准 xyz 字段优先复用公共 RVV load/store 封装；字段 offset 和 stride 不能硬编码具体点型。 |
| `doc-rvv/rvv/RVV Generic Point Type Strategy.zh.md` | 模板点类型必须用 traits / POD / standard-layout / offset 证明；连续 z-buffer 诊断不能外推为泛型 `PointInT` production。 |

## 候选 production 范围

| dimension | planned scope |
| --- | --- |
| production entry | `IntegralImageNormalEstimation<PointInT, PointOutT>::computeFeature(PointCloudOut&)` 中 depth-change map 和 distance-map initialization 前缀。 |
| point type | 初始生产探针建议使用 traits-gated z field（按字段特征门控的 z 字段）或先收窄到已证明的 xyz-like float AoS 点型；其它模板实例 fallback。 |
| `Scalar` | 只覆盖 `float` depth value；不声明 `double` 或非 float z 字段。 |
| layout | organized cloud 的 `input_->points` 连续 AoS（结构数组）布局；按 `width * height` 访问相邻右侧和下侧点。 |
| row source | full organized image。`indices_` 或 part 输出路径不属于 map-prep 输入，但后续 output path 仍保持标量。 |
| timer boundary | production direct bench 必须区分 map-prep-only diagnostic、完整 `computeFeature()` 和 public caller wrapper；不能用 Phase 000 helper timing 直接替代。 |

## 不接入范围

- distance transform 两遍传播保持标量，因为行内依赖未被证明可安全改写。
- `computeFeatureFull()`、`computeFeaturePart()`、border mirror、normal solver、curvature 和 viewpoint flip 保持标量。
- `indices_` 输出路径、泛型 `PointOutT` 写回和 `Scalar=double` 不在本 PI1 候选范围内。
- 若 traits / layout gate 不能在当前 production 代码中保持可维护，PI2 应停下并保留 Phase 000 为 diagnostic positive，而不是强行接入。

## Dispatch / Fallback 草案

| gate | planned behavior | required evidence |
| --- | --- | --- |
| non-RVV build | 不编译 RVV helper，自然走 Std fallback。 | non-RVV build / test。 |
| size gate | 小于一个 VL chunk 或 `width < 2 || height < 2` 走 Std fallback。 | fallback correctness case。 |
| point layout gate | z 字段必须是单个 `float`，POD / standard-layout / stride / offset 满足 RVV load 前提。 | traits compile tests 或 focused production direct tests。 |
| dense / finite semantics | `z` 的 NaN / Inf 检查必须保持标量语义。 | production direct correctness 和边界输入。 |
| distance transform / output stages | RVV 只写 map-prep intermediate；后续 stages 继续调用 Std continuation。 | same-chain correctness（同构链路正确性）和 public test。 |

## PI2-PI5 前置命令计划

| phase | command / artifact | completion criterion |
| --- | --- | --- |
| PI2 production patch | 修改 `features/include/pcl/features/impl/integral_image_normal.hpp`，抽出 Std/RVV helper 或等价 continuation。 | diff 只触碰目标 production 文件和必要 test assets。 |
| PI3 production direct correctness | 新增/扩展 topic-local test，使公开入口命中 RVV map-prep 后仍和 Std 输出一致。 | QEMU Std/RVV correctness pass，fallback gates 单独覆盖。 |
| PI4 asm + board | `dump_test_rvv` 或 production symbol dump；board repeated production bench。 | RVV 指令归因到 production helper；board 5-run summary + Evidence Doctor。 |
| PI5 decision | 写 PI result、matrix、evaluation 和 Handoff。 | 若证据支持采纳，暂停等待用户确认保留；若证据不支持，暂停等待用户确认是否回滚。 |

## 停止条件

本阶段写完 PI1 计划后暂停在 production 修改前。继续到 PI2 会修改 `features/include/pcl/features/impl/integral_image_normal.hpp`，根据当前 workflow 需要用户明确确认 production patch 范围。
