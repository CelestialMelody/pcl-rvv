# VFH 测试总览

## 测试职责

本 topic 的测试资产验证 VFH（Viewpoint Feature Histogram，视点特征直方图）默认公开入口的 RVV
（RISC-V Vector，可变长度向量）生产接入。QEMU（仿真器）只用于 correctness（正确性）和日志形状；
性能结论只来自 board（板卡）或目标硬件。

当前 adopted production behavior（已采用生产行为）只覆盖：
`PointNormal -> VFHSignature308` 测试实例、xyz / normal 单 `float` AoS（结构数组）布局、dense full-cloud
sequential indices（全点云顺序索引）、`normalize_distances=false`、`size_component=false`、未使用给定
centroid（质心）或 normal（法线）。
当前 RVV helper 还要求 `normalize_bins=true`；关闭 bin normalize（分箱归一化）时保持标量 fallback（回退路径）。

## Target 粒度审计

| target 类别 | 当前入口 | 状态 | 说明 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make -C test-rvv/features/vfh run_test_compare` | adopted | Std/RVV 两个构建都运行 gtest，RVV 构建额外覆盖 production helper hit。 |
| correctness aliases（正确性细分入口） | gtest filter 由 `src/test_vfh.cpp` 提供；Makefile 未拆独立 alias。 | not_applicable with evidence | 当前测试族数量少，总入口已能覆盖 public reference、candidate 和 fallback gate；后续扩展点型时再拆 alias。 |
| bench diagnostic aliases（bench 诊断入口） | `run_bench_compare` + bench label 字典。 | adopted | `src/bench_vfh.cpp` 输出 candidate 与 production label；QEMU 不用该目标给性能结论。 |
| QEMU smoke aliases（QEMU 小型验证入口） | `run_test_compare`、`dump_bench_rvv`。 | adopted | QEMU 侧只承担 correctness、构建和反汇编生成。 |
| board smoke aliases（板卡小型验证入口） | `board_smoke BENCH_ARGS='--side 80 --iterations 3 --warmup 1'` | adopted | 用于证明板卡可运行和日志形状，不作为 repeated performance。 |
| board repeated aliases（板卡重复采集入口） | `board_repeated REPEATED_BOARD_OUTPUT_DIR=... BENCH_ARGS='--side 80 --iterations 8 --warmup 2'` | adopted | Phase 060 的 production speedup 来自该 target。 |
| doctor / registry aliases（证据体检和登记入口） | `evidence_doctor_repeated`、`script/generate_vfh_evidence_manifest.py` | adopted / registry partial | Evidence Doctor manifest 已生成；topic 尚无独立 `log/evidence_registry.json`，closeout 用 manifest + 路径限定 status 扫描人工检查。 |
| historical probe guarded aliases（历史探针保护入口） | 无独立 guarded target。 | not_applicable with evidence | 历史 diagnostic label 仍在 bench 中用于对照，但 production adoption 只引用 `production_vfh_compute_default`。 |

## 覆盖矩阵

| 测试 / 证据 | 覆盖 | 不覆盖 |
| --- | --- | --- |
| public reference gtest | 默认公开 `compute()` 与 topic-local reference 的 descriptor 一致性。 | 非 dense、subset indices、CVFH / OUR-CVFH。 |
| candidate gtest | Phase 000-030 test-only candidate 的 same-chain（同构链路）一致性。 | production dispatch（生产分流）。 |
| production helper gtest | `computeVFHSignatureRVV()` 默认边界、over-range normal 语义、`size_component` 与 `normalize_bins=false` fallback。 | 其它 fallback gate 由源码 gate 保证，尚无逐 gate 独立 gtest。 |
| `dump_bench_rvv` | production helper 符号和关键 RVV 指令存在。 | 性能大小。 |
| Phase 060 board repeated | `production_vfh_compute_default` 目标硬件收益。 | 其它输入规模、点型、参数和调用方。 |
| Evidence Doctor | checksum、A/B 边界、metadata 风险和 near-threshold 提示。 | 不自动证明实现正确；Error / Warning 仍需人工解释。 |

## 默认复现命令

```bash
make -B -C test-rvv/features/vfh run_test_compare
make -B -C test-rvv/features/vfh dump_bench_rvv
make -C test-rvv/features/vfh board_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase060-postreview BENCH_ARGS='--side 80 --iterations 8 --warmup 2'
make -C test-rvv/features/vfh evidence_doctor_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase060-postreview
```
