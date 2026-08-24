# Phase 000 Plan: Current State And Map-Prep Diagnostic

## 阶段意图和边界

本阶段要证明 `computeFeature()` 中 depth-change map（深度突变图）和 distance-map initialization
（距离图初始化）是否可以由 test-only RVV candidate（仅测试使用的 RVV 候选）保持标量语义。阶段边界：

- 覆盖入口：pre-production diagnostic helper，形状来自 `computeFeature()`。
- 覆盖 row source：ordered organized image（按 width × height 顺序扫描的有组织图像）。
- 覆盖类型：合成连续 `float z` buffer，不声明任意 `PointInT` layout。
- 覆盖规模：小图、tail（尾段）、常规 VGA-like synthetic 尺寸。
- 不覆盖：distance transform 两遍传播、`computeFeatureFull()`、`computeFeaturePart()`、border mirror、indices 子集、production dispatch 和泛型点类型 gate。

## 当前状态清单

| area | current state |
| --- | --- |
| production | 未修改，目标源码仍是纯标量 |
| upstream tests | `test/features/test_ii_normals.cpp` 和 `test_normal_estimation.cpp` 有既有覆盖 |
| topic test dir | 本阶段新建 `test-rvv/features/integral_image_normal` |
| board availability | 用户说明板卡可用；`test-rvv/config.mk` 已有 `REMOTE_*` 配置入口 |
| evidence registry | 当前 topic 尚无 registry，Phase 000 先人工记录 freshness |

## 假设与候选族

map-prep RVV candidate 使用 VL chunk（可变向量长度分块）处理每行 `width - 1` 个像素。右向和下向 depth edge
各产生 mask（掩码），用 masked byte store（掩码字节存储）把当前、右邻和下邻 map lane 置 0。多次写 0 是幂等的，
应保持标量语义。distance initialization 用 byte mask 选择 0.0f 或 `width + height`。

## 优化矩阵

见 `../optimization-matrix.zh.md`。本阶段只允许把 `map-prep RVV masked stores` 从 `planned`
推进到 `attempted`、`rejected`、`deferred` 或 `partial-production-candidate`。

## 实现和测试动作

| action | artifact / command | expected evidence | done criterion |
| --- | --- | --- | --- |
| RED correctness tests | `src/test_integral_image_normal.cpp` | 测试在 helper 缺失时编译失败 | failure is expected missing candidate API |
| candidate helper | `include/integral_image_normal.h`, `include/impl/integral_image_normal_map_prep.hpp` | Std and RVV builds share public test helper API | `run_test_compare` passes |
| bench wrapper | `src/bench_integral_image_normal.cpp` | 输出 case、iterations、checksum、ms | QEMU only as log-shape smoke; board for performance |
| asm check | `make dump_test_rvv` | RVV asm contains vector loads/stores or mask ops | asm path recorded |
| board smoke / bench | `make board_smoke` | board test and bench logs | only if local correctness passes |

## Evidence Doctor 和 Registry 规则

本阶段如果生成 board compare summary，必须运行或人工填写 Evidence Doctor（证据体检）结果。由于 topic
尚无 manifest wrapper，Phase 000 可先使用 summary-only 模式并把 `metadata_incomplete` 写入 result。
raw logs 默认 local-only，不进入提交边界。

## 板卡复跑预算和决策桶

默认先执行一次 `board_smoke`。若 summary 接近阈值或 Evidence Doctor 出现方向性 warning，最多追加一次同边界复跑。
决策桶：

- `positive`：主要 case 稳定大于 1.20x，checksum 一致，asm 归属闭合。
- `weak_positive`：1.05x 到 1.20x，且实现小、风险低。
- `neutral`：0.95x 到 1.05x。
- `negative`：低于 0.95x。
- `unstable`：复跑跨桶或 checksum / metadata 矛盾。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar for map-prep candidate |
| diagnostic 是否可外推到 production | unknown；helper 只覆盖连续 z buffer，不覆盖真实 `PointInT` 字段布局和后续 normal 输出 |
| comparison-boundary / baseline mismatch 风险 | yes；diagnostic 不含 distance transform、normal solver 和 output writes |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no；先完成 map-prep bench 和 production dataflow audit |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no for first candidate；若后续已有 adopted family，再补 family comparison |

## 继续 / 停止条件

默认继续到 RED test、candidate implementation、QEMU correctness、asm 和 board smoke。合法停止条件只有：
测试或工具链不可用、板卡不可达、Evidence Doctor Error 未解决、dirty isolation 不安全，或证据显示该候选不建议继续。

## 文档更新清单

阶段结束时更新 `result.zh.md`、optimization matrix、roadmap、evaluation 和 Handoff。没有 adopted production behavior
前不创建 `doc-rvv/features/integral_image_normal-RVV.zh.md`。
