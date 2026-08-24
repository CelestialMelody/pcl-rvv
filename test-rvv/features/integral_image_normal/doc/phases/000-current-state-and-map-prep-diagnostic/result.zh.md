# Phase 000 Result: Current State And Map-Prep Diagnostic

## 执行范围

本阶段按 `plan.zh.md` 关闭 `computeFeature()` 开头的 map-preparation diagnostic（地图预处理诊断）。实际覆盖范围保持不变：

- 入口：仅测试使用的 map-prep helper，不修改 production（生产源码）。
- row source（行来源）：按 `width * height` 顺序扫描的 organized image（有组织图像）。
- 点类型 / 布局：合成连续 `float z` buffer，不外推到任意 `PointInT` 字段布局。
- 片段：depth-change map（深度突变图）和 distance-map initialization（距离图初始化）。
- 未覆盖：distance transform（距离传播）、`computeFeatureFull()`、`computeFeaturePart()`、indices 子集、border mirror、normal solver、真实 dispatch（分流逻辑）和泛型点类型 gate。

## 计划动作回填

| action | status | command / artifact | evidence / result |
| --- | --- | --- | --- |
| RED correctness tests | done | `src/test_integral_image_normal.cpp` | 首次构建在 candidate API 缺失时失败，作为 red 阶段证据；随后补 helper。 |
| candidate helper | done | `include/integral_image_normal.h`、`include/impl/integral_image_normal_map_prep.hpp` | `make -C test-rvv/features/integral_image_normal run_test_compare` 通过 Std/RVV 对拍。 |
| bench wrapper | done | `src/bench_integral_image_normal.cpp` | QEMU 只用于日志形状 smoke；性能结论只采纳 board repeated。 |
| asm check | done | `make -C test-rvv/features/integral_image_normal dump_test_rvv` | `build/asm/riscv/test_integral_image_normal_rvv.asm` 中可归因到候选的 RVV 指令包括 `vle32.v`、masked `vse8.v`、`vmseq`、`vmerge`、`vsetvli`。 |
| board smoke / bench | done | `make -C test-rvv/features/integral_image_normal board_smoke BENCH_ARGS='50'`；随后 5-run `run_board_bench_compare` | 板卡 correctness 通过；5-run repeated summary 稳定 positive。 |
| Evidence Doctor | done | `python3 test-rvv/script/evidence_doctor.py --manifest test-rvv/features/integral_image_normal/log/board/evidence_manifest.json --output test-rvv/features/integral_image_normal/log/board/evidence_doctor.md --fail-on never` | Errors=0，Warnings=0，Suggestions=0。 |

## 正确性、反汇编和板卡证据

| evidence layer | current result | path |
| --- | --- | --- |
| QEMU correctness（QEMU 正确性） | Std/RVV helper 与标量参考一致；QEMU timing 不进入性能结论。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| asm attribution（反汇编归属） | RVV binary 的候选范围出现向量 load、mask compare、masked byte store 和 `vsetvli`。 | `build/asm/riscv/test_integral_image_normal_rvv.asm` |
| board correctness（板卡正确性） | board smoke 中 unit test 通过。 | `log/board/run_test.log` |
| board performance（板卡性能） | 5-run repeated board summary 稳定正向。 | `log/board/repeated-summary.md`、`log/board/evidence_manifest.json` |
| Evidence Doctor（证据体检） | 脚本检查无 Error / Warning / Suggestion。 | `log/board/evidence_doctor.md` |

5-run 统计：

| case | min speedup | median speedup | mean speedup | max speedup | checksum | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `map_prep_320x240` | 5.47x | 5.50x | 5.52x | 5.57x | `1.7621e+07` | positive |
| `map_prep_641x481_tail` | 5.14x | 5.29x | 5.27x | 5.39x | `1.41569e+08` | positive |

