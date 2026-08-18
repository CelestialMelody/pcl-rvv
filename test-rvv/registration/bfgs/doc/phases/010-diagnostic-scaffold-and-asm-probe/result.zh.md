# Phase 010: diagnostic scaffold and asm probe 结果

## 执行范围

本阶段按计划只修改 `test-rvv/registration/bfgs/` 下的测试、bench、脚本和 topic-local 文档资产。`registration/include/pcl/registration/bfgs.h` 未修改，`doc-rvv/registration/bfgs-RVV.zh.md` 仍为 `not_applicable`。

## 计划动作回填

| action | 状态 | 证据路径 / 命令 | 结论 |
| --- | --- | --- | --- |
| 建立 topic-local include 结构 | done | `include/bfgs.h`、`include/impl/bfgs_fixtures.hpp`、`include/impl/bfgs_references.hpp`、`include/impl/bfgs_candidates.hpp` | test / bench 只 include 聚合入口；helper 职责按 fixtures / reference / candidate 拆分。 |
| 实现 reference / candidate helper | done | `include/impl/bfgs_references.hpp`、`include/impl/bfgs_candidates.hpp` | direction update、move-to+slope、dot / norm / linear helper 在 Std / RVV build 下均可编译。 |
| 实现 correctness tests | done | `src/test_bfgs.cpp`；`make -C test-rvv/registration/bfgs run_test_compare` | QEMU Std/RVV 各 6 个 gtest 全部通过。 |
| 实现 bench smoke | done | `src/bench_bfgs.cpp`；`log/qemu/run_bench_direction_update_rvv.log` | QEMU RVV smoke 可输出 case、iteration、warm-up、checksum 和 Total Time；计时只作日志形状。 |
| 建立 Makefile 和 manifest / doctor 钩子 | done | `Makefile`、`script/generate_bfgs_qemu_evidence_manifest.py`、`log/qemu/evidence_manifest.json`、`log/qemu/evidence_doctor.md` | `run_test_compare`、`dump_bench_rvv`、`run_qemu_smoke_evidence_doctor`、`record_qemu_smoke_evidence_state` 可用。 |
| evidence registry | done | `log/evidence_registry.json`；`make -C test-rvv/registration/bfgs evidence_status` | registry 已登记 correctness、QEMU smoke 和 Phase 020 board negative；当前 fresh。 |

## Correctness 结果

命令：

```bash
make -C test-rvv/registration/bfgs run_test_compare
```

结果：

| build | 结果 | 日志 |
| --- | --- | --- |
| Std | 6 tests passed | `test-rvv/registration/bfgs/log/qemu/run_test_std.log` |
| RVV | 6 tests passed | `test-rvv/registration/bfgs/log/qemu/run_test_rvv.log` |

覆盖的 gtest 包括：

- direction update `Vector6d` same-chain（同构链路）对拍。
- direction update 128 维 VL chunk（可变向量长度分块）对拍。
- `dxdg == 0` 边界。
- 空输入 fallback shape（回退形状）。
- `moveTo(alpha)` + `slope()` 对拍。
- `BFGS<QuadraticFunctor>` public API smoke（公开 API 小型验证）。

## QEMU smoke 和 asm 结果

命令：

```bash
make -C test-rvv/registration/bfgs run_qemu_smoke_evidence_doctor
```

生成文件：

| 文件 | 角色 | 提交边界 |
| --- | --- | --- |
| `test-rvv/registration/bfgs/log/qemu/run_bench_direction_update_rvv.log` | QEMU RVV direction-update smoke raw log | raw log，默认不提交 |
| `test-rvv/registration/bfgs/build/asm/riscv/bench_bfgs_rvv.asm` | filtered RVV asm summary（过滤后的 RVV 反汇编摘要） | build 输出，默认不提交；本阶段只引用路径和行数 |
| `test-rvv/registration/bfgs/log/qemu/evidence_manifest.json` | Evidence Doctor manifest（证据清单） | 机器可读摘要，默认按 summary-only 审查 |
| `test-rvv/registration/bfgs/log/qemu/evidence_doctor.md` | Evidence Doctor report（证据体检报告） | 摘要证据，review 后可提交候选 |

QEMU smoke 当前解析到两个 case：

