# Phase 000: 当前状态与组件消融结果

## 执行范围

本阶段按 `plan.zh.md` 完成 test-only component ablation（测试专用组件消融）：`plane_d_dot_rvv`、`boundary_gather_rvv` 和 `viewpoint_projection_rvv`。未修改 production（生产源码），未声明 production direct（真实生产路径证据）。

## 动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| A1 RED test | done | `make ... run_test_rvv` 首轮因 RVV helper 缺失失败 | RED 失败点符合预期 |
| A2 Std/RVV helpers | done | `make -C test-rvv/segmentation/organized_multi_plane_segmentation run_test_compare` | Std/RVV 3 个组件 correctness（正确性）通过 |
| A3 component bench | done | `src/bench_omps.cpp`；`run_bench_rvv` 小规模 QEMU smoke | QEMU 只证明日志形状，不用于性能结论 |
| A4 反汇编归属 | done | `make -C test-rvv/segmentation/organized_multi_plane_segmentation dump_bench_rvv` | `computePlaneDValuesRVV`、`gatherBoundaryCloudRVV`、`projectBoundaryFromViewpointRVV` 有 RVV 指令归属 |
| A5 板卡 repeated bench | done | `log/board/repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | 板卡 5-run 完成 |
| A6 文档回填 | done | 本 result、optimization matrix、roadmap、evaluation | Phase 000 关闭到诊断边界 |

## 板卡结果

| case | 证据角色 | median speedup | values | decision bucket | 处理 |
| --- | --- | ---: | --- | --- | --- |
| `plane_d_dot` | diagnostic | 0.81x | 0.81x, 0.85x, 0.76x, 0.81x, 0.77x | negative | rejected：5/5 退化，不继续 production-shaped phase |
| `boundary_gather` | diagnostic | 1.03x | 1.03x, 1.01x, 1.16x, 1.02x, 1.25x | neutral | attempted：近阈值且长尾，不能作为接入依据 |
| `projection` | diagnostic | 1.34x | 1.34x, 1.31x, 1.29x, 1.35x, 1.38x | positive | upgraded：进入 Phase 010 生产形态诊断 |

## Evidence Doctor 处理

`log/board/repeated/evidence_doctor.md` 输出 `Errors=1，Warnings=3，Suggestions=1`。Error 是 `plane_d_dot` 的 `ba_degradation_frequency`，处理方式不是修 correctness，而是把该候选在当前诊断边界降级为 rejected，不作为 production evidence（生产证据）。`boundary_gather` 的长尾和 near-threshold suggestion 已在 Phase 010 中通过 `region_gather_only` 生产形态诊断继续拆分。

## 诊断证据链

Correctness 来自 `run_test_compare`，板卡性能来自 `log/board/repeated/summary.md`，反汇编来自 `build/asm/riscv/bench_omps_rvv.asm`。这些证据只覆盖测试专用 helper，不包含真实 `segment` / `segmentAndRefine` public entry（公开入口）、CCL、region fitting、refine、`PlanarRegion` 构造和 production dispatch。

## Diagnostic 到 production 错配审计回填

| question | answer |
| --- | --- |
| evidence role | diagnostic component ablation |
| A/B boundary | test helper，Std/RVV 只替换局部 helper |
| 当前决策问题 | 哪些局部片段值得升级到 production-shaped diagnostic |
| diagnostic 是否可外推到 production | 不能直接外推；`projection` 只能升级为下一阶段候选 |
| comparison-boundary / baseline mismatch 风险 | 存在，完整 public entry 成本会被 CCL、fitting 和输出组织稀释 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `plane_d_dot` 不允许；`boundary_gather` 仅允许作为 Phase 010 拆分项；`projection` 允许进入 Phase 010 |
| clean adoption 是否需要 production boundary 内 A/B | 需要，本阶段不支持 clean adoption |

## Continue / Stop Decision

Phase 000 完成，`projection` 的局部 positive 触发 Phase 010。Phase 000 自身不建议 production 接入；下一阶段必须用 production-shaped diagnostic 验证收益是否仍可见。
