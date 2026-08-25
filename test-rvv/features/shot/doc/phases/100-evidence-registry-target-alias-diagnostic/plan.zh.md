# Phase 100 计划：evidence registry / target alias diagnostic

## 阶段意图和边界

本阶段补齐 Phase 090 发现的结构缺口：topic-local evidence registry（证据登记表）/ freshness check（新鲜度检查）和常用 target alias（target 别名）。范围只包括 `test-rvv/features/shot/Makefile` 与 topic-local 文档；不修改 production（生产源码）、C++ candidate helper 或 bench case 逻辑。

## 当前状态

| area | 当前状态 | 本阶段目标 |
| --- | --- | --- |
| Evidence Doctor | `run_evidence_doctor` 已生成 manifest 和 doctor。 | 增加 record/check target，把 summary / manifest / doctor 的状态登记到 `log/evidence_registry.json`。 |
| correctness alias | 只有 aggregate `run_test_compare`；手动 gtest filter 不稳定。 | 增加 public / normalize / shape-bin / interpolation / color alias。 |
| board repeated alias | 可手动 `run_board_bench_compare BENCH_ARGS=...`。 | 增加常用 single-case board evidence alias，避免误用 `CASE_FILTER`。 |
| docs | Phase 090 文档已写明缺口。 | 更新 testing overview、benchmark/evidence、README、phase index、roadmap、matrix。 |

## 计划动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| Makefile registry target | `record_board_evidence_state`、`run_board_shot_evidence`、`evidence_status`、`check_evidence_doc_refs` | `make -n` 可展开；实际 `record_board_evidence_state` 能基于当前 log 写 registry。 |
| correctness aliases | `run_test_public`、`run_test_normalize`、`run_test_shape_bin`、`run_test_interpolation`、`run_test_color` | 对应 target 实际运行通过或至少 aggregate run 验证仍通过。 |
| board case aliases | `run_board_shape_bin_indexed_evidence`、`run_board_interpolation_bin_selection_evidence`、`run_board_color_rgb_lut_evidence` | target 使用 `BENCH_ARGS` 传 case-filter，并在本地 fetch / doctor / record。 |
| docs sync | Phase 100 result、README、testing overview、benchmark/evidence、roadmap、matrix、queue | 默认恢复入口一致。 |

## Evidence Doctor 和 registry 策略

Registry 只记录生成证据文件状态，不把 raw log 自动变成提交候选。`evidence_status` 使用 `--fail-on never`，用于 worker 恢复时暴露 `unregistered_change` / `unregistered_file`；`check_evidence_doc_refs` 使用 `--require-doc-ref` 但默认不作为硬失败目标。

## 完成条件

- 新 target 不改变已有 `run_test_compare`、`dump_bench_rvv` 和 `run_evidence_doctor` 行为。
- 当前本地 `log/board/analyze_bench_compare.log`、`evidence_manifest.json`、`evidence_doctor.md` 能被 registry 记录。
- 文档写清 registry 是 freshness aid（新鲜度辅助），不是 production evidence。

## Continue / stop 条件

若 Phase 100 验证通过，当前 topic 在未授权 production 的范围内已主要剩下两类动作：继续性能搜索需要新的 evidence question，或用户授权 PI2 shape-bin indexed production probe。若仍发现别的 topic-local unblocked 缺口，继续下一 phase；否则输出明确 handoff，不自行修改 production。
