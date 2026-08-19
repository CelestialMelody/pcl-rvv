# NDT RVV 诊断主题

## 当前结论

EvidenceDecision（证据决策）：`bench-only/no-production`。当前实现是 test-only diagnostic（测试专用诊断），只覆盖 `ndt.hpp` 中 `computeDerivatives` 内部 `updateDerivatives` 的 staged derivative accumulation（分阶段暂存后的导数累加）数学核；不修改 production（生产源码），也不建议把本候选接入 `registration/include/pcl/registration/impl/ndt.hpp`。

板卡 repeated bench（重复板卡性能测试）显示两个 case 都稳定退化：`derivative-hessian-staged` median 0.320x，`derivative-gradient-staged` median 0.401x，5/5 run 都低于 1.0。phase 020 的 topic-local double `exp` RVV 原型也仍是 hessian 0.321x、gradient 0.405x。Evidence Doctor（证据体检）因此给出 `ba_degradation_frequency` Errors；这些 Errors 支持把当前候选降级为 negative diagnostic（负向诊断）。

## 先读哪份文档

| 目的 | 路径 |
| --- | --- |
| 函数级评估、Traceability Map（可追踪性地图）和生产接入判断 | `doc/ndt-evaluation.zh.md` |
| 测试入口和 target 粒度 | `doc/testing-overview.zh.md` |
| correctness（正确性）TEST 字典 | `doc/correctness-tests.zh.md` |
| bench、QEMU、board、Evidence Doctor 和 registry | `doc/benchmark-and-evidence.zh.md` |
| 候选取舍索引 | `doc/optimization-evidence.zh.md` |
| 测试支撑代码地图 | `doc/test-support-code-map.zh.md` |
| 阶段恢复入口 | `doc/phases/README.zh.md` |
| 跨阶段路线 | `doc/optimization-roadmap.zh.md` |

`doc-rvv/registration/ndt-RVV.zh.md` 当前为 `not_applicable with evidence`：没有 adopted production behavior（已采用生产行为），也没有 PI5 production evidence（生产证据闭环）通过并等待采纳。

## 常用命令

```bash
make -C test-rvv/registration/ndt run_test_compare
make -C test-rvv/registration/ndt record_qemu_smoke_evidence_state
make -C test-rvv/registration/ndt run_board_test_smoke
make -C test-rvv/registration/ndt run_board_bench_repeated
make -C test-rvv/registration/ndt run_board_bench_ndt_double_exp_repeated
make -C test-rvv/registration/ndt evidence_status
```

QEMU（仿真器）只作为 correctness、日志形状和反汇编路径证据；性能结论只使用 `log/board/derivative_accumulation_repeated/summary.md`。

## 当前可提交证据

| 证据 | 路径 | 角色 |
| --- | --- | --- |
| QEMU Evidence Doctor | `test-rvv/registration/ndt/log/qemu/evidence_doctor.md` | QEMU smoke；Errors=0，Warnings=0 |
| Board repeated summary | `test-rvv/registration/ndt/log/board/derivative_accumulation_repeated/summary.md` | 当前性能结论主证据 |
| Board Evidence Doctor | `test-rvv/registration/ndt/log/board/derivative_accumulation_repeated/evidence_doctor.md` | 负向诊断解释 |
| Double exp probe summary | `test-rvv/registration/ndt/log/board/double_exp_probe_repeated/summary.md` | phase 020 负向消融证据 |
| Evidence registry | `test-rvv/registration/ndt/log/evidence_registry.json` | 当前已登记证据入口 |

默认不提交 raw run logs、`build/`、本机 `config.mk`、板卡私有地址或远端路径细节。
