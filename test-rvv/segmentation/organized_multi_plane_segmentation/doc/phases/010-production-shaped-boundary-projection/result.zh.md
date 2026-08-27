# Phase 010: 生产形态 boundary/projection 诊断结果

## 执行范围

本阶段新增 production-shaped diagnostic（生产形态诊断）helper：按多个 region 的 boundary indices（边界索引）复制 boundary cloud，并在 `project_points=true` 时调用 viewpoint projection（视点投影）。它模拟 `segment` / `segmentAndRefine` 返回 `PlanarRegion` 前的 boundary 输出末段，但不修改 production 源码，也不覆盖 CCL、refine、region fitting 或真实 public entry。

## 动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| A1 RED test | done | `make ... clean_test_rvv run_test_rvv` | 因 `RegionBoundaryInput` / `assembleRegionBoundaries*` 缺失失败，失败点符合预期 |
| A2 GREEN helper | done | `include/impl/omps_components.hpp` | 新增 `assembleRegionBoundariesStd/RVV`，单测对拍通过 |
| A3 bench 扩展 | done | `src/bench_omps.cpp` | 新增 `region_projected`、`region_gather_only` case-filter |
| A4 QEMU / asm | done | `run_bench_rvv BENCH_ARGS='--size 1024 --iterations 1 --warmup 0 --case-filter region_projected'`；`dump_bench_rvv` | QEMU 只证明日志形状；反汇编显示 `assembleRegionBoundariesRVV` 调用 RVV gather/projection helper |
| A5 board repeated | done | `log/board/phase010-region_projected/*`、`log/board/phase010-region_gather_only/*` | 两个生产形态 case 均稳定退化 |
| A6 文档刷新 | done | 本 result、matrix、roadmap、evaluation、Handoff | 当前 topic 进入 no-production diagnostic closeout |

## 板卡结果

| case | 证据角色 | median speedup | values | decision bucket | 处理 |
| --- | --- | ---: | --- | --- | --- |
| `region_projected` | production-shaped diagnostic | 0.89x | 0.90x, 0.91x, 0.89x, 0.89x, 0.89x | negative | rejected：局部 projection 正向无法穿透 region boundary 输出组织 |
| `region_gather_only` | production-shaped diagnostic | 0.80x | 0.79x, 0.77x, 0.80x, 0.80x, 0.80x | negative | rejected：多 region gather/output 边界下 RVV 更慢 |

Std/RVV checksum 在两个 case 中一致。板卡 target 每个 case 使用 5 runs、8 iterations、2 warmup iterations。QEMU timing（QEMU 计时）未用于性能判断。

## Evidence Doctor 处理

两个 Phase 010 Evidence Doctor 报告均为 `Errors=1，Warnings=0，Suggestions=0`，Error 均为 `ba_degradation_frequency`：候选 5/5 低于 1。处理方式是把这两条生产形态诊断候选降级为 rejected，并停止进入 PI1。该 Error 不表示 correctness bug；它表示当前证据不能支持生产接入或正向性能结论。

## 诊断证据链

Correctness：`run_test_compare` 中 4 个测试全部通过，包括新增 `RegionBoundaryProjectionMatchesScalarShape`。
反汇编：`assembleRegionBoundariesRVV` 在 `bench_omps_rvv.full.asm` 中可见，并调用 `gatherBoundaryCloudRVV` / `projectBoundaryFromViewpointRVV`；callee 内存在 `vluxei32.v`、`vfdiv.vv`、`vfmacc`、`vsse32.v`。
板卡性能：`region_projected` 与 `region_gather_only` 均为 negative。
边界：证据仍不是 production direct，不覆盖真实 `OrganizedMultiPlaneSegmentation` dispatch、fallback、泛型点型或 `Scalar=double`。

## Diagnostic 到 production 错配审计回填

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper，模拟 production boundary 输出末段 |
| 当前决策问题 | 是否进入 PI1 production integration plan |
| diagnostic 是否可外推到 production | 可以作为拒绝当前 helper family 的强负向信号，但不能证明所有未来 production 形态都无收益 |
| comparison-boundary / baseline mismatch 风险 | 存在；boundary discovery 与真实 `PlanarRegion` 构造仍不在计时内，但当前已覆盖多 region 临时输出和 projection 组合 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前不允许；两个 production-shaped case 均 5/5 退化，且有 Evidence Doctor Error |
| clean adoption 是否需要 production boundary 内 A/B | 需要；当前未进入 PI1，clean adoption 不适用 |

## Doc Suite Role Inventory

| role | 状态 | 证据 / 路径 |
| --- | --- | --- |
| topic_navigation | standalone:`README.zh.md` | 已更新阅读路径、命令和证据边界 |
| testing_overview | standalone:`doc/testing-overview.zh.md` | 已覆盖 target 分类和测试矩阵 |
| correctness_tests | standalone:`doc/correctness-tests.zh.md` | 已覆盖 4 个 gtest 证明范围 |
| benchmark_and_evidence | standalone:`doc/benchmark-and-evidence.zh.md` | 已覆盖 case-filter、board summary、doctor、registry |
| optimization_evidence | standalone:`doc/optimization-evidence.zh.md` | 已映射候选取舍和证据 |
| optimization_roadmap | standalone:`doc/optimization-roadmap.zh.md` | 已更新 no-production 结论和恢复条件 |
| test_support_code_map | standalone:`doc/test-support-code-map.zh.md` | 已覆盖 helper、bench、script 和 output |
| phase_index / phase_result / matrix | standalone:`doc/phases/` | 已更新阶段索引和矩阵 |
| evaluation_diagnostic | standalone:`doc/organized_multi_plane_segmentation-evaluation.zh.md` | 已写 no-production 诊断证据链 |
| production_topic_doc | not_applicable with evidence | 没有 adopted production behavior 或 PI5 证据，不创建 `doc-rvv` |

## Continue / Stop Decision

当前 EvidenceDecision：`bench-only/no-production`。停止条件命中：生产形态诊断两个候选均在板卡上稳定 negative，且 Evidence Doctor 明确阻止把它们升级为 production evidence。当前 topic 范围内没有未阻塞的高优先级 production 候选；未来只有在真实 profile 证明 boundary/projection 占比更高、或出现能避免 per-region 临时 cloud/gather 成本的新实现族时，才建议重开新 phase。
