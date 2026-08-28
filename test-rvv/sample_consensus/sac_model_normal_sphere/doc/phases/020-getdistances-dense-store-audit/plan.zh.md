# Phase 020: getDistances dense-store 审计计划

## 阶段意图和边界

本阶段把 `getDistancesToModelCandidate` 从 Phase 000 的附带 same-chain（同构链路）证据中拆出来，独立审计 full-RVV dense double store（完整 RVV 稠密 double 写回）是否值得继续作为后续候选。阶段只触碰 `test-rvv/sample_consensus/sac_model_normal_sphere/` 下的测试资产、证据脚本和 topic-local 文档，不修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp`。

| scope item | 本阶段冻结值 |
| --- | --- |
| validated_scope | `PointXYZ + Normal`、`PointXYZI + Normal`，direct indexed source/normal，`Scalar=float` 字段输入，`std::vector<double>` dense output。 |
| unvalidated_scope | production dispatch（生产分流）、其它点型 / normal layout、`Scalar=double`、非 indexed 入口、真实 public overload 的 RVV 命中和 production fallback。 |
| phase_closeout_boundary | 只关闭测试专用 `getDistancesToModelCandidate` 的 dense-store diagnostic（诊断）审计；不关闭 production adoption（生产采纳）或 select/count 的 PI1 决策。 |

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| candidate helper | `getDistancesToModelCandidateRVV` 已在 RVV 构建下按 VL chunk（可变向量长度分块）计算 float distance，再经 `vfwcvt` 写入 double 输出。 | `include/impl/sac_model_normal_sphere_access.hpp` |
| correctness | 现有 `expectSameNormalSphereOutputs` 已对 public、scalar reference 和 candidate 的 dense output 做 same-chain 对拍，覆盖 `PointXYZ`、`PointXYZI` 和球心退化方向。 | `src/test_sac_model_normal_sphere.cpp` / `include/test_sac_model_normal_sphere.h` |
| bench label | 已输出 `diagnostic candidate getDistancesToModel`，但 Phase 000 只把它当附带诊断，没有专用 phase manifest / doctor / registry。 | `include/bench_sac_model_normal_sphere.h` |
| prior board signal | Phase 000 single board smoke 中该 label 为正向，但不是本阶段专用 EvidenceDecision。 | `doc/phases/000-normal-sphere-count-select-diagnostic/board-evidence-summary.md` |
| production | 未修改 production 源码；`doc-rvv` 长期生产主题文档不适用。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` |

## 假设与候选族

`getDistancesToModel` 没有 select/count 的 early continue（提前跳过），每个输入都要计算 normal angle（法线夹角）并写出 double distance。因此本阶段主要验证两个假设：

