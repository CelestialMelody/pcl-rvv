# CPPF 函数级评估

## 当前结论

`features/include/pcl/features/impl/cppf.hpp` 当前不接入 RVV production patch（生产补丁）。Phase 000 只证明了 test-only component ablation（测试专用组件消融）边界下的正确性与负向性能：`pair/HSV` RVV 候选板卡 median `0.49x`，`alpha_m` RVV 候选板卡 median `0.82x`，均低于本阶段进入 bounded production probe（有界生产探针）的门槛。

`doc-rvv/features/cppf-RVV.zh.md` 当前为 `not_applicable`：没有 adopted production behavior（已采用生产行为）、没有用户确认保留的 production patch，也没有 PI5 production evidence（生产证据）闭环。

## S2 函数级评估

CPPF（Colored Point Pair Feature，彩色点对特征）的公开入口是 `CPPFEstimation::computeFeature`。它按 `indices_ × input_` 形成 all-pairs output（所有点对输出）：同一点对写入 `NaN` 并将 `is_dense=false`，非同点对调用 `computeCPPFPairFeature` 生成 `f1..f10`，再用 Eigen `AngleAxisf` / `Affine3f` 计算 `alpha_m`。

可 RVV 化的片段有三类：

| 片段 | 可向量化理由 | 主要风险 |
| --- | --- | --- |
| pair geometry `f1..f4` | 非同点对之间的距离和 dot product（点积）是 lane-local 算术。 | AoS（结构数组）输入需要 staging 或 gather；输出仍按 production 顺序写回。 |
| HSV conversion `f5..f10` | RGB 分支可用 vector mask（向量掩码）表达。 | 分支多、整数到浮点转换多，staging 内存流量可能大于收益。 |
| `alpha_m` | Eigen 旋转对象可改写为 closed-form（闭式公式）后批处理。 | `atan2`、`sin` 符号修正和 near-X normal（接近 X 轴法线）边界需要误差预算。 |

本阶段不覆盖泛型彩色点型、`Scalar=double`、乱序 indices、非 dense 输入、correspondence（对应关系）或真实 production dispatch。`features/src/cppf.cpp` 中的 `computeCPPFPairFeature` 没有本地批量 loop（循环），只随 caller-shaped diagnostic（调用方形态诊断）取证。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `CPPFEstimation::computeFeature` | production public entry | 公开入口，生成 all-pairs CPPF output。 | `estimator.compute(output)` | `computeCPPFPairFeature`、Eigen `alpha_m` 链路 | production boundary（生产边界）对照；本轮未修改 | `features/include/pcl/features/impl/cppf.hpp` |
| `computeCPPFPairFeature` | production shared helper | 计算 `f1..f10`，包含 HSV conversion。 | `computeFeature` 和 test reference | `RGBtoHSV` | shared helper evidence（共享 helper 证据），不单独实施 | `features/src/cppf.cpp` |
| `computeCPPFReference` | diagnostic reference | 复刻公开入口输出语义和 checksum。 | gtest、bench | production helper、`computeAlphaMReference` | correctness baseline（正确性基线） | `test-rvv/features/cppf/include/impl/cppf_reference.hpp` |
| `computeCPPFPairHSVBatchRVV` | candidate formula / staging | SoA staging 后用 RVV 计算 `f1..f10`，`alpha_m` 保持标量。 | gtest、bench case | `computePairHSVFeaturesRVV` | component ablation；已拒绝 production probe | `test-rvv/features/cppf/include/impl/cppf_pair_hsv_candidate.hpp` |
| `computeCPPFAlphaMBatchRVV` | candidate formula / staging | 标量生成 `f1..f10` 后批量覆盖 `alpha_m`。 | gtest、bench case | `computeAlphaMClosedFormRVV` | component ablation；已拒绝 production probe | `test-rvv/features/cppf/include/impl/cppf_alpha_candidate.hpp` |
| `src/test_cppf.cpp` | correctness tests | 5 个 gtest 验证 reference、identity NaN、两个 RVV candidate 和 closed-form。 | `make run_test_compare`、board test | test-support helpers | QEMU/board correctness gate | `test-rvv/features/cppf/src/test_cppf.cpp` |
| `src/bench_cppf.cpp` | bench wrapper | 输出 4 个 case 的 ms/checksum，兼容共享分析脚本。 | QEMU smoke、board repeated | candidate/reference/public entry | board performance boundary（板卡性能边界） | `test-rvv/features/cppf/src/bench_cppf.cpp` |
| `generate_cppf_evidence_manifest.py` | analysis script | 将 repeated board analyze log 聚合成 Evidence Doctor manifest。 | `make evidence_doctor_repeated` | `test-rvv/script/evidence_doctor.py` | summary artifact 生成 | `test-rvv/features/cppf/script/generate_cppf_evidence_manifest.py` |
| `evidence_manifest.json` / `evidence_doctor.md` | evidence output summary | 保存 repeated board B/A、run paths 和 Doctor finding。 | phase result、evaluation | reviewer / worker closeout | diagnostic evidence summary；默认 local-only | `test-rvv/features/cppf/log/board/repeated/` |
| `doc/phases/000-current-state-and-gaps/result.zh.md` | documentation section | Phase 000 结果、doc-suite parity 和 Handoff Packet。 | README / evaluation | reviewer / 下一轮 worker | recovery pointer（恢复入口） | `test-rvv/features/cppf/doc/phases/000-current-state-and-gaps/result.zh.md` |

