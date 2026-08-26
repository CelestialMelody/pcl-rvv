# PPF Optimization Roadmap

## 当前边界

当前 topic scope（主题范围）是 `features/include/pcl/features/impl/ppf.hpp` 的
`PPFEstimation::computeFeature`。当前 validated scope（已验证范围）是 test-only reference
（测试专用参考链路）、Phase 010 的 `PointXYZ + Normal` / float / AoS / ordered index row source
pair-feature batch RVV 诊断候选、Phase 030 的 `alpha_m` batch RVV 诊断候选，以及 Phase 040
exact `PointXYZ + Normal + PPFSignature` public production probe（公开生产探针）。

Phase 010 证明 SoA-staged pair-feature batch RVV correctness 成立，但 board repeated
benchmark（板卡重复性能测试）为 negative，因此不进入 production。Phase 030 证明 `alpha_m`
后段 test-only RVV candidate 在同一诊断边界下 5-run board positive，可作为 bounded production
probe（有界生产探针）的候选。Phase 040 已完成真实 public 入口接入和 production-public 板卡
验证；Phase 050 已完成用户确认后的 S11 production closeout。Phase 060 又把 exact gate 扩展为
traits-gated（基于字段特征门控）source xyz AoS + normal AoS + exact `PPFSignature`，并用
`PointXYZI + Normal`、`PointXYZ + PointNormal` 两个 production-public board case 验证收益。
当前 `alpha_m` RVV production behavior 已采纳，正式长期文档为 `doc-rvv/features/ppf-RVV.zh.md`。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| same-chain scalar reference | 当前源码 | all-pairs output、identity NaN、`computePairFeatures` helper choice | 给后续 RVV 对拍提供 oracle（判定参考）。 | 若误用 `computePPFPairFeature` 会偏离当前 production。 | Std/RVV correctness 都通过。 | adopted for diagnostics | none |
| pair-feature batch RVV | PFH direct-AoS sibling 经验 + 当前 PPF all-pairs | `f1..f4` 的 PFH helper 数学链路 | all-pairs 算术规模大，可能有局部收益。 | `atan2`/sqrt/FMA 误差、helper failure mask、output 顺序、SoA staging 成本。 | correctness、QEMU path、asm、5-run board component bench、Evidence Doctor。 | attempted / negative | closed in Phase 010 |
| alpha_m formula audit | Phase 010 负向结果 + 当前源码 Eigen rotation 后段 | `alpha_m` 后段公式 | 先判断是否存在可稳定对拍、可消融的 RVV 片段，避免盲写复杂三角函数 kernel。 | normal 接近 `-x`、非归一化 normal 和非有限输入仍未覆盖。 | scalar formula audit、数值预算、边界样本、必要时 RED test。 | adopted for next diagnostic | Phase 020 |
| alpha_m batch RVV | `alpha_m formula audit` 后续 | `alpha_m` 后段公式 | closed-form 已去掉 Eigen 对象构造，可用 `atan2_RVV_f32m2` 和向量代数批量计算。 | RVV `atan2` 误差、parallel-to-x mask、staging 成本、production dispatch 和泛型点类型曾是未闭合项，已由 Phase 040 / 060 的 production-public 证据替代诊断结论。 | same-chain 对拍、component bench、asm、board repeated、Evidence Doctor；production 结论看 Phase 040 / 060。 | diagnostic positive / superseded by production probes | none |
| output staging / direct store audit | 复筛表风险 + Phase 010 负向 | `output[index_i * input_size + j]` 写回 | 可能减少 temporary `PointOutT p` 和容器写回成本。 | 当前源码实际是 resize + indexed store；Phase 010 的 SoA staging 已明显负向。 | source audit、direct-AoS A/B、profile。 | attempted / negative for SoA staging; direct-AoS deferred | only after `alpha_m` audit shows value |
| production alpha_m batch RVV | phase loop + Phase 030 positive | 真实 `PPFEstimation::computeFeature` dispatch | Phase 040 public production probe 在 exact gate 下 5-run board speedup 约 `1.37x`。 | 泛型点类型、normal traits、`Scalar=double`、其它 row source 仍未闭合。 | PI1-PI5 已执行；Phase 050 已创建正式 `doc-rvv`。 | adopted production behavior / superseded by wider gate | closed in Phase 050 |
| ppf point type expansion | 用户要求继续扩大覆盖 + generic point type strategy | `RVVXYZAoSFloatLayout<PointInT>` source、PPF normal AoS normal、exact `PPFSignature` | 扩大同一 `alpha_m` RVV family 覆盖面；代表性点型板卡仍 positive。 | 其它自定义 traits-compatible 点型未逐个 board；`Scalar=double`、非 `PPFSignature` 和其它 row source 不覆盖。 | production direct tests、fallback test、QEMU smoke、asm、两组 5-run board repeated 和 Evidence Doctor。 | adopted production behavior | closed in Phase 060 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 000 | Phase 010 应先做 pair-feature batch RVV，不要把 `alpha_m` 混进第一轮。 | reference 已能复刻 production 输出，适合拆分候选。 | correctness、bench、asm、board、Evidence Doctor。 | completed |
| Phase 010 | `alpha_m` formula audit 应排在 direct-AoS gather 之前。 | SoA-staged pair-feature RVV 5-run negative，继续只改 pair feature 或 store 形态缺少收益支撑；真正未知成本在 `alpha_m` 后段。 | Phase 020 plan、数值预算、必要时 component candidate。 | high |
| Phase 020 | `alpha_m` closed-form helper 可作为 RVV candidate 的 reference。 | Std/RVV correctness 均通过，说明可先写 test-only RVV batch helper，不必先改 production。 | Phase 030 correctness、bench、asm、board、Evidence Doctor。 | high |
| Phase 030 | `alpha_m` batch RVV 是当前唯一支持有界生产探针的候选。 | 5-run board repeated 为 `1.57, 1.57, 1.56, 1.57, 1.57`，且 doctor 没有 alpha 退化 Error。 | 用户确认 PI1 后，补 production direct / fallback / asm / board 计划；先保持 pair-feature batch RVV rejected。 | high / blocked on authorization |
| Phase 040 | production-public 证据支持 exact gate 下的 `alpha_m` RVV 生产补丁。 | 5-run public entry speedup 为 `1.35, 1.35, 1.37, 1.39, 1.38`，Evidence Doctor 无 Error / Warning。 | Phase 050 采纳确认后创建 `doc-rvv/features/ppf-RVV.zh.md`。 | completed |
| Phase 050 | S11 production closeout 已完成。 | 正式长期文档、evaluation、matrix、README 和筛选清单同步。 | 后续泛型点型已由 Phase 060 处理；其它方向仍只能以新 phase 处理证据增强或 direct-AoS pair-feature revisit。 | superseded by Phase 060 |
| Phase 060 | point-type expansion 已完成并采纳。 | `PointXYZI + Normal` mean speedup 约 `1.384x`，`PointXYZ + PointNormal` mean speedup 约 `1.33x`，两组 Doctor 均 `0E/0W/2S`。 | 恢复扫描发现 doc-suite role inventory 和独立 role docs 尚未闭合，因此进入 Phase 070。 | superseded by Phase 070 |
| Phase 070 | doc-suite parity closeout 已完成。 | 复杂 topic 的 testing overview、correctness tests、benchmark/evidence、optimization evidence 和 test-support code map 已拆成独立文档，并回填 `doc_suite_role_inventory`。 | 当前 topic 内没有新的高优先级未阻塞优化动作；后续只建议按新授权做 evidence hardening、profile-driven pair-feature revisit 或 PPFRGB 独立评估。 | ready for review |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| 直接替换为 `computePPFPairFeature` | 当前 production `impl/ppf.hpp` 实际调用 `computePairFeatures`，直接替换会改变 `f1..f4` 语义。 | 若另有 issue 明确要求修正算法语义，再作为行为变更 topic 处理。 |
| SoA-staged pair-feature batch RVV | Phase 010 board repeated 显示 5/5 退化，Evidence Doctor 报 `ba_degradation_frequency` Error。 | 只有新增 profile 证明退化来自可移除 staging / buffer，而 direct-AoS 同边界 A/B 有独立正向信号时才恢复。 |
| 直接把 Phase 030 helper 当作已采纳 production 行为 | Phase 030 是 test helper boundary；production 判断必须使用 Phase 040 production-public 数据。 | 已由 Phase 040 / Phase 050 替代；当前 adopted 结论来自 production-public 数据。 |
| 未经代表性 production-public 证据就把 exact gate 写成泛型结论 | exact gate 只能是阶段范围，不能自动覆盖其它点型。 | Phase 060 已闭合 source xyz AoS 和 normal AoS 的代表性生产证据；其它自定义 traits-compatible 点型仍不逐个外推 board 数值。 |
| 继续微调 `alpha_m` staging buffer | 当前 production-public 已稳定正向，进一步收益预期小；需要同边界 A/B 和板卡证据。 | 只有 profile 或 reviewer 要求证明 staging 成本时恢复。 |
| `Scalar=double` / 非 `PPFSignature` output | 当前 RVV helper 直接读 float AoS 字段并写 `PPFSignature::alpha_m`，输出语义不适合自动泛化。 | 需要单独设计和完整 production direct、fallback、asm、board 证据。 |

