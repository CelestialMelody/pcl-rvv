# Phase 030 Result: Legacy Cleanup And Evidence Registry

## 执行范围

本阶段按 plan 做 legacy cleanup（旧入口清理）和 evidence registry（证据登记表）接入。实际执行范围只覆盖
当前 topic 的测试资产入口、topic-local 文档和证据 freshness（新鲜度）状态：

- 删除根目录 evaluation legacy pointer（旧路径指针）。
- 删除旧 `test_support_transformation_estimation_point_to_plane_lls.hpp` compatibility alias（兼容别名）。
- 新增 `log/evidence_registry.json`，登记当前 board summary-only evidence（板卡摘要证据）和本轮 QEMU correctness log（QEMU 正确性日志）。
- 更新 phase README、optimization matrix、roadmap、evaluation 和长期 topic doc。

production header、RVV hot path、dispatch gate、fallback gate、测试逻辑、bench case 和 case filter 均未在本阶段修改。

## 计划动作完成矩阵

| id | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| C0 写 phase plan | done | `doc/phases/030-legacy-cleanup-and-evidence-registry/plan.zh.md` | plan 先于 Phase 030 文件删除和 registry 写入存在，范围锁定为 legacy / registry。 |
| C1 删除无依赖旧入口 | done | `rg -n "transformation_estimation_point_to_plane_lls-evaluation\\.zh\\.md\\|test_support_transformation_estimation_point_to_plane_lls\\.hpp\\|include/teptpl\\.h" ...`；删除根目录旧文件和旧 alias 头。 | 当前源码、Make target 和脚本不依赖旧入口；历史 phase 文档保留当时事实，不再作为当前入口。 |
| C2 接入 evidence registry | done | `test-rvv/script/evidence_registry.py record/check`；`log/evidence_registry.json`。 | registry 记录 board summary 和 QEMU correctness log 的 size/hash/doc refs；最终 check 为 fresh。 |
| C3 同步文档 | done | `doc/phases/README.zh.md`、`optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、evaluation、topic doc。 | 当前恢复入口、Traceability Map 和 evidence policy 均指向 registry；legacy / alias 状态不再冲突。 |
| C4 验证 | done | `git diff --check`；`run_test_std`；`run_test_rvv`；registry check。 | diff check 干净；std/RVV QEMU correctness 均 40/40 通过；registry check fresh。 |
| C5 回填 phase result | done | 本文件。 | 阶段闭合。 |

## Legacy / Alias 决策

| 项 | 当前决策 | 事实和理由 |
| --- | --- | --- |
| 根目录 evaluation legacy pointer | deleted | 当前主归属是 `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md`。恢复检查未发现当前源码、脚本或 Make target 依赖旧根目录文件。 |
| 旧 test support alias | deleted | `src/test_teptpl.cpp` 和 `src/bench_teptpl.cpp` 已 include `include/teptpl.h`；旧头只转发新入口，没有独立证据角色。 |
| 历史 phase 文档 | retained as historical fact | Phase 010/020 plan/result 仍记录当时保留 alias / pointer 的事实；不回写历史结果制造“当时已删除”的假象。 |
| 重新引入条件 | deferred / reviewer-triggered | 若外部脚本、reviewer workflow 或用户明确依赖旧入口，可新开 phase 恢复 alias，并记录依赖证据和删除条件。 |

## Evidence Registry 和 Freshness

本阶段使用现有通用脚本：

```text
test-rvv/script/evidence_registry.py
```

登记文件：

| 路径 | backend | evidence_role | run_label |
| --- | --- | --- | --- |
| `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/production_dispatch_generic_representative_5run_summary.md` | board | `post_production_performance_summary` | `production-dispatch-generic-representative-5run` |
| `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/block_fused_formula_5run_summary.md` | board | `diagnostic_performance_summary` | `block-fused-formula-5run` |
| `test-rvv/registration/transformation_estimation_point_to_plane_lls/log/qemu/run_test_std.log` | qemu | `correctness_log` | `qemu-correctness-std-rvv-phase-030` |
| `test-rvv/registration/transformation_estimation_point_to_plane_lls/log/qemu/run_test_rvv.log` | qemu | `correctness_log` | `qemu-correctness-std-rvv-phase-030` |

最终检查命令：

```text
python3 test-rvv/script/evidence_registry.py check \
  --registry test-rvv/registration/transformation_estimation_point_to_plane_lls/log/evidence_registry.json \
  --scan-glob 'test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/**/*.md' \
  --scan-glob 'test-rvv/registration/transformation_estimation_point_to_plane_lls/log/qemu/*.log' \
  --doc doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md \
  --doc test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md \
  --doc test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/phases/README.zh.md \
  --require-doc-ref \
  --fail-on any
