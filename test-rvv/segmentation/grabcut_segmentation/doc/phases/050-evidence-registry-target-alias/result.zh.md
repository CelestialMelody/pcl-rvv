# Phase 050: evidence registry target alias result

## 阶段结论

本阶段已接通 topic-local evidence registry（证据登记表）入口。`Makefile` 新增 Phase 030 repeated
summary（重复板卡摘要）登记 target，并让 `run_initgraph_repeated_evidence_doctor` 在重新生成 manifest
（证据清单）和 Evidence Doctor（证据体检）后立即刷新 registry，避免仅因 mtime（修改时间）变化导致
恢复阶段误判 stale。

本阶段只改变 `test-rvv/segmentation/grabcut_segmentation/**` 内的 target、registry 和文档，不修改
production（生产源码），也不改变 Phase 030 的 diagnostic evidence（诊断证据）边界。

## 计划动作回填

| 计划动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| 新增 registry 变量和 record target | done | `Makefile` 中 `EVIDENCE_REGISTRY`、`record_initgraph_repeated_evidence_files`、`record_initgraph_repeated_evidence_state` | Phase 030 repeated manifest、Markdown Doctor 和 JSON Doctor 均可登记。 |
| 新增 registry status target | done | `Makefile` 中 `evidence_status` | `evidence_registry.py check` 可扫描 Phase 030 与 Phase 060 summary 文件，并启用 `--require-doc-ref`。 |
| 运行 registry target | done | `log/evidence_registry.json` | 已登记 Phase 030 repeated summary；raw board logs 不进入登记和默认提交边界。 |
| 回填 Phase 050 result | done | 本文件 | 记录 target、freshness 状态和 production 边界。 |
| 同步导航和矩阵 | partial / continued by Phase 060 | README、phase index、roadmap、optimization matrix、evaluation、Phase 040 result | Phase 060 生产证据刷新改变了当前恢复状态，最终同步由 Phase 060 result 统一落地。 |

## 命令和结果

| 命令 | 结果 | 证据边界 |
| --- | --- | --- |
| `make run_initgraph_repeated_evidence_doctor && make evidence_status` | pass，`evidence registry check: fresh` | 只证明 Phase 030 `initgraph_no_solve` summary artifact（摘要产物）已登记且被文档引用。 |
| `make run_test_compare` | pass，Std 4/4，RVV 6/6 | Makefile target 改动未破坏 QEMU correctness（QEMU 正确性）。 |

## Evidence Doctor 和 registry

| 项 | 状态 |
| --- | --- |
| Evidence Doctor | Phase 030 repeated Doctor 仍为 `Errors=0, Warnings=0, Suggestions=0`。 |
| registry 文件 | `log/evidence_registry.json`。 |
| 登记文件 | `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-manifest.json`、`doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md`、`doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.json`。 |
| raw log 策略 | `repeated-board-*/*.log` 是 local-only 取证输入，不作为 summary-only 默认提交文件。 |

## Continue / stop decision

本阶段自身已完成，且没有新的性能结论。由于随后进入 Phase 060 production evidence（生产证据）闭环，
当前默认恢复状态不再是 PI2 授权边界，而是 Phase 060 的 PI5 用户检查点。Phase 050 的 registry target
继续作为恢复和提交前 freshness check（新鲜度检查）的一部分保留。
