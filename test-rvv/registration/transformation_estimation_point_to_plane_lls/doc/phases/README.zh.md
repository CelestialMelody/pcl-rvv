# transformation_estimation_point_to_plane_lls Phase Loop

本目录记录 `test-rvv/registration/transformation_estimation_point_to_plane_lls`
的可恢复优化阶段。阶段文档只保存计划、执行事实、Evidence Doctor（证据体检）
和继续 / 停止决策；长期 production 行为仍以
`doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md`
为主归属，函数级决策审计以
`test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md`
为主归属。

## 当前恢复入口

| phase | 状态 | 默认恢复动作 |
| --- | --- | --- |
| `000-current-state-and-gaps` | done | 已验证 reference / production-detail 边界清理。 |
| `010-test-harness-layout-audit` | done / ready_for_review | 已迁移 test/bench 源码到 `src/`，新增 `include/teptpl.h` 聚合入口，并验证 std/RVV correctness 与 RVV bench compile smoke。 |
| `020-roadmap-and-evaluation-recovery` | done / ready_for_review | 已补齐 `doc/optimization-roadmap.zh.md`，迁移 evaluation 主归属到 `doc/`，并验证 std/RVV correctness。 |
| `030-legacy-cleanup-and-evidence-registry` | done / ready_for_review | 删除无依赖旧 pointer / alias，并接入 `log/evidence_registry.json` freshness check。 |
| `040-doc-suite-parity` | done / ready_for_review_validity_checked | 已补齐 README、testing overview、correctness tests、benchmark/evidence、optimization evidence 和 test-support code map；std/RVV QEMU correctness 40/40 通过，registry fresh。 |
| `050-structure-layout` | done / structure queue closed | 已合并闭合 test-source-split 与 internal-helper-layout：gtest 拆成四个 `src/test_teptpl_*.cpp`，bench 收敛为薄入口，旧内部 helper 迁到 `include/impl/`；std/RVV correctness 40/40，bench compile / asm smoke 通过。 |

## 文档归属

| 信息 | 主归属 |
| --- | --- |
| production 覆盖范围、fallback 矩阵、正确性与高效性证据链 | `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` |
| EvidenceDecision、测试 inventory、bench 审计、Traceability Map、remaining risk | `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md` |
| 阶段计划、阶段结果、optimization matrix、dirty isolation、Evidence Doctor 人工检查 | `doc/phases/` |
| 跨阶段候选搜索空间、阶段反思新增路线、恢复条件 | `doc/optimization-roadmap.zh.md` |
| 测试类型、运行入口、case 语义、bench label、证据白名单和代码地图 | `README.zh.md`、`doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` |
| 板卡 repeated summary | `output/board/*_summary.md` |
| test harness layout audit（测试框架布局审计） | `doc/phases/010-test-harness-layout-audit/` 和 evaluation 文档 |
| doc suite parity（文档套件对齐） | `doc/phases/040-doc-suite-parity/`、README 和 topic-local doc suite |
| structure layout split（结构布局拆分） | `doc/phases/050-structure-layout/`、`include/`、`include/impl/`、`src/` 和 code map |
| evidence registry（证据登记表） | `log/evidence_registry.json`、`doc/benchmark-and-evidence.zh.md` 和 `doc/phases/030-legacy-cleanup-and-evidence-registry/` |

## Evidence Registry

当前 topic 已接入 `log/evidence_registry.json`，且 Phase 030 registry check 为 fresh。恢复和收尾时优先运行 registry check；人工审计仍关注这些路径：

- `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/production_dispatch_generic_representative_5run_summary.md`
- `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/block_fused_formula_5run_summary.md`
- `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/qemu/run_test_std.log`
- `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/qemu/run_test_rvv.log`
- `test-rvv/registration/transformation_estimation_point_to_plane_lls/log/qemu/run_test_std.log`
- `test-rvv/registration/transformation_estimation_point_to_plane_lls/log/qemu/run_test_rvv.log`

若后续新增或覆盖 evidence summary，应使用 `test-rvv/script/evidence_registry.py record`
更新登记表，并在阶段 `result.zh.md` 写出 check 命令、freshness 状态和文档引用状态；若未同步文档，
registry check 会暴露 `stale_doc_pending_refresh` 或 `doc_ref_missing` 风险。

## Ready For Review Validity

Phase 040 重新检查了 Phase 030 的旧 `ready_for_review`。旧停止位因 doc suite 缺口被降级为
stale stop decision（过期停止决策）。随后 Phase 050 再次恢复 roadmap 默认队列，把
`test-source-split` 和 `internal-helper-layout` 从 `phase_deferred + unblocked` 推进为 adopted。

Phase 050 后，当前不再保留 topic-local 测试资产 / 文档边界内的高优先级结构迁移队列项。
仍可继续的方向都需要扩大范围或新增证据：

- `060-helper-shape-review`：会触碰 production RVV helper，需独立 correctness / asm / board 风险判断。
- 更多点型、`Scalar=double`、indexed / correspondences production follow-up：会扩大证据范围，必须独立 phase。
- topic-local scripts / Evidence Doctor wrapper：只有新增 repeated board collection 或 manifest generation 时再恢复。
