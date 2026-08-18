# 优化证据索引

## 当前结论摘要

Phase 016 后，当前生产提交候选保留三类 RVV path：
`ordered-cloud-pair`、`source-indexed-cloud-pair`、`dual-indexed-cloud-pair`。
三类 retained production public board repeated 均为 positive，Evidence Doctor 均为 `0/0/0`。
`correspondence-pair` production RVV 已移除；Phase 015 的 negative / doctor Error 只作为历史负向证据。

## 优化方式总表

| 优化方式 | 状态 | 代码路径 | test target | bench / evidence target | 当前证据 | 边界 |
| --- | --- | --- | --- | --- | --- | --- |
| ordered-cloud-pair production C1/C2 RVV | `retained_production_candidate` | production header | `make run_test_compare` | `make run_board_bench_production_public_repeated` | board median `3.232x / 3.644x / 3.656x`；doctor `0/0/0` | `Scalar=float`、dense representative xyz AoS；其它 gate 回退。 |
| source-indexed production direct gather | `retained_production_candidate` | production header | `make run_test_compare` | 同上 | board median `2.540x / 2.631x / 2.586x`；doctor `0/0/0` | valid 32-bit source indices；越界或 unsupported layout 回退。 |
| dual-indexed production direct gather | `retained_production_candidate` | production header | `make run_test_compare` | 同上 | board median `2.015x / 1.808x / 2.021x`；doctor `0/0/0` | valid 32-bit source / target indices；其它 gate 回退。 |
| correspondence production RVV | `removed_from_production` | not retained | public scalar compatibility tests | Phase 015 historical summary | 64K / 256K negative；doctor `2/3/0` | 公开入口保持标量；后续需另开专项。 |
| C1/C2 accumulation-only component | `diagnostic_component_positive` | `include/impl/tedq_candidates.hpp` | `make run_test_compare` | `make record_board_component_ablation_state` | component 约 2.1x positive；doctor clean | 只解释前端收益，不替代 production direct。 |
| source / dual / correspondence staged diagnostics | `historical_diagnostic` | test support | `make run_test_compare` | row-source board summaries | source positive，dual / correspondence weak-positive | 不直接代表 production。 |
| indexed direct gather family diagnostics | `diagnostic_support` | test support | `make run_test_compare` | family comparison summaries | source / dual positive；correspondence weak-positive | 仅作为设计依据。 |
| correspondence segment-load / locality-aware candidates | `rejected_with_evidence` | test support | `make run_test_compare` | Phase 009 / 010 summaries | segment-load negative / unstable；local-window negative | 不进入 production。 |
| point-type layout diagnostics | `diagnostic_positive_with_warnings` | test support | `make run_test_compare` | Phase 012 / 013 summaries | representative extra-field layouts positive with warnings | 不扩大当前 production performance conclusion。 |

## 标量路径与 RVV 路径差异

标量 production 使用 iterator 顺序读取点对并累加 C1/C2。RVV retained 路径把逐点 C1/C2
前端换成 VL chunk（可变向量长度分块）：ordered 用 strided load（跨步加载），indexed 用
gather（离散加载），并在 double 上做 reduction（规约）。Eigen 4x4 solve 和矩阵构造保持标量。

## 细粒度 target 字典

| target | 角色 | 证据 |
| --- | --- | --- |
| `run_test_compare` | correctness gate（正确性验收） | Std `28/28`、RVV `32/32` |
| `run_qemu_smoke_evidence_doctor` | QEMU smoke doctor | retained 9 comparisons；Errors=0，Warnings=9，Suggestions=0 |
| `dump_bench_rvv` | asm smoke | local asm dump |
| `run_board_bench_production_public_repeated` | retained production board collect + record | 三类 retained summaries / manifests / doctors |
| historical correspondence targets | diagnostic / historical probe | 只供 correspondence 专项恢复 |

## 结论边界

QEMU smoke 不进入 speedup 排序。当前生产收益结论只覆盖三类 retained row source、当前合成 dense xyz AoS 输入和 `Scalar=float`。
correspondence、`Scalar=double`、unsupported point layout、非 dense cloud、小规模输入和越界 / 过大 indices 均不在 RVV production 覆盖范围内。