旧的 single-run `log/board/analyze_bench_compare.log` 只保留为历史 quick smoke。当前结论以 `repeated-summary.md`、`evidence_manifest.json` 和 `evidence_doctor.md` 为准。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | diagnostic；manifest 中的 `strict_ab` 表示同边界 A/B 对比合同，不表示 production evidence。 |
| A/B boundary | test helper。Std 和 RVV 两侧使用同一 bench wrapper、同一合成输入、同一 timer boundary（计时边界）和同一 checksum policy（校验策略）。 |
| 当前决策问题 | RVV-vs-scalar for map-prep candidate。 |
| diagnostic 是否可外推到 production | 只能外推为“值得做 PI1 生产接入计划”。不能外推为 production-ready，因为真实 `PointInT` 字段 offset、`input_->points` 布局、indices、后续 distance transform 和 normal 输出尚未验证。 |
| comparison-boundary / baseline mismatch 风险 | 当前 repeated board 对 diagnostic helper 是同边界；与 production 之间仍存在 baseline mismatch 风险。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本轮结果为 positive，因此允许进入 PI1 计划；若后续 production direct 证据低于阈值，应保留补丁等待用户 PI5 判断，不从本 diagnostic 直接推出 no-production。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前是第一候选，clean adoption 首先需要 production direct Std/RVV、fallback、asm 和 board 证据；若后续出现第二个 map-prep RVV family，再补同一 production boundary 内的 RVV-vs-RVV detail A/B。 |

## Evidence Doctor 和 Registry

- Evidence Doctor result：Errors=0，Warnings=0，Suggestions=0。
- `taskset`、`governor`、`freq`、`temperature`、`binary_hash` 目前在 manifest 中记录为 `not_recorded`；本轮 doctor 规则未把这些标成 warning，但 production direct 阶段应补齐或写清缺失影响。
- evidence registry status：fresh。Phase 000 初次 closeout 时先人工标记 repeated summary / manifest / doctor 为 fresh；本轮已补 `record_board_evidence_state` 和 `evidence_status` target，并记录到 `log/evidence_registry.json`。raw logs 仍默认 local-only。

后续补齐状态：topic Makefile 已新增 `run_board_evidence_doctor`、`record_board_evidence_state` 和 `evidence_status`。`record_board_evidence_state` 会记录 `log/board/repeated-summary.md`、`log/board/evidence_manifest.json` 和 `log/board/evidence_doctor.md` 到 `log/evidence_registry.json`。这些文件仍按 summary-only 策略默认留在 ignored-local evidence 中。

## Doc Suite Closeout

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | README 已列当前结论、命令和文档路径 | topic_navigation role | adopted | `README.zh.md` | 进入 PI2 时补 production direct 命令 |
| testing overview | 已拆出 `doc/testing-overview.zh.md` | testing_overview role | adopted | target 粒度审计覆盖 Makefile / board.mk / script | PI2 后补 production target |
| correctness tests | 已拆出 `doc/correctness-tests.zh.md` | correctness_tests role | adopted | 2 个 gtest 均有字典 | PI2 后补 fallback / production direct TEST |
| benchmark and evidence | 已拆出 `doc/benchmark-and-evidence.zh.md` | benchmark_and_evidence role | adopted | board repeated summary / manifest / doctor 路径可定位 | production bench 另建 run label |
| optimization evidence | 已拆出 `doc/optimization-evidence.zh.md` | optimization_evidence role | adopted | candidate 状态与 matrix 一致 | 后续候选增量更新 |
| test support code map | 已拆出 `doc/test-support-code-map.zh.md` | test_support_code_map role | adopted | include / impl / src / script 路径可定位 | production patch 后更新调用图 |
| production topic doc | 当前无 production patch | `doc-rvv` 只适用于 adopted production behavior | not_applicable with evidence | Phase 000 是 diagnostic | PI5 通过并经用户采纳后再创建 |

## 阶段决策

`map-prep RVV masked stores` 在 Phase 000 的 diagnostic boundary 下判为 `partial-production-candidate`：

- correctness：通过。
- asm attribution：通过。
- board performance：positive，5-run speedup 稳定大于 5x。
- production boundary：未闭合，不能直接修改长期 `doc-rvv` 主题文档，也不能写成 production-ready。

`continue_stop_decision`：Phase 000 完成；下一阶段默认进入 `010-pi1-production-integration-plan`。PI1 只写生产接入计划和门禁，不修改 production。真正进入 PI2 修改 `features/include/pcl/features/impl/integral_image_normal.hpp` 前需要用户确认，因为该动作会越过当前 diagnostic topic-local 边界。
