# extract_polygonal_prism_data Phase 000 Plan

## S0 偏好与边界冻结

| 字段 | 本阶段取值 |
| --- | --- |
| loaded_instruction_sources | `AGENTS.md`、`.agents/config/defaults.yaml`、`.agents/knowledge/pcl-rvv-knowledge-map.md`、`rvv-workflow/SKILL.md`、`short-prompt-entry.zh.md`、`reviewability-and-language.zh.md`、`s0-preferences-and-recovery.zh.md`、`topic-lifecycle.zh.md`、`handoff-packet.zh.md`、`worker-quality-gates.zh.md`、`rvv-test/SKILL.md`、`optimization-phase-loop.zh.md`、`test-taxonomy.zh.md`、`entry-shapes-and-test-support.zh.md`、`function-evaluation-and-closeout.zh.md`、`document-ownership-and-traceability.zh.md`、`doc-suite-quality-bar.zh.md`、`evaluation-doc-structure.md`、`makefile-env.md` |
| preferences_loaded | defaults loaded；local override absent；prompt override 指定 topic、worker 角色、持续推进和板卡可用 |
| work_preferences | 测试资产和诊断代码使用中文说明；production 注释保持克制；证据默认 summary-only；不提交 raw log；instruction feedback 默认为 report-only |
| commit_preferences | 本轮不自动提交；若后续进入提交阶段，topic 产物、证据日志和 agent asset patch 分开处理 |
| dirty_isolation | 工作区已有其它 `.agents`、features、APMF 和 segmentation queue 变更；本阶段只允许触碰 `test-rvv/segmentation/extract_polygonal_prism_data/**`，暂不修改 production |

## 阶段意图和范围

本阶段建立 `ExtractPolygonalPrismData<PointT>::segment` 的函数级评估、测试脚手架和第一条 production-shaped diagnostic（生产形态诊断，尽量贴近真实入口但仍在测试资产中运行）。阶段目标是先证明 height mask（高度范围筛选）、point-to-plane signed distance（点到平面有符号距离）和 polygon parity predicate（多边形奇偶判定）能被稳定对拍，再评估是否值得进入后续 RVV candidate（RVV 候选实现）和板卡 bench（目标硬件性能测试）。

本阶段不修改 `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp`。production（生产源码）接入必须等诊断证据、反汇编归属和板卡性能闭合后，另开 production integration loop（生产接入闭环）。

## 函数级当前状态

| 对象 | 当前源码事实 | RVV 判断 |
| --- | --- | --- |
| `ExtractPolygonalPrismData<PointT>::segment` | 公开入口先计算 hull 平面，调用 `SampleConsensusModelPlane::projectPoints` 投影，再逐 `indices_` 扫描原始点高度、投影点二维坐标和 polygon 列表。 | 主循环来自 `projected_points.size()`，保序输出 `output.indices` 是可 RVV 化的压缩边界。 |
| `isXYPointIn2DXYPolygon` | 对单点遍历 polygon edges，遇到交叉条件时翻转 `in_poly`。 | 可把“同一 edge 对多点”向量化；多 polygon concave hull 用 XOR 合并，必须保留语义。 |
| `projectPoints` 前置阶段 | 当前由 SAC plane model 完成，生成与 `indices_` 对齐的 projected cloud。 | 本阶段保留标量，不把投影前置成本归因给 polygon predicate。 |
| 上游测试 | `test/segmentation/test_segmentation.cpp` 有基础空输出 case；`test/segmentation/test_concave_prism.cpp` 有 two-rings concave XOR 和输出顺序检查。 | 作为语义参照；专项 test-rvv 需要可控合成数据和可复现 bench case。 |

## Phase Scope 与扩展队列

