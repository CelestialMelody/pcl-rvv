# Phase 004 Result：production-boundary-probe

## 结论

本阶段闭合 production-boundary probe（生产边界探针）审计，范围只限
`test-rvv/registration/transformation_estimation_dual_quaternion` 的证据解释和
topic-local 文档，没有修改
`registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp`。

Phase 003 已经证明 `PointXYZ`、`PointXYZI` 和 `PointXYZRGB` 满足公共
`RVVXYZAoSFloatLayout` gate（结构数组布局门控），因此 Phase 002 的 public neutral
不再优先归因于代表点型被 gate 静默挡住。把 Phase 002 / 003 的 summary 放在同一张边界图里看，
更合理的解释是：test-support helper、C1/C2 accumulation-only component（只测累加前端组件）
和 production public entry（真实公开入口）不是同一个计时边界；临时 production patch 的 path-hit
（路径命中）、asm attribution（反汇编归因）、fallback（回退路径）和 public wrapper（公开入口包装层）
证据合同不足，不能用 helper positive 反推 production-ready（可接入生产）。

当前 EvidenceDecision（证据决策）保持
`no_production_after_component_positive_public_neutral`。Phase 004 的新增决策是
`production_reentry_requires_new_PI1_contract`：若后续重开 production integration loop
（生产接入闭环），必须先按本阶段定义的 PI1 证据合同冻结范围并重新取得真实 production direct 证据。

## 执行动作

| action | 状态 | 产物 / 证据 | 结论 |
| --- | --- | --- | --- |
| A1 freshness check | done | `make evidence_status`；`log/evidence_registry.json` | 初次检查发现两个 ignored-local QEMU component bench raw log 未登记；已移到 `tmp/rvv-cleanup/tedq-phase004/`，重新检查为 `fresh`。 |
| A2 boundary evidence audit | done | Phase 002 / 003 board summaries | strict component A/B 和 helper full-estimate 为 positive，public entry context / production-public direct 为 neutral，public/helper 表只能作为 mixed-boundary cross-check（混合边界交叉检查）。 |
| A3 path-hit contract | done | 本 result 的“下一次 PI1 证据合同” | 下一次 production 尝试必须先补 path-hit、fallback、production symbol asm 和 repeated board direct，不允许只凭 helper/component positive 接 production。 |
| A4 production reentry decision | done | roadmap / matrix / evaluation 更新 | 本阶段不请求重加 production patch；默认下一 phase 转向 row-source expansion（行来源扩展）诊断。 |

## 证据新鲜度

本阶段运行：

```bash
make evidence_status
```

第一次输出 `unregistered_file`，路径为：

