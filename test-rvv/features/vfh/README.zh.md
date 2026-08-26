# VFH RVV 主题入口

本目录评估并验证 `features/include/pcl/features/impl/vfh.hpp` 中 `VFHEstimation::compute()` /
`computeFeature()` 的 RVV（RISC-V Vector，可变长度向量）生产优化。当前 production（生产源码）状态：
已采用 Phase 060 的有界 RVV 路径。公开入口在满足 dense full-cloud sequential indices（全点云顺序索引）、
默认 VFH（Viewpoint Feature Histogram，视点特征直方图）参数和 float AoS（结构数组）布局 gate 时走
`pcl::detail::computeVFHSignatureRVV()`；其它路径自然回退到原标量主体。

最终采用证据来自 `production_vfh_compute_default` 的 Phase 060 post-review production-public board evidence
（公开生产入口板卡证据）：5-run checksum 全一致，mean Std `8.81437 ms`、mean RVV `5.37768 ms`、
mean speedup `1.63906x`，Evidence Doctor（证据体检）为 `0E/0W/11S`。

## 阅读路径

| 文档 | 作用 |
| --- | --- |
| `doc/vfh-evaluation.zh.md` | 函数级评估、Traceability Map（可追踪性地图）和 production 接入判断。 |
| `doc/testing-overview.zh.md` | 测试入口、target 粒度和 QEMU / board 证据边界。 |
| `doc/correctness-tests.zh.md` | gtest 测试族、输入、断言和覆盖范围。 |
| `doc/benchmark-and-evidence.zh.md` | bench label、checksum、board repeated、Evidence Doctor 和证据提交边界。 |
| `doc/optimization-evidence.zh.md` | Phase 000-060 候选、采用 / 暂缓 / 拒绝路线和证据映射。 |
| `doc/test-support-code-map.zh.md` | 测试支撑代码、脚本、production helper 和输出位置的代码地图。 |
| `doc/phases/README.zh.md` | phase loop（阶段循环）索引和默认恢复入口。 |
| `doc/phases/040-production-rvv-probe/result.zh.md` | Phase 040 生产接入探针结果。 |
| `doc/phases/050-chunk-local-staging/result.zh.md` | Phase 050 chunk-local staging 结果。 |
| `doc/phases/060-rvv-bin-index-precompute/result.zh.md` | Phase 060 最终采用结果和 doc-suite parity 审计。 |
| `doc/phases/optimization-matrix.zh.md` | 跨阶段优化矩阵。 |
| `doc/optimization-roadmap.zh.md` | topic-level optimization roadmap（主题级优化路线图）。 |
| `doc-rvv/features/vfh-RVV.zh.md` | adopted production behavior（已采用生产行为）的长期维护文档。 |

## 常用命令

```bash
make -B -C test-rvv/features/vfh run_test_compare
make -B -C test-rvv/features/vfh dump_bench_rvv
make -C test-rvv/features/vfh board_smoke BENCH_ARGS='--side 80 --iterations 3 --warmup 1'
make -C test-rvv/features/vfh board_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase060-postreview BENCH_ARGS='--side 80 --iterations 8 --warmup 2'
make -C test-rvv/features/vfh evidence_doctor_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase060-postreview
make -C test-rvv/features/vfh evidence_doctor_phase030
```

`run_bench_compare` 默认受 guard（保护门）限制，不用于 QEMU 性能结论。真实性能结论必须来自 board
（板卡）或目标硬件。当前正式性能结论只引用 Phase 060 repeated board 的
`production_vfh_compute_default`。

## 证据提交边界

默认 evidence policy（证据策略）是 `summary-only`。`log/board`、`log/qemu`、`build` 和 raw run log
默认留在本机，不自动提交。Phase 060 的 run logs / manifest / Evidence Doctor 路径已被 topic-local
文档和 `doc-rvv/features/vfh-RVV.zh.md` 引用，可作为 review 时的 evidence summary（证据摘要）候选；raw
logs 不进入默认提交边界。