## 正确性证据

| 命令 / 证据 | 结果 | 证明范围 |
| --- | --- | --- |
| `make -C test-rvv/features/cppf run_test_compare` | QEMU Std/RVV 均通过 5/5 gtest。 | reference 与 production public compute 对拍；两个 RVV candidate 与 reference 对拍。 |
| `make -C test-rvv/features/cppf run_board_test fetch_board_logs ...` | 板卡 gtest 通过 5/5；日志路径 `test-rvv/features/cppf/log/board/test/run_test.log`。 | 目标硬件上 RVV test binary 可运行且 correctness 通过。 |
| checksum policy | board repeated 4 个 case checksum 均匹配；manifest 记录 checksum 来源。 | 计时 case 没有输出错配；不证明 production dispatch。 |

## 诊断证据链

板卡 repeated benchmark 使用 `--side 24 --index-count 64 --repeat 6 --iterations 8 --warmup 2 --case-filter all`，run count 为 5。性能结论只来自 board；QEMU bench compare 仅作为 log-shape smoke。

| case | B/A values | mean / median | EvidenceDecision |
| --- | --- | --- | --- |
| `component_cppf_reference` | `1.00, 1.01, 1.02, 1.01, 1.00` | mean `1.008x`，median `1.01x` | scalar reference 构建差异接近中性，只作基线健康检查。 |
| `candidate_cppf_pair_hsv_batch_rvv` | `0.48, 0.49, 0.49, 0.49, 0.48` | mean `0.486x`，median `0.49x` | rejected for production probe。 |
| `candidate_cppf_alpha_m_batch_rvv` | `0.82, 0.83, 0.82, 0.82, 0.82` | mean `0.822x`，median `0.82x` | rejected for production probe。 |
| `public_cppf_compute` | `0.99, 0.98, 0.99, 1.00, 0.99` | mean `0.99x`，median `0.99x` | current production has no RVV dispatch；只说明当前 public baseline 在 Std/RVV 构建间接近中性。 |

Evidence Doctor 报告 `Errors=3, Warnings=0, Suggestions=9`。3 个 Error 都是 `ba_degradation_frequency`（B/A 高频退化），对应两个 RVV 候选和 public smoke；它们不是 correctness bug，但阻止把本轮数据写成 production evidence（生产证据）或严格正向性能结论。9 个 Suggestion 是环境 metadata（taskset/governor/freq/temperature）和 binary identity（binary hash）缺失，以及 scalar reference near-threshold（接近阈值）。

## Diagnostic 到 Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic`；候选 helper 是 test-only component ablation。 |
| A/B boundary | Std/RVV build 下的 test helper 和 public smoke；没有 production dispatch。 |
| 当前决策问题 | 是否值得进入 bounded production probe。 |
| diagnostic 是否可外推到 production | 当前不能。候选引入 SoA staging、额外向量临时 buffer 和测试专用 output write，且 board 数据为负向。 |
| comparison-boundary / baseline mismatch 风险 | 有。`component_cppf_reference` 与 public `push_back` 路径不同，候选不是生产 helper。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段计划要求 component 至少 weak-positive 且 Doctor 无阻塞 Error；当前两个候选均 negative，不建议继续 production probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要；但当前未进入 production integration loop。 |

## S11 Closeout

本 topic 保留测试支撑和诊断文档，作为 CPPF / PPFRGB all-pairs color descriptor 后续筛选证据。当前未闭合范围包括泛型点型、不同 row source、`Scalar=double`、output resize 标量生产探针和更细 profile；这些不是当前 no-production 决策的阻塞项，因为 Phase 000 的主要 RVV candidate 已稳定负向，继续会扩大到新的 production 或 profiling topic。
