# Phase 040: cylinder point type expansion 结果

## 当前状态

本阶段验证已采纳的 cylinder production RVV（生产 RVV）路径在更多代表点型组合上的 correctness（正确性）、
反汇编归属和 board repeated（重复板卡测试）表现。生产源码未新增算法形态；本阶段只扩大当前 traits-gated
（字段特征门控）生产分流的证据范围。

最终 EvidenceDecision（证据决策）：
`production-adopted/representative-point-types-direct-indexed`。该结论覆盖
`PointXYZ + Normal`、`PointXYZI + Normal`、`PointXYZRGB + Normal` 和 `PointXYZ + PointNormal`
四组代表点型组合、direct indexed `indices_`、float xyz / normal AoS（结构数组）布局、`Eigen::VectorXf`
系数和 65536 点 shuffled adjacent pairs bench case。它不证明所有自定义点型、`Scalar=double`、identity-index
专门路径或真实上游 workload（工作负载）都已闭合。

## 实际执行范围

| 计划动作 | 状态 | 证据路径 / 命令 | 结论 |
| --- | --- | --- | --- |
| typed fixtures | done | `src/test_sac_model_cylinder.cpp`、`src/bench_sac_model_cylinder.cpp` | 同一圆柱几何输入可生成 `PointXYZI + Normal`、`PointXYZRGB + Normal` 和 `PointXYZ + PointNormal`。 |
| typed correctness | done | `make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare` | Std/RVV 各 11 个 gtest 通过；三组新增点型三入口均与 Standard helper 对齐。 |
| typed bench labels | done | `src/bench_sac_model_cylinder.cpp`、`script/generate_cylinder_board_evidence_manifest.py` | production manifest 现在包含 12 个 public comparison。 |
| asm | done | `make -C test-rvv/sample_consensus/sac_model_cylinder clean_bench_rvv check_production_asm` | 新增点型实例化后，三条 production helper 仍有 RVV 指令归属。 |
| board repeated | done | `make -C test-rvv/sample_consensus/sac_model_cylinder collect_production_repeated_board_evidence` | 5-run board；每轮 11 个 gtest 通过，12 个 comparison 全部 positive。 |
| Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_cylinder record_production_board_evidence_state`、`production_evidence_status` | Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0；registry fresh。 |

## Production Direct Board Summary

证据路径：

- manifest：`test-rvv/sample_consensus/sac_model_cylinder/doc/phases/020-cylinder-production-integration/production-repeated-evidence-manifest.json`
- Evidence Doctor：`test-rvv/sample_consensus/sac_model_cylinder/doc/phases/020-cylinder-production-integration/production-repeated-evidence-doctor.md`
- registry：`test-rvv/sample_consensus/sac_model_cylinder/log/evidence_registry.json`

| point type | entry | run count | Std mean ms | RVV mean ms | median | min | max | checksum | decision bucket |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `PointXYZ + Normal` | `countWithinDistance` | 5 | `11.656673` | `2.167619` | `5.3124x` | `5.1606x` | `5.6082x` | `41164 == 41164` | positive |
| `PointXYZ + Normal` | `selectWithinDistance` | 5 | `12.588158` | `2.726701` | `4.6445x` | `4.5348x` | `4.6579x` | `5168917350774937783 == 5168917350774937783` | positive |
| `PointXYZ + Normal` | `getDistancesToModel` | 5 | `15.146606` | `2.182286` | `6.9318x` | `6.8120x` | `7.0688x` | `65740 == 65740` | positive |
| `PointXYZI + Normal` | `countWithinDistance` | 5 | `11.715698` | `2.138036` | `5.5145x` | `5.3724x` | `5.5245x` | `41164 == 41164` | positive |
| `PointXYZI + Normal` | `selectWithinDistance` | 5 | `12.648999` | `2.826342` | `4.4819x` | `4.3949x` | `4.5514x` | `5168917350774937783 == 5168917350774937783` | positive |
| `PointXYZI + Normal` | `getDistancesToModel` | 5 | `15.144458` | `2.244163` | `6.7851x` | `6.5743x` | `6.8318x` | `65740 == 65740` | positive |
| `PointXYZRGB + Normal` | `countWithinDistance` | 5 | `11.698597` | `2.176810` | `5.3986x` | `5.2755x` | `5.4519x` | `41164 == 41164` | positive |
| `PointXYZRGB + Normal` | `selectWithinDistance` | 5 | `12.637544` | `2.851038` | `4.4373x` | `4.3543x` | `4.5229x` | `5168917350774937783 == 5168917350774937783` | positive |
| `PointXYZRGB + Normal` | `getDistancesToModel` | 5 | `15.145902` | `2.081939` | `7.3186x` | `7.1422x` | `7.3352x` | `65740 == 65740` | positive |
| `PointXYZ + PointNormal` | `countWithinDistance` | 5 | `11.715236` | `2.167212` | `5.4105x` | `5.3094x` | `5.4691x` | `41164 == 41164` | positive |
| `PointXYZ + PointNormal` | `selectWithinDistance` | 5 | `12.645315` | `2.977867` | `4.3314x` | `4.0728x` | `4.3635x` | `5168917350774937783 == 5168917350774937783` | positive |
| `PointXYZ + PointNormal` | `getDistancesToModel` | 5 | `15.195919` | `2.443617` | `6.2453x` | `6.0669x` | `6.2818x` | `65740 == 65740` | positive |

12 个 comparison 均高于 positive 阈值 `1.20x`，且 5/5 run 都大于 `1.0x`。板卡日志仍有远端 make clock skew
（时钟偏移）warning，但没有导致编译、测试、bench、manifest 或 Evidence Doctor 异常。

## Diagnostic 到 Production Mismatch Audit 回填

| question | answer |
| --- | --- |
| evidence role | 当前最终证据是 `production_direct`；不使用 diagnostic performance 外推。 |
| A/B boundary | Std build public overload vs RVV build 同一 public overload。 |
| 当前决策问题 | 已接入的 traits-gated RVV family 是否可采纳到新增代表点型边界。 |
| diagnostic 是否可外推到 production | 不外推；本阶段直接运行生产公开入口。 |
| comparison-boundary / baseline mismatch 风险 | 通过同一点型、同一 public entry、同一 shuffled bench case、checksum 和 manifest metadata 控制。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已是有界 production probe；结果全部 positive，不触发降级。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不选择新 RVV family，只扩大同一 production family 的代表点型范围；不需要 RVV-vs-RVV family A/B。 |

## Evidence Doctor 和 Registry

Evidence Doctor 输入为当前 production manifest，输出为 Errors=0、Warnings=0、Suggestions=0。`production_evidence_status`
检查 registry fresh。manifest 目录仍沿用 Phase 020 的 production repeated evidence 路径，但标题和 comparison
内容已经刷新为 Phase 040 的 12 项当前证据；较早的 3 项 summary 只能作为历史 run，不再是当前 truth。

## Phase Scope 与扩展队列

`validated_scope`：三条 public entry、direct indexed `indices_`、四组代表点型组合、float xyz / normal AoS、
`Eigen::VectorXf` coefficients、65536 shuffled adjacent pairs、production direct board repeated。

`unvalidated_scope`：自定义点型全集、`PointXYZRGBA + Normal`、`PointXYZINormal` 这类 source / normal 复合点型、
`Scalar=double`、identity-index 专门路径、NaN/Inf、超大 byte offset、真实 RANSAC / MSAC / MLESAC workload。

`point_type_expansion_queue`：当前代表点型扩展已 closed / adopted。继续扩大到更多点型应另开新的 point-type
phase，并先说明目标点型的源码调用价值；不建议把所有可编译自定义点型做成无边界 bench 矩阵。

## 继续 / 停止决策

`continue_stop_decision`：Phase 040 closed / adopted；当前 cylinder topic 内没有仍建议默认推进的未阻塞优化方向。

`stop_condition_hit`：当前 phase 的 correctness、QEMU log-shape、asm、5-run board、Doctor、registry 和文档刷新均闭合。
剩余方向需要新的触发证据，不适合无 profile 盲跑：

- `identity-index-A/B` 只在真实 workload/profile 显示 identity indices 占主导，且 gather 成本成为瓶颈时再启动。
- `optimizeModelCoefficients` staging 只在 profile 显示 Eigen array staging 接近主成本，且不会与 Eigen solver 优化重复时再启动。
- cone / normal-sphere / circle3d 属于后续 topic，不是 cylinder 当前 phase 的未完成动作。

`next_phase_default`：当前 topic 暂停在 adopted / ready_for_review；如果继续 sample_consensus 保留队列，优先转入下一个独立 topic。
