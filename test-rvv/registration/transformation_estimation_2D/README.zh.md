# transformation_estimation_2D RVV 主题入口

本目录是 `registration/transformation_estimation_2D` 的 RVV topic（主题）入口。当前 production（生产源码）保留一个窄范围 RVV candidate（候选）：只覆盖 ordered-cloud-pair、`PointXYZ -> PointXYZ`、`Scalar=float`、两侧 dense finite 且点数不少于 16；其它公开入口继续走标量路径。

目标 production 文件：

```text
registration/include/pcl/registration/transformation_estimation_2D.h
registration/include/pcl/registration/impl/transformation_estimation_2D.hpp
```

测试资产里的 topic token（主题短标识）为 `te2d`。topic 名和 production 符号保持 `transformation_estimation_2D`。

## 当前结论

当前 EvidenceDecision（证据决策）是 `production-candidate-supported / user-review-pending`：

- `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` 当前保留 production patch；RVV 命中失败时回退现有标量 iterator 路径。
- QEMU correctness（QEMU 正确性）通过：Std / RVV 构建各 16 个 gtest 全绿，日志为 `log/qemu/run_test_std.log` 和 `log/qemu/run_test_rvv.log`。
- Phase 020 的 test-only fused 2D correlation candidate（测试专用融合 2D 相关项候选）仍保留为诊断资产；它的板卡 repeated summary 为 `weak_positive`，但它不是 production evidence（生产证据）。
- Phase 050 的 QEMU production-public smoke（生产公开入口 QEMU 小型验证）通过，`log/qemu/production_public/evidence_doctor.md` 为 Errors=0、Warnings=0、Suggestions=0；反汇编摘要能在 public overload 或 `runPublicCase` 内联边界看到关键 RVV 指令。
- 最新一次真实板卡 `Milkv-Jupiter` production-public repeated 为：4K median `4.222x`、64K `5.310x`、256K `4.947x`，三个规模均为 `positive`，每个规模 `B/A<1` 为 `0/5`；board Evidence Doctor 为 `0/0/0`。
- Phase 030 的 source-indexed、dual-indexed 和 correspondence row-source candidate 已完成 materialize-to-ordered（先物化为顺序点云对）诊断和真实板卡 repeated：Std / RVV correctness 各 16/16，QEMU smoke 覆盖 9 个 case，row-source Evidence Doctor 为 Errors=1、Warnings=2、Suggestions=6。
- row-source 64K 存在退化或长尾信号，三类 candidate 仍只是 test-only 诊断，不自动扩大 production dispatch；当前 production patch 只覆盖 ordered-cloud-pair 窄范围。

当前 production patch 和 PI5 证据闭环已存在，长期主题文档记录这一窄范围 candidate 及其 fallback 边界；不把 row-source 诊断写成生产行为。

## 先读哪份文档

| 问题 | 文档 |
| --- | --- |
| 函数做什么、标量路径如何工作、当前为什么不接 production | `doc/transformation_estimation_2D-evaluation.zh.md` |
| 测试入口和覆盖矩阵 | `doc/testing-overview.zh.md` |
| 每个 gtest 的输入、断言和证明范围 | `doc/correctness-tests.zh.md` |
| bench label、QEMU / board / Evidence Doctor 边界 | `doc/benchmark-and-evidence.zh.md` |
| candidate family（候选族）和后续恢复动作 | `doc/optimization-roadmap.zh.md` |
| 候选 × row source × 证据状态 | `doc/phases/optimization-matrix.zh.md` |
| 测试支撑代码地图 | `doc/test-support-code-map.zh.md` |
| 当前 phase loop（阶段循环）如何恢复 | `doc/phases/README.zh.md` |

## 目录分工

