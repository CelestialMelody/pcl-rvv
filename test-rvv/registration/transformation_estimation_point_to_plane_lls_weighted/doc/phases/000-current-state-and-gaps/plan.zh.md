# Phase 000: 当前状态与缺口恢复计划

## 阶段意图和边界

本阶段先恢复当前证据状态，补齐 phase loop 所需的计划、矩阵、证据边界和文档归属。阶段目标不是马上改 production C++，而是把 full-cloud、source-indexed、dual-indices、correspondences 四类 row source policy（行来源策略）放进同一张 candidate family（候选实现族）矩阵，避免把某个旧 helper 的正向结果直接外推成 production adopted（生产已采纳）。

本阶段允许修改：

- `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/`
- `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/*.zh.md`
- 必要的 topic-local evidence manifest wrapper（主题本地证据清单脚本）和生成的 summary-only / manifest / doctor 摘要。

本阶段先不直接修改：

- `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp`
- 新的 production dispatch / fallback 逻辑
- dual-indices 或 correspondences production 接入

## S0 恢复与偏好冻结

| 项 | 当前冻结 |
| --- | --- |
| `preferences_loaded` | 已读取 `AGENTS.md`、`.agents/config/defaults.yaml`；`.agents/local/user-preferences.yaml` 不存在；当前 prompt 要求进入 phase loop 并先建阶段文档。 |
| 注释策略 | test-rvv、diagnostic 和 prototype 默认详细中文；production 注释保持克制，只解释维护边界、fallback、dispatch、数值风险和数据布局。 |
| 文档策略 | closeout 当前状态优先；长期文档不保留对话过程话术；阶段探索归属 `doc/phases/`。 |
| 证据策略 | `summary-only` 默认；raw logs 不默认提交。需要把 board summary 写成结论前，必须记录 Evidence Doctor Errors / Warnings / Suggestions。 |
| dirty isolation | 工作树进入本阶段前已经 dirty，包含 `.agents/` 资产、topic 文档、生产 C++、test-rvv 脚本和日志。当前阶段只审查 / 追加本 topic phase、topic docs 和必要 evidence boundary；不回退已有 diff。 |

## 当前状态清单

| row source | 当前 production 状态 | 当前实现族 | correctness / test | bench / board | asm / doctor | 当前结论 |
| --- | --- | --- | --- | --- | --- | --- |
| full-cloud | adopted | block-reduction + A/B/C/N block groups + fused-abcd-ilp code shape | `run_test_compare`、`run_test_production_direct`、`run_test_candidates`；QEMU std `45 passed + 1 skipped`，RVV `47 passed`。 | `production_dispatch_fused_abcd_ilp/summary.md` 三类代表点型 262144 点均正向；`production_default_fused_abcd_ilp/trace_summary.md` 和 `checksum_validation.md` 覆盖默认 RVV path。 | `asm_production_symbol_attribution.md`；`production_dispatch_fused_abcd_ilp/evidence_manifest.json` 和 `evidence_doctor.md` 已存在，doctor 结果为 0 Errors / 0 Warnings / 3 Suggestions。 | adopted，边界是 `Scalar=float`、连续 weights、source xyz f32 AoS、target xyz+normal f32 AoS、size/VLEN/byte-offset gate。 |
| source-indexed | adopted | valid-index scan + uint32_t staging + source gather + target stride load + `vcompress` / scalar tail，当前不是 full-cloud family 的简单搬运。 | `run_test_source_indices_compare`；std `6 passed`，RVV `7 passed`；`run_test_production_direct` 也覆盖 source-indexed direct。 | `production_source_indices_staged_gather/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` 在三类代表点型、65536/262144 点均正向；doctor 结果为 0 Errors / 7 Warnings / 6 Suggestions。 | source-indexed 专属 asm attribution 仍未闭合；doctor 警告集中在 asm boundary missing、binary identity missing 和长尾。 | adopted for current family，但 source-indexed block-reduction / fused-formula / ILP family comparison 仍是 unblocked audit。 |
| dual-indices | production 保持标量 | test-only row-source candidate：双侧 gather + continuous weight | `run_test_public_semantics`、`run_test_row_sources` 覆盖有效输入语义和 candidate correctness。 | `run_board_bench_row_sources/analyze_bench_compare.log`、`evidence_manifest.json`、`evidence_doctor.md` 已把 row-source diagnostic 边界写清楚；dual-indices 负向。 | 无 production asm；row-source diagnostic 仍是 pre-production 证据，不是 production direct。 | diagnostic only；进入 production 前必须先补同 family candidate / test / bench，而不是直接 production。 |
| correspondences | production 保持标量 | test-only row-source candidate：展开 query/match/weight 后双侧 gather | `run_test_public_semantics`、`run_test_row_sources` 覆盖有效 correspondence 语义和 candidate correctness。 | `run_board_bench_row_sources/analyze_bench_compare.log`、`evidence_manifest.json`、`evidence_doctor.md` 已把 row-source diagnostic 边界写清楚；correspondences 负向。 | 无 production asm；row-source diagnostic 仍是 pre-production 证据，不是 production direct。 | diagnostic only；还需拆 query/match 展开、权重来源、容器访问、gather 和 tail 成本。 |