| case | QEMU observed_ms | checksum | 证据边界 |
| --- | ---: | --- | --- |
| `direction-update-vector6` | 0.047198 | `9700448875786200448` | GICP 维度形态 smoke；不证明真实性能 |
| `direction-update-vector128` | 0.0680525 | `186901855604011235` | 长向量 VL chunk smoke；不代表 production 规模 |

`bench_bfgs_rvv.asm` 过滤后 RVV 指令行数为 936。该数字只说明 RVV build 的 bench 二进制中存在 RVV 指令；helper 可能被内联，hot symbol attribution（热点符号归属）尚未闭合，不能写成 production asm evidence。

## Evidence Doctor

`test-rvv/registration/bfgs/log/qemu/evidence_doctor.md`：

| Errors | Warnings | Suggestions | 解释 |
| ---: | ---: | ---: | --- |
| 0 | 0 | 0 | 当前 manifest 明确为 `qemu_smoke_only`，没有 Std/RVV performance A/B 字段，因此 doctor 没有把 QEMU timing 解释成性能证据。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `qemu_smoke_only` + test-only diagnostic correctness |
| A/B boundary | `test_support_candidate_wrapper` / bench binary smoke；不是 production public overload |
| 当前决策问题 | 局部 BFGS direction update 是否值得进入板卡诊断 |
| diagnostic 是否可外推到 production | no；缺少 GICP caller hotspot、production direct、fallback 和同 production boundary board 证据 |
| comparison-boundary / baseline mismatch 风险 | yes；当前 helper 使用 test-only `std::vector<double>`，production 是 Eigen 模板状态机 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有后续 caller hotspot 和用户授权成立时才允许 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；当前没有 adopted production family，也没有 production public A/B |

## Optimization matrix 更新

| candidate family | Phase 010 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| Direction update fused diagnostic | attempted / correctness passed / QEMU smoke clean | gtest Std/RVV 通过；QEMU smoke 2 case；doctor 0/0/0 | 进入 Phase 020 board repeated diagnostic |
| Move-to + slope combined diagnostic | correctness passed / bench deferred | gtest 对拍通过；未跑独立 smoke | 视 Phase 020 direction-update 结果决定是否扩展 |
| Eigen vector expression baseline | partial asm probe | filtered RVV asm 存在，但未归属到 production helper | 后续若进入 production 需更细符号归属 |
| GICP caller hotspot audit | deferred | 只有 public API smoke，不是 GICP caller smoke/profile | 方向更新板卡结果有信号后再做 |
| Production dispatch | blocked / not authorized | production 源码未修改 | 需要用户授权 PI1，且先补 caller 与 board evidence |

## 诊断证据链

当前证据能证明：

- test-only direction update 和 move-to+slope helper 在 Std / RVV build 下与标量 reference 同构。
- RVV build 在 QEMU 下可以执行 direction-update smoke，并输出可解析 checksum。
- RVV bench 二进制存在 RVV 指令，Evidence Doctor 对 `qemu_smoke_only` manifest 没有异常。

当前证据不能证明：

- QEMU timing 代表目标硬件性能。
- RVV 指令属于 production `BFGS` 热点或 GICP caller 热点。
- 修改 `registration/include/pcl/registration/bfgs.h` 值得接入生产。

## Continue / stop decision

本阶段完成，但 topic 还没有到用户可判断 production 接入的证据点。`micro_stop_guard` 在当时触发继续：Phase 010 只完成了 correctness / QEMU smoke / binary asm，仍缺 board performance（板卡真实性能）、caller hotspot（调用方热点）和 production boundary（生产边界）审计。后续 Phase 020 已补 board repeated negative 证据，并把当前结论收敛到 `diagnostic_stop_no_production`。

默认下一阶段是 `020-board-direction-update-diagnostic`：

- 在不改 production 的前提下，跑 `direction-update` Std/RVV board repeated diagnostic。
- 生成 board summary、manifest、Evidence Doctor 和 registry 记录。
- 若板卡结果为 positive / weak-positive，再进入 caller hotspot audit；若为 neutral / negative，只能说明 test-only direction-update 诊断不支持继续，不能直接推出 production no-go。Phase 020 实际结果为 negative，因此默认停止，不再自动排队 caller hotspot audit。

## 文档和 registry 后续

本结果引用了当前证据路径。文档回填后已复跑：

```bash
make -C test-rvv/registration/bfgs evidence_status
```

当前 registry fresh；若后续再次报 `doc_ref_missing` 或 `unregistered_file`，再继续补文档引用或调整 scan / registration 边界。