| 路径 | 当前状态 | 作用 |
| --- | --- | --- |
| `Makefile`、`board.mk` | created | topic-local build / QEMU / board skeleton，包含 production-public probe 的 evidence target。 |
| `src/test_te2d.cpp` | created | public semantics、row-source 标量边界和 fused candidate correctness。 |
| `src/bench_te2d.cpp` | created | QEMU smoke、test-only candidate bench 和 production-public probe bench 入口。 |
| `include/te2d.h` | created | 稳定聚合入口。 |
| `include/impl/te2d_candidates.hpp` | created | fixtures、reference、candidate 和 checksum helper。 |
| `doc/*.zh.md` | created | topic-local doc suite。 |
| `doc/phases/010-scaffold-and-ordered-cloud-pair-correlation-diagnostic/` | done | Phase 010 计划和结果。 |
| `doc/phases/020-board-and-asm-evidence/` | done | Phase 020 诊断证据计划和结果。 |
| `doc/phases/040-production-integration-plan/` | archived | PI1 计划历史状态；当前恢复入口由 Phase 050 决定。 |
| `doc/phases/050-pi2-production-patch-and-direct-evidence/` | done / current evidence refreshed | PI2-PI5 production-public probe、真实板卡复跑和窄范围 production candidate 决策。 |
| `doc/phases/030-row-source-family-carryover/` | done / diagnostic only | 三类 row-source materialize-to-ordered candidate 的 correctness、QEMU、asm、板卡 repeated 和 Doctor 已完成；仍不进入 production。 |
| `log/` | local-only | QEMU / board evidence pointers 和 `evidence_registry.json`，默认不提交。 |
| `build/` | local-only | 二进制和 asm dump，默认不提交。 |

## 常用命令

```bash
make -C test-rvv/registration/transformation_estimation_2D run_test_compare
make -C test-rvv/registration/transformation_estimation_2D run_test_public_semantics
make -C test-rvv/registration/transformation_estimation_2D run_test_candidates
make -C test-rvv/registration/transformation_estimation_2D run_bench_ordered_cloud_pair_smoke
make -C test-rvv/registration/transformation_estimation_2D run_bench_ordered_cloud_pair_public_smoke
make -C test-rvv/registration/transformation_estimation_2D run_bench_row_source_smoke
make -C test-rvv/registration/transformation_estimation_2D run_qemu_production_public_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D run_qemu_row_source_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_ordered_cloud_pair_public_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_row_source_repeated
make -C test-rvv/registration/transformation_estimation_2D evidence_status
```

QEMU 只用于 correctness、构建和日志形状。性能结论必须来自 board / target hardware repeated benchmark（板卡或目标硬件重复性能测试）。

## 当前可提交证据

| 产物 | 提交边界 |
| --- | --- |
| `README.zh.md`、`Makefile`、`board.mk`、`include/**`、`src/**`、`script/**`、`doc/**` | review 后可作为 topic test asset。 |
| `log/**`、`build/**` | local-only，默认不提交。 |
| `tmp/rvv-work-logs/registration/transformation_estimation_2D/**` | local-only，默认不提交。 |
| `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` | current candidate record；只记录已接入窄范围和 fallback。 |

## 默认不提交的生成产物

`build/`、raw QEMU logs、raw board logs、本机 `config.mk`、私有部署路径和 `log/evidence_registry.json` 默认不提交。当前文档引用的 summary-only（摘要级）证据路径包括：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/run_bench_row_source_fused_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/evidence_doctor.md`

用户明确要求提交 evidence logs（证据日志）时，先做脱敏和提交边界审计。

## 默认恢复动作

当前窄范围 production candidate 的证据已闭合，下一阶段进入 review 和边界扩展规划，不自动恢复旧 PI2。推荐下一阶段是：

```text
060-production-candidate-review-and-row-source-boundaries
```

先审阅当前 production diff、fallback 和 16/16 correctness；若要扩大到 indexed / correspondence，必须重新设计 gather/staging 并建立新的 PI1，不得把 Phase 030 的弱收益直接升级为 production。
