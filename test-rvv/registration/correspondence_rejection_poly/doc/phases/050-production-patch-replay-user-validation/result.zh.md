# Phase 050 结果：production patch replay 与用户验证

## 阶段结论

本阶段按 Phase 050 plan 重新保留临时 production patch（生产接入补丁），并直接运行
`production-direct` target；没有使用额外的 topic 专用开关。

当前 PI5 EvidenceDecision（证据决策）为 `rollback/no-production`：

- correctness（正确性）仍成立，Std / RVV checksum 一致。
- QEMU production-direct smoke（QEMU 生产直连小型验证）只作为日志形状和路径证据，Evidence Doctor clean。
- board production-direct repeated（板卡生产直连重复测试）为 `negative`。
- Evidence Doctor 报告 `Errors=2，Warnings=0，Suggestions=0`，两个 Error 都是 5/5 degradation。
- 按 agent lifecycle 规则，负向证据先保留 patch 等待用户检查；用户已明确确认不接入，worker 已还原两个目标 production 文件。

## 实际执行范围

| 计划动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| 目标 production diff 用户检查 | done | `registration/include/pcl/registration/correspondence_rejection_poly.h`、`registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp` | 回滚前 `Standard` / `RVV` 分层保持可见；用户确认不接入后已还原，当前 production diff 为空。 |
| QEMU production-direct smoke | done | `ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/correspondence_rejection_poly run_bench_production_direct_smoke` | checksum match；QEMU timing 不作为性能结论。 |
| QEMU Evidence Doctor | done | `make -C test-rvv/registration/correspondence_rejection_poly run_evidence_doctor_production_direct_qemu` | `Errors=0，Warnings=0，Suggestions=0`。 |
| board production-direct repeated | done | `make -C test-rvv/registration/correspondence_rejection_poly run_board_bench_production_direct_repeated` | summary decision bucket 为 `negative`。 |
| asm refresh | done | `make -C test-rvv/registration/correspondence_rejection_poly dump_bench_rvv` | 当前 full asm 中可见 `getRemainingCorrespondencesRVV`、`getRemainingCorrespondencesStandard` 和 RVV 指令。 |
| evidence registry refresh | done | `python3 test-rvv/script/evidence_registry.py check --registry test-rvv/registration/correspondence_rejection_poly/log/evidence_registry.json --fail-on never` | `evidence registry check: fresh`。 |

## 当前证据

| 证据层 | 路径 | 结果 | 证据边界 |
| --- | --- | --- | --- |
| QEMU production-direct smoke | `log/qemu/analyze_bench_compare_production_direct.log` | 2048 checksum match；8192 checksum match | 只证明日志形状、输出合同和路径可运行。 |
| QEMU Evidence Doctor | `log/qemu/production_direct/evidence_doctor.md` | `Errors=0，Warnings=0，Suggestions=0` | 不支持目标硬件性能结论。 |
| board production-direct summary | `log/board/production_direct_repeated/summary.md` | decision bucket `negative` | 回滚前临时 production patch 的 strict A/B。 |
| board Evidence Doctor | `log/board/production_direct_repeated/evidence_doctor.md` | `Errors=2，Warnings=0，Suggestions=0` | 两个 Error 都阻止 production adoption（生产采纳）。 |
| asm summary / full asm | `build/asm/riscv/bench_correspondence_rejection_poly_rvv.asm`、`build/asm/riscv/bench_correspondence_rejection_poly_rvv.full.asm` | 回滚前 full asm 可见 RVV / Standard helper 符号；summary 有 RVV 指令 | 反汇编说明路径存在，不抵消 board 退化证据。 |
| registry | `log/evidence_registry.json` | fresh | 新 QEMU、board 和 asm 产物已登记。 |

## Board 结果

`speedup = std_ms / rvv_ms`，大于 1 表示 RVV build 更快。

| case | runs | min | median | max | speedup_values | degradation_frequency | checksum |
| --- | ---: | ---: | ---: | ---: | --- | --- | --- |
| `production-direct public entry 2048 correspondences` | 5 | 0.888x | 0.904x | 0.924x | 0.901x, 0.924x, 0.904x, 0.913x, 0.888x | 5/5 | match |
| `production-direct public entry 8192 correspondences` | 5 | 0.892x | 0.977x | 0.984x | 0.892x, 0.977x, 0.984x, 0.971x, 0.977x | 5/5 | match |

该结果比历史 Phase 030 的绝对数字略有波动，但 decision bucket 没有改变：当前 replay 仍然不支持采纳 production RVV path。

## Evidence Doctor 处理

Board doctor 报告两个 `ba_degradation_frequency` Error：

- 2048 correspondences：5/5 speedup 小于 1。
- 8192 correspondences：5/5 speedup 小于 1。

这些 Error 不证明实现有功能 bug；checksum 已经一致。它们说明当前 production direct strict A/B 不能支持性能采纳。用户已确认不接入，production patch 已回滚。若用户想继续查原因，下一阶段应先做 full-entry profile / component ablation（组件消融），而不是直接保留 production dispatch。

## Optimization matrix 更新

| candidate family | 当前状态 | evidence | decision | 下一步 |
| --- | --- | --- | --- | --- |
| `production_edge_batch_rvv` | replay negative; reverted | board median 0.904x / 0.977x；doctor Errors=2 | `rollback/no-production` | 无 production action；若继续，先要求追加 profile / ablation。 |
| `edge_gather_staging` | historical diagnostic | 历史 board weak-positive | 不替代 production direct | 若继续性能探索，先解释 full entry 退化来源。 |
| `accept_rate_filter` | historical neutral | board confirm neutral + warning | no-production diagnostic | profile 指向后再恢复。 |

## Continue / Stop Decision

stop condition 曾是 `production_rollback_requires_user_authorization`。证据已经支持回滚 / 不采纳，但 production patch 是用户要求恢复用于检查的临时补丁；因此本阶段先合法停止在用户确认点。用户随后明确确认不接入，worker 已执行回滚：

- 已回滚两处 production patch，保留 topic-local 诊断 / 证据文档，进入 rollback/no-production closeout。
- 若用户要求追加验证：先写新的 profile / ablation phase plan，再运行对应 target；不要直接扩大 production patch。
- 若用户明确确认采纳：需要人工接受当前 negative evidence 的风险；随后才可进入 production closeout，但当前证据不推荐这样做。

`doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md` 仍为 `not_applicable`，因为没有 adopted production behavior（已采纳生产行为）。
