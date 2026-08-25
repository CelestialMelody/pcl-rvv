# Phase 010 Pair Feature Batch Diagnostic Result

## 执行范围

本阶段完成 `pfh-pair-feature-staged-rvv` test-only diagnostic（测试专用诊断）闭环。改动只发生在 `test-rvv/features/pfh/**`：

- 新增 `include/impl/pfh_pair_batch_candidate.hpp`，实现 scalar staging（标量暂存）+ RVV pair tuple（成对特征三元组）+ scalar histogram scatter（标量直方图离散累加）。
- `src/test_pfh.cpp` 增加候选与 production helper 的 histogram 对拍。
- `src/bench_pfh.cpp` 增加 `candidate_pfh_pair_batch_rvv` bench case。
- `script/generate_pfh_evidence_manifest.py` 增加该 case 的 evidence role（证据角色）和 A/B boundary（A/B 边界）metadata。

未修改 `features/include/pcl/features/impl/pfh.hpp` production（生产源码）。本阶段不能声明 production dispatch（生产分流）已存在。

## 计划动作回填

| action | status | 证据 |
| --- | --- | --- |
| RED-010 | done | `make -B -C test-rvv/features/pfh run_test_rvv` 曾因候选入口缺失失败，随后实现候选。 |
| GREEN-010 | done | `make -B -C test-rvv/features/pfh run_test_rvv` 通过；`make -B -C test-rvv/features/pfh run_test_compare` 通过。 |
| BENCH-010 | done | `make -B -C test-rvv/features/pfh dump_bench_rvv` 通过，生成 `build/asm/riscv/bench_pfh_rvv.full.asm` 和 `.asm`。 |
| BOARD-010 smoke | done | `make -C test-rvv/features/pfh board_smoke BENCH_ARGS='--side 32 --k 32 --iterations 3 --warmup 1'` 通过；candidate 单次约 `2.34x`。 |
| BOARD-010 repeated | done | `make -C test-rvv/features/pfh board_repeated BENCH_ARGS='--side 32 --k 32 --iterations 8 --warmup 2'` 完成 5-run repeated。 |
| DOCTOR-010 | done_with_findings | `make -C test-rvv/features/pfh evidence_doctor_repeated` 生成 `log/board/repeated/evidence_manifest.json`、`evidence_doctor.md`、`evidence_doctor.json`；Markdown 报告为 Errors=0、Warnings=1、Suggestions=8。 |
| REGISTRY-010 | done | `python3 test-rvv/script/evidence_registry.py check --registry test-rvv/features/pfh/log/evidence_registry.json --scan-glob 'test-rvv/features/pfh/log/board/repeated/evidence_*' --doc test-rvv/features/pfh/doc/phases/010-pair-feature-batch-diagnostic/result.zh.md --fail-on never` 输出 `fresh`。 |

## 证据摘要

| case | evidence role | A/B boundary | B/A values | decision bucket | 解释 |
| --- | --- | --- | --- | --- | --- |
| `candidate_pfh_pair_batch_rvv` | diagnostic | test helper | `2.33x, 2.33x, 2.34x, 2.34x, 2.35x` | positive | RVV build 中的 staged pair tuple candidate 明显快于非 RVV fallback reference；checksum 一致，且 0/5 退化。 |
| `component_pfh_signature` | production-shaped diagnostic baseline | production-shaped helper | `0.99x, 1.00x, 1.00x, 1.00x, 1.01x` | neutral | 仍是无 production patch 的 component baseline；Doctor 报告 1/5 退化频率 Warning，不能写成收益。 |
| `public_pfh_k` | production_public baseline | public overload | `1.00x, 1.00x, 1.00x, 1.00x, 1.02x` | neutral | 公开入口还没有 PFH RVV dispatch；结果只说明当前 public Std/RVV 构建差异接近噪声。 |

B/A 方向：`B/A = Std build ms / RVV build ms`，大于 1 表示 RVV build 更快。candidate case 的 Std build 调用非 `__RVV10__` fallback reference，RVV build 调用本阶段 staged RVV candidate。

## Correctness / QEMU / Asm / Board