```

结果为 `evidence registry check: fresh`。registry 只记录 digest（摘要指纹）和文档引用状态；它不替代
Evidence Doctor（证据体检）manifest，也不改变现有性能结论。

## 验证结果

| 命令 | 结果 | 证据路径 / 边界 |
| --- | --- | --- |
| `git diff --check` | pass | 命令无输出。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_std` | pass，40/40 tests | `test-rvv/registration/transformation_estimation_point_to_plane_lls/log/qemu/run_test_std.log`；QEMU correctness，不是性能证据。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_rvv` | pass，40/40 tests | `test-rvv/registration/transformation_estimation_point_to_plane_lls/log/qemu/run_test_rvv.log`；QEMU correctness，不是性能证据。 |
| registry check | pass / fresh | `test-rvv/registration/transformation_estimation_point_to_plane_lls/log/evidence_registry.json`。 |

本阶段没有运行 QEMU bench、反汇编或板卡 benchmark（性能测试），因为改动不触碰 RVV hot path、bench case、
case filter、dispatch gate 或 fallback gate。

## Evidence Doctor、板卡预算和决策桶

本阶段没有生成新的 benchmark、board summary、checksum summary（校验和摘要）或 asm attribution（反汇编归属），
因此没有运行 JSON manifest 形式 Evidence Doctor。既有 board summary 仍是 summary-only evidence。

板卡复跑预算为 `0`，实际复跑 `0`。既有 production-dispatch decision bucket 保持
`positive` within current production candidate boundary。

## Dirty Isolation

当前可审查 topic diff 包含：

```text
doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md
registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp
test-rvv/.gitignore
test-rvv/registration/transformation_estimation_point_to_plane_lls/
```

`test-rvv/.gitignore` 同时包含已有 weighted topic allowlist 修改；本阶段只追加当前 topic 的
`log/evidence_registry.json` 白名单。worktree 中仍存在 weighted topic dirty 和 generated board evidence，
它们属于 separate review，不纳入本 phase 的 topic 结论。`build/`、raw logs 和未被文档引用的 generated
outputs 仍默认不提交。

## Optimization Matrix / Roadmap 更新

`optimization-matrix.zh.md` 中 `legacy cleanup / evidence registry adoption` 更新为 `done / ready_for_review`。
roadmap 中 evidence registry adoption 和 legacy cleanup 更新为 `adopted`。剩余候选仍是独立后续范围：

- `040-helper-shape-review`：会触碰 production RVV helper，需独立 correctness / asm / 板卡风险判断。
- `040-test-source-split`：大规模 test / bench 搬迁，需保留 target 名并重跑 correctness / bench compile smoke。
- `040-doc-suite-parity`：只有 reviewer 需要更完整 topic-local doc suite 时再做。
- 更多点型、`Scalar=double`、indexed / correspondences production follow-up 仍需独立证据，不能继承 full-cloud 结论。

## Continue / Stop Decision

当前 phase 完成，`unblocked_next_actions=none` inside Phase 030。停止条件命中：

- C0-C5 已闭合。
- 删除旧入口后 std/RVV correctness 仍通过。
- registry check fresh。
- 当前 production candidate、bench case、dispatch gate 和 fallback gate 没有变化。

`continue_stop_decision=stop_for_review`。`next_phase_default=ready_for_review`。若 reviewer 要求继续当前 topic，
从 roadmap 选择一个新的 `040-*` phase，并先写 plan。
