# Phase 020: cylinder production integration 结果

## 当前状态

本阶段完成 `countWithinDistance` 与 `selectWithinDistance` 的 PI2-PI5 production integration loop（生产接入闭环）。
后续 Phase 030 又把 `getDistancesToModel` 接入同一 production boundary（生产边界），Phase 040 进一步补齐
代表点型证据，并刷新了本阶段沿用的 production repeated manifest。因此本文件中的当前 production board summary
以 Phase 040 fresh manifest 为准，覆盖四组代表点型 × 三条公开入口；本阶段原始三项数字只作为历史运行背景。

最终 EvidenceDecision（证据决策）：
`production-adopted/count-select-getDistances-direct-indexed-representative-point-types`。该结论覆盖本阶段、Phase 030
和 Phase 040 已验证的公开入口、direct indexed `indices_`、`PointXYZ + Normal`、`PointXYZI + Normal`、
`PointXYZRGB + Normal`、`PointXYZ + PointNormal` 四组代表点型、float xyz/normal AoS（结构数组）布局和
65536 点 shuffled adjacent pairs bench case。它不覆盖 `optimizeModelCoefficients`、`projectPoints`、
`doSamplesVerifyModel`、`Scalar=double`、其它 row source policy（行来源策略）或自定义点型全集。

## 实际执行范围

| 计划动作 | 状态 | 证据路径 / 命令 | 结论 |
| --- | --- | --- | --- |
| PI2 production patch | done | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` | 原 count/select 标量主体抽成 Standard helper；`__RVV10__` 构建下按 layout / normal / index / size gate 尝试 RVV，失败自然回退标量。 |
| PI3 production direct correctness | done | `make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare` | 当前 Std/RVV 各 11 个 gtest 通过；包括 Phase 030 getDistances 对拍和 Phase 040 代表点型扩展。 |
| PI4 production asm attribution | done | `make -C test-rvv/sample_consensus/sac_model_cylinder clean_bench_rvv check_production_asm` | `countWithinDistanceRVVCylinder` 306 条、`selectWithinDistanceRVVCylinder` 338 条、`getDistancesToModelRVVCylinder` 298 条 RVV 指令。 |
| PI4 production repeated board | done | `make -C test-rvv/sample_consensus/sac_model_cylinder collect_production_repeated_board_evidence` | 5-run board，每轮 11 个 gtest 通过；12 项 public comparison 都是 positive。 |
| PI5 Evidence Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_cylinder record_production_board_evidence_state` | `production-repeated-evidence-doctor.md` 为 Errors=0、Warnings=0、Suggestions=0；registry 已登记 production manifest / doctor。 |

板卡日志中远端 make 多次报告 `script/rvv-board-run.mk` clock skew（时钟偏移）warning。该 warning 未导致编译、
测试、bench 或 rsync 失败；Evidence Doctor 没有把当前 repeated 数据判为异常。

## Production Patch 摘要