- `dense-store` 候选：RVV 计算距离后使用 `vfwcvt` + `vse64` 直接写回 double 输出，可能减少逐点标量开销。
- 风险假设：没有 early continue 时，`sqrt`、`acos` helper 和 double 写回的成本可能让收益低于 count/select；若结果正向，也只能说明测试专用 dense output 有潜力。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| getDistances full-RVV dense store | indexed source + indexed normal | `PointXYZ + Normal` / float input / double output / AoS | `getDistancesToModelCandidate` production-shaped diagnostic | `run_test_compare` existing dense-output same-chain | `diagnostic candidate getDistancesToModel` | run Phase 020 board smoke | expect `vfwcvt` / `vse64` under candidate RVV helper | run Phase 020 doctor | planned |
| getDistances full-RVV dense store | indexed source + indexed normal | `PointXYZI + Normal` / float input / double output / AoS | representative source layout expansion | `run_test_compare` existing dense-output same-chain | `diagnostic candidate getDistancesToModel` | run Phase 020 board smoke | same helper boundary | run Phase 020 doctor | planned |

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| 写阶段计划 | `doc/phases/020-getdistances-dense-store-audit/plan.zh.md` | 在修改 Makefile / 脚本前存在。 |
| 补 Phase 020 manifest 支持 | `script/generate_normal_sphere_evidence_manifest.py` | `--phase 020` 只生成 `diagnostic candidate getDistancesToModel` 的候选比较和 public 上下文。 |
| 补 Make target | `Makefile` | `generate_phase020_evidence_manifest`、`run_phase020_evidence_doctor`、`record_phase020_evidence_state` 和 `evidence_status` 覆盖 Phase 020。 |
| correctness / QEMU | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_compare` | Std/RVV gtest 全通过；dense output 对拍仍在既有测试内。 |
| asm attribution（反汇编归属） | `make -C ... clean_bench_rvv dump_bench_rvv` + grep | RVV bench binary 中可见 dense double store 相关 `vfwcvt` / `vse64`，且候选 helper 存在。 |
| board smoke（板卡小型验证） | 分别以 `PointXYZ` 和 `PointXYZI` 运行 `board_smoke` 到 Phase 020 目录 | 板卡 gtest 和 bench 日志可解析；性能只作为 single board smoke。 |
| Evidence Doctor / registry | `record_phase020_evidence_state` + `evidence_status` | doctor 无 Error；若有 Warning，result 解释边界；registry fresh。 |

## Evidence Doctor 和 registry 规则

Phase 020 生成：

- `doc/phases/020-getdistances-dense-store-audit/board-evidence-summary.md`
- `doc/phases/020-getdistances-dense-store-audit/board-evidence-manifest.json`
- `doc/phases/020-getdistances-dense-store-audit/board-evidence-doctor.md`
- `doc/phases/020-getdistances-dense-store-audit/board-evidence-doctor.json`

Evidence Doctor（证据体检）若出现 checksum、A/B boundary（对比边界）或 metadata Error，本阶段不能关闭。`low_run_count` 这类 Warning 可以保留，但 result 必须写清它只支撑 single board smoke，不支撑 repeated board 稳定结论。registry 使用 `log/evidence_registry.json` 登记 summary / manifest / doctor。

## 板卡复跑预算和决策桶

本阶段预算为 `1/5` single board smoke；若任一点型 speedup 接近 `0.95x-1.05x`、Evidence Doctor 报告方向异常或新旧结论反转，最多允许同边界再复跑一次。桶定义：`>=1.20x` 为 positive，`>=1.05x` 为 weak_positive，`0.95x-1.05x` 为 neutral，`<0.95x` 为 negative；跨桶摇摆时标为 unstable，不继续无限复跑。

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断）。 |
| A/B boundary | board Std build fallback vs board RVV build test-only `getDistancesToModelCandidate`。 |
| 当前决策问题 | RVV-vs-scalar diagnostic；是否把 dense-store 候选从 deferred 改成 PI1-adjacent candidate。 |
| diagnostic 是否可外推到 production | 不能直接外推。production 源码没有 RVV dispatch、fallback gate、生产直连测试或 production asm。 |
| comparison-boundary / baseline mismatch 风险 | 有。该候选通过测试专用派生类计时，public overload 仍是标量上下文，且 Phase 020 不是 production direct。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 可以允许，但只作为用户授权后的有界生产探针；不能用弱 / 负 / 中性诊断直接推出 no-production。 |
| clean adoption 是否需要同一 production boundary 内证据 | 需要。必须另走 PI1-PI5，且 PI5 后等待用户明确确认采纳或回滚。 |

## 继续 / 停止条件

若 Phase 020 positive 或 weak_positive，只把 `getDistancesToModel` 标为 dense-store diagnostic candidate；production 修改仍需要用户明确授权进入 production integration loop。若 Phase 020 negative 或 neutral，降级为 attempted / deferred，并保留 select/count 作为更强 PI1 候选。无论结果如何，本轮不因单个候选审计完成而自动 closeout；若仍有当前授权范围内的文档、证据或测试结构缺口，继续按 roadmap 创建下一 phase。

## 文档更新清单

完成后更新 `result.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/sac_model_normal_sphere-evaluation.zh.md` 和 `README.zh.md`。`doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md` 继续判为 not_applicable。
