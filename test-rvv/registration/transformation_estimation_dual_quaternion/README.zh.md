# transformation_estimation_dual_quaternion RVV 主题入口

## 当前结论

本 topic 已完成 Phase 016 production adoption scope finalization（生产采纳范围收敛）。
当前提交候选只保留三类 production RVV path（生产 RVV 路径）：
`ordered-cloud-pair`、`source-indexed-cloud-pair` 和 `dual-indexed-cloud-pair`。
`correspondence-pair` 的 production RVV dispatch（生产分流）已移除，公开入口继续走原
`ConstCloudIterator` 标量路径；test-rvv 中的 correspondence 诊断候选和历史证据保留为后续专项输入。

当前停在提交前检查点：代码、测试和文档已按“保留三类、取消 correspondence production RVV”
整理，等待用户判断提交或取消接入。

## 先读哪份文档

1. `doc/transformation_estimation_dual_quaternion-evaluation.zh.md`：函数级评估、当前 EvidenceDecision（证据决策）和 Traceability Map（可追踪性地图）。
2. `doc/phases/016-production-adoption-scope-finalization/result.zh.md`：Phase 016 的 production diff、验证命令和 retained-only board 结果。
3. `doc/benchmark-and-evidence.zh.md`：case-filter、QEMU / board 证据边界和提交证据白名单。
4. `doc/optimization-evidence.zh.md`：候选采用 / 拒绝 / 暂缓状态。
5. `doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md`：长期 production RVV 行为说明；只覆盖当前三类提交候选。

## 目录分工

| 路径 | 职责 |
| --- | --- |
| `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` | 当前 production RVV patch；三类 retained row source 命中 RVV，correspondence 保持标量。 |
| `include/tedq.h` | test / bench 稳定聚合入口。 |
| `include/impl/tedq_adapters.hpp` | deterministic fixtures、indices / correspondence 构造和 staging/materialize 辅助。 |
| `include/impl/tedq_candidates.hpp` | test-only 标量 reference、RVV accumulation candidate 和诊断候选。 |
| `src/test_tedq.cpp` | correctness、fallback、row-source 语义和 retained production path-hit 测试。 |
| `src/bench_tedq.cpp` | bench case-filter 与 QEMU / board 输出入口。 |
| `script/` | QEMU / board summary、manifest 和 Evidence Doctor 输入生成脚本。 |
| `log/evidence_registry.json` | 当前 topic 的 evidence registry（证据登记表）。 |

## 常用命令

```bash
make run_test_compare
make run_qemu_smoke_evidence_doctor
make dump_bench_rvv
make run_board_bench_production_public_repeated
make evidence_status
```

默认 QEMU smoke 使用 `production-public-retained-row-sources`，只覆盖 retained 三类生产公开入口。
QEMU timing（QEMU 计时）不能写成性能结论；性能结论来自 `Milkv-Jupiter` board repeated。

## 当前可提交证据

默认提交边界是 summary-only（只提交摘要证据）。raw `.log`、`build/`、远端路径和本机
`config.mk` 不进入默认提交边界。

| 证据路径 | 角色 | 当前结果 |
| --- | --- | --- |
| `log/evidence_registry.json` | evidence registry | Phase 016 retained-only QEMU / board 证据已登记。 |
| `log/qemu/run_test_std.log` | QEMU correctness raw log | Std `28/28` passed；ignored-local。 |
| `log/qemu/run_test_rvv.log` | QEMU correctness raw log | RVV `32/32` passed；ignored-local。 |
| `log/qemu/evidence_manifest.json` / `log/qemu/evidence_doctor.md` | QEMU smoke manifest / doctor | 9 comparisons；Errors=0，Warnings=9，Suggestions=0。Warnings 仅来自 `warmup=0` 的 smoke 边界。 |
| `log/board/production_public_retained_row_sources_repeated/summary.md` | ordered-cloud-pair production direct | median B/A `3.232x / 3.644x / 3.656x`；doctor `0/0/0`。 |
| `log/board/production_public_source_indexed_cloud_pair_repeated/summary.md` | source-indexed production direct | median B/A `2.540x / 2.631x / 2.586x`；doctor `0/0/0`。 |
| `log/board/production_public_dual_indexed_cloud_pair_repeated/summary.md` | dual-indexed production direct | median B/A `2.015x / 1.808x / 2.021x`；doctor `0/0/0`。 |
| `log/board/production_public_correspondence_pair_repeated/summary.md` | Phase 015 historical negative | correspondence production RVV 已移除；该历史 evidence 不属于 retained production。 |

## doc-rvv 适用性

`doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md` 当前适用，因为存在已验证的
retained production patch 提交候选。该长期文档只描述三类 retained production RVV 行为；
若用户取消接入，需要同步回滚或删除对应长期文档更新。
