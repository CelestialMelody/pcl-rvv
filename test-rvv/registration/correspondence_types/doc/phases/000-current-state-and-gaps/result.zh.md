# Phase 000 结果：current-state-and-gaps

## 当前状态提示

本文件保留 Phase 000 完成时的历史结论，其中 `board performance=missing / blocked` 是当时状态。Phase 010 已补齐板卡证据，并把当前 EvidenceDecision（证据决策）刷新为 `rollback/no-production`；当前恢复入口见 `doc/phases/010-board-diagnostic-and-production-decision/result.zh.md`、`doc/phases/optimization-matrix.zh.md` 和 `doc/optimization-roadmap.zh.md`。

## 实际执行范围

本阶段按 `plan.zh.md` 建立 `registration/correspondence_types` 的 test-rvv（RVV 测试资产）诊断 scaffold（脚手架），运行 QEMU correctness（QEMU 正确性验证）、QEMU bench smoke（QEMU 小型性能测试日志形状验证）、asm smoke（反汇编路径验证）和 Evidence Doctor（证据体检）。

本阶段没有修改 production（生产源码）。`registration/include/pcl/registration/impl/correspondence_types.hpp` 仍保持标量实现。

## 计划动作回填

| action | status | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| A1 建立 test support 聚合入口 | done | `include/correspondence_types.h`、`include/impl/correspondence_types_candidates.hpp` | 已采用 `include/` + `include/impl/` 布局；候选 helper 明确写成 test-only diagnostic（仅测试使用诊断），不扩展 production |
| A2 建立 correctness tests | done | `src/test_correspondence_types.cpp`；`make run_test_compare`；`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | Std/RVV 两构建均 6/6 通过；覆盖 query/match 顺序、sentinel、空输入、distance stats 一般和高动态范围样本、`n==1` 既有 NaN 边界 |
| A3 建立 bench smoke | done with boundary | `src/bench_correspondence_types.cpp`；历史 `make run_bench_compare` 输出 `log/qemu/analyze_bench_compare.log` | Std/RVV checksum 一致；QEMU timing 已标成 `qemu_smoke_only`，不作为性能排序、采纳审计或 EvidenceDecision |
| A4 反汇编 smoke | done | `make dump_bench_rvv`；`build/asm/riscv/bench_correspondence_types_rvv.asm` | 反汇编中存在 `vlse32.v`、`vse32.v`、`vfmul.vv`；`vfwredosum.vs` 只记录为诊断二进制内的编译器自动向量化信号 |
| A5 写阶段结果和文档 | done with later publication-boundary correction | 本文件、`doc/phases/optimization-matrix.zh.md`、`doc/correspondence_types-evaluation.zh.md` | 文档已从 planned 状态刷新为 diagnostic current state（诊断当前状态）；Phase 010 后确认 no-production 不发布 `doc-rvv` 长期主题文档 |
| A6 补 Evidence Doctor manifest | done | `script/generate_qemu_evidence_manifest.py`；`make run_evidence_doctor_qemu`；`log/qemu/evidence_manifest.json`、`log/qemu/evidence_doctor.md` | 机器可读 manifest 解决了原先 `missing_comparisons`；doctor 结果为 Errors=0 / Warnings=0 / Suggestions=0 |

## 优化矩阵更新

主矩阵见 `doc/phases/optimization-matrix.zh.md`。

| candidate family | 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| strided-index-extract | `attempted_diagnostic_board_blocked` | correctness 通过；QEMU smoke checksum 一致；asm 出现 `vlse32.v` / `vse32.v` | 需要板卡 repeated benchmark；若正向再考虑 PI1 |
| distance-stats-reduction | `attempted_diagnostic_no_production` | correctness 通过；same-chain 输出对齐；asm 出现 `vfmul.vv`，但 production reduction tree 未接入 | 不接 production；若继续，先做板卡 A/B、数值预算和更严 asm 归属 |
| production-dispatch | `deferred_requires_authorization_and_board` | 无 production direct / fallback / board 证据 | 只有板卡诊断正向且用户授权 production integration loop（生产接入闭环）后才进入 PI1 |

## 分层证据结论

| 证据层 | 状态 | 说明 |
| --- | --- | --- |
| correctness | pass | `make run_test_compare` 重新运行通过；Std/RVV 两构建各 6/6 tests passed |
| QEMU path / log shape | pass as smoke | bench smoke 输出 dataset、iterations、warmup、case 和 checksum；checksum 在 Std/RVV 间一致 |
| QEMU timing | not performance evidence | 既有 `analyze_bench_compare.log` 只能说明 QEMU smoke 路径可运行；所有时间数字从性能结论中排除 |
| asm smoke | partial pass | bench 二进制含目标 RVV 指令；当前不证明 production helper 符号命中 |
| board performance | missing / blocked | 本阶段未运行板卡或目标硬件 repeated benchmark |
| production direct | not_started | 没有 production patch、fallback test、production bench 或 production asm attribution |

## Evidence Doctor 和 Registry

Evidence Doctor 输入从旧的 Markdown summary 检查升级为 topic-local manifest：

- manifest：`log/qemu/evidence_manifest.json`
- report：`log/qemu/evidence_doctor.md`
- 命令：`make run_evidence_doctor_qemu`
- 结果：Errors=0，Warnings=0，Suggestions=0

这个结果只说明 QEMU smoke manifest 的字段和 checksum 边界在脚本规则内没有异常；它仍不能替代板卡性能证据。

`log/evidence_registry.json` 尚未接入，`evidence_registry_status=not_available`。人工检查路径包括：

- `log/qemu/evidence_manifest.json`
- `log/qemu/evidence_doctor.md`
- `log/qemu/analyze_bench_compare.log`
- `build/asm/riscv/bench_correspondence_types_rvv.asm`

## 数值预算结果

`getCorDistMeanStd` candidate 刻意保持 float-square same-chain（同构链路）：先用 RVV 对 `float distance * float distance` 做 chunk 内平方，再存回临时数组，并按输入顺序累加到 double。测试结果与 production 标量 helper 对齐，包括 `n==1` 时 `stddev=NaN` 的既有边界。

本阶段没有把 vector reduction（向量规约）作为 production 候选。反汇编中出现的 `vfwredosum.vs` 只作为诊断二进制事实记录，不能写成 production reduction tree 已被批准。

## 继续 / 停止决策

当前 phase 完成。默认下一阶段是 `010-board-diagnostic-and-production-decision`，但它依赖板卡或目标硬件重复性能测试；本地 QEMU 和文档范围内没有继续扩大生产结论的未阻塞动作。

`stop_condition_hit`：

- 性能结论需要 board / target hardware，本阶段不可获得。
- production integration loop 需要用户明确授权和板卡正向证据。
- 当前 topic 测试资产、manifest、phase result、optimization matrix 和 evaluation 已刷新到当前证据状态。

`next_phase_default`：评审通过且板卡可用时，创建 `010-board-diagnostic-and-production-decision/plan.zh.md`，先按 5-run repeated board 预算验证 `strided-index-extract`，再决定是否进入 PI1。若板卡结果为 neutral / negative，保持 production 标量并 closeout 为 no-production diagnostic。