- `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/run_bench_component_std.log`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/run_bench_component_rvv.log`

这两个文件是 QEMU bench raw log（原始日志），不是当前 summary-only evidence（只提交摘要证据）边界内的可提交证据；它们也不支撑当前 performance（性能）结论。为避免 registry 扫描继续把本地 raw log 误判为当前证据，将它们和 `script/__pycache__/` 一起移入：

- `tmp/rvv-cleanup/tedq-phase004/`

随后重新运行 `make evidence_status`，结果为：

```text
evidence registry check: fresh
```

## Boundary Evidence Audit

| 边界 | 证据 | 关键结果 | 证据角色 |
| --- | --- | --- | --- |
| helper full-estimate | `log/board/rvv_accum_full_cloud_repeated/summary.md`、`log/board/component_ablation_repeated/summary.md` | 4K / 64K / 256K median B/A 约 `2.014x / 2.066x / 2.097x`；Phase 003 context 为 `1.979x / 2.093x / 2.115x` | 说明 test-support helper 的 C1/C2 + Eigen solve 在同边界下仍正向。 |
| C1/C2 accumulation-only component | `log/board/component_ablation_repeated/summary.md` | strict A/B median B/A 为 `2.076x / 2.102x / 2.096x`，overall `positive`，doctor clean | 说明前端累加本身有局部收益。 |
| solve-only component | `log/board/component_ablation_repeated/summary.md` | 64K / 256K median 约 `1.000x`，4K 因极小耗时 `unstable` | 说明 Eigen 4x4 solve 和矩阵构造不是当前 RVV 受益点，也不是主要稀释项。 |
| public entry context | `log/board/component_ablation_repeated/summary.md` | 当前 no-production 源码下 public entry Std/RVV median B/A 为 `0.999x / 1.003x / 1.003x` | 只说明当前 public scalar entry 的构建对照，不能写成 RVV production speedup。 |
| Phase 002 production-public direct | `log/board/production_public_full_cloud_repeated/summary.md` | 临时 production dispatch 下 median B/A 为 `1.001x / 1.002x / 1.001x`，overall `neutral`，doctor 为 Errors=1、Warnings=2、Suggestions=3 | 阻止保留临时 production patch，是当前 no-production 决策的直接证据。 |
| public/helper mixed-boundary cross-check | `log/board/component_ablation_repeated/summary.md` | public/helper median：Std 为 `1.768x / 1.819x / 1.821x`，RVV 为 `3.508x / 3.773x / 3.781x` | 只提示 public wrapper / baseline 边界差异明显；不能作为严格 A/B。 |

这个审计把 Phase 002 / 003 的冲突收敛到生产边界问题：局部 C1/C2 前端有收益，但当前证据不能证明 public entry 真实、稳定、低成本地命中了可保留的 RVV production path。

## 下一次 PI1 证据合同

若用户或 reviewer 后续授权重新进入 PI1 production integration plan（生产接入计划），必须先冻结以下合同，再进入 production patch：

| 证据项 | 必须回答的问题 | 最小验收 |
| --- | --- | --- |
| scope（范围） | 只覆盖哪个入口、点型、`Scalar`、layout 和规模 gate | ordered-cloud-pair、representative xyz AoS、`Scalar=float`、小规模 fallback；indices / correspondences 明确保持标量或另开 phase。 |
| path-hit（路径命中） | public entry 是否真实命中 RVV dispatch，而不是只在 test helper 中命中 | production direct gtest 或 test-only hook 能区分 RVV hit / Std fallback；不改变 public API；非 RVV 构建不暴露伪阳性。 |
| fallback（回退路径） | 每个不覆盖条件是否单独回到标量语义 | `Scalar=double`、小输入、layout 不满足、source-indexed、dual-indexed、correspondence-pair 至少有独立 correctness 或明确 not_applicable 证据。 |
| asm attribution（反汇编归因） | RVV 指令是否归属 production hot boundary | 可提交 asm summary 记录边界；若 helper 被 inline 或 clone，summary 说明真实承载符号，不能只写 bench binary 有 RVV 指令。 |
| board repeated direct | public RVV path 是否快于 public scalar path | 重新生成 production-public repeated summary / manifest / doctor / registry；结果必须至少 `weak_positive` 且 doctor Error 为 0。 |
| evidence registry | 新证据是否 fresh 且被文档引用 | `make evidence_status` 不得有未解释 `unregistered_change`、`unregistered_file` 或 `stale_doc_pending_refresh`。 |

在这些条件闭合前，component evidence 仍只是 diagnostic evidence（诊断证据），不能替代 production evidence（生产证据）。

## Optimization Matrix 更新

| candidate family | row source policy | point type / Scalar / layout | board / doctor | decision | next action |
| --- | --- | --- | --- | --- | --- |
| production-boundary path-hit probe | ordered-cloud-pair | `PointXYZ` / `float` / x,y,z AoS | 不新增 board；复用 Phase 002 / 003 summary | `contract_defined` | 只有获得 PI1 授权后才重开 production patch。 |
| public/helper boundary audit | ordered-cloud-pair | `PointXYZ` / `float` | public/helper mixed-boundary context 已审计 | `boundary_mismatch_explains_conflict` | 不把 helper positive 外推成 production-ready。 |
| representative xyz AoS gate | ordered-cloud-pair production candidate scope | `PointXYZ` / `PointXYZI` / `PointXYZRGB` | QEMU gtest pass | `gate_allows_representative_layouts` | 后续 PI1 不再优先按 layout gate blocked 归因。 |
| production dispatch | ordered-cloud-pair production public entry | representative xyz AoS / `Scalar=float` | Phase 002 direct neutral；doctor Errors=1 | `rejected_phase002_neutral` | 保持撤回状态。 |
| row-source expansion | source-indexed / dual-indexed / correspondence-pair | identity first, then non-identity gather | not_started | `phase_deferred + unblocked` | `005-row-source-expansion`。 |

## 诊断证据链

Correctness（正确性）：Std/RVV 双构建各 10 个 gtest 通过，覆盖 public/reference/candidate 对拍、小规模 fallback、`PointXYZI` layout、representative xyz AoS gate、`Scalar=double` 保守路径、source-indexed / dual-indexed / correspondence identity public boundary。

QEMU path evidence（QEMU 路径证据）：QEMU 只用于 correctness 和日志形状；本阶段没有新增 QEMU bench 性能结论。两个旧 component raw log 已移出 topic 证据扫描边界，registry 当前为 fresh。

Board performance（板卡性能）：本阶段没有新增 board run。当前性能事实仍来自 Phase 001 / 002 / 003 summary：helper 和 C1/C2 component 为 positive，production-public direct 为 neutral。

Production boundary（生产边界）：production 源码无 TEDQ diff。Phase 004 只定义下一次 PI1 证据合同，不进入 production integration loop。

## 继续 / 停止判断

Phase 004 的计划动作已闭合，且没有命中 production 修改授权。当前仍有一个低风险、topic-local 的未阻塞方向：`005-row-source-expansion`，用于把 source-indexed、dual-indexed 和 correspondence row source policy（行来源策略）从 identity public boundary 提升到 test-rvv 诊断候选、bench case-filter 和证据计划。该方向不应从 ordered-cloud-pair 自动继承 production 结论，也不应在未获得 PI1 授权前修改 production。

`continue_stop_decision`：本阶段收口，默认下一 phase 为 `005-row-source-expansion`。若用户明确要重开 production integration loop，则必须先按“下一次 PI1 证据合同”冻结范围。

`next_phase_default`：`005-row-source-expansion`。

## 提交边界

| 路径 | 状态 | 说明 |
| --- | --- | --- |
| `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` | unchanged | production 仍保持标量。 |
| `test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/004-production-boundary-probe/result.zh.md` | to-be-staged | 本阶段 result。 |
| `tmp/rvv-cleanup/tedq-phase004/` | local-only | 从 topic 扫描边界移出的 raw log / `__pycache__`，不进入默认提交。 |
