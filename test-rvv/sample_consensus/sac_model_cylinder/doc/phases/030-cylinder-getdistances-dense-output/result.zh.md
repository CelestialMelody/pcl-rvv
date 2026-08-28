# Phase 030: cylinder getDistances dense output 结果

## 当前状态

本阶段把 `SampleConsensusModelCylinder::getDistancesToModel` 接入 production RVV（生产 RVV）路径，并用接入后的
production direct（真实生产路径）证据判断是否采纳。用户偏好已冻结为：接入后板卡测试显示收益即可采纳，正式
`doc-rvv` 文档使用接入后的板卡测试数据。

最终 EvidenceDecision（证据决策）：
`production-adopted/getDistances-direct-indexed-PointXYZ-Normal`。该阶段结论只覆盖 public `getDistancesToModel`、
direct indexed `indices_`、`PointXYZ + pcl::Normal` 代表数据、float xyz/normal AoS（结构数组）布局、
`Eigen::VectorXf` coefficients 和 65536 点 shuffled adjacent pairs bench case。Phase 040 已刷新同一
production manifest（生产证据清单），当前 topic truth 以四组代表点型 × 三入口的 12 项 production direct
board summary 为准；本文件只保留 Phase 030 的 getDistances 接入闭环和历史异常处理。

## 实际执行范围

| 计划动作 | 状态 | 证据路径 / 命令 | 结论 |
| --- | --- | --- | --- |
| 生产 helper | done | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` | 抽出 `getDistancesToModelStandardCylinder`，新增 `getDistancesToModelRVVCylinder`，公开入口先尝试 RVV，失败回到 Standard helper。 |
| correctness | done | `make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare` | 当前 Std/RVV 各 11 个 gtest 通过；包括小样本 public-vs-Standard、4096 点 bench-shaped correctness 和 Phase 040 代表点型扩展。 |
| bench 输出 | done | `src/bench_sac_model_cylinder.cpp`、`script/generate_cylinder_board_evidence_manifest.py` | bench 增加 `public getDistancesToModel` timing 和 checksum 行，production manifest 支持 `getdistances` item。 |
| asm | done | `make -C test-rvv/sample_consensus/sac_model_cylinder clean_bench_rvv check_production_asm` | 当前 `getDistancesToModelRVVCylinder` 有 298 条 RVV 指令，包含 `vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`。 |
| board repeated | done | `make -C test-rvv/sample_consensus/sac_model_cylinder collect_production_repeated_board_evidence` | 5-run board 每轮 11 个 gtest 通过；getDistances speedup bucket 稳定 positive，且 Phase 040 已把 manifest 扩到 12 项 comparison。 |
| Evidence Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_cylinder record_production_board_evidence_state`、`production_evidence_status` | production Doctor Errors=0、Warnings=0、Suggestions=0；registry fresh。 |

## Production Direct Board Summary

证据路径：

- manifest：`test-rvv/sample_consensus/sac_model_cylinder/doc/phases/020-cylinder-production-integration/production-repeated-evidence-manifest.json`
- Evidence Doctor：`test-rvv/sample_consensus/sac_model_cylinder/doc/phases/020-cylinder-production-integration/production-repeated-evidence-doctor.md`
- registry：`test-rvv/sample_consensus/sac_model_cylinder/log/evidence_registry.json`

| comparison | run count | Std mean ms | RVV mean ms | median | min | max | checksum | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| public `getDistancesToModel` / `PointXYZ + Normal` | 5 | `15.146606` | `2.182286` | `6.9318x` | `6.8120x` | `7.0688x` | `65740 == 65740` | positive |

该结果远高于 positive 阈值 `1.20x`，且 5/5 run 均大于 `1.0x`。同一 fresh manifest 中 count/select 也保持正向：
count median `5.3124x`，select median `4.6445x`。Phase 040 的三组新增代表点型也全部 positive，完整 12 项数据见
`040-cylinder-point-type-expansion/result.zh.md` 和长期 production 文档。

## Checksum 异常处理

本阶段第一次板卡 5-run 曾触发 Evidence Doctor Error：`getDistancesToModel` checksum mismatch。该异常不是性能回归，
而是 bench checksum policy（校验和策略）过严：旧 `hashDistances` 直接 hash 浮点距离值，而 RVV float 中间量与标量
double 中间量允许 `1e-5` 量级差异。处理动作：

1. 将 `hashDistances` 改为只返回 `distances.size()`，用于保护 dense vector 输出规模。
2. 新增 `GetDistancesBenchShapedPublicEntryMatchesStandardHelper`，用 4096 点 bench-shaped 输入逐项检查 `1e-5` 近似一致性。
3. 重新运行 production repeated board、manifest、Doctor 和 registry。

带 Error 的旧 run 已降级为 historical contaminated run（历史污染运行），不参与当前性能结论。

## Diagnostic 到 Production Mismatch Audit 回填

| question | answer |
| --- | --- |
| evidence role | 当前最终证据是 `production_direct`；dense-store 经验只作为候选来源。 |
| A/B boundary | Std build public `getDistancesToModel` vs RVV build 同一 public `getDistancesToModel`。 |
| 当前决策问题 | 新增 getDistances RVV path 是否值得接入并按用户准则采纳。 |
| diagnostic 是否可外推到 production | 不外推 diagnostic performance；最终只使用接入后的 board repeated。 |
| comparison-boundary / baseline mismatch 风险 | 已通过同一 public overload、同一 bench case、同一 checksum policy 和 gtest 数值对拍控制。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive，不触发弱 / 负 / 中性 / 不稳定处理。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有既有 getDistances RVV family，不是 family-selection；不需要 RVV-vs-RVV A/B。 |

## Phase Scope 与扩展队列

`validated_scope`：public `getDistancesToModel`、direct indexed `indices_`、`PointXYZ + pcl::Normal` 代表数据、
float xyz/normal AoS、`Eigen::VectorXf` coefficients、dense `std::vector<double>` output、65536 shuffled adjacent pairs。

`unvalidated_scope`：更多 PointXYZ-like / Normal-like 点型、custom layout、`Scalar=double`、identity-index 专门路径、
真实上游 RANSAC / MSAC / MLESAC workload 和其它 public entry。

`point_type_expansion_queue`：Phase 040 已完成 `PointXYZI + Normal`、`PointXYZRGB + Normal` 和
`PointXYZ + PointNormal` 三组代表点型扩展。继续扩大到 `PointXYZRGBA`、`PointXYZINormal` 或自定义点型应先有
真实调用价值、profile（性能剖析）或用户点名范围；当前代表点型性能不能外推为完整模板泛型性能。

## 继续 / 停止决策

`continue_stop_decision`：当前 Phase 030 closed / adopted，并已被 Phase 040 的代表点型扩展刷新为 topic 当前证据链的一部分。

`stop_condition_hit`：本阶段 plan 的 implementation、correctness、bench、asm、board、Doctor 和 registry 均闭合；
当前授权采纳范围已经有接入后板卡收益。Phase 040 已完成原先最值得继续的覆盖面扩展；剩余方向需要新的触发证据：

- `identity-index-A/B`：暂不建议默认推进，除非有真实 workload/profile 显示 identity indices 为主。
- `optimizeModelCoefficients` staging：暂不建议默认推进，除非 profile 显示 Eigen array staging 接近主成本。

`next_phase_default`：当前 cylinder topic 暂停在 adopted / ready_for_review；若转模块队列，cylinder 正向已解除 cone 的等待条件。
