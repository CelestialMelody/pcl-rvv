# Phase 107 Plan: production-adoption-and-regression-loop

## 阶段意图和边界

本阶段把用户在 2026-08-19 的新授权转成 production integration loop（生产接入闭环）：

- 保留已采纳的 ordered-cloud-pair traits-gated generic RVV 和 Phase 091 source-indexed
  `PointXYZ -> PointXYZ` narrow RVV。
- 将 Phase 093 dual-indexed exact `PointXYZ -> PointXYZ` positive candidate 作为本阶段
  adopt candidate，完成接入后 correctness、QEMU smoke、asm、board repeated、Evidence Doctor
  和 registry 证据。
- 将 Phase 103/104 source-indexed generic PointXYZ-like widening 视为已试接入但 Phase 106
  20-run 仍负向的 guarded probe，本阶段退回到 Phase 091 narrow gate；不回滚 Phase 091。
- 将 Phase 094 correspondence exact `PointXYZ -> PointXYZ` 视为 guarded trial candidate；
  先做接入后复测，若 production-detail family A/B 仍触发 negative / unstable 或 Evidence Doctor
  Error，则只退回 correspondence dispatch，不影响 ordered / source-indexed / dual-indexed。

不覆盖范围：

- source-indexed / dual-indexed / correspondence generic point types；
- `Scalar=double` RVV；
- RGB/RGBA 或自定义 traits 点型逐类型性能；
- 非法 index / correspondence 输入安全合同扩大；
- 自动提交、自动 staging 或跨 topic 清理。

## 当前状态清单

| 项 | 当前状态 |
| --- | --- |
| production header | 已含 ordered generic、source-indexed generic guarded、dual exact、correspondence exact RVV helper；source-indexed helper 中有重复 `SourceLayout`，reduction helper 有 tab 缩进。 |
| Phase 091 | source-indexed `PointXYZ -> PointXYZ` adopted-by-user，board 4K / 64K / 256K 为 `4.103x / 4.818x / 4.575x`，Doctor `0/0/0`。 |
| Phase 093 | dual-indexed exact family A/B positive；4K `1.018x` 带长尾，64K / 256K 为 `1.671x / 1.554x`，Doctor `0/3/1`。 |
| Phase 094 | correspondence public positive，但 family A/B 256K 有 `5/20` below-1，Doctor `1/3/0`，不能 clean-adopt。 |
| Phase 106 | source-indexed generic public representative 20-run variance negative for full widening；board `12 positive / 1 weak_positive / 3 negative`，Doctor `1/27/0`。 |
| evidence_status | Phase 105/106 后为 fresh；本阶段修改后必须刷新。 |

