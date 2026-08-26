# Phase 000 Current State And VFH Scaffold Plan

## 阶段意图和边界

本阶段开启 `features/include/pcl/features/impl/vfh.hpp` 的 VFH（Viewpoint Feature Histogram，视点特征直方图）
RVV topic（主题）。阶段目标是建立可恢复的 VFH 专属测试和 benchmark（性能测试）脚手架，先证明标量
reference（参考链路）能复刻当前 production（生产源码）语义，再用 test-only RVV candidate（测试专用 RVV 候选）
评估 centroid-to-point SPFH-like pair math（从质心到点的简化点特征直方图点对数学）是否值得进入后续生产接入闭环。

本阶段不直接修改 `features/include/pcl/features/impl/vfh.hpp`。当前只覆盖 `PointNormal -> VFHSignature308`、
`float`、dense finite synthetic cloud（有限合成点云）、默认 45/128 bin 布局、顺序 full-cloud indices
和默认 public `VFHEstimation::compute()` 形态。非 dense 输入、CVFH / OUR-CVFH callers、泛型点类型、
`Scalar=double`、自定义 bin 参数和 production dispatch（生产分流）不在本阶段证明范围内。

## 当前状态清单

| 项目 | 当前事实 |
| --- | --- |
| 队列来源 | `doc-rvv/library-screening/features/features-retained-candidate-rescreen.zh.md` 将 VFH 列为执行清单第一条未完成主题，建议先做 `production-value evaluation`。 |
| 源码热点 | `computeFeature()` 有 centroid / normal centroid 两条 O(N) 扫描，`computePointSPFHSignature()` 有 centroid-to-point O(N) pair math，viewpoint histogram 还有一条 O(N) 扫描。 |
| 共享经验 | PFH direct-AoS production 成功只说明 pair math helper 可作为候选；FPFH weighted helper 成功只说明 fixed-bin accumulation 值得评估。两者不能替代 VFH 证据。 |
| 生产状态 | VFH 尚无 RVV production patch，本阶段只建立诊断证据，不创建 `doc-rvv/features/vfh-RVV.zh.md`。 |
| 板卡状态 | 当前 prompt 明确说明板卡可用；若 correctness 和 bench 构建通过，应继续执行有界 board repeated。 |

## Sibling Experience Migration Audit

| sibling 经验维度 | sibling topic 里的机制 | 当前 topic 是否适用 | 状态 | 证据 / 理由 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| row source | PFH 用固定邻域和 public KSearch；FPFH 用 SPFH row remap。 | VFH 是 full-cloud sequential indices，没有每点 KSearch 内层。 | adopted | 当前源码 `computeFeature()` 对 `*indices_` 线性遍历。 | Phase 000 只测 full-cloud sequential。 |
| shared math pipeline | PFH 的 direct pair tuple math 可批处理。 | VFH 也调用 `computePairFeatures`，但只有 centroid-to-point O(N)。 | attempted | 先做 test-only helper，不直接改共享 `features/src/pfh.cpp`。 | RED/GREEN 后跑 asm 和 board。 |
| fixed-bin accumulation | FPFH 33-bin weighted helper 已证明 fixed-bin 累加可正向。 | VFH 有 45/128-bin scatter，但 index conflict 和 bin stability 仍需本 topic 验证。 | deferred | Phase 000 先保留 histogram scatter 标量，避免把数学近似和 scatter 冲突混在一起。 | 视结果恢复 viewpoint / scatter phase。 |
| evidence model | PFH/FPFH 都用 correctness、asm、board repeated、Doctor 分层。 | 完全适用。 | adopted | 当前 Makefile 和 manifest wrapper 对齐相同证据合同。 | Phase 000 执行同类证据链。 |
| production boundary | PFH/FPFH 只有 production direct 证据后才采纳。 | 完全适用。 | adopted | 当前不修改 production，诊断正向也只能进入 PI1。 | S10 决策后再判断是否请求 production integration。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `vfh-scalar-reference-scaffold` | 先建立同构标量 reference，后续 RVV candidate 才能用可失败 correctness gate 对拍。 | reference 若漏掉 viewpoint、size component 或 normalize_distances，会导致后续证据失真。 |
| `vfh-centroid-spfh-rvv` | centroid-to-point pair tuple math 可以用 RVV 批处理。 | O(N) 规模小于 PFH O(k^2)，staging 和 histogram scatter 可能抵消收益。 |
| `vfh-public-dilution-baseline` | 即使 component 正向，也要检查公开 `compute()` 中 centroid、viewpoint 和输出复制是否稀释收益。 | 当前没有 production dispatch，所以 public baseline 不能写成采纳证据。 |

## 优化矩阵

