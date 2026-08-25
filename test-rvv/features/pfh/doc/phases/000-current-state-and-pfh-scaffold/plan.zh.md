# Phase 000 Current State And PFH Scaffold Plan

## 阶段意图和边界

本阶段开启 `features/include/pcl/features/impl/pfh.hpp` 的 PFH（Point Feature Histogram，点特征直方图）RVV topic（主题）。阶段目标是建立可恢复的 PFH 专属测试和 benchmark（性能测试）脚手架，先证明标量 reference（参考链路）能复刻当前 production（生产源码）语义，再用 board（板卡）组件 bench 判断 pair feature batch（成对特征批处理）是否值得进入后续生产接入闭环。

本阶段不直接修改 `features/include/pcl/features/impl/pfh.hpp`。当前只覆盖 `PointNormal -> PFHSignature125`、`float`、dense finite synthetic neighborhood（有限合成邻域）、默认 `nr_split=5` 的 `computePointPFHSignature` 组件和 `PFHEstimation::compute` 合成公开入口。`use_cache_`、非 dense 输入、其它点类型、`Scalar=double`、PFHRGB/VFH/FPFH 共享 helper 和 production dispatch（生产分流）不在本阶段证明范围内。

## 当前状态清单

| 项目 | 当前事实 |
| --- | --- |
| 队列来源 | `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md` 将 `impl/pfh.hpp` 列为优先级 6，建议进行 RVV 优化。 |
| 源码热点 | `computePointPFHSignature` 对邻域做 `k*(k-1)/2` pair 遍历，调用 `computePairFeatures` 后将 f1/f2/f3 scatter（离散累加）到 125-bin histogram。 |
| 共享经验 | FPFH 已有 `test-rvv/features/fpfh`，其 `computePointSPFHSignature` component board 结果接近中性，只能作为结构和风险参照，不能外推到 PFH。 |
| 生产状态 | PFH 尚无 RVV production patch，本阶段建立诊断证据，不创建 `doc-rvv/features/pfh-RVV.zh.md`。 |
| 板卡状态 | 当前 prompt 明确说明板卡可用；若本阶段 bench 构建和 smoke 成功，应继续执行有界 repeated board。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `pfh-scalar-reference-scaffold` | 先建立同构标量 reference，后续 RVV candidate 才能用可失败 correctness gate 对拍。 | reference 若漏掉 pair order、finite gate 或 histogram index，会导致后续证据失真。 |
| `pfh-pair-feature-batch-diagnostic` | `computePairFeatures` 的向量化批处理可能减少 O(k^2) pair math 成本。 | `acos`/`atan2`、zero-distance/zero-cross fallback、pair order、histogram scatter 和非结合累加语义复杂。 |
| `pfh-histogram-copy-rvv` | `computeFeature` 中 125-bin copy 可用 RVV 加速。 | 相比 O(k^2) pair feature 可能收益过小；应先作为低优先级候选。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `pfh-scalar-reference-scaffold` | fixed neighborhood indices | `PointNormal -> PFHSignature125`, float, `nr_split=5` | `computePointPFHSignature` component | `run_test_compare` | not_applicable | not_applicable | not_applicable | not_applicable | planned | write failing test, then implement test reference |
| `pfh-pair-feature-batch-diagnostic` | fixed neighborhood indices | same as above | test-only component candidate | planned after reference passes | `component_pfh_signature` | planned repeated board if smoke passes | `dump_bench_rvv` after candidate exists | planned | deferred | build bench scaffold and collect baseline |
| `pfh-public-k-dilution-check` | public KSearch neighborhood | same as above | `PFHEstimation::compute` public-like synthetic cloud | `run_test_compare` public finite descriptor | `public_pfh_k` | planned if component positive | planned | planned | deferred | keep as dilution check, not adoption evidence |

## 实现和测试动作

