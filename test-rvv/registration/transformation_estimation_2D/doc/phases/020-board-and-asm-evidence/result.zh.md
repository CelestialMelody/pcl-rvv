# Phase 020 Result: board-and-asm-evidence

## 计划版本和实际范围

本阶段按 `plan.zh.md` 执行，范围保持在 `test-rvv/registration/transformation_estimation_2D/**` 和当前 Handoff Packet。Production 源码未修改。

实际完成：

- 新增 Make target，接入 asm attribution（反汇编归因）摘要、QEMU smoke manifest（证据清单）、Evidence Doctor（证据体检）、board repeated summary（板卡重复摘要）和 evidence registry（证据登记表）。
- 运行 QEMU smoke、asm summary、QEMU Evidence Doctor。
- 板卡 `Milkv-Jupiter` 可达，完成 `ordered-cloud-pair-fused` 的 5 次 repeated board bench。
- 同步 summary、manifest、doctor 和 registry 路径。

## 执行动作回填

| id | 状态 | 命令 | 证据路径 | 结论 |
| --- | --- | --- | --- | --- |
| A1 QEMU manifest wrapper | done | `make -C test-rvv/registration/transformation_estimation_2D generate_qemu_smoke_evidence_manifest` | `test-rvv/registration/transformation_estimation_2D/log/qemu/evidence_manifest.json` | QEMU smoke 可解析为 diagnostic manifest；未输出 repeated `ba_values`。 |
| A2 asm attribution summary | done | `make -C test-rvv/registration/transformation_estimation_2D generate_asm_attribution_summary` | `test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.md`、`test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.json` | 候选 RVV 指令归属到 `runFusedCase` lambda 内联边界，同时发现 Eigen / production 标量边界也有 RVV 指令。 |
| A3 QEMU Evidence Doctor | done | `make -C test-rvv/registration/transformation_estimation_2D run_qemu_smoke_evidence_doctor` | `test-rvv/registration/transformation_estimation_2D/log/qemu/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=0；该结果只检查 QEMU smoke 证据合同。 |
| A4 QEMU registry | done | `make -C test-rvv/registration/transformation_estimation_2D record_qemu_smoke_evidence_state` | `test-rvv/registration/transformation_estimation_2D/log/evidence_registry.json` | QEMU smoke、manifest、doctor 和 asm summary 已登记。 |
| A5 board availability | done | `make -C test-rvv/registration/transformation_estimation_2D check_board_ssh` | board config: `REMOTE_USER` / `REMOTE_IP` / `BOARD_LABEL=Milkv-Jupiter` | SSH 可达。 |
| A6 board repeated benchmark | done | `make -C test-rvv/registration/transformation_estimation_2D run_board_bench_ordered_cloud_pair_repeated` | `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/summary.md`、`test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/evidence_manifest.json`、`test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/evidence_doctor.md` | 5-run summary 为 `weak_positive`；Doctor Errors=0，Warnings=0，Suggestions=0。 |
| A7 文档刷新 | done | 本文件及 topic-local docs | `README.zh.md`、`doc/*.zh.md`、`doc/phases/*.zh.md`、Handoff | 当前证据路径、决策和下一阶段入口已同步。 |

## QEMU 和 asm 结论

QEMU smoke 的 role 是 `qemu_smoke_only`。它证明 RVV bench binary 可运行、label / checksum / metadata 可解析，不能证明目标硬件性能。

ASM summary 的关键事实：

| item | value |
| --- | ---: |
| Std build RVV lines | 837 |
| RVV build RVV lines | 876 |
| delta | +39 |
| candidate lambda boundary RVV lines | 83 |
| production scalar boundary RVV lines | 76 |
| Eigen / stdlib RVV lines | 651 |

候选边界包含 `vlsseg3e32.v=4`、`vfmacc=4`、`vfredosum=16`、`vfsub=8`、`vfadd=4`、`vsetvli=9`。结论是 `candidate_inline_boundary_present_with_other_rvv_boundaries`：candidate hot path 存在 RVV 指令，但整份 RVV binary 中还有 Eigen、stdlib 和 production 标量相关 RVV 指令，不能把二进制总 RVV 指令数全部归因到候选。

## Board repeated 结论

运行合同：

- run label：`ordered_cloud_pair_repeated`。
- registry label：`board-te2d-ordered-cloud-pair-repeated-phase020`。
- device：`Milkv-Jupiter`。
- case-filter：`ordered-cloud-pair-fused`。
- runs：5。
- iterations：20。
- warm-up iterations：5。

`B/A = Std_ms / RVV_ms`，大于 1 表示 RVV build 更快。

| case | median | min | max | B/A<1 | bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `fused 2D correlation ordered-cloud-pair 4K` | 1.176x | 1.166x | 1.191x | 0 | `positive` |
| `fused 2D correlation ordered-cloud-pair 64K` | 1.096x | 1.068x | 1.145x | 0 | `weak_positive` |
| `fused 2D correlation ordered-cloud-pair 256K` | 1.096x | 1.080x | 1.121x | 0 | `weak_positive` |

整体 decision bucket 是 `weak_positive`。这是接入生产前诊断证据，不能替代 production direct（真实生产路径证据）。

## Evidence Doctor

| input | Errors | Warnings | Suggestions | 处理 |
| --- | ---: | ---: | ---: | --- |
| `log/qemu/evidence_manifest.json` | 0 | 0 | 0 | 作为 QEMU smoke contract pass 使用；不写性能结论。 |
| `log/board/ordered_cloud_pair_repeated/evidence_manifest.json` | 0 | 0 | 0 | 作为 pre-production board diagnostic 使用；支持进入 PI1 计划。 |

Doctor 没有发现脚本规则覆盖范围内的异常。人工边界仍然保留：QEMU timing 不用于性能；board summary 只覆盖 test-only fused candidate，不证明 production dispatch。

## Registry 状态

本阶段登记了：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/evidence_doctor.md`

这些文件位于 ignored `log/` 下，默认不提交；文档只把它们作为本机可复核证据指针。

## Optimization Matrix 更新

| candidate family | row source policy | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- |
| two-pass centered fused 2D correlation accumulator | ordered-cloud-pair | 5-run `weak_positive` | candidate inline boundary present, with other RVV boundaries | QEMU 0/0/0；board 0/0/0 | `diagnostic-positive / partial-production-candidate-for-PI1-plan` | `040-production-integration-plan`；PI2 production patch 仍需用户确认。 |
| common RVV reuse audit | ordered-cloud-pair | included indirectly by asm categories | Eigen / production scalar RVV boundaries observed | no Error | attempted | PI1 中继续判断 production helper 是否应复用 common path 或保持 narrow fused helper。 |
| source-indexed / dual-indexed / correspondence carry-over | non-ordered policies | not_run | not_run | not_run | deferred | PI1 后或另开 row-source phase，不能继承 ordered-cloud-pair 结论。 |

## 诊断证据链

当前证据链证明：

- correctness：Phase 010 的 `run_test_compare` / `run_test_candidates` 仍是数值正确性主证据。
- QEMU：`log/qemu/evidence_manifest.json` 和 `log/qemu/evidence_doctor.md` 证明 smoke 证据合同可解析。
- asm：`log/qemu/asm_attribution.md` 证明 candidate lambda 内联边界有关键 RVV 指令。
- board：`log/board/ordered_cloud_pair_repeated/summary.md` 证明 test-only fused candidate 在目标板卡上有 `weak_positive` 诊断收益。
- production boundary：production 源码未改；没有 production direct、fallback matrix 或 public dispatch 证据。

当前证据不能证明：

- production public overload 已经走 RVV。
- indexed / correspondences row source 也有同样收益。
- QEMU timing 可用于性能排序。
- Eigen / production 标量边界中的 RVV 指令属于 fused candidate。

## Continue / Stop Decision

`continue_stop_decision`：Phase 020 complete，停止在生产接入前边界。

`stop_condition_hit`：下一步进入 `040-production-integration-plan` 会开始 production integration loop（生产接入闭环）的 PI1 计划；PI2 production patch 不能在没有明确用户授权时直接执行。

`next_phase_default`：`040-production-integration-plan`。

`030-row-source-family-carryover` 仍保留在 roadmap，但 ordered-cloud-pair 已给出 `weak_positive` 诊断信号，默认先做 PI1 计划，冻结窄范围 production gate、fallback matrix、production direct tests、asm 和 board 证据需求。若用户不授权 production 方向，再回到 `030-row-source-family-carryover` 或 no-production closeout。
