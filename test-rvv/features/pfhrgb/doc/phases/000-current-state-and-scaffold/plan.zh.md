# Phase 000 Current State And PFHRGB Scaffold Plan

## 阶段意图和边界

本阶段开启 `features/include/pcl/features/impl/pfhrgb.hpp` 的 PFHRGB（带颜色的点特征直方图）RVV topic（主题）。阶段目标是先建立可恢复的 topic-local 测试、bench（性能测试）和文档脚手架，并用 same-chain correctness（同构链路正确性）证明测试专用参考链路能够复刻当前 production（生产源码）语义。随后再用 color pair helper（颜色点对 helper）诊断候选和 public dilution check（公开入口收益稀释检查）判断是否值得进入 production integration loop（生产接入闭环）。

本阶段不修改 `features/include/pcl/features/impl/pfhrgb.hpp` 或 `features/src/pfh.cpp`。当前只覆盖 `pcl::PointXYZRGBNormal -> pcl::PFHRGBSignature250`、`float`、AoS（结构数组）xyz / normal / rgb 字段、dense finite synthetic neighborhood（有限合成邻域）、默认 `nr_split=5` 的 `computePointPFHRGBSignature` 组件。泛型 RGB 点型、`PointXYZRGB + Normal`、非默认 bins、search 真实数据分布、OMP、cache path 和 production dispatch（生产分流）不在本阶段证明范围内。

## 当前状态清单

| 项目 | 当前事实 |
| --- | --- |
| 队列来源 | `doc-rvv/library-screening/features/features-retained-candidate-rescreen.zh.md` 将 PFHRGB 列为建议启动函数级评估第 2 项。 |
| 源码热点 | `computePointPFHRGBSignature` 对邻域做双重 pair 遍历，调用 `computeRGBPairFeatures`，再把几何 3-bin 和颜色 3-bin scatter（离散累加）到 250-bin histogram。 |
| 共享 helper | `features/src/pfh.cpp::computeRGBPairFeatures` 没有本地 batch API（批处理 API），只能随 PFHRGB caller-shaped（调用方形态）topic 取证。 |
| 生产状态 | 当前没有 PFHRGB RVV production patch；本阶段只建立 diagnostic（诊断）证据。 |
| 板卡状态 | 当前会话说明板卡可用；若 correctness、bench build 和 smoke 成功，本阶段继续执行有界 repeated board。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `pfhrgb-scalar-reference-scaffold` | 测试专用标量 reference 先复刻 production pair order、RGB ratio、bin clamp 和两段 histogram scatter，作为后续候选验收基线。 | reference 若漏掉颜色除零、ratio 反号或双向 pair order，会污染候选判断。 |
| `pfhrgb-color-pair-batch-rvv` | PFH direct-AoS 成功模式可以扩展到 PFHRGB 的几何 pair math，颜色 ratio 用整数加载和向量分支或标量尾段处理。 | RGB ratio 分支、`atan2`、zero-distance / zero-cross fallback、histogram scatter 和 pair order 都可能吞掉收益。 |
| `pfhrgb-public-k-dilution-check` | 若组件 helper 正向，`PFHRGBEstimation::compute` 的 KSearch 公开路径可能仍保留可见收益。 | search、descriptor copy 和 synthetic 数据可能稀释 component 收益。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `pfhrgb-scalar-reference-scaffold` | fixed neighborhood indices | `PointXYZRGBNormal`, float, AoS xyz/normal/rgb, `nr_split=5` | `computePointPFHRGBSignature` component | `run_test_compare` | not_applicable | not_applicable | not_applicable | not_applicable | planned | write failing same-chain test, then implement test reference |
| `pfhrgb-color-pair-batch-rvv` | fixed neighborhood indices | same as above | test-only component candidate | planned after reference passes | `component_pfhrgb_signature` | planned repeated board if smoke passes | `dump_bench_rvv` after candidate exists | planned | deferred | build candidate and bench scaffold |
| `pfhrgb-public-k-dilution-check` | public KSearch neighborhood | same as above | `PFHRGBEstimation::compute` synthetic public-like entry | planned | `public_pfhrgb_k` | planned if component positive | planned | planned | deferred | keep as dilution check, not adoption evidence |

## 实现和测试动作

