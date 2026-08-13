# Phase 050 Structure Layout Result

## 结果摘要

本阶段按 `050-structure-layout/plan.zh.md` 合并闭合两个默认恢复队列项：

| queue item | result | evidence |
| --- | --- | --- |
| `test-source-split` | adopted | gtest 拆成四个 `src/test_teptpl_*.cpp`；Makefile `SRCS_TEST` 保留同一 target 名。 |
| `internal-helper-layout` | adopted | 旧 `test_support/*.hpp` 迁到 `include/impl/teptpl_*.hpp`；新增 test / bench 专用聚合入口。 |

这是 layout-only（仅布局）阶段：没有修改 production API（生产公开接口）、production hot path、bench label、case-filter、board summary 或 weighted topic。

## 实际执行范围

| plan action | status | actual files / note |
| --- | --- | --- |
| 创建 050 plan | done | `doc/phases/050-structure-layout/plan.zh.md` 先于结构迁移存在。 |
| 迁移旧内部 helper | done | `include/impl/teptpl_common.hpp`、`teptpl_rvv_math.hpp`、`teptpl_row_sources.hpp`、`teptpl_reductions.hpp`、`teptpl_candidates.hpp`。 |
| 新增 gtest 聚合入口和 helper | done with small deviation | `include/test_teptpl.h`、`include/impl/teptpl_test_helpers.hpp`。plan 中提到的独立 `teptpl_assertions.hpp` 没有新增；assertions 保留在 `teptpl_test_helpers.hpp`，避免拆出过小头文件。 |
| 拆分 gtest 源码 | done | `src/test_teptpl_public_semantics.cpp`、`src/test_teptpl_candidates.cpp`、`src/test_teptpl_production_direct.cpp`、`src/test_teptpl_row_sources.cpp`。 |
| bench 收敛为薄入口 | done | `src/bench_teptpl.cpp` 只转发到 `run_teptpl_bench`；bench fixture / component / case registry 位于 `include/impl/teptpl_bench_*.hpp`。 |
| 文档和恢复队列同步 | done | README、code map、correctness / benchmark / optimization evidence、evaluation、roadmap、matrix、phase README 和 `doc-rvv` 均已同步到 `include/impl` 与 split source。 |

旧根目录 test / bench 源文件、旧 alias 头和旧 `test_support/` 目录不再作为当前 include 入口；没有新增 compatibility alias（兼容别名）。

## 验证结果

| command | result | interpretation |
| --- | --- | --- |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_std TEST_STD_OUTPUT_FILE=/tmp/teptpl_phase050_run_test_std.log` | pass, 40/40 | std correctness；临时日志不覆盖 registry-tracked logs。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_rvv TEST_RVV_OUTPUT_FILE=/tmp/teptpl_phase050_run_test_rvv.log` | pass, 40/40 | RVV correctness；临时日志不覆盖 registry-tracked logs。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls USE_PCL_RVV10=0 TARGET_BENCH=bench_transformation_estimation_point_to_plane_lls_std build/riscv/bench_transformation_estimation_point_to_plane_lls_std` | pass | std bench compile smoke；不运行 bench，不产生性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls dump_bench_rvv` | pass | RVV bench compile / asm smoke；生成本地 asm，不作为新热点归因。 |
| `git diff --check -- test-rvv/registration/transformation_estimation_point_to_plane_lls doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` | pass | 当前 topic 和长期文档 diff 无 whitespace error。 |
| `python3 test-rvv/script/evidence_registry.py check ... --require-doc-ref --fail-on any` | pass, fresh | 本阶段未生成新 board summary；registry 仍能匹配当前文档引用。 |

Evidence Doctor（证据体检）：本阶段没有生成新的 benchmark summary、board summary、checksum summary 或 EvidenceDecision 数值表。人工检查结果为 layout-only / no-new-performance-evidence：Errors 0、Warnings 0、Suggestions 0。旧 board summary 的 metadata incomplete 状态不变，不因此升级或降级性能结论。

## Optimization Matrix 更新

`doc/phases/optimization-matrix.zh.md` 新增 / 更新 `structure layout split` 行：

- decision：`done / adopted`。
- unblocked next action：`test-source-split` 和 `internal-helper-layout` 均为 none。
- remaining candidates：production helper shape review、row-source production carry-over、更多点型和 `Scalar=double` 都需要 production 或证据范围扩展，不能在 layout-only phase 内继续。

## Roadmap 更新

`doc/optimization-roadmap.zh.md` 已把 `roadmap_default_recovery_queue` 更新为：

1. `050-structure-layout/test-source-split`：adopted。
2. `050-structure-layout/internal-helper-layout`：adopted。

`ready_for_review_validity_checked` 只保留为检查标签，不再作为默认恢复动作。Phase 050 后，当前没有仍在 topic-local 测试资产 / 文档边界内的 high-priority `phase_deferred + unblocked` 结构动作。

## Continue / Stop Decision

`continue_stop_decision=stop_for_review_after_structure_queue_closed`。

`stop_condition_hit`：

- Phase 050 计划内 layout actions 已闭合并通过本地验证。
- 继续到 production helper shape review 会触碰 production hot path。
- 继续到 row-source production、更多点型或 `Scalar=double` 会扩大证据范围并需要独立 phase。
- 继续新增 Evidence Doctor wrapper 或 topic-local scripts 需要新的 repeated board / manifest generation 需求；当前没有新性能证据输入。

`next_phase_default=review`。若 reviewer 或用户要求继续当前 topic，建议从以下独立 phase 中选择：

| next phase | resume condition |
| --- | --- |
| `060-helper-shape-review` | 合入前要求压缩 production RVV helper 或减少 A/B/C/N 重复 load/formula。 |
| `060-row-source-family-carryover` | 要继续 source-indexed、dual-indices 或 correspondences production exploration。 |
| `060-pointtype-or-scalar-expansion` | 要补更多 f32 AoS 点型板卡证据或探索 `Scalar=double`。 |

## Dirty Isolation

本阶段只把当前 topic 测试资产、topic-local docs 和对应 `doc-rvv` 长期文档作为允许范围。已有 dirty production header、weighted topic、`.agents` 资产和其它模块文档均未作为本阶段编辑对象，也没有回滚。