## 默认恢复动作

`roadmap_default_recovery_queue`：

| order | action | status | resume condition |
| --- | --- | --- | --- |
| 1 | 当前 traits-gated production boundary ready for review。 | turn_stop_deferred with stop_condition_hit | Phase 060 已完成采纳收尾；roadmap / matrix 已无当前 PPF topic 内高优先级未阻塞优化动作。 |
| 2 | 若用户要求更严格归档，创建 evidence hardening phase。 | separate phase required | 补 taskset、governor、freq、temperature 和 binary hash 后重跑 bounded board evidence；这是归档增强，不是新优化。 |
| 3 | 若 profile 指向 `f1..f4`，创建 direct-AoS pair-feature revisit phase。 | separate phase required | 必须避免 Phase 010 SoA staging 退化，并用同边界 A/B 证明收益。 |
| 4 | 若要扩大到 PPFRGB / CPPF 或其它 caller。 | separate topic required | caller 语义、color / region search、row source 和 output 边界不同，不能从 PPF production-public 数据外推。 |
| 5 | Phase 070 后 ready for review。 | turn_stop_deferred with stop_condition_hit | production boundary、doc-suite role docs、matrix、roadmap 和 Handoff 已闭合；当前 PPF topic 内没有高优先级未阻塞优化动作。 |