## 假设与候选族

| candidate family | 当前理解 | 需要验证的假设 |
| --- | --- | --- |
| staged-gather / compressed-tail | source-indexed 当前 adopted；dual-indices / correspondences 当前只在 test-rvv diagnostic 中有类似 staged/gather 思路。 | 对每条 row source 独立检查 gather、index staging、weight load、finite mask、`vcompress` 和 scalar tail 成本。 |
| block-reduction + A/B/C/N | full-cloud adopted。 | source-indexed 是否能在 gather 后复用相同 normal-equation block groups；dual-indices / correspondences 是否被双侧 gather 或 weight 展开成本抵消。 |
| fused formula / fused-abcd-ilp | full-cloud adopted / accepted-risk。`AbcdFusedIlp` 当前是 code-shape preference，不声明独立机器码收益。 | 对 source-indexed、dual-indices、correspondences 只能先做同边界 test-rvv candidate / bench / board；不能只因 full-cloud adopted 就外推。 |
| LMUL / ILP variants | full-cloud 当前使用 f32m2 相关 helper 和显式 code shape。 | indexed / correspondence ingress 可能增加寄存器压力、spill、`vsetvli` 或 gather 负担；是否换 m1/m2/m4 需单独 board / asm。 |
| production direct | full-cloud 和 source-indexed 已接入；dual-indices / correspondences 未接。 | 新 row source 即便诊断正向，也必须先过 PI1-PI5：production patch、direct tests、fallback、asm attribution、repeated board 和 EvidenceDecision。 |

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| block-reduction + A/B/C/N | full-cloud | `PointNormal`、`PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal` / `Scalar=float` / f32 AoS gated | production public full-cloud overload | `run_test_production_direct`、`run_test_candidates`、`run_test_compare` | `run_bench_production_dispatch`、`run_bench_production_default_fused_abcd_ilp_rvv` | `production_dispatch_fused_abcd_ilp/summary.md`、`production_default_fused_abcd_ilp/trace_summary.md` | `production_default_fused_abcd_ilp/asm_production_symbol_attribution.md` | `production_dispatch_fused_abcd_ilp/evidence_doctor.md` | adopted | 只需保留边界和 accepted-risk；不外推到 indexed。 |
| fused-abcd-ilp code shape | full-cloud | 同上 | production default RVV path | `ProductionDefaultFusedAbcdIlpRepresentativePointTypesMatchStd` | `run_bench_fused_formula`、`run_bench_generic_fused_formula`、历史 RVV-vs-RVV B/A | `production_dispatch_fused_abcd_ilp/summary.md`、`trace_summary.md` | `asm_production_symbol_attribution.md` | 当前 doctor 0 Errors / 0 Warnings / 3 Suggestions | adopted / accepted-risk | 文档中继续说明 ILP 是 code-shape preference，不写成独立机器码收益。 |
| staged-gather / compressed-tail | source-indexed | 三类代表点型 / `Scalar=float` / source xyz + target normal f32 AoS / valid source index | production public source-indexed overload | `run_test_source_indices_compare`、`run_test_production_direct`、`run_test_input_semantics` | `run_bench_production_source_indices` | `production_source_indices_staged_gather/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | source-indexed-specific asm missing | source-indexed-specific manifest / doctor done；doctor 0E / 7W / 6S | adopted for current family | 保持 current family adopted；若要替换 family，先补 source-indexed same-boundary candidate / bench / board。 |
| block-reduction + A/B/C/N + fused formula carry-over | source-indexed | 同上 | test-rvv candidate first，不直接 production | missing dedicated candidate target | missing dedicated family-comparison bench | missing board A/B | missing asm | missing doctor | planned / audit | 新阶段先补 source-indexed same-family candidate / test / bench；若正向再考虑 production replacement。 |
| staged-gather / compressed-tail | dual-indices | `PointNormal` diagnostic / `Scalar=float` / 双 index stream | test-rvv row-source candidate | `run_test_row_sources` | `run_bench_row_sources` | `run_board_bench_row_sources/analyze_bench_compare.log`、`evidence_manifest.json`、`evidence_doctor.md` | none | diagnostic manifest / doctor done；observed speedup 保留为 single-run data | attempted / diagnostic negative | 负向已经写回诊断边界；若继续此 topic，先补同 family candidate / test / bench / board。 |
| block-reduction + A/B/C/N + fused formula carry-over | dual-indices | planned diagnostic / `Scalar=float` / 双 gather | test-rvv candidate first | missing | missing | missing | missing | missing | planned / audit | 若继续此 topic，先实现同 family diagnostic candidate 和 component ablation，再谈 production。 |
| staged-gather / compressed-tail | correspondences | `PointNormal` diagnostic / `Scalar=float` / query/match/weight from correspondences | test-rvv row-source candidate | `run_test_row_sources` | `run_bench_row_sources` | `run_board_bench_row_sources/analyze_bench_compare.log`、`evidence_manifest.json`、`evidence_doctor.md` | none | diagnostic manifest / doctor done；observed speedup 保留为 single-run data | attempted / diagnostic negative | 负向已经写回诊断边界；拆 query/match/weight 展开和容器访问成本。 |
| block-reduction + A/B/C/N + fused formula carry-over | correspondences | planned diagnostic / `Scalar=float` / 双 gather + correspondence weight | test-rvv candidate first | missing | missing | missing | missing | missing | planned / audit | 先补 candidate / bench / board 证据；不直接 production。 |
| RVV family | `Scalar=double` 或 layout miss | double Scalar、double normal、非 f32 AoS | fallback only | `run_test_production_direct` fallback smoke | none | none | not_applicable | not_applicable | deferred / fallback only | 若要扩展，需要先写误差预算和 traits gate 计划。 |

## 接入生产前与接入生产后测试分层

| 层级 | 当前 target / 文件 | 当前状态 | 文档动作 |
| --- | --- | --- | --- |
| pre-production diagnostic（接入生产前诊断） | `run_test_candidates`、`run_test_row_sources`、`run_bench_row_sources`、`run_bench_fused_formula`、`run_bench_generic_fused_formula` | 已覆盖 full-cloud 候选和当前 row-source candidate；source-indexed block/fused family 还缺同边界 candidate。 | 在 topic docs 中明确这些 target 只能筛选方向，不能替代 production direct。 |
| production-shaped diagnostic（生产形态诊断） | `run_bench_production_shaped_fused_formula`、`run_bench_generic_fused_*` | 已用于 full-cloud fused formula 筛选。 | 保持为候选层证据；不能写成真实 public overload。 |
| post-production direct correctness（接入生产后真实路径正确性） | `run_test_production_direct`、`run_test_source_indices` | full-cloud 和 source-indexed 已覆盖；dual-indices / correspondences 未接 production。 | 真实 production 路径测试与 row-source candidate 测试分开列。 |
| post-production performance（接入生产后性能） | `run_bench_production_dispatch`、`run_bench_production_source_indices`、`collect_board_production_dispatch_repeated`、`collect_board_production_source_indices_repeated` | full-cloud 和 source-indexed 均有 repeated board summary。 | source-indexed summary 需补 manifest / doctor 边界；row-source 触发日志只保留 diagnostic role。 |
| implementation-family comparison（实现族比较） | 当前没有 dedicated source-indexed / dual / correspondence family-comparison target | 缺口 | 在矩阵中保持 `planned / audit`，不能把 staged-gather adopted 写成全家族最优。 |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| A1 建立 phase 文档入口 | `doc/phases/README.zh.md`、本 `plan.zh.md` | phase plan written before further implementation / production edits。 |
| A2 补 source-indexed production summary 的 manifest / doctor 边界 | `production_source_indices_staged_gather/evidence_manifest.json`、`evidence_doctor.md` 或在文档中明确 `summary_only_metadata_missing` 边界 | summary 不再只有自然语言引用；doctor finding 写入 result、benchmark docs 和 Handoff。 |
| A3 补 row-source diagnostic 的 manifest / doctor 边界 | `run_board_bench_row_sources/evidence_manifest.json`、`evidence_doctor.md` 或等价人工 doctor 摘要 | row-source 触发日志明确是 diagnostic evidence，不替代 production evidence。 |
| A4 同步 topic docs | README、testing overview、benchmark-and-evidence、optimization-evidence、evaluation | 读者能从 topic docs 找到 phase、pre/post split、family audit gap 和 doctor 边界。 |
| A5 更新 phase result | `result.zh.md` | 每个动作有 `done / partial / deferred / blocked` 状态和证据路径。 |
| A6 输出 Handoff Packet | 最终回复 | 包含 dirty isolation、phase_loop_state、doctor result、optimization matrix 状态和 next action。 |

## Evidence Doctor 规则

| 证据 | 输入 | 预期输出 | 异常处理 |
| --- | --- | --- | --- |
| full-cloud production-dispatch | `production_dispatch_fused_abcd_ilp/evidence_manifest.json` | 已有 `evidence_doctor.md`，当前为 0 Errors / 0 Warnings / 3 Suggestions。 | Suggestions 不阻塞 adopted；保留 binary identity 建议。 |
| source-indexed production summary | `production_source_indices_staged_gather/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | 已补 machine-readable 边界；doctor 结果为 0 Errors / 7 Warnings / 6 Suggestions。 | 若缺 source-indexed asm 或 binary identity，仍要降级为未闭合 warning / suggestion，不推翻 correctness 和 repeated board 正向，但禁止写“全证据 clean pass”。 |
| row-source diagnostic | `run_board_bench_row_sources/analyze_bench_compare.log`、`evidence_manifest.json`、`evidence_doctor.md` | 已补 diagnostic 边界；doctor 维持 0 Errors / 0 Warnings / 0 Suggestions。 | 诊断负向只支持 attempted / diagnostic negative；不能单因归因为 gather。 |

## 阶段完成条件

- `phase_plan_written_before_edits`：本文件已存在并列出允许范围。
- `optimization_matrix_ready`：矩阵覆盖 candidate family × row source policy × point type / `Scalar` / layout × test × bench × board × asm × Evidence Doctor × decision。
- source-indexed current family 和 source-indexed block/fused family comparison 不再混成一个 adopted 结论。
- dual-indices / correspondences 明确为 diagnostic only；若以后尝试 full-cloud adopted family，先在 test-rvv 补 candidate / bench / board。
- source-indexed production summary 和 row-source diagnostic 的 Evidence Doctor / manifest 边界已进入 topic docs 和 Handoff。

## Continue / Stop Decision

默认继续。当前未阻塞动作是 A4-A6。A2/A3 已完成并写入 manifest / doctor 边界。只有下列情况允许停止：

- Evidence Doctor 产生 Error，需要修复脚本或降级证据边界后才能继续。
- 继续需要开始 production C++ family replacement 或新增 dual / correspondence production dispatch，而当前阶段尚未授权。
- dirty isolation 变得无法判断哪些文档 / 脚本属于本阶段。
- A2-A6 均完成，且下一步已明确为新阶段 `010-source-indexed-family-carry-over` 或等价计划。