## Diagnostic-to-production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 107 需要 production-public 和 production-detail。QEMU 只用于 smoke / asm / manifest。 |
| A/B boundary | dual-indexed 与 correspondence 的保留/退回看真实 public overload 或同一 RVV binary production-detail family A/B；source-indexed generic rollback 看 Phase 106 同 public boundary 20-run variance。 |
| 当前决策问题 | beneficial implementation adoption、fallback correctness、negative trial rollback。 |
| diagnostic 是否可外推到 production | Phase 099/100/101 generic diagnostic 不可直接外推；Phase 106/094 public 或 detail evidence 可用于本阶段生产取舍。 |
| comparison-boundary / baseline mismatch 风险 | public Std/RVV positive 不能证明 direct RVV family 优于 materialize+ordered RVV；family-selection 需要同边界 RVV-vs-RVV。 |
| negative/unstable 时是否允许 bounded production probe | 已由用户授权试接入；复测仍负向时退回对应候选。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | dual-indexed exact 需要并已有 Phase 093；correspondence exact 需要，Phase 094 当前不通过；source-indexed generic full widening 因 Phase 106 不通过。 |

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | production action | required evidence | decision rule |
| --- | --- | --- | --- | --- | --- |
| ordered generic RVV | ordered-cloud-pair | traits-gated PointXYZ-like, `float` | keep adopted | aggregate correctness、existing public QEMU/board evidence | adopted remains adopted。 |
| source-indexed narrow RVV | source-indexed-cloud-pair | exact `PointXYZ -> PointXYZ`, `float` | keep adopted | correctness/fallback、source-indexed public QEMU/asm、board repeated | adopted remains adopted。 |
| source-indexed generic widening | source-indexed-cloud-pair | representative PointXYZ-like, `float` | rollback to narrow gate | Phase 106 negative already sufficient; post-edit fallback correctness must pass | if fallback hits and no source-indexed generic RVV symbol required, mark rolled back / not adopted。 |
| dual-indexed exact RVV | dual-indexed-cloud-pair | exact `PointXYZ -> PointXYZ`, `float` | adopt candidate / keep dispatch | correctness/fallback、dual QEMU/asm、board family A/B or public repeated | positive with no Error at main scales -> adopted candidate; still no auto-commit。 |
| correspondence exact RVV | correspondence-pair | exact `PointXYZ -> PointXYZ`, `float` | guarded trial | correctness/fallback、correspondence public and family A/B QEMU/asm、board repeated | if family A/B remains Error/negative -> rollback correspondence dispatch; if clean positive -> stop at user decision before adopted。 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| P107-1 plan | 本文件 | 在 production edits 前存在。 |
| P107-2 production header cleanup | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` | 修复重复 using 和缩进；source-indexed generic guarded widening 不再作为默认生产分流。 |
| P107-3 correctness | `make -C test-rvv/registration/transformation_estimation_2D run_test_compare` | Std/RVV aggregate pass；fallback tests 反映退回范围。 |
| P107-4 QEMU / asm | `record_qemu_source_indexed_public_state`、`record_qemu_dual_indexed_family_ab_state`、correspondence public/family state as needed | QEMU Doctor 无 Error；asm 归属到实际保留 production symbol。 |
| P107-5 board repeated | 使用 Phase 107 独立 board run label / evidence dir，不覆盖 Phase 093/094/106 历史证据 | dual exact、correspondence trial 的 summary / manifest / Doctor 生成。 |
| P107-6 rollback if needed | production header narrow edit | 负向 trial 只退回对应 helper/dispatch；保留已采纳路径。 |
| P107-7 docs / registry | Phase 107 result、README、roadmap、matrix、evaluation、`doc-rvv`、current handoff、registry | evidence_status fresh，长期文档只写当前真实 production behavior。 |
| P107-8 hygiene | `git diff --check` | 无 whitespace error；不提交。 |

## 板卡复跑预算和决策桶

- board availability：用户已说明板卡恢复，可继续。
- default runs：5，用于 positive adoption candidate；若 correspondence 或 source-indexed negative rollback 判断需要
  方差确认，可升到 20，但必须使用 Phase 107 独立 label / dir。
- decision bucket：`positive` 需要 median 明确大于 1、主决策规模无 below-1 退化频率 Error；
  `weak_positive` 只允许低风险保留并写 caveat；`negative` / `unstable` 或 Doctor Error 触发
  guarded trial rollback。
- budget exhausted：bucket 稳定即关闭；仍摇摆则降级为 unstable 并停在用户检查点。

## 文档更新清单

- 本阶段 `result.zh.md`。
- `doc/phases/README.zh.md`。
- `doc/optimization-roadmap.zh.md`。
- `doc/phases/optimization-matrix.zh.md`。
- `doc/transformation_estimation_2D-evaluation.zh.md`。
- `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` 大改：只保留当前 adopted / retained production
  truth、接入后证据链、fallback 矩阵和 non-adopted 边界。
- `tmp/rvv-work-logs/registration/transformation_estimation_2D/current-handoff/current-handoff.yaml` / `.zh.md`。

## 继续 / 停止条件

本阶段不停在单个测试或单个 board run。完成接入、复测、Evidence Doctor、registry、文档同步和
`git diff --check` 后，停止在用户决策点：

- 若 evidence 支持 dual exact：报告为 adopted candidate / no auto commit。
- 若 correspondence 仍 negative：报告对应 rollback 已完成或需要用户检查保留 patch。
- 若任何证据矛盾、board target 失败、Evidence Doctor Error 未能解释，停止并保留可复现命令。
