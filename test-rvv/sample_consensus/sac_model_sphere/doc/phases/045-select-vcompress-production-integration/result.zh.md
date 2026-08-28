# Phase 045: selectWithinDistance vcompress 生产接入结果

## 执行范围

本阶段把 Phase 040 的 `vcompress` 候选接入真实 production
`SampleConsensusModelSphere<PointT>::selectWithinDistanceRVV`，并按 Phase 045 plan
执行 PI2-PI5。接入范围没有扩大：只覆盖 direct indexed `indices_`、
`RVVXYZFloatLayout<PointT>`、32-bit byte offset gate（32 位字节偏移准入条件）和
当前 `PointXYZ` board performance（板卡性能）case。`getDistancesToModel`、其它
row source、`Scalar=double` 和更多点型性能不在本阶段采纳范围内。

## 动作回填

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| PI2 production patch | done | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` 中 `selectWithinDistanceRVV` 使用 `vcompress` 压缩命中 index 和平方距离，再由标量尾段写回精确 `sqrt` error distance。 |
| PI3 correctness（正确性） | done | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare`；Std/RVV 构建各 5 个 gtest 通过。 |
| PI4 asm attribution（反汇编归属） | done | `make -C test-rvv/sample_consensus/sac_model_sphere clean_bench_rvv dump_bench_rvv`；`selectWithinDistanceRVV` 符号内可见 4 条 `vcompress.vm`，manifest 记录 `rvv_instr_count=27`。 |
| PI4 board repeated（重复板卡测试） | done | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere check_board_ssh collect_production_vcompress_repeated_board_evidence`；5-run 均完成，board gtest 每轮通过。 |
| PI4 doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_sphere record_production_vcompress_board_evidence_state`；生成并登记 manifest、Markdown Doctor 和 JSON Doctor。 |
| PI5 user checkpoint | blocked by required user confirmation | 证据支持保留 patch，但 workflow 要求 PI5 后暂停，等待用户确认采纳 / 保留或回滚。 |

## 板卡结果

板卡为 Milkv-Jupiter，case 为 `PointXYZ`、65536 点、direct indexed `indices_`、
200 iterations、5 warmup。`B/A` 表示 Std 构建 public entry 耗时除以 RVV 构建
public entry 耗时，`>1` 表示 RVV 更快。

| case | B/A values | median | min / max | 结论 |
| --- | --- | ---: | ---: | --- |
| public `selectWithinDistance` | `2.0409, 2.0407, 2.0720, 2.0435, 2.0588` | `2.0435x` | `2.0407x / 2.0720x` | Phase 045 vcompress production patch 在真实 public entry 中稳定正向。 |
| public `countWithinDistance` | `3.5206, 3.5001, 3.5089, 3.5107, 3.5141` | `3.5107x` | `3.5001x / 3.5206x` | 既有 count RVV 回归仍稳定正向。 |
| public `getDistancesToModel` | `0.9936, 0.9980, 1.0302, 0.9852, 1.0057` | `0.9980x` | `0.9852x / 1.0302x` | public entry 仍为标量；不是 RVV 采纳证据。 |
| diagnostic select candidate | `1.4055, 1.4132, 1.4177, 1.4133, 1.4144` | `1.4133x` | `1.4055x / 1.4177x` | 历史测试专用候选仍正向，但已被 production direct 证据取代。 |
| diagnostic getDistances candidate | `0.7756, 0.7751, 0.7798, 0.7709, 0.7896` | `0.7756x` | `0.7709x / 0.7896x` | 继续支持当前 scratch + scalar sqrt/store 实现族 rejected。 |

本轮板卡执行过程中远端 `make` 报 clock skew（时钟偏斜）warning。该 warning 没有造成
gtest 或 bench target 失败；本阶段把它记录为环境 warning，不把它解释成 correctness 或
performance failure。

## Evidence Doctor 处理

`production-vcompress-repeated-evidence-doctor.md` 输出
`Errors=2, Warnings=0, Suggestions=0`。

两个 Error 都属于本阶段不采纳的 `getDistancesToModel` 行：

- public `getDistancesToModel` 在 Std/RVV 构建间 3/5 run 低于 1，但该 public entry 没有 RVV
  dispatch；这里只能说明当前构建噪声 / 标量基线对比不稳定，不能作为 `selectWithinDistance`
  vcompress patch 的负向证据。
- diagnostic `getDistancesToModel` candidate 5/5 run 低于 1，继续支持 Phase 000 对当前
  scratch + scalar sqrt/store 实现族的拒绝结论。

`selectWithinDistance` production row 没有 Error / Warning，且 A/B boundary（A/B 边界）为
同一 public overload：Std build public entry 对 RVV build public entry。Phase 040 的
mixed-boundary Warning 已通过本阶段 production direct 重跑闭合。

## Diagnostic 到 Production 错配审计回填

| question | answer |
| --- | --- |
| evidence role | production direct（真实生产入口直连证据） |
| A/B boundary | Std build public `selectWithinDistance` vs RVV build public `selectWithinDistance` |
| 当前决策问题 | `vcompress` production patch 是否值得保留到用户确认点 |
| diagnostic 是否可外推到 production | Phase 040 只作为进入生产探针理由；本阶段结论来自接入后的 production direct 证据。 |
| comparison-boundary / baseline mismatch 风险 | `selectWithinDistance` 主行无 mismatch；`getDistancesToModel` Error 降级为 out-of-scope。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已完成 bounded production probe；若用户不采纳，应保留证据后回滚或继续诊断。 |
| clean adoption 是否需要同一 production boundary 内证据 | 已具备同边界 production direct 证据，但 PI5 仍必须等待用户确认。 |

## EvidenceDecision

阶段证据支持把 `vcompress` production patch 保留为采纳候选：public
`selectWithinDistance` 5-run board median 为 `2.0435x`，全 run 正向；QEMU correctness
通过；`selectWithinDistanceRVV` 符号内可见 `vcompress.vm`，且 Evidence Doctor 对 select
production row 无 Error / Warning。

但是按 workflow 规则，PI5 是用户检查点。当前状态不是 `adopted` closeout，而是
`PI5 pending user confirmation`：production patch 保留在工作区，等待用户确认“保留 / 采纳”
后，才能把 Phase 045 写入长期 adopted production behavior、更新 `doc-rvv` 的最终生产事实并进入
S11 closeout；如果用户确认回滚，则应保留本 phase 证据并撤回 production patch。

## Evidence Registry

本阶段可提交 summary evidence（摘要证据）如下；raw board logs 仍默认 local-only。

| run label | path |
| --- | --- |
| `sphere-phase045-production-vcompress-select-repeated-board` | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-manifest.json` |
| `sphere-phase045-production-vcompress-select-repeated-board` | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-doctor.md` |
| `sphere-phase045-production-vcompress-select-repeated-board` | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-doctor.json` |

## Continue / Stop Decision

`continue_stop_decision`：`turn_stop_deferred with stop_condition_hit`。

`stop_condition_hit`：继续到 S11 production closeout、长期 `doc-rvv` 采纳刷新或回滚都会跨过
PI5 用户确认边界；worker 不能自动替用户决定保留或撤回 production patch。

`next_phase_default`：等待用户确认 Phase 045 vcompress patch。若用户确认保留，下一 phase 是
`046-vcompress-production-closeout` 或等价 S11 closeout，用于把 matrix、roadmap、evaluation、
`doc-rvv` 和队列表升级为 adopted；若用户要求回滚，则创建 rollback closeout，保留本 phase 证据并撤回
production diff。`050-point-type-expansion` 仍是后续独立扩展 phase，需在 production patch 决策后恢复。
