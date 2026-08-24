# 010 diagnostic-repeated-board 结果

## 执行范围

本阶段执行 DON helper-only diagnostic（诊断）的 5-run repeated board（重复板卡测试）采集和证据体检。阶段没有修改 production（生产源码），没有把 `computeDoNRVV()` 接入 `features/include/pcl/features/impl/don.hpp`。

| 计划动作 | 状态 | 证据 | 缺口 |
| --- | --- | --- | --- |
| repeated collect target | done | `make -C test-rvv/features/don run_board_don_repeated` 生成 `log/board/repeated_phase010_diagnostic/run-01..run-05/` | raw run logs 默认不进入提交候选 |
| repeated summary wrapper | done | `script/generate_don_repeated_summary.py` 生成 `summary.md` 和 `evidence_manifest.json` | 环境 metadata 中 taskset / governor / freq / temperature / binary_hash 仍为 `not_recorded` |
| Evidence Doctor | done | `log/board/repeated_phase010_diagnostic/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=0 | doctor 证明 manifest 范围内无异常，不证明 production direct |
| registry | done | `make -C test-rvv/features/don evidence_status` 输出 `evidence registry check: fresh` | registry 只登记 summary / manifest / doctor |
| 文档回填 | done | 本 result、optimization matrix、roadmap 和 evaluation 已更新 | production 长期 `doc-rvv` 不适用 |

## Repeated board 结果

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `don_normal_pair,points=262144` | 5 | `1.087x` | `1.069x` | `1.135x` | 0/5 | `weak_positive` |

每轮 checksum 均一致，说明当前 checksum policy（输出 normal 和 curvature）下 Std/RVV 输出一致。计时边界是 `helper_only_no_setup`：不包含前置 normal estimation、`Feature::compute()` 输出准备、对象状态或 production dispatch。

## Evidence Doctor 解释

`test-rvv/features/don/log/board/repeated_phase010_diagnostic/evidence_doctor.md` 报告 `Errors=0，Warnings=0，Suggestions=0`。phase 000 的 `low_run_count` warning 已通过 5-run `ba_values` 消除。

这仍是 diagnostic evidence（诊断证据）。它证明 test-only helper 边界下 RVV 对 ordered normal cloud 有稳定弱正向信号；它不能证明真实 `DifferenceOfNormalsEstimation` production direct 入口加速，也不能覆盖泛型 `PointNT` / `PointOutT` normal-like 点类型。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar；是否允许有界 production probe |
| diagnostic 是否可外推到 production | 只能外推为“值得进入 PI1 计划”。不能外推成 production-ready，因为计时不含公开入口状态、fallback 和前置 normal estimation |
| comparison-boundary / baseline mismatch 风险 | 有；Std/RVV 对比只改变 test helper build，不是 public overload |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前为稳定 `weak_positive`，允许 PI1 计划；如果 PI2-PI5 production direct 降为 neutral/negative，应回收生产 patch |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已采用 DON RVV family；不需要 family A/B，但需要 production public Std/RVV repeated evidence |

## 阶段决策

本阶段决策为 `partial-production-candidate`。默认下一阶段是 `020-pi1-production-integration-plan`：冻结 production 接入范围、fallback 和测试命令。进入 PI2 production patch 需要用户明确授权 production integration loop；未授权前不修改 `features/include/pcl/features/impl/don.hpp`。

## 阶段反思新增路线

| new idea | reason | next evidence |
| --- | --- | --- |
| exact `pcl::Normal` production probe | repeated diagnostic 为稳定弱正向，且 DON production 源码很短、fallback 可控 | PI1 plan；production direct correctness、fallback、asm、board repeated、Evidence Doctor |
| generic normal point type expansion | `PointNT` 与 `PointOutT` 是模板参数，exact `pcl::Normal` 不能代表完整泛型入口 | 用户确认 exact probe 后，读取 / 应用 generic point type strategy，补 traits gate 和代表点型证据 |
