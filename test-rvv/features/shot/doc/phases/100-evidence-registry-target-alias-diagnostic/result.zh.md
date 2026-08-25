# Phase 100 结果：evidence registry / target alias diagnostic

## 执行范围

本阶段只修改 `test-rvv/features/shot/Makefile` 和 topic-local 文档，不修改 C++ helper、bench case 逻辑或 production（生产源码）。新增 target alias（target 别名）用于固定已有 gtest filter / bench case-filter；新增 registry target（证据登记 target）用于记录 summary / manifest / doctor 的文件状态。

## 计划动作回填

| action | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| correctness aliases | done | `run_test_public`、`run_test_normalize`、`run_test_shape_bin`、`run_test_interpolation`、`run_test_color` dry-run 展开正确；最终 `run_test_compare` 已验证 Std / RVV 两侧各 12 个 gtest 通过。 | alias 只封装 gtest filter，不改变测试内容。 |
| board case aliases | done | `run_board_shape_bin_indexed_evidence`、`run_board_interpolation_bin_selection_evidence`、`run_board_color_rgb_lut_evidence`。 | alias 使用 `BENCH_ARGS`，避免误用 `CASE_FILTER`。 |
| per-run output dirs | done | `log/board/board-shot-interpolation-bin-selection-diagnostic-phase080/` | 代表 alias 验证使用独立 run-label 目录，避免覆盖根 `log/board`。 |
| registry record | done | `record_board_evidence_state` 写入 `log/evidence_registry.json`。 | registry 记录 summary / manifest / doctor 三个文件。 |
| evidence status | partial then fixed | 初版扫描根 `log/board/*` 时暴露未登记历史残留；Makefile 已收窄到 `log/board/board-shot-*`。 | 根目录日志降级为历史残留，不作为 freshness target。 |

## 实际 alias 验证

代表性执行：

```bash
make -C test-rvv/features/shot run_board_interpolation_bin_selection_evidence
```

结果写入：

- `log/board/board-shot-interpolation-bin-selection-diagnostic-phase080/analyze_bench_compare.log`
- `log/board/board-shot-interpolation-bin-selection-diagnostic-phase080/evidence_manifest.json`
- `log/board/board-shot-interpolation-bin-selection-diagnostic-phase080/evidence_doctor.md`
- `log/evidence_registry.json`

该 run 的 board summary 为 Std 4.4264 ms、RVV 5.3635 ms、0.83x，checksum match（校验和一致）。这不是 Phase 080 原 3-run budget 的新增采纳依据，只是 Phase 100 alias / registry 验证；它进一步支持 Phase 080 的 `attempted / unstable / not recommended` 边界。

## Evidence Doctor（证据体检）

代表 alias 的 per-run doctor 为 Errors=1、Warnings=1、Suggestions=0：

- Error：`ba_degradation_frequency`，B/A=0.825x，1/1 低于 1。
- Warning：`low_run_count`。

处理动作：该 run 不能支撑 production evidence（生产证据）或严格性能结论，只作为 registry / alias target 的可运行性验证和 Phase 080 候选降级的附加迹象。

## Registry / freshness 结果

`log/evidence_registry.json` 记录了 `board-shot-interpolation-bin-selection-diagnostic-phase080` run label 下的 summary / manifest / doctor。初版 `evidence_status` 扫描根 `log/board/analyze_bench_compare.log`、`evidence_manifest.json` 和 `evidence_doctor.md` 时报告 `unregistered_file`；这些根文件来自历史 overwrite flow（覆盖式流程），已从 Phase 100 freshness target 中移除，后续以 `board-shot-*` run-label 目录为准。

## Target 粒度审计更新

| target 类别 | decision | evidence |
| --- | --- | --- |
| correctness aliases | adopted | 五个 `run_test_*` alias 已接入。 |
| board repeated / case aliases | adopted for representative cases | 三个 `run_board_*_evidence` alias 已接入；当前不是 repeated summary，只是 single-case evidence flow。 |
| doctor / registry aliases | adopted for per-run directories | `record_board_evidence_state`、`evidence_status`、`check_evidence_doc_refs` 已接入。 |
| historical root `log/board` | not_applicable for freshness | 根路径日志是覆盖式历史残留，不作为 registry scan source。 |

## Continue / stop decision

- `continue_stop_decision`：stop due to authorization boundary（因授权边界停止）。
- `stop_condition_hit`：继续高价值 production integration（生产接入）需要用户明确授权修改 `features/include/pcl/features/impl/shot.hpp`；当前未授权范围内的高优先级 test-only 结构缺口已经闭合，插值 / color staging 候选不建议继续作为生产探针。
- `next_phase_default`：`PI2-shape-bin-indexed-production-probe` only if explicitly authorized；否则保持 production untouched。

## Handoff note

若用户授权 PI2，恢复入口是 `PI1-shape-bin-indexed-production-probe-plan/plan.zh.md`，范围只限 `createBinDistanceShape` 的 indexed normal gather。若不授权 production，当前 topic 应保持 diagnostic state（诊断状态），不再默认追加 interpolation scalar-tail 或 color staging 局部优化。