见 `doc/phases/optimization-matrix.zh.md`。本 phase 只能关闭 `vfh-scalar-reference-scaffold` 和
`vfh-centroid-spfh-rvv` 的 diagnostic 条目，不能关闭 production topic scope。

## 实现和测试动作

| action | 产物 | 命令 / 验收 |
| --- | --- | --- |
| RED-1 | `src/test_vfh.cpp` 中新增 candidate correctness test。 | `make -B -C test-rvv/features/vfh run_test_rvv` 应先失败，失败原因是 candidate helper 返回 false。 |
| GREEN-1 | 在 `include/impl/vfh_reference.hpp` 实现 `computeVFHSignatureCentroidSPFHRVV`。 | `make -B -C test-rvv/features/vfh run_test_compare` Std/RVV 两侧通过。 |
| BENCH-1 | `src/bench_vfh.cpp` 输出 reference、candidate 和 public baseline case。 | QEMU 只编译或 asm dump；不运行 QEMU bench compare 形成性能结论。 |
| ASM-1 | `dump_bench_rvv` 生成 RVV 反汇编。 | filtered asm 中应出现候选 helper 相关向量指令。 |
| BOARD-1 | `board_smoke` 与 `board_repeated`。 | 板卡 5-run repeated 作为性能证据。 |
| DOCTOR-1 | `evidence_doctor_repeated`。 | Errors / Warnings / Suggestions 必须解释或降级。 |

## Evidence Doctor 和 registry 规则

本阶段 board summary 进入 EvidenceDecision 前必须通过 `script/generate_vfh_evidence_manifest.py` 生成 manifest，
再运行 `test-rvv/script/evidence_doctor.py`。若只有 QEMU correctness 或 build smoke，不写性能结论。
本 topic 尚未建立 `log/evidence_registry.json`；若 board repeated 产生可引用 summary，本阶段 result 中必须写
registry 状态为 `not_created_in_phase000` 或补登记目标。

## 阶段完成条件

- `vfh-scalar-reference-scaffold` 只有在 public compute 默认参数和 size component 两个 reference 对拍通过后，才能成为 `adopted_as_test_baseline`。
- `vfh-centroid-spfh-rvv` 只有在 RVV correctness、asm、board repeated 和 Doctor 均闭合后，才能从 `planned` 进入 `positive_diagnostic_candidate` 或 `attempted_negative/neutral`。
- 若 board repeated 为 weak、negative、neutral 或 unstable，必须先完成 diagnostic-to-production mismatch audit，不能直接推出 no-production 或拒绝 bounded production probe。

## 板卡复跑预算和决策桶

计划 repeated board runs 为 5，warmup 2，iterations 8。若 `candidate_vfh_centroid_spfh_rvv` speedup 最小值大于
1.15 且 `B/A < 1` 频率为 0，标为 `positive`；median 1.03 到 1.15 标为 `weak_positive`；0.97 到 1.03
标为 `neutral`；小于 0.97 标为 `negative`；跨方向且一次确认复跑后仍摇摆标为 `unstable`。预算内最多允许
1 次同边界确认复跑。

## 继续 / 停止条件

默认继续 RED/GREEN、correctness、asm、board repeated 和 Evidence Doctor。只有以下情况停止：reference 无法可靠复刻
production 语义、构建工具或板卡不可达、dirty isolation 暴露当前 topic 外冲突、或继续需要修改未授权 production /
public API。只完成 scaffold 不是停止条件。

## 文档更新清单

本阶段更新 `README.zh.md`、`doc/vfh-evaluation.zh.md`、`doc/optimization-roadmap.zh.md`、phase README、
phase result、optimization matrix 和 Handoff。没有 adopted production behavior 前，`doc-rvv/features/vfh-RVV.zh.md`
判为 not_applicable。

## Roadmap 同步动作

阶段结束后把 scalar reference、candidate correctness、asm、board、Doctor 和是否新增 centroid / viewpoint / scatter
后续路线回填到 roadmap。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic`；本阶段不产生 production direct 证据。 |
| A/B boundary | test helper；`public_vfh_compute_baseline` 只是公开入口基线，不含新 production RVV path。 |
| 当前决策问题 | RVV-vs-scalar feasibility（可行性）和 implementation-shape（实现形态）。 |
| diagnostic 是否可外推到 production | unknown；只有 candidate 与 public baseline 都支持继续时，才允许后续 PI1 bounded production probe。 |
| comparison-boundary / baseline mismatch 风险 | yes；candidate 不含 production dispatch，public baseline 不含新 helper。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 可在条件下允许：需要源码静态质量理由、public baseline 未显示明显稀释，并在 PI1 冻结 fallback / 点型 / CVFH 边界。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；若后续出现多个 RVV family，clean adoption 需要同边界 RVV-vs-RVV detail A/B。 |