| action | 产物 | 命令 / 验收 |
| --- | --- | --- |
| RED-1 | `test-rvv/features/pfhrgb/src/test_pfhrgb.cpp` 中新增 PFHRGB reference correctness 测试。 | `make -B -C test-rvv/features/pfhrgb run_test_std` 先失败，失败原因是 reference/test support 尚未实现。 |
| GREEN-1 | 新增 `include/pfhrgb.h` 和 `include/impl/pfhrgb_reference.hpp`，复刻当前 production 的 tuple、bin index、颜色 ratio 和 histogram scatter。 | `run_test_compare` Std/RVV 两侧通过。 |
| BENCH-1 | 新增 `src/bench_pfhrgb.cpp`，输出 `component_pfhrgb_signature` 与 `public_pfhrgb_k`。 | QEMU 只编译或窄 smoke；真实性能只写板卡结果。 |
| EVIDENCE-1 | 新增 topic-local manifest wrapper 和 evidence registry 初始状态。 | Evidence Doctor 若只有 smoke 或 metadata 缺口，必须降级说明。 |

## Evidence Doctor 和 registry 规则

本阶段 benchmark、board summary（板卡摘要）或 EvidenceDecision（证据决策）前必须运行 `test-rvv/script/evidence_doctor.py`，优先通过 `test-rvv/features/pfhrgb/script/generate_pfhrgb_evidence_manifest.py` 生成 manifest。若只完成 QEMU correctness（QEMU 正确性验证）或 build smoke（构建冒烟），不写性能结论。`test-rvv/features/pfhrgb/log/evidence_registry.json` 只登记被文档引用的 summary / doctor；raw logs（原始日志）默认 local-only（仅本机保留）。

## 阶段完成条件

- `pfhrgb-scalar-reference-scaffold` 只有在 `run_test_compare` 两侧通过，且 reference 覆盖 normal finite input、颜色除零、ratio clamp、degenerate pair fallback 和 public finite descriptor 后，才能变成 `adopted_as_test_baseline`。
- `pfhrgb-color-pair-batch-rvv` 没有 board repeated（重复板卡测试）和 Evidence Doctor 前不能声明 production-ready（可生产接入）。
- 若 diagnostic 结果为 weak（弱正向）、neutral（中性）、negative（负向）或 unstable（不稳定），必须先回填 diagnostic-to-production mismatch audit（诊断到生产错配审计），不能直接拒绝 bounded production probe（有界生产探针）。

## 板卡复跑预算和决策桶

计划 repeated board runs 为 5，warmup 2，iterations 8。若 component speedup 稳定大于 1.15 且 `B/A < 1` 频率为 0，标为 `positive`；1.03 到 1.15 标为 `weak_positive`；0.97 到 1.03 标为 `neutral`；小于 0.97 标为 `negative`；跨方向且一次确认复跑后仍摇摆标为 `unstable`。预算内最多允许 1 次同边界确认复跑。

## 继续 / 停止条件

默认下一步是完成 RED/GREEN scaffold，然后跑 correctness、bench build、asm attribution（反汇编归属）和板卡 component repeated。只有 reference 无法可靠复刻 production 语义、构建工具或板卡不可达、dirty isolation（脏工作区隔离）暴露当前 topic 外冲突，或继续需要修改未授权 production/public API 时停止。只完成脚手架不是停止条件。

## 文档更新清单

本阶段更新 `README.zh.md`、`doc/pfhrgb-evaluation.zh.md`、`doc/optimization-roadmap.zh.md`、phase README、phase result、optimization matrix 和 Handoff。没有 adopted production behavior（已采用生产行为）前，`doc-rvv/features/pfhrgb-RVV.zh.md` 判为 not_applicable。

## Roadmap 同步动作

阶段结束后把 `pfhrgb-scalar-reference-scaffold` 状态、`pfhrgb-color-pair-batch-rvv` 的 bench / board 结果、RGB ratio 或数学 helper gap 回填到 roadmap。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic / production-shaped diagnostic，本阶段不产生 production direct 证据。 |
| A/B boundary | test helper 或 public-like wrapper；不是真实 production dispatch。 |
| 当前决策问题 | RVV-vs-scalar feasibility（可行性）和 implementation-shape（实现形态）。 |
| diagnostic 是否可外推到 production | unknown；只有 strong positive 且 public dilution check 也正向时，才允许后续 PI1 bounded production probe。 |
| comparison-boundary / baseline mismatch 风险 | yes；component 不包含 search、完整对象状态和真实 production dispatch。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 可在条件下允许：源码静态质量足够、public-like smoke 不退化、PI1 明确不扩大点型或 public API。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；若后续存在多个 RVV family，clean adoption 需要同边界 RVV-vs-RVV detail A/B。 |
