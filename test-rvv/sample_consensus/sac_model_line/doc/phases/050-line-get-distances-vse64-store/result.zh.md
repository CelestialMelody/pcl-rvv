# Phase 050 Result: line getDistances RVV direct double store

## 结果摘要

Phase 050 已完成 `SampleConsensusModelLine<PointT>::getDistancesToModelRVV` 的 store-shape（写回形态）替换：旧形态在 `vfsqrt` 后先 `vse32` 写 `float` scratch，再用标量 lane loop（逐通道标量循环）转成 `double` 写入 `std::vector<double>`；当前形态改为 RVV `vfwcvt`（float 到 double 拓宽转换）加 `vse64`（64 位向量写回）直接写 dense double distance output（连续 double 距离输出）。

本阶段不改变 public API（公开接口）、不改变 count/select 两个入口、不扩大点型、`Scalar`、layout 或 row source（行来源）。当前 production boundary（生产边界）仍是 `PointXYZ` 风格 float xyz AoS（结构数组）、direct indexed `indices_`、signed 32-bit `pcl::index_t`、u32 byte offset gate（32 位字节偏移门禁）和 RVV 构建。

EvidenceDecision：`getDistances-vfsqrt-vse64-store` 在当前 production boundary 下 adopted（已采纳）。Phase 050 接入后 public `getDistancesToModel` 板卡 5-run repeated 的 median/min/max 为 `4.2237x / 4.0338x / 4.2728x`，RVV avg 为 `0.548416 ms`；相对 Phase 040 `getDistances-vfsqrt-scratch-scalar-store` 的 RVV avg `0.651839 ms`，当前形态约 `1.19x` 更快，RVV 耗时降低约 `15.9%`。Evidence Doctor 为 Errors=0 / Warnings=0 / Suggestions=0。

## 实际执行范围

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| RED asm gate | done | 旧实现下强化后的 `check_production_asm` 能因首个 `vfsqrt` 窗口出现 scratch `vse32` 而失败。 | asm gate 能区分旧 scratch 写回和新 direct double store。 |
| production patch | done | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` | 删除 `chunk_distances` 和标量 lane 写回，改为 `vfwcvt_f_f_v_f64m4` + `vse64`。 |
| GREEN asm gate | done | `make -C test-rvv/sample_consensus/sac_model_line check_production_asm` | `getDistancesToModelRVV` 命中 `vfsqrt`、`vfwcvt`、`vse64`，首个 `vfsqrt` 窗口不再出现 scratch `vse32`。 |
| correctness | done | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` | Std/RVV 各 7 个 gtest 全部通过。 |
| board repeated | done | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_vse64_evidence` | 5/5 runs 完成，RVV gtest 每轮 7/7 通过。 |
| Evidence Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_vse64_evidence_state`、`repeated_production_vse64_evidence_status` | manifest、doctor Markdown、doctor JSON 已登记，freshness check 通过。 |

## Production direct 证据

命令：

```bash
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_vse64_evidence
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_vse64_evidence_state
make -C test-rvv/sample_consensus/sac_model_line repeated_production_vse64_evidence_status
```

证据路径：

- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-doctor.json`

| production public entry | Std avg ms | RVV avg ms | B/A values | median / min / max | decision bucket |
| --- | ---: | ---: | --- | --- | --- |
| `countWithinDistance` | 1.777136 | 0.370890 | `4.7791x, 4.8269x, 4.8269x, 4.7463x, 4.7793x` | `4.7793x / 4.7463x / 4.8269x` | positive-stable |
| `selectWithinDistance` | 2.476793 | 0.791395 | `3.0786x, 3.2815x, 3.0318x, 3.1014x, 3.1593x` | `3.1014x / 3.0318x / 3.2815x` | positive-stable |
| `getDistancesToModel` | 2.280606 | 0.548416 | `4.2728x, 4.0365x, 4.2292x, 4.0338x, 4.2237x` | `4.2237x / 4.0338x / 4.2728x` | positive-stable |

这些数据来自接入后的公开入口 Std/RVV 构建对比。QEMU（仿真器）不用于性能结论。count/select 数字来自同一 Phase 050 binary 的 public rows，用于确认本次 getDistances 写回调整没有破坏其它两个入口的 production direct bucket；它们的实现族没有改变。

## Phase 040 对照

Phase 040 是本阶段 A/B 的历史 production baseline（生产基线）：`getDistancesToModelRVV` 使用 `vfsqrt` 后 scratch `vse32` + 标量 lane double store。Phase 040 public `getDistancesToModel` RVV avg 为 `0.651839 ms`，median/min/max 为 `3.4036x / 3.3409x / 3.5229x`。Phase 050 public `getDistancesToModel` RVV avg 为 `0.548416 ms`，median/min/max 为 `4.2237x / 4.0338x / 4.2728x`。两批数据来自不同 repeated batch，因此不写成逐 run 严格同步 A/B；但两者同属 production helper boundary，且 Phase 050 的提升幅度明显，足以保留 direct double store 形态。

## Correctness、asm 和 Evidence Doctor

| 证据 | 命令 / 路径 | 结果 | 边界 |
| --- | --- | --- | --- |
| QEMU correctness | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` | Std/RVV 各 7 个 gtest 全部通过。 | 证明功能和路径可运行，不证明目标硬件性能。 |
| production asm attribution | `make -C test-rvv/sample_consensus/sac_model_line check_production_asm` | 三个 production RVV helper 均找到预期 RVV 指令；`getDistancesToModelRVV` 的首个 `vfsqrt` 写回窗口为 `vfwcvt` + `vse64`。 | 证明新写回形态进入 production helper 机器码。 |
| Evidence Doctor | `production-repeated-evidence-doctor.md` | Errors=0 / Warnings=0 / Suggestions=0。 | 脚本范围内未发现重复板卡数据异常。 |
| evidence registry | `make -C test-rvv/sample_consensus/sac_model_line repeated_production_vse64_evidence_status` | fresh。 | `log/evidence_registry.json` 是本地 ignored metadata。 |

