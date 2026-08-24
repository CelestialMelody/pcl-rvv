# Phase 030: Average 3D Gradient Production-Shaped Profile Plan

## 阶段意图和边界

本阶段只在 `test-rvv/features/integral_image_normal` 内新增 production-shaped profile（生产形态剖析，测试代码尽量模拟生产阶段边界）和 component ablation（组件消融，拆分各段耗时）证据，不修改 `features/include/pcl/features/impl/integral_image_normal.hpp`。目标是回答 Phase 020 的恢复条件：`initAverage3DGradientMethod()` 中 diff_x / diff_y buffer（差分缓冲区）是否足以主导完整 AVERAGE_3D_GRADIENT 初始化和查询链路。

本阶段不证明 production direct（真实生产入口证据），不覆盖泛型 `PointInT` traits（点类型字段特征）、`Scalar=double`、indices / correspondences，且不把 component-only 结果写成 production 采纳结论。

## 当前状态清单

| area | current state | evidence |
| --- | --- | --- |
| map-prep diagnostic | stable positive，仍是 partial-production-candidate | `000-current-state-and-map-prep-diagnostic/result.zh.md`、`log/board/repeated-summary.md` |
| PI1 map-prep production plan | 已写计划，但 PI2 修改 production 仍需用户显式确认 | `010-pi1-production-integration-plan/plan.zh.md` |
| diff-buffer diagnostic | `641x481_tail` 正向，`320x240` near-threshold 且 1/5 退化 | `020-average-3d-gradient-diff-buffer-diagnostic/result.zh.md` |
| evidence registry | 当前 Phase 020 证据 fresh | `make evidence_status` |

## 假设与候选族

| hypothesis | how this phase tests it | expected decision impact |
| --- | --- | --- |
| diff-buffer 只占完整链路小比例 | 新增 profile case，同时输出 diff、integral image construction（积分图构建）和 query loop（查询循环）计时 | 若成立，Phase 020 的 no-production-now 维持；后续不应单独生产化 diff-buffer。 |
| integral image construction 或 query loop 主导 | 同一输入下输出组件耗时占比和 Std/RVV total speedup | 若成立，roadmap 优先转向积分图构建或 normal query 消融。 |
| 大图 tail 的 diff-buffer 正向会被完整链路稀释 | 新增 `avg3d_profile_320x240` 和 `avg3d_profile_641x481_tail` | 若 total speedup 接近 1，即使 diff-buffer 局部正向也不支持生产探针。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback | bench / board | asm | doctor | decision before phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| average 3D gradient production-shaped profile | ordered-organized-image | `XYZPadPoint`, float, 4-float stride | 依赖 Phase 020 diff-buffer correctness；本阶段 checksum 同边界对比 | 新增 board repeated profile cases | profile binary 应保留 Phase 020 RVV diff 指令 | manifest / Evidence Doctor 扩展 | planned diagnostic |

## 实现和测试动作

| action | artifact / command | completion criteria |
| --- | --- | --- |
| 扩展 bench profile harness | `src/bench_integral_image_normal.cpp` | 输出 `avg3d_profile_*` total 和 `profile_component_*` 组件计时 / checksum。 |
| 扩展 manifest wrapper | `script/generate_integral_image_normal_evidence_manifest.py` | 将 profile total 作为 strict A/B comparison，将 component rows 写入 metadata 或独立 diagnostic group。 |
| 更新 Makefile evidence refs | `Makefile` | repeated label、case-filter、doc-ref 包含 Phase 030；registry 可记录新 summary。 |
| QEMU correctness / log-shape | `make run_test_compare`，bench binary build | gtest 仍通过；bench 能编译，QEMU 不作为性能结论。 |
| asm check | `make dump_test_rvv` 或 `dump_bench_rvv` + `rg` | RVV diff-buffer 指令仍存在；profile 自身不是 production symbol。 |
| board repeated | `make run_board_integral_image_normal_repeated` | 5-run 完成，summary / manifest / doctor / registry 刷新。 |
| docs | 本 result、roadmap、matrix、evaluation、bench/evidence 文档、Handoff | 写清 component-only 不能替代 production evidence。 |

## Evidence Doctor 和 registry 规则

输入仍使用 `log/board/evidence_manifest.json`，摘要仍写 `log/board/repeated-summary.md`，Evidence Doctor 输出 `log/board/evidence_doctor.md`。若出现 checksum mismatch、strict A/B 元数据缺失或 profile total 与 component 行无法对应，先修脚本并重跑。Warnings 必须解释后才能进入 EvidenceDecision。

registry 由 `record_board_evidence_state` 更新；若新增 summary 后 `make evidence_status` 不是 fresh，本阶段不能关闭。

## 板卡复跑预算和决策桶

默认 5-run repeated board。若 Evidence Doctor 出现 near-threshold、方向反转或 `B/A < 1` 频率异常，最多追加一次同边界复跑；若 decision bucket 仍摇摆，写成 `unstable` 或降级为 diagnostic clue（诊断线索）。本阶段不追求精确耗时完全一致，只判断 total speedup 是否足以改变 Phase 020 的恢复条件。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper / production-shaped helper |
| 当前决策问题 | implementation-shape and bottleneck localization |
| diagnostic 是否可外推到 production | no。它用测试专用点型和自包含 profiling harness，只能帮助判断是否值得申请 production probe。 |
| comparison-boundary / baseline mismatch 风险 | medium。同一 bench wrapper 内 Std/RVV 可比，但完整 production 对象状态、PCL IntegralImage2D 真实实现和 public dispatch 未覆盖。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 profile 显示对应组件主导，且用户授权 PI2 修改 production 时允许。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。当前阶段不会 clean-adopt。 |

## 继续 / 停止条件

继续条件：profile 显示积分图构建、query loop、map-prep 或其它当前 topic-local 候选仍有未阻塞消融动作。

停止条件：继续需要修改 production source、Evidence Doctor Error 无法修复、registry 不 fresh、板卡不可用或数据在复跑预算内仍不稳定。

`next_phase_default`：若 profile total 不支持 diff-buffer production probe，优先把 roadmap 转向 full init / query loop 的下一处瓶颈；若 profile 显示 map-prep production probe 仍是唯一高优先级候选，则回到 Phase 010 的 PI2 用户授权门禁。