| 项 | 内容 |
| --- | --- |
| validated_scope | `PointXYZ`、`Scalar=float`、ordered indices（索引按输入顺序）、horizontal plane `z=0` 的 square / two-rings case；输出顺序必须等于标量参考链路。 |
| unvalidated_scope | 任意倾斜平面、arbitrary indices gather（任意索引离散加载）、其它点类型、`Scalar=double`、production dispatch、`projectPoints` 本身、真实 concave hull 构造成本。 |
| point_type_expansion_queue | 若 Phase 000 证据正向，后续 phase 先扩展到倾斜平面 + arbitrary indices，再考虑 traits / offset（字段特征和偏移）策略；每次扩展都需要 correctness、asm、board 和 Evidence Doctor 输入。 |
| phase_closeout_boundary | 只能关闭 `PointXYZ/float/ordered/horizontal` 的诊断脚手架和候选可行性，不能关闭 production 或泛型模板入口。 |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper，输入模拟 `segment` 中 `projectPoints` 之后的逐点扫描 |
| 当前决策问题 | RVV-vs-scalar 候选是否值得后续 production probe |
| diagnostic 是否可外推到 production | unknown；它能证明投影后扫描段，但不能证明 `projectPoints` 前置成本、真实 dispatch 或 fallback |
| comparison-boundary / baseline mismatch 风险 | yes；若 bench 只测扫描段，不能代表完整 `segment` 调用 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但仅当完整入口 profiling 证明扫描段仍是主要成本，且 fallback / dispatch 范围可控 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；若后续已有 adopted RVV family 或多个候选，必须同边界比较 |

## 测试与实现动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 写 failing test（失败测试） | `src/test_eppd.cpp`、`Makefile` | `make run_test_std` 因缺少 `eppd::segmentPolygonalPrismReference` 或候选入口失败，证明测试会约束新 helper |
| 实现标量参考 helper | `include/impl/eppd_reference.hpp`、`include/eppd.h` | square height/order 和 two-rings XOR case 通过 |
| 实现 RVV candidate | `include/impl/eppd_candidates.hpp` | `run_test_compare` 中 Std/RVV 输出一致；RVV 构建必须真的编译 `__RVV10__` 路径 |
| 增加 bench harness | `src/bench_eppd.cpp`、`board.mk` | 输出 `Dataset:`、`Iterations:`、case avg、`Total Time` 和 checksum，可被 compare 脚本解析 |
| 反汇编与板卡证据 | `dump_bench_rvv`、`board_smoke` 或细分 target | RVV asm 可归属到 candidate；性能结论只来自 board summary |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| scalar reference scaffold | ordered-cloud-pair-like scan | `PointXYZ` / `float` / AoS stride | test helper after projection | planned `run_test_std` | not_applicable | not_applicable | not_applicable | not_applicable | planned | 写 failing test |
| RVV edge-parity candidate | ordered-cloud-pair-like scan | `PointXYZ` / `float` / AoS stride | test helper after projection | planned `run_test_compare` | planned `run_bench_*`；QEMU 只 build / smoke | planned board repeated if QEMU correctness passes | planned `dump_bench_rvv` | planned | planned | 实现后跑 QEMU correctness、asm、board |
| production integration | public `segment` | template `PointT` | real dispatch | deferred | deferred | deferred | deferred | deferred | phase_deferred + unblocked only after positive diagnostic | 等 S10 决策 |

## 板卡复跑预算和决策桶

板卡当前由用户确认可用；本阶段若 bench 二进制和 QEMU correctness 闭合，默认继续运行 board target。预算为 3 组 repeated collection，每组 `iterations=8`、`warmup=2`。决策桶：median speedup `>=1.20x` 为 positive，`1.05x-1.20x` 为 weak-positive，`0.95x-1.05x` 为 neutral，`<0.95x` 为 negative；若三组方向摇摆则标为 unstable 并降级 EvidenceDecision。

## 文档更新清单

| 文档 | 本阶段职责 |
| --- | --- |
| `doc/extract_polygonal_prism_data-evaluation.zh.md` | S2 函数级评估、Traceability Map、诊断证据链和 production 接入判断主归属 |
| `doc/optimization-roadmap.zh.md` | 跨阶段 candidate family、扩展队列和默认恢复动作 |
| `doc/phases/README.zh.md` | 阶段索引和默认恢复入口 |
| `doc/phases/optimization-matrix.zh.md` | 跨阶段证据状态 |
| `doc/phases/000-current-state-and-scaffold/result.zh.md` | Phase 000 执行结果、Evidence Doctor 和继续 / 停止决策 |

## 继续 / 停止条件

默认继续到 QEMU correctness、反汇编和板卡 bench。只有以下情况停止：编译工具或板卡不可达；测试暴露 scalar/reference 语义不清；Evidence Doctor Error 无法解释；继续需要修改 production；dirty isolation 显示本 topic 文件与其它用户变更冲突。