板卡日志里出现 `script/rvv-board-run.mk` clock skew warning（远端文件时间戳告警）。所有 gtest、bench 和 fetch 命令返回 0，checksum 与 manifest 中记录一致；该告警作为环境风险保留，不改变本阶段 decision bucket。

## diagnostic-to-production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | production-detail（生产细节实现族比较）和 production-public（公开入口 Std/RVV 对比）。 |
| A/B boundary | A 是 Phase 040 已采纳 `getDistances-vfsqrt-scratch-scalar-store`；B 是 Phase 050 `getDistances-vfsqrt-vse64-store`。两者都在接入后的 `getDistancesToModelRVV` production helper 边界内。 |
| 当前决策问题 | 新 store shape 是否比当前已采纳 family 更值得保留。 |
| diagnostic 是否外推到 production | 不使用 diagnostic 外推；本阶段直接改 production helper 并重跑 public entry。 |
| comparison-boundary / baseline mismatch 风险 | Phase 040 和 Phase 050 来自不同 binary / run batch，不能写成逐 run 同步 A/B；但 comparison boundary 都是 production public `getDistancesToModel`，且 Phase 050 改善幅度明显。 |
| 弱 / 负 / 中性 / 不稳定时 bounded production probe | 未触发；Phase 050 getDistances 5-run bucket 为 positive-stable，且相对 Phase 040 RVV 耗时降低。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 本阶段已用 Phase 040 与 Phase 050 的同 production boundary repeated summary 做 detail 对照；不需要额外 diagnostic 外推。 |

## Optimization matrix 更新

| candidate family | production direct board | asm | doctor | decision |
| --- | --- | --- | --- | --- |
| `getDistances-vfsqrt-scratch-scalar-store` | Phase 040 public getDistances median/min/max `3.4036x / 3.3409x / 3.5229x` | `vfsqrt` + scratch `vse32` + 标量 lane store | 0/0/0 | historical adopted baseline，已由 Phase 050 替换 |
| `getDistances-vfsqrt-vse64-store` | Phase 050 public getDistances median/min/max `4.2237x / 4.0338x / 4.2728x` | `vfsqrt` + `vfwcvt` + `vse64`，首个写回窗口无 scratch `vse32` | 0/0/0 | `adopted_production_behavior` |

## 继续 / 停止决策

`continue_stop_decision`: `continue_to_S11_refresh_after_phase050`

`stop_condition_hit`: not_hit。Phase 050 改变了当前 production truth（生产事实）的 getDistances 数据和写回形态，必须刷新 evaluation、optimization evidence、roadmap、matrix、topic README、benchmark/evidence、正式 `doc-rvv` 和 Handoff。

`next_phase_default`: `S11-refresh-after-phase050`。刷新完成并通过 final verification 后，若 roadmap 和 matrix 仍没有当前授权范围内的未阻塞优化动作，则可进入 ready_for_review。

`identity-index-strided-load` 和 `point-type-expansion` 在 Phase 050 结束时仍是后续新 scope：前者改变 production family，后者扩大点型 / layout 证据边界。后续状态：Phase 070 已尝试并拒绝 identity-index strided load；`point-type-expansion` 仍需单独 scope。
