# Phase 111 Plan: source-indexed-normal-negative-case-investigation

## 阶段意图和边界

本阶段继续 Phase 106 的 source-indexed generic negative-case investigation（源索引泛型负向样本追查）。
目标是解释 Normal 类点型在 source-indexed public generic widening（公开入口泛型扩大）中的退化模式，
判断是否存在值得进入后续 production probe（生产探针）的窄子范围，或应把 Normal 类 source-indexed
generic widening 明确保留为 rejected / deferred。

本阶段只做诊断和证据整理，不修改 production gate（生产分流门控）：

- 入口：source-indexed public overload。
- row source：source-indexed-cloud-pair。
- point types：`PointNormal -> PointNormal`、`PointXYZINormal -> PointXYZINormal`、
  `PointXYZINormal -> PointNormal`、`PointNormal -> PointXYZINormal`，并与 `PointXYZ` /
  `PointXYZI` 正向子范围对照。
- `Scalar=float`。
- evidence role：diagnostic / production-public evidence audit（生产公开入口证据审计）。

本阶段不证明：

- Phase 110 `PointXYZI -> PointXYZI` probe 已采纳；
- source-indexed full generic widening 可采纳；
- dual-indexed / correspondence 的新结论；
- `Scalar=double`、RGB/RGBA、自定义 traits 点型；
- 非法 index 安全合同。

## 当前状态清单

| 项 | 当前事实 |
| --- | --- |
| Phase 110 | exact `PointXYZI -> PointXYZI` source-indexed public probe positive，但 pending user adoption。 |
| Phase 106 | full source-indexed generic public representative 20-run 为 negative：12 positive、1 weak_positive、3 negative，Board Doctor `1/27/0`。 |
| Phase 106 negative | `PointNormal->PointNormal 256K` 为 `7/20` below-1；`PointXYZINormal->PointNormal 64K` 和 `PointXYZINormal->PointXYZINormal 256K` 各 `1/20` below-1。 |
| Phase 104 | `PointNormal->PointNormal 256K` 单 case 5-run rerun positive，但不足以推翻 Phase 106 20-run representative negative。 |
| production source | 当前只保留 adopted `PointXYZ -> PointXYZ`、pending `PointXYZI -> PointXYZI` probe；Normal 类点型仍 fallback。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic evidence audit over prior production-public evidence。 |
| A/B boundary | 使用 Phase 106 public source-indexed generic evidence 的 case-level board summary / manifest / Doctor；不新增 production patch。 |
| 当前决策问题 | Normal 类点型是否存在可收窄的后续 production probe 候选，或应继续拒绝 source-indexed Normal generic widening。 |
| diagnostic 是否可外推到 production | Phase 106 已是 public boundary，但属于 full generic guarded probe；本阶段只分析现有证据和必要的补充 smoke，不直接采纳。 |
| comparison-boundary / baseline mismatch 风险 | 有。Phase 104 single-case rerun 与 Phase 106 20-run representative run label 不同；必须以 Phase 106 20-run 为主。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有当某个 exact Normal 子范围在独立 repeated board 中稳定 positive 且用户授权时，后续才允许另开 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若后续要采纳新的 exact Normal probe，需要接入后 correctness、QEMU/asm、独立 board repeated、Doctor 和 PI5 用户确认；本阶段不采纳。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Normal-layout negative-case audit | source-indexed-cloud-pair | Normal / XYZINormal same and mixed pairs, `Scalar=float` | public source-indexed guarded generic evidence audit | existing `run_test_compare` 84/84 and Phase 106 correctness | existing Phase 106 summary / manifest; optional QEMU smoke only if docs or registry stale | Phase 106 20-run board summary | existing source-indexed generic production boundary | Phase 106 board Doctor `1/27/0` | pending |

## 实现和测试动作

1. 解析 Phase 106 board summary / manifest，按 source point、target point、规模、below-1 频率、min/median/max、p10/p90 分组。
2. 审计 `PointXYZ`、`PointXYZI`、`PointNormal`、`PointXYZINormal` 的 layout / stride / source-target 组合差异，解释 Normal 类点型为什么不能继承 `PointXYZI` positive probe。
3. 判断是否存在值得后续独立 production probe 的 exact Normal 子范围；若没有，写出拒绝 / 暂缓理由和恢复条件。
4. 必要时只运行现有 freshness / correctness target；不新增 production source change。
5. 同步 result、matrix、roadmap、README、evaluation、Handoff 和 `evidence_status`。

## Evidence Doctor 和 registry 规则

- Phase 106 board Doctor 的 Error 不能被本阶段覆盖或淡化。
- 若本阶段不生成新 board evidence，必须写清本阶段是 evidence audit，不新增 performance authority。
- 若补充 QEMU / correctness target 造成 logs 变化，必须更新 registry 并跑 `evidence_status`。

## 板卡复跑预算和决策桶

本阶段默认不复跑 board，因为当前问题首先是解释 Phase 106 已有 20-run negative evidence。
若分析发现一个可单独复核的 exact Normal 子范围，下一阶段再申请独立 label / evidence dir 的 20-run
board budget。

## 继续 / 停止条件

- 若没有 Normal 类 production-safe 子范围：本阶段关闭为 rejected / deferred，并回到 Phase 110 用户决策点或其它 roadmap 候选。
- 若发现可验证子范围：新建后续 bounded production probe phase；不得在本阶段改 production gate。
- 若 evidence_status、registry 或文档引用不 fresh：先修复 freshness。
