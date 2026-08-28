# Phase 046: vcompress 生产采纳收尾计划

## 阶段意图和边界

用户已确认：板卡上的接入后测试结果只要显示有收益即可采纳。Phase 045 的接入后 production direct（真实生产入口直连）证据显示 public `selectWithinDistance` 5-run board repeated median 为 `2.0435x`，因此本阶段只做 S11 production closeout（生产收尾）：把 Phase 045 从 `PI5_pending_user_confirmation` 刷新为 adopted/current production behavior（已采纳的当前生产行为），并同步 topic-local docs、optimization matrix、roadmap、长期 `doc-rvv` 和 Handoff。

本阶段不改 production 源码，不扩大 `selectWithinDistanceRVV` 覆盖范围，不把 `getDistancesToModel` 写成 RVV adopted，也不把 `PointXYZ` 板卡性能外推到所有 PointXYZ-like 点型。

## 当前状态清单

| 项 | 当前事实 |
| --- | --- |
| production patch | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` 已接入 `selectWithinDistanceRVV` 的 `vcompress` 实现。 |
| correctness | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare` 在 Phase 045 通过，Std/RVV 各 5 个 gtest。 |
| asm | `selectWithinDistanceRVV` 符号 `rvv_instr_count=27`，可见 `vcompress.vm`。 |
| board | Phase 045 production repeated board：public `selectWithinDistance` median `2.0435x`，min/max `2.0407x / 2.0720x`。 |
| Evidence Doctor | select production row clean；两个 Error 属于 out-of-scope `getDistancesToModel` 行。 |
| registry | `production_vcompress_evidence_status` 已复核为 fresh。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test | bench / board | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| production select dispatch with `vcompress` | direct indexed `indices_` | `PointXYZ` board performance；`RVVXYZFloatLayout<PointT>` production gate；`Scalar=float` xyz fields | Phase 045 done | Phase 045 production direct `2.0435x` | select row clean | 本阶段升级为 adopted/current production behavior |
| point type expansion | direct indexed `indices_` | non-`PointXYZ` PointXYZ-like layouts | `PointXYZI` correctness only | dedicated board not started | not started | phase_deferred + unblocked，下一阶段 Phase 050 |
| `getDistancesToModel` current family | direct indexed `indices_` | `PointXYZ` | correctness done | candidate negative | Error | rejected for current family |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 文档事实刷新 | README、evaluation、benchmark/evidence、optimization evidence、roadmap、matrix、`doc-rvv` | 当前文档不再把 Phase 045 写成等待用户确认；只保留历史语境。 |
| Phase 046 result | `result.zh.md` | 记录用户确认、采纳边界、证据路径和下一阶段默认入口。 |
| registry / freshness 检查 | `production_vcompress_evidence_status` | fresh；若文档引用刷新导致 stale，则重跑 record/status target。 |
| pending 术语扫描 | `rg` 限定 topic paths | 当前事实文档无未解决的 PI5 pending 表述；Phase 045 历史 result 可保留“当时等待确认，Phase 046 已解除”。 |

## Evidence Doctor 和 registry 规则

本阶段不生成新 board 数据；继续使用 Phase 045 summary evidence。若文档引用改变造成 registry stale，运行：

```bash
make -C test-rvv/sample_consensus/sac_model_sphere record_production_vcompress_board_evidence_state production_vcompress_evidence_status
```

## 完成条件

Phase 046 完成时：

- `production select dispatch with vcompress` 在 matrix、roadmap、evaluation 和 `doc-rvv` 中为 adopted/current production behavior。
- `doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md` 使用 Phase 045 接入后的板卡数据作为当前生产证据。
- `050-point-type-expansion` 成为下一阶段默认入口，因为点型性能扩展仍未闭合且当前授权范围内未阻塞。

## 板卡复跑预算和决策桶

本阶段不复跑板卡。Phase 045 已有 5-run budget，decision bucket 为 positive-stable。若 closeout 后发现 evidence freshness 失效，只重建 manifest / doctor / registry，不重跑 raw board，除非日志缺失或 Doctor 结果矛盾。

## 继续 / 停止条件

本阶段本身不命中停止条件。完成 closeout 后，按 roadmap 继续 Phase 050 点类型扩展；只有 Phase 050 发现板卡不可用、证据矛盾、生产 gate 需要重新授权扩大 / 收窄，或无值得继续的方向时才暂停。

## 文档更新清单

更新 `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、topic-local role docs、`doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md` 和 current handoff。