| action | 产物 | 命令 / 验收 |
| --- | --- | --- |
| RED-1 | `test-rvv/features/pfh/src/test_pfh.cpp` 中新增 PFH reference correctness 测试。 | `make -B -C test-rvv/features/pfh run_test_std` 应先失败，失败原因是 reference/test support 尚未实现。 |
| GREEN-1 | 新增 `include/pfh.h` 和 `include/impl/pfh_reference.hpp`，复刻 current production 的 pair tuple、bin index 和 histogram scatter。 | `run_test_compare` Std/RVV 两侧通过。 |
| BENCH-1 | 新增 `src/bench_pfh.cpp`，输出 `component_pfh_signature` 与 `public_pfh_k`。 | QEMU 只编译或 narrow smoke；性能结论只等 board repeated。 |
| EVIDENCE-1 | 新增 topic-local manifest wrapper 和 evidence registry 初始状态。 | Evidence Doctor 若只有 smoke 或 metadata 缺口，必须降级说明。 |

## Evidence Doctor 和 registry 规则

本阶段 benchmark 或 board summary 进入 EvidenceDecision 前必须运行 `test-rvv/script/evidence_doctor.py`，优先通过 `test-rvv/features/pfh/script/generate_pfh_evidence_manifest.py` 生成 manifest。若只是 QEMU correctness 或 build smoke，不写性能结论。`test-rvv/features/pfh/log/evidence_registry.json` 初始登记 topic-local summary/Doctor 路径；raw logs 默认不进入提交边界。

## 阶段完成条件

- `pfh-scalar-reference-scaffold` 只有在 `run_test_compare` 两侧通过，并且 reference 与 production helper 对拍覆盖 normal finite input、degenerate pair fallback 和 public finite descriptor 时，才能从 `planned` 变成 `adopted_as_test_baseline`。
- `pfh-pair-feature-batch-diagnostic` 本阶段最多进入 `planned` 或 `attempted`；没有 board repeated 和 Evidence Doctor 前不能声明 production-ready（可生产接入）。
- 若 board repeated 显示 component 明显负向或中性，必须先完成 diagnostic-to-production mismatch audit（诊断到生产错配审计），不能直接拒绝 bounded production probe（有界生产探针）。

## 板卡复跑预算和决策桶

计划 repeated board runs 为 5，warmup 2，iterations 8。若 component speedup 稳定大于 1.15 且 `B/A < 1` 频率为 0，标为 `positive`；1.03 到 1.15 标为 `weak_positive`；0.97 到 1.03 标为 `neutral`；小于 0.97 标为 `negative`；跨方向且一次确认复跑后仍摇摆标为 `unstable`。预算内最多允许 1 次同边界确认复跑。

## 继续 / 停止条件

默认下一步是完成 RED/GREEN scaffold，然后跑 correctness、asm build 和板卡 component repeated。只有以下情况停止：reference 无法可靠复刻 production 语义、构建工具或板卡不可达、dirty isolation 暴露当前 topic 外冲突、或继续需要修改未授权 production/public API。只完成测试 scaffold 不是停止条件。

## 文档更新清单

本阶段更新 `README.zh.md`、`doc/pfh-evaluation.zh.md`、`doc/optimization-roadmap.zh.md`、phase README、phase result、optimization matrix 和 Handoff。没有 adopted production behavior 前，`doc-rvv/features/pfh-RVV.zh.md` 判为 not_applicable。

## Roadmap 同步动作

阶段结束后把 `pfh-scalar-reference-scaffold` 的状态、`pfh-pair-feature-batch-diagnostic` 的 bench/board 结果、以及是否新增 `rvv-math-vectorization` helper gap 回填到 roadmap。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic / production-shaped diagnostic，本阶段不产生 production direct 证据。 |
| A/B boundary | test helper 或 public-like wrapper；不是真实 production dispatch。 |
| 当前决策问题 | RVV-vs-scalar feasibility（可行性）和 implementation-shape（实现形态）。 |
| diagnostic 是否可外推到 production | unknown；只有 strong positive 且 public dilution check 也正向时，才允许后续 PI1 bounded production probe。 |
| comparison-boundary / baseline mismatch 风险 | yes；test component 不包含 search、cache、output copy 和真实对象状态。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 可在条件下允许：必须有源码静态质量理由、public-like smoke 不退化、并且 PI1 明确不扩大点类型或 public API。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；若后续已有 RVV family 或多个 candidate，clean adoption 需要同边界 RVV-vs-RVV detail A/B。 |