| 文件 | 变更 |
| --- | --- |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` | 新增 cylinder 三入口的 Standard helper、RVV layout gate、RVV helper 和公开入口 dispatch。 |
| `test-rvv/sample_consensus/sac_model_cylinder/src/test_sac_model_cylinder.cpp` | 增加 public entry 对 Standard helper 的生产直连 correctness、getDistances bench-shaped correctness、代表点型和 fallback 回归测试。 |
| `test-rvv/sample_consensus/sac_model_cylinder/src/bench_sac_model_cylinder.cpp` | public timing 行和 typed public timing 行作为 production direct 证据；diagnostic candidate 行保留为历史交叉检查。 |
| `test-rvv/sample_consensus/sac_model_cylinder/script/check_cylinder_production_asm.py` | 检查三条 production RVV helper 的热点符号与目标 RVV 指令。 |
| `test-rvv/sample_consensus/sac_model_cylinder/script/generate_cylinder_board_evidence_manifest.py` | `--mode production` 只把 public 行写成 production direct manifest；当前 items 为 `count,select,getdistances,pointtypes`。 |

生产源码没有修改 public API（公开接口）声明。非 RVV 构建没有 RVV helper 实例，公开入口直接调用 Standard helper。
RVV 构建的 fallback gate 包括：点 / 法线字段布局不匹配、`pcl::index_t` 不是 signed 32-bit、cloud 或 normal
无法用 32-bit byte offset 表示、normal cloud 小于 input cloud。

## Production Direct Board Summary

证据路径：

- manifest：`test-rvv/sample_consensus/sac_model_cylinder/doc/phases/020-cylinder-production-integration/production-repeated-evidence-manifest.json`
- Evidence Doctor：`test-rvv/sample_consensus/sac_model_cylinder/doc/phases/020-cylinder-production-integration/production-repeated-evidence-doctor.md`
- registry：`test-rvv/sample_consensus/sac_model_cylinder/log/evidence_registry.json`

| comparison | run count | Std mean ms | RVV mean ms | median | min | max | checksum | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| public `countWithinDistance` / `PointXYZ + Normal` | 5 | `11.656673` | `2.167619` | `5.3124x` | `5.1606x` | `5.6082x` | `41164 == 41164` | positive |
| public `selectWithinDistance` / `PointXYZ + Normal` | 5 | `12.588158` | `2.726701` | `4.6445x` | `4.5348x` | `4.6579x` | `5168917350774937783 == 5168917350774937783` | positive |
| public `getDistancesToModel` / `PointXYZ + Normal` | 5 | `15.146606` | `2.182286` | `6.9318x` | `6.8120x` | `7.0688x` | `65740 == 65740` | positive |
| typed public entries | 5 | 见 Phase 040 result | 见 Phase 040 result | count `5.5145x / 5.3986x / 5.4105x`，select `4.4819x / 4.4373x / 4.3314x`，getDistances `6.7851x / 7.3186x / 6.2453x` | 全部大于 `4.0x` | 全部 checksum matched | positive |

12 项 public comparison 都远高于 Phase 020 / 030 / 040 计划中的 positive 阈值 `1.20x`。本阶段不需要追加复跑；
rerun budget（复跑预算）5 次已用完，decision bucket 稳定。

## Diagnostic 到 Production Mismatch Audit 回填

| question | answer |
| --- | --- |
| evidence role | 当前最终证据是 `production_direct`，Phase 000 diagnostic 只作为候选来源和历史对照。 |
| A/B boundary | baseline 是 Std build 的 public overload；candidate 是 RVV build 的同一 public overload。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path，并且是否按用户准则保留 production patch。 |
| diagnostic 是否可外推到 production | 不外推 performance。最终采纳只使用当前 production direct board 数据。 |
| comparison-boundary / baseline mismatch 风险 | 已通过 production public Std/RVV repeated 消除主要 mismatch；仍不证明其它入口或未测试点型。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前为 positive；不触发弱 / 负 / 中性 / 不稳定处理。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前 cylinder 没有既有 adopted RVV family，本阶段不是 RVV-family-selection；不需要 RVV-vs-RVV family A/B。 |

## Doc Suite Closeout Audit

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | `README.zh.md` 已指向三入口 production-adopted 状态。 | README 必须给阅读路径、命令和证据提交边界。 | adopted | 本阶段同步刷新。 | 无。 |
| testing_overview | `doc/testing-overview.zh.md` 区分 production direct、historical diagnostic、board repeated 和 registry。 | target 粒度审计必须可恢复。 | adopted | 本阶段同步刷新。 | 无。 |
| correctness_tests | `doc/correctness-tests.zh.md` 覆盖 11 个 gtest 字典。 | 每个 TEST 说明输入、被测路径、断言和不能证明范围。 | adopted | 本阶段同步刷新。 | 无。 |
| benchmark_and_evidence | `doc/benchmark-and-evidence.zh.md` 引用 fresh production manifest。 | 性能结论必须来自 board repeated。 | adopted | Doctor 0/0/0。 | 无。 |
| optimization_evidence | `doc/optimization-evidence.zh.md` 已映射 adopted / historical / deferred candidate。 | adopted / deferred candidate 要连接代码和证据。 | adopted | 本阶段同步刷新。 | 无。 |
| optimization_roadmap | `doc/optimization-roadmap.zh.md` 记录 Phase 040 已完成，并保留 profile 触发路线。 | roadmap 不能把可继续范围伪装成完成。 | adopted | 当前采纳范围闭合。 | 无默认继续项。 |
| test_support_code_map | `doc/test-support-code-map.zh.md` 加入 production helper、asm、manifest 和 registry。 | 代码地图应能定位 production 和证据。 | adopted | 本阶段同步刷新。 | 无。 |
| phase index / matrix | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` 加入 Phase 040。 | phase loop 必须能短 prompt 恢复。 | adopted | 本阶段同步刷新。 | 无。 |
| production_topic_doc | `doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md` 已创建。 | 长期文档只写已采纳 production 行为。 | adopted | 使用接入后 fresh board 数据。 | 无。 |
| artifact tracking | topic-local docs、manifest、doctor、registry 都在当前 topic 路径扫描中可见。 | 新增文档需列入当前 topic artifact 集合，raw logs 默认排除。 | adopted | 最终检查使用包含 untracked 的路径限定扫描。 | 无。 |

## 继续 / 停止决策

`continue_stop_decision`：当前已授权采纳范围 closed / ready_for_review。Phase 040 已补齐原先可继续的代表点型扩展。

`stop_condition_hit`：当前 phase plan 的 PI2-PI5 证据矩阵已闭合，且用户准则允许“接入后板卡有收益即可采纳”。
剩余方向需要新 phase scope（阶段范围），不应混入本阶段：

- 自定义点型全集：Phase 040 只批准三组新增代表点型；继续扩大需要真实调用价值、profile 或用户点名范围。
- `identity-index-A/B`：仅在真实 workload 大量 identity indices 且当前 gather 成本被 profile 证明为瓶颈时再做。
- `optimizeModelCoefficients` staging：需要 profile 证明 Eigen array staging 接近主成本。

`next_phase_default`：当前 cylinder topic 暂停在 adopted / ready_for_review；若按模块队列推进，cylinder 正向后可复筛 cone 或转入其它独立 topic。
