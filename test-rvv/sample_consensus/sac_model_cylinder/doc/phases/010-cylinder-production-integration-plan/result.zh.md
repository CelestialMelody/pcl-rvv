# Phase 010: cylinder production integration plan 结果

## 执行摘要

本阶段完成 PI1 production integration plan（生产接入计划）。计划把 Phase 000 的
`partial-production-candidate`（局部生产候选）收束成一个有界 production probe（生产探针）：
只考虑 `countWithinDistance` 和 `selectWithinDistance` 两个公开入口，优先使用 traits-gated
xyz + normal AoS float（点 / 法线字段特征准入的结构数组 float 布局）路径；未覆盖点型、
`Scalar=double`、`getDistancesToModel` 和其它生产入口保持标量或另起 phase。

本阶段没有修改 production（生产源码）。继续到 PI2 production patch 会改
`sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp`，因此当前阶段命中
`turn_stop_deferred with stop_condition_hit`：停止条件是“需要用户明确授权 production 修改”，不是板卡、
工具链、correctness 或 Evidence Doctor（证据体检）失败。

## 计划动作回填

| 计划动作 | 状态 | 结果 |
| --- | --- | --- |
| 冻结 production scope（生产范围） | done | 只覆盖 `countWithinDistance` / `selectWithinDistance`；`getDistancesToModel`、`optimizeModelCoefficients`、`projectPoints`、`doSamplesVerifyModel` 不进入 PI2 默认范围。 |
| 冻结 point type / layout gate（点型 / 布局准入） | done | PI2 默认优先 traits-gated xyz + normal AoS float。若 normal traits 或公共 wrapper 不足，只允许阶段性 exact gate，并必须让其它模板实例 fallback。 |
| 冻结 fallback 矩阵 | done | 非 RVV 构建、layout 不满足、normal 字段不满足、32-bit byte offset 不可表示、`pcl::index_t` 不满足、small size 和 invalid model 均列入 PI2/PI3 验证要求。 |
| 冻结 evidence plan（证据计划） | done | PI3/PI4 需要 production direct correctness、QEMU correctness / log shape、production asm、5-run production board repeated、manifest / doctor / registry freshness。 |
| 判断是否进入 PI2 | blocked_before_PI2 | 当前对话尚无明确授权修改 production 源码；按 AGENTS.md 不能自行进入 PI2。 |

## Diagnostic 到 production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role（证据角色） | Phase 000 仍是 `production_shaped_diagnostic`；PI1 只创建 production integration plan。 |
| A/B boundary（对比边界） | Phase 000 是 test helper；PI2-PI4 必须改为 public overload 或 production helper 同边界。 |
| 当前决策问题 | 是否允许把 diagnostic candidate 转成有界 production probe。当前答案是技术上可计划，授权上停在 PI2 前。 |
| diagnostic 是否可外推到 production | 只能外推公式、访存和输出组织；不能外推真实 dispatch、fallback、production asm 或 public performance。 |
| comparison-boundary / baseline mismatch 风险 | 仍存在，PI4 必须重新跑 production public Std/RVV repeated。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | Phase 000 为 positive；若 PI4 弱、负或不稳定，不能自动采纳或回滚，必须停在 PI5 等待用户确认。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前没有已采纳 cylinder RVV family；若 PI2 只迁移 Phase 000 family，不需要 family-selection A/B。若 PI2 引入替代 family，需要同边界 RVV-vs-RVV 对照。 |

## 当前 evidence 与 registry 状态

Phase 010 没有新跑 board，也没有生成新的 manifest。当前可用 evidence 仍来自 Phase 000：

- `doc/phases/000-cylinder-count-select-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/000-cylinder-count-select-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/000-cylinder-count-select-diagnostic/repeated-evidence-doctor.json`
- `log/evidence_registry.json`

Phase 000 的 registry 状态已通过 `make -C test-rvv/sample_consensus/sac_model_cylinder repeated_evidence_status`
检查为 `fresh`。Phase 010 没有覆盖这些证据文件。

## Phase scope 与扩展队列

`validated_scope`：PI1 只验证“生产接入计划是否可控”，没有新增 runtime coverage（运行时覆盖）。

`unvalidated_scope`：真实 production dispatch、production direct correctness、production asm、production board repeated、
production Evidence Doctor、非 RVV fallback、traits-gated 点型扩展、`Scalar=double`、`getDistancesToModel`。

`point_type_expansion_queue`：生产窄范围接入后，另起 phase 扩展 PointXYZ-like / Normal-like traits、custom layout
和 `Scalar=double`。具体点型或代表性点型不能外推成完整泛型模板结论。

## Continue / stop decision

`continue_stop_decision`：本阶段停止在 PI2 前。

`stop_condition_hit`：继续会修改未明确授权的 production 文件
`sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp`。这符合 AGENTS.md 中 production integration
loop 的权限边界。

`next_phase_default`：用户明确授权 production patch 后，进入 PI2，范围限定为 PI1 计划中的 count/select public entry。
未授权前，下一轮 worker 应先读取本 result、PI1 plan、Phase 000 result、optimization matrix 和 roadmap，然后等待授权或转做
不触碰 production 的独立 topic / 后续筛选工作。
