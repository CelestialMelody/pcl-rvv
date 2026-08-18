# Phase 003 Plan：component-ablation

## 阶段意图和边界

本阶段解释 Phase 001 与 Phase 002 的证据冲突：test-only helper（测试专用 helper）同边界 Std/RVV 有约 2x，但临时 production public dispatch（生产公开入口分流）只有 1.00x 附近。目标是用 component ablation（组件消融，用来隔离某段成本占比）拆分 C1/C2 accumulation（累加前端）和 Eigen 4x4 solve（特征求解后段）成本，并检查 public wrapper（公开入口包装层）与 test-only helper 的边界差异。

本阶段不修改 production 源码，不重新接入 production dispatch，不覆盖 source-indexed、dual-indexed 或 correspondence-pair。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| production 源码 | TEDQ 无 diff，保持标量 | `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` |
| Phase 001 helper evidence | board repeated positive，4K / 64K / 256K median B/A 约 2.014x / 2.066x / 2.097x | `log/board/rvv_accum_full_cloud_repeated/summary.md` |
| Phase 002 production-public evidence | board repeated neutral，4K / 64K / 256K median B/A 约 1.001x / 1.002x / 1.001x；doctor Errors=1、Warnings=2、Suggestions=3 | `log/board/production_public_full_cloud_repeated/summary.md` |
| QEMU correctness | Std/RVV 各 9 tests passed | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| evidence registry | fresh | `log/evidence_registry.json` |

## 假设与候选族

| hypothesis | 要验证的现象 | 证据动作 | 可能结论 |
| --- | --- | --- | --- |
| H1 solve dilution | Eigen 4x4 solve 和矩阵构造稀释了 C1/C2 前端收益 | 新增 `component accum only` 与 `component solve only` bench | 若 accum-only 正向但 solve-only 占比高，production 需要更宽 timer boundary 或不接入 |
| H2 wrapper/baseline mismatch | public entry 与 test-only helper 的标量基线、模板展开或调用边界差异导致 Phase 002 neutral | 同一 bench run 内同时输出 public、helper full-estimate 和 component cases | 若 public 标量显著快于 helper 标量，不能把 helper positive 外推到 public |
| H3 transient dispatch issue | Phase 002 临时 patch 可能没有稳定命中预期热路径或引入额外开销 | 本阶段只做 no-production 诊断，不重开 production；把该假设留给后续 production probe | 需要新 production 设计和 asm direct attribution 后才能重开 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C1/C2 accumulation-only ablation | ordered-cloud-pair | `PointXYZ` / `float` / x,y,z AoS | test-support component helper，不含 Eigen solve | `make run_test_compare` 保护 matrix / accumulation checksum | `--case-filter component-ablation` | planned 5-run | bench binary RVV asm | planned | planned |
| solve-only ablation | ordered-cloud-pair | precomputed accumulation | test-support component helper，只含 Eigen solve + matrix checksum | shared helper checksum | `--case-filter component-ablation` | planned 5-run | scalar expected | planned | planned |
| public/helper mixed-boundary cross-check | ordered-cloud-pair | `PointXYZ` / `float` | public entry vs test-support helper，同 run 对照 | not strict A/B | summary cross-check only | planned 5-run | not production attribution | planned | diagnostic only |

## 实现和测试动作

| action | 内容 | 产物 | 完成判据 |
| --- | --- | --- | --- |
| A1 helper checksum | 为 `DualQuaternionAccumulation` 增加稳定 checksum，并保持 matrix checksum 不变 | `include/impl/tedq_candidates.hpp` | QEMU Std/RVV tests 通过 |
| A2 bench cases | 新增 `component accum only ordered-cloud-pair` 和 `component solve only ordered-cloud-pair` 三个规模；`component-ablation` filter 同时跑 public、helper full-estimate 和两个 component family | `src/bench_tedq.cpp` | QEMU smoke 可输出全部 case label |
| A3 summary script | 扩展 board repeated summary，支持 `--component-ablation`，分表输出 strict component A/B 和 mixed-boundary cross-check | `script/generate_tedq_board_repeated_summary.py` | summary / manifest / doctor 可生成 |
| A4 Make targets / allowlist | 增加 component ablation repeated collect / record target，并放行 summary evidence | `Makefile`、`test-rvv/.gitignore` | `make record_board_component_ablation_state` 成功 |
| A5 docs | 更新 README、benchmark/evidence、optimization evidence、roadmap、matrix 和 phase result | topic-local docs、模块 second-pass | 当前状态可恢复 |

## Evidence Doctor 和 registry 规则

- summary path：`log/board/component_ablation_repeated/summary.md`
- manifest path：`log/board/component_ablation_repeated/evidence_manifest.json`
- doctor path：`log/board/component_ablation_repeated/evidence_doctor.md`
- registry run label：`board-tedq-component-ablation-repeated-phase003`
- evidence role：`component_ablation`
- raw `run-*/*.log` 继续 ignored-local；summary / manifest / doctor 进入 summary-only 提交边界。

若 component strict A/B 有 checksum mismatch 或 Evidence Doctor Error，本阶段不能写成有效消融。若 mixed-boundary cross-check 方向异常，只能作为线索，不作为 strict speedup。

## 板卡复跑预算和决策桶

- repeated runs：`5`
- iterations：`20`
- warm-up iterations：`5`
- case-filter：`component-ablation`
- strict B/A：`Std component ms / RVV component ms`，大于 1 表示 RVV component 更快。
- component bucket：沿用 `positive` / `weak_positive` / `neutral` / `negative` / `unstable` 规则；64K / 256K 为主要判断规模。
- mixed-boundary cross-check 只报告 ratio，不进入 production gate。

## 继续 / 停止条件

本阶段完成后若 C1/C2 accumulation-only 仍稳定 positive，但 public/helper cross-check 说明边界差异明显，则下一 phase 默认转向 `004-production-boundary-probe-plan` 或 `004-static-dispatch-cost-audit`，不直接接 production。若 component no-solve 也 neutral 或 negative，则把 TEDQ production 方向降级，保留 test-only 诊断和 row-source future work。

## 文档更新清单

更新：

- `doc/phases/003-component-ablation/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/transformation_estimation_dual_quaternion-evaluation.zh.md`
- `README.zh.md`

不创建 `doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md`，因为没有 adopted production behavior（已采用生产行为）。
