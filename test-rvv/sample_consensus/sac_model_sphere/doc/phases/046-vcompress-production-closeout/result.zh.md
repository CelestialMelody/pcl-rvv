# Phase 046: vcompress 生产采纳收尾结果

## 执行范围

本阶段把 Phase 045 的 `selectWithinDistanceRVV` `vcompress` production patch 从 PI5 用户检查点升级为 adopted/current production behavior（已采纳的当前生产行为）。触发条件是用户明确确认：接入后板卡测试结果若显示收益即可采纳；Phase 045 已有接入后 public `selectWithinDistance` 5-run board repeated median `2.0435x`。

本阶段没有修改 production 源码，也没有扩大覆盖范围。性能采纳范围仍是 direct indexed `indices_`、registered single-float xyz layout、32-bit byte offset gate 和 `PointXYZ` board case。

## 动作回填

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| Phase 045 PI5 决策解除 | done | 用户已确认“板卡上的测试结果如果显示有收益即可采纳”，Phase 045 positive-stable production data 支撑采纳。 |
| 长期 `doc-rvv` 刷新 | done | `doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md` 记录 `vcompress` 为当前采用实现，数据来自 Phase 045 接入后板卡 evidence。 |
| topic-local 文档同步 | done | README、evaluation、testing overview、benchmark/evidence、optimization evidence、roadmap 和 matrix 均把 Phase 045/046 写成 adopted。 |
| 下一阶段恢复入口 | done | Phase 050 `point-type-expansion` 继续处理非 `PointXYZ` 点型性能证据。 |

## EvidenceDecision

`continue_stop_decision`：`continue_to_next_phase`。

`production select dispatch with vcompress` 的当前 decision 为 `adopted/current production behavior`。采用理由：

- correctness（正确性）：Phase 045 `run_test_compare` Std/RVV 两侧各 5 个 gtest 通过。
- asm attribution（反汇编归属）：`selectWithinDistanceRVV` 符号级 RVV instruction count 为 `27`，可见 `vcompress.vm`。
- board performance（板卡性能）：public `selectWithinDistance` 5-run B/A 为 `2.0409, 2.0407, 2.0720, 2.0435, 2.0588`，median `2.0435x`，全 run 正向。
- Evidence Doctor（证据体检）：select production row 无 Error / Warning；两个 Error 均属于未采纳的 `getDistancesToModel` 行。

## 仍未覆盖的范围

| 范围 | 当前状态 | 下一步 |
| --- | --- | --- |
| `PointXYZI` / 其它 PointXYZ-like 点型性能 | correctness 已覆盖 `PointXYZI`，dedicated board performance 未闭合 | Phase 050 补 bench point-type 参数、manifest point_type 解析和 board repeated evidence。 |
| `getDistancesToModel` RVV | 当前 scratch + scalar sqrt/store family rejected | 只有出现 RVV sqrt/helper 审计或新的 dense-store 消融时恢复。 |
| 其它 row source / `Scalar=double` / 非 xyz layout | 不在当前 production gate 范围内 | 需要独立 phase 和证据，不从 Phase 045/046 外推。 |

## Evidence Registry

本阶段沿用 Phase 045 summary evidence：

- `test-rvv/sample_consensus/sac_model_sphere/doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_sphere/doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_sphere/doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-doctor.json`

Raw board logs 仍为 local-only，不进入默认提交边界。

## Continue / Stop Decision

`stop_condition_hit`：none。

`next_phase_default`：`050-point-type-expansion`。理由是 production gate 已是 `RVVXYZFloatLayout<PointT>` 泛型 layout gate，但当前板卡性能只证明 `PointXYZ`。点型性能扩展仍在当前 topic 授权范围内，板卡当前可用，因此继续推进。
