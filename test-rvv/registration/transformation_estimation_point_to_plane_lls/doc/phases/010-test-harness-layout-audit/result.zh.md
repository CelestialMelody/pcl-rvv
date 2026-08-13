# Phase 010 Result: Test Harness Layout Audit

## 执行范围

本阶段按 plan 补齐 test harness layout audit（测试框架布局审计）。实际执行范围只覆盖
test-rvv 测试资产布局：迁移 test/bench 源码到 `src/`，新增 `include/teptpl.h` 聚合入口，
保留旧聚合头作为 compatibility alias（兼容别名），并更新 Makefile、phase matrix 和
evaluation 文档。production header、RVV hot path、dispatch gate、fallback gate、bench case
和 case filter 均未修改。

## 计划动作完成矩阵

| id | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| L0 写 phase plan | done | `doc/phases/010-test-harness-layout-audit/plan.zh.md` | plan 先于 Phase 010 文件迁移存在，范围锁定为 layout-only。 |
| L1 迁移源码入口 | done | `src/test_teptpl.cpp`、`src/bench_teptpl.cpp`、Makefile `SRCS_TEST/SRCS_BENCH`。 | target 名仍为 `test_transformation_estimation_point_to_plane_lls_*` 和 `bench_transformation_estimation_point_to_plane_lls_*`；旧根目录大 cpp 不再作为 Makefile source。 |
| L2 迁移聚合入口并保留兼容别名 | done | `include/teptpl.h`；`test_support_transformation_estimation_point_to_plane_lls.hpp`。 | 新 test/bench include `teptpl.h`；旧头只转发到新入口，不再直接聚合内部 helper。 |
| L3 文档同步 layout audit | done | `doc/phases/README.zh.md`、`optimization-matrix.zh.md`、evaluation “测试框架布局审计”。 | `src/include` adopted，`include/impl` 内部重命名 deferred，weighted sibling 只作为结构 quality bar。 |
| L4 验证 layout 迁移 | done | `git diff --check`；`make -C ... run_test_std`；`make -C ... run_test_rvv`；std bench compile smoke；`make -C ... dump_bench_rvv`。 | diff check 干净；std QEMU correctness 40/40 通过；RVV QEMU correctness 40/40 通过；std/RVV bench compile smoke 通过。 |
| L5 回填 phase result | done | 本文件。 | 阶段闭合，当前默认进入 review。 |

## Layout Audit 决策

| 审计项 | 决策 | 事实和理由 |
| --- | --- | --- |
| `src/` 源码目录 | adopted | `test_transformation_estimation_point_to_plane_lls.cpp` 和 `bench_transformation_estimation_point_to_plane_lls.cpp` 迁移为 `src/test_teptpl.cpp` 与 `src/bench_teptpl.cpp`。 |
| 长 topic 缩写 token | adopted | 使用 `teptpl` 作为 test-rvv 文件 token。它只影响测试资产文件名，不改变 production 符号、target 名或 topic 名。 |
| `include/` 聚合入口 | adopted | 新增 `include/teptpl.h`，文件级注释说明 test-rvv support 边界。 |
| 兼容别名 | adopted | 旧 `test_support_transformation_estimation_point_to_plane_lls.hpp` 保留为 alias，降低旧文档或临时引用迁移风险；新源码不再 include 旧头。 |
| `include/impl` 内部重命名 | deferred | 既有 `test_support/` 已按 common、RVV math、row sources、reductions 和 candidates 拆分。本阶段不把路径迁移、内部重命名和大型 helper 拆分混在一起。 |
| sibling 经验迁移 | adopted as structure quality bar | weighted sibling 的 `src/include` 布局作为结构参照；source-indexed production 方案、证据数字和算法 helper 未迁移。 |

## 验证结果

| 命令 | 结果 | 证据路径 / 边界 |
| --- | --- | --- |
| `git diff --check` | pass | 命令无输出。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_std` | pass，40/40 tests | `log/qemu/run_test_std.log`，本机 generated log，默认不提交。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_rvv` | pass，40/40 tests | `log/qemu/run_test_rvv.log`，本机 generated log，默认不提交。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls USE_PCL_RVV10=0 TARGET_BENCH=bench_transformation_estimation_point_to_plane_lls_std build/riscv/bench_transformation_estimation_point_to_plane_lls_std` | pass | 编译 `src/bench_teptpl.cpp` 的 std bench binary；compile smoke，不运行 bench。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls dump_bench_rvv` | pass | 编译 `src/bench_teptpl.cpp` 并生成 `build/asm/riscv/bench_transformation_estimation_point_to_plane_lls_rvv.asm`；这是 compile / asm shape smoke，不是新性能证据。 |

QEMU correctness（QEMU 正确性验证）只证明构建、路径和功能，不证明目标硬件性能。两个 bench compile smoke
都没有运行 benchmark（性能测试），因此不产生 QEMU timing 结论。

## Evidence Doctor、Registry 和 Freshness

本阶段没有生成新的 benchmark、board summary（板卡摘要）或 checksum summary（校验和摘要），因此没有运行
JSON manifest 形式 Evidence Doctor（证据体检）。`dump_bench_rvv` 生成的反汇编只用于证明新 bench
源路径可编译；本阶段不把它升级为新的 hotspot attribution（热点归属）结论。

`evidence_registry_status=not_available`。本 topic 仍没有 `log/evidence_registry.json`。本轮生成的
`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` 和 `build/asm/...` 是本机验证产物，默认不提交。
既有 production-dispatch 5-run summary 未刷新，继续保持 Phase 000 记录的 summary-only 边界。

## Optimization Matrix 更新

`optimization-matrix.zh.md` 中 `test harness layout migration` 从 `in_progress` 更新为
`done / ready_for_review`。其它候选状态不变：

- fused-formula block-reduction full-cloud production dispatch 仍是当前 production candidate。
- source-indexed、dual-indices、correspondences 仍保持 historical diagnostic / no-production。
- reference / production-detail boundary cleanup 保持 done。

## 板卡复跑预算与决策桶

本阶段计划预算为 0 次板卡复跑，实际复跑 0 次。理由是 layout 迁移没有改变 RVV hot path、bench case、
case filter、dispatch gate 或 fallback gate。既有 production-dispatch 5-run decision bucket 保持
`positive` within current boundary；若后续拆分 bench case 或改 hot path，需要新建 phase 并定义 rerun budget。

## Dirty Isolation

当前可审查 topic diff 包含：

```text
doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md
registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp
test-rvv/registration/transformation_estimation_point_to_plane_lls/
```

`.agents/skills/...` 中已有 agent asset patch 属于 separate review，不纳入本 phase 的 topic 产物。
`build/`、`log/` 和 raw output 仍是本机生成物 / 忽略边界，默认不提交。

## Continue / Stop Decision

当前 phase 完成，`unblocked_next_actions=none` within this phase。停止条件命中：

- L0-L5 已闭合。
- 继续推进需要进入新的范围：拆分大型 test/bench 源文件、迁移 `test_support/` 到 `include/impl/`、
  接入 evidence registry、压缩 production RVV helper、重跑板卡或扩展 row source。

`next_phase_default=ready_for_review`。若 reviewer 要求继续当前 topic，推荐按具体范围新建 phase：

- `020-test-source-split`：按 public semantics、candidate、row source、production direct 和 bench harness
  拆分大型源文件。
- `020-evidence-registry-adoption`：接入 topic-local `log/evidence_registry.json`。
- `020-helper-shape-review`：只审查 / 压缩 production RVV block helper size，并重跑 correctness。