Correctness（正确性）：`run_test_compare` 的 Std/RVV 两侧都通过。候选 histogram 与 production helper 在 `PointNormal -> PFHSignature125`、`nr_split=5`、固定 wrapped neighborhood 下逐 bin 对拍，阈值为 `2e-3`。

QEMU：只用于 correctness 和日志形状；未运行 QEMU bench compare，未使用 QEMU timing（QEMU 计时）支撑性能结论。

Asm（反汇编）：`dump_bench_rvv` 生成 RVV bench dump；full asm 中可定位 `computePointPFHSignaturePairBatchRVV` 调用，并能看到 `vle32.v`、`vfmacc`、`vfsqrt`、`vsetvli` 等 RVV 指令。由于候选可被 inline / clone，当前归属是 bench binary 级别和候选调用链级别，不是 production hot symbol 归属。

Board（板卡）：Milkv-Jupiter 上 5-run repeated 已完成。candidate 的强正向来自目标硬件，不来自 QEMU；但环境 metadata（taskset/governor/freq/temperature）和 binary hash 仍是 Doctor Suggestions。

## Evidence Doctor 处理

Doctor Markdown 报告：`Errors=0，Warnings=1，Suggestions=8`。

- Warning：`component_pfh_signature` 的 `ba_degradation_frequency`，1/5 低于 1。处理方式：component baseline 继续标为 neutral，不作为 production 收益证据。
- Suggestions：三个 case 均缺少环境 metadata 和 binary identity；component/public 还有 near-threshold 提醒。处理方式：不阻塞 candidate diagnostic positive，但 PI1 / PI4 若进入 production direct（真实生产路径证据）必须补同边界 manifest、环境字段或明确记录缺口。

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | candidate 是 `diagnostic`；component baseline 是 `production_shaped_diagnostic`；public case 是 `production_public` baseline。 |
| A/B boundary | candidate 是 test helper；不是 public overload，也不是 production detail helper。 |
| 当前决策问题 | `RVV-vs-scalar` 用于筛选 staged pair-feature candidate 是否值得进入有界 production probe。 |
| diagnostic 是否可外推到 production | 只能外推为“pair tuple 数学链路值得做生产探针”。不能外推为 production 已加速，因为 production 还没有 staging、dispatch、fallback 和 direct bench。 |
| comparison-boundary / baseline mismatch 风险 | 有。candidate Std 侧是 test-only reference，RVV 侧是 staged candidate；public case 仍未命中 candidate，search/output 稀释也未评估。 |
| diagnostic positive 时是否允许 bounded production probe | 允许进入 PI1 production integration plan；PI2 生产补丁需冻结范围并获得用户明确授权。 |
| diagnostic weak / negative / neutral / unstable 时的处理 | 本轮 candidate 不是弱/负/中性/不稳定；component/public baseline neutral 不能拒绝 production probe，因为它们没有接入 candidate。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。若 PI1 选择 staged family 接入，PI4 必须补真实 production direct correctness、fallback、asm 和 board repeated；若出现 direct AoS family，还需同 production boundary 做 family comparison。 |

## Optimization Matrix 更新

| candidate family | decision | unblocked next action |
| --- | --- | --- |
| `pfh-pair-feature-staged-rvv` | positive_diagnostic_candidate | 进入 `020-production-integration-plan`，只写 PI1 范围和 gate，不直接修改 production。 |
| `pfh-direct-point-load-rvv` | phase_deferred + unblocked | PI1 中审计是否可替代 staging 或作为后续 family comparison；需要 normal field traits、AoS load 和 histogram scatter 计划。 |
| `pfh-public-k-dilution-check` | deferred | PI4 production direct 前继续作为 public dilution check，当前不支撑 adoption。 |

## Continue / Stop Decision

`continue_stop_decision`: turn_stop_deferred at production authorization boundary。

停止条件命中：继续到 PI2 会修改 `features/include/pcl/features/impl/pfh.hpp` production 源码，而当前短 prompt 没有显式授权生产补丁。下一阶段默认入口已创建为 `020-production-integration-plan`；它冻结生产接入范围、fallback、点类型 / Scalar 边界和暂停条件，等待用户确认是否进入 PI2-PI5。
