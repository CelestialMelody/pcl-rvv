# Phase 010 计划：board repeated diagnostic 和 Evidence Doctor

## 阶段意图和边界

本阶段在板卡恢复可达后，为 `fused_full_cloud_accum` 补齐 board repeated benchmark
（板卡重复性能测试）、summary / manifest（摘要 / 证据清单）、Evidence Doctor（证据体检）和
evidence registry（证据登记表）刷新。Phase 010 仍只覆盖 test-only diagnostic（测试专用诊断）：
dense ordered-cloud-pair（稠密顺序点云对）、`PointXYZ`、`Scalar=float`、xyz AoS（结构数组）布局。

本阶段不修改 production（生产源码），不进入 PI1 production integration plan（生产接入计划）。
如果板卡证据稳定正向，下一阶段只能先进入 PI1；如果板卡证据负向或 Evidence Doctor 有阻塞 Error，
EvidenceDecision 保持 `diagnostic` 或降级为 `rollback/no-production` 候选。

## 当前状态清单

| area | 当前状态 | 证据路径 |
| --- | --- | --- |
| QEMU correctness（正确性） | Std/RVV 各 5 个 gtest 通过 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU smoke（日志形状） | `fused-full-cloud` 3 iterations / 1 warm-up，只作 log shape | `log/qemu/analyze_bench_compare.log` |
| QEMU Evidence Doctor | Errors=0、Warnings=0、Suggestions=0 | `log/qemu/evidence_doctor.md` |
| bench asm input（反汇编输入） | bench RVV dump 可见 RVV load / FMA / reduction 指令 | `build/asm/riscv/bench_transformation_estimation_svd_rvv.asm` |
| board availability（板卡可用性） | `check_board_ssh` 已通过 | 当前阶段命令输出 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `fused_full_cloud_accum` | ordered-cloud-pair | `PointXYZ` / `float` / dense xyz AoS | test-only fused candidate | 已通过 `run_test_compare` | `all` case-filter：public baseline + fused candidate | planned 5-run | bench helper asm 已有 | planned board doctor | diagnostic pending board | 跑 repeated board summary |
| `public_umeyama_baseline` | ordered-cloud-pair | `PointXYZ` / `float` | current production public default | semantic anchor 已通过 | `public-umeyama` case | planned sanity | production asm not_applicable | included as baseline | baseline only | 用作 mixed-boundary 对照 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| B1 board correctness smoke | `make -C test-rvv/registration/transformation_estimation_svd run_board_test_smoke` | board RVV gtest 通过并抓回 `log/board/test_smoke/run_test.log`。 |
| B2 repeated board collect | `make -C test-rvv/registration/transformation_estimation_svd collect_board_bench_fused_full_cloud_repeated` | 5 个 `run-*` 目录都有 Std/RVV log 和 analyze log。 |
| B3 summary / manifest | `make -C test-rvv/registration/transformation_estimation_svd generate_board_evidence_manifest_fused_full_cloud` | 生成 `log/board/fused_full_cloud_repeated/summary.md` 和 `evidence_manifest.json`。 |
| B4 Evidence Doctor | `make -C test-rvv/registration/transformation_estimation_svd run_board_evidence_doctor_fused_full_cloud` | 生成 `evidence_doctor.md`，Errors / Warnings / Suggestions 写入 result。 |
| B5 registry record / freshness | `record_board_fused_full_cloud_state`、`evidence_status` | summary / manifest / doctor 登记；文档引用刷新后 registry fresh。 |

## Evidence Doctor 和 registry 规则

summary 脚本必须分开输出两类读法：

- same-boundary fused Std/RVV（同边界 fused 标量 / RVV 对比）：只判断 RVV fused accumulation
  相对同一 test-support fused helper 的收益。
- mixed-boundary public-vs-fused-RVV（混合边界 public baseline 与 fused RVV 对照）：只判断是否值得进入
  PI1；它不是 production direct（真实生产路径证据），不能直接写 production-ready。

若任一 size 出现 `B/A < 1` 高频、checksum 风险、run count 不足或 metadata 缺口，Evidence Doctor finding
必须写入 result 和 evaluation。若 summary / doctor 覆盖旧结果，必须刷新 roadmap、matrix、README 和状态表。

## 板卡复跑预算和决策桶

| 项 | 设置 |
| --- | --- |
| 初始 run budget | 5-run repeated board。 |
| 同边界追加预算 | 最多 1 次同边界确认复跑；只有 decision bucket 摇摆或 Evidence Doctor finding 需要确认时使用。 |
| iterations / warm-up | 默认 20 iterations、5 warm-up iterations。 |
| positive | 目标 size 全部 median `> 1.15` 且无 `B/A < 1`。 |
| weak_positive | 目标 size median `>= 1.03` 且 min `>= 0.97`。 |
| neutral | 目标 size 全部落在 `0.97x..1.03x`。 |
| negative | 目标 size median `< 0.97` 或高频 `B/A < 1`。 |
| unstable | 追加预算后 bucket 仍反转或长尾无法解释。 |

4K 若负向但 64K / 256K 正向，只能形成 size-gated diagnostic candidate（带规模门控的诊断候选），
后续 PI1 必须把 small-input fallback 写成生产门禁。

## 继续 / 停止条件

继续条件：

- board correctness、repeated collect、summary、doctor 或 registry 仍能在当前 topic-local 范围内补齐。
- board 结果稳定正向但只支持进入 PI1 计划，不直接修改 production。

停止条件：

- board SSH / rsync / 远端运行再次失败，且本地证据和恢复命令已记录。
- Evidence Doctor Error 不能修复，或 repeated board bucket 为 negative / unstable。
- 继续需要修改 production、public API、泛型点类型 gate、`Scalar=double` 或其它 row source。

## 文档更新清单

- `doc/phases/010-board-and-asm-diagnostic/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/transformation_estimation_svd-evaluation.zh.md`
- `README.zh.md`
- `doc-rvv/library-screening/registration/registration-module-second-pass.zh.md`
