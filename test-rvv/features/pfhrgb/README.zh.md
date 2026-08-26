# PFHRGB RVV Topic

本目录保存 `features/include/pcl/features/impl/pfhrgb.hpp` 的 RVV topic-local（主题本地）测试、
bench（性能测试）、阶段文档和证据摘要。当前已存在 adopted production behavior（已采用生产行为）
的 exact-gated（精确门控）PFHRGB RVV 路径，因此 `doc-rvv/features/pfhrgb-RVV.zh.md` 现在适用。

## 当前状态

当前 EvidenceDecision（证据决策）是 `adopted production behavior / exact-gated production`。
Phase 030/040 之后又补跑了接入后的 Milkv-Jupiter 5-run repeated board（板卡重复测试）和 Evidence Doctor（证据体检）：

| case | 当前 B/A | decision |
| --- | --- | --- |
| `public_pfhrgb_k` | `1.28, 1.27, 1.28, 1.27, 1.27`，median `1.27x` | positive production-public（正向生产公开入口） |
| `public_pfhrgb_k_with_candidate_reuse` | `1.23, 1.23, 1.25, 1.25, 1.24`，median `1.24x` | positive production-shaped diagnostic（正向生产形态诊断） |
| `public_pfhrgb_k_with_candidate` | `1.21, 1.21, 1.22, 1.22, 1.21`，median `1.21x` | positive production-shaped diagnostic |
| `candidate_pfhrgb_pair_batch_rvv` | `0.98, 0.97, 0.99, 0.98, 0.97`，median `0.98x` | attempted / negative-current-rerun |
| `component_pfhrgb_signature` | `1.00, 1.00, 1.02, 1.01, 0.99`，median `1.00x` | attempted / neutral-negative |

Evidence Doctor 当前结果为 Errors=1 / Warnings=1 / Suggestions=6。唯一 Error 仍来自 helper-only case 的
退化频率，说明它不能再被写成独立稳定收益；production-public 的 `public_pfhrgb_k` 这次没有 case-specific
Error，且 5/5 全部高于 1，支持当前 exact-gated production 采纳。

默认恢复入口：如果要继续泛型点型扩展，需另起新的 point-type expansion phase（点类型扩展阶段）；
当前 exact-gated production 结果已经采纳，不再停留在 PI1 检查点。

## 阅读路径

| 目的 | 入口 |
| --- | --- |
| 先读当前判断和 Traceability Map（可追踪性地图） | `doc/pfhrgb-evaluation.zh.md` |
| 查看测试入口总览 | `doc/testing-overview.zh.md` |
| 查看 correctness（正确性）测试说明 | `doc/correctness-tests.zh.md` |
| 查看 bench、board 和 Evidence Doctor 边界 | `doc/benchmark-and-evidence.zh.md` |
| 查看候选取舍索引 | `doc/optimization-evidence.zh.md` |
| 查看测试支撑代码地图 | `doc/test-support-code-map.zh.md` |
| 恢复当前阶段 | `doc/phases/README.zh.md` |
| 查看 Phase 030 reuse 结果 | `doc/phases/030-staging-reuse-ablation/result.zh.md` |
| 查看 Phase 040 doc-suite 结果 | `doc/phases/040-structure-parity-doc-suite/result.zh.md` |
| 查看候选搜索空间 | `doc/optimization-roadmap.zh.md` |
| 查看证据矩阵 | `doc/phases/optimization-matrix.zh.md` |

## 常用命令

| 命令 | 作用 |
| --- | --- |
| `make -C test-rvv/features/pfhrgb run_test_std` | 运行标量构建 correctness 测试。 |
| `make -C test-rvv/features/pfhrgb run_test_rvv` | 运行 RVV 构建 correctness 测试；QEMU 结果不代表真实性能。 |
| `make -C test-rvv/features/pfhrgb run_test_compare` | 顺序运行 Std/RVV 两侧 correctness 测试。 |
| `make -C test-rvv/features/pfhrgb dump_bench_rvv` | 构建 RVV bench 二进制并导出反汇编。 |
| `make -C test-rvv/features/pfhrgb check_board_ssh` | 检查板卡 SSH 是否可达。 |
| `make -C test-rvv/features/pfhrgb REPEATED_BOARD_RUNS=5 board_repeated evidence_doctor_repeated` | 在板卡做 5 轮 repeated bench 并刷新 Evidence Doctor。 |

## 证据提交边界

`log/board/repeated/evidence_manifest.json`、`log/board/repeated/evidence_doctor.md`、
`log/board/repeated/evidence_doctor.json` 和 `log/evidence_registry.json` 是 summary-only（仅摘要）
证据候选。`log/board/repeated/run_*`、`log/qemu/*` 和 `build/` 默认 local-only（仅本机保留），不进入默认提交。
