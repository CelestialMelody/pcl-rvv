# Phase 060 Plan: getDistances production probe

## 阶段意图和边界

本阶段把 Phase 050 的 `getDistancesToModel` full-RVV（完整 RVV）诊断候选推进到 bounded production probe（有界生产探针）。生产补丁只覆盖 `SampleConsensusModelCircle2D<PointT>::getDistancesToModel` 的 direct indexed `indices_` 入口、`PointT` 满足 `x/y` 单个 `float` AoS（结构数组）字段布局、`pcl::index_t` 为 signed 32-bit、输出为 `std::vector<double>` 的既有公开 API。

本阶段允许修改 `sample_consensus/include/pcl/sample_consensus/sac_model_circle.h` 和 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp`。不修改公开 API，不新增用户可见参数，不把 `PointXYZ` 板卡结果外推到所有点型；不改其它 SAC topic。PI5（生产接入闭环的用户检查点）前不把该补丁写成 adopted production behavior（已采用生产行为）。

## S0 恢复和偏好冻结

| 字段 | 本阶段记录 |
| --- | --- |
| preferences_loaded | defaults loaded；local override absent；prompt override loaded，用户说明板卡诊断有收益即可进入接入测试，接入后仍需测试确认是否值得采纳。 |
| loaded_instruction_sources | `AGENTS.md`、`.agents/config/defaults.yaml`、`rvv-workflow/SKILL.md`、`rvv-workflow/references/reviewability-and-language.zh.md`、`rvv-test/SKILL.md`、`rvv-test/references/optimization-phase-loop.zh.md`、`rvv-test/references/entry-shapes-and-test-support.zh.md`、`rvv-implementation/SKILL.md`、`rvv-implementation/references/implementation-patterns.md`、`rvv-implementation/references/fallback-and-dispatch.md`、`rvv-implementation/references/point-load-store.md`、`rvv-documentation/SKILL.md`。 |
| work_preferences | production 注释克制，只解释 fallback / dispatch / layout 边界；test-rvv 注释中文主导；证据 summary-only；不提交 commit。 |
| dirty_isolation | 本阶段只碰 circle production 头、circle test-rvv 资产、`doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md` 和 current Handoff；不回滚 line/stick/sphere/segmentation 或 `.agents` 上的无关改动。 |

## 当前证据

| area | 当前事实 | 证据 |
| --- | --- | --- |
| Phase 020 | `RVV sqr + scalar sqrt/store` 候选 5-run B/A median `0.6590x`，只拒绝该实现族。 | `020-circle-getdistances-ablation/result.zh.md` |
| Phase 050 | full-RVV 诊断候选 5-run B/A 为 `1.4700, 1.4740, 1.4745, 1.4827, 1.4905`，median `1.4745x`；Evidence Doctor `0/0/0`。 | `050-getdistances-vfsqrt-full-rvv/getdistances-full-rvv-repeated-evidence-*` |
| asm | `getDistancesToModelFullRVV` 边界已确认包含 `vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v`。 | `make check_getdistances_full_rvv_asm` |
| production | 当前 production 只有 select/count RVV；`getDistancesToModel` 仍是公开入口内联标量循环。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |

## 实现计划

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 抽出标量 helper | `getDistancesToModelStandard` 声明和实现 | 公开入口不再保留大段标量主体；非 RVV 和 fallback 仍走原公式。 |
| 接入 RVV helper | `getDistancesToModelRVV` 声明和实现 | 复用 Phase 050 code shape：gather x/y、sqr、`vfsqrt`、abs、`vfwcvt`、`vse64`。 |
| 公开入口 dispatch | `getDistancesToModel` | 先保留 `isModelValid` 检查；命中 `__RVV10__`、`x/y` float layout、signed 32-bit index、u32 byte offset gate 时短路 RVV，否则 Standard。 |
| 测试适配 | `include/impl/sac_model_circle_candidates.hpp`、`src/test_sac_model_circle.cpp` | 暴露 `getDistancesToModelStandard` / `getDistancesToModelRVV`，新增 public vs Standard / direct RVV 对拍。 |
| bench / manifest | `src/bench_sac_model_circle.cpp`、`script/generate_circle_board_evidence_manifest.py`、`Makefile` | public `getDistancesToModel` 在 RVV build 中命中生产 RVV；新增 Phase 060 production-direct evidence target，避免与 Phase 050 diagnostic 混淆。 |
| 证据 | QEMU correctness、asm、board repeated、Evidence Doctor、registry | PI5 前必须有接入后 public Std/RVV board 数据；若不支持采纳，保留 patch 并暂停等待用户决策。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 050 是 production-shaped diagnostic；Phase 060 将生成 production direct。 |
| A/B boundary | Phase 050 是 public companion vs test-only helper；Phase 060 必须是 Std build public entry vs RVV build public entry。 |
| 当前决策问题 | 接入后真实公开入口是否仍保留收益，且 fallback / asm / correctness 是否闭合。 |
| diagnostic 是否可外推到 production | 不能直接外推；它只支持本阶段 bounded production probe。 |
| comparison-boundary / baseline mismatch 风险 | Phase 060 用 public Std/RVV 消除测试 helper wrapper mismatch；仍只覆盖当前 bench 输入边界。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 diagnostic 为 positive-stable，允许本阶段 probe。 |
| clean adoption 是否需要同一 production boundary 内证据 | 需要；PI5 后仍需用户明确确认采纳，才可更新正式 `doc-rvv` 为 adopted。 |

## 板卡复跑预算和决策桶

生产接入后使用 5-run repeated board。`B/A = Std public getDistancesToModel ms / RVV public getDistancesToModel ms`。若 5/5 大于 1 且 Evidence Doctor 无阻塞 Error，记录为 `positive-stable` 并停在 PI5 用户确认点；若中性、负向或不稳定，记录实际桶并停在 PI5 等待用户决定保留、调整或回滚。

## 继续 / 停止条件

本阶段的合法停止点是 PI5：生产补丁已存在，接入后 correctness、asm、board repeated、Evidence Doctor 和 registry 均已刷新，并向用户报告是否值得采纳。PI5 之前不能更新长期 `doc-rvv` 为 adopted，也不能自行回滚或提交。
