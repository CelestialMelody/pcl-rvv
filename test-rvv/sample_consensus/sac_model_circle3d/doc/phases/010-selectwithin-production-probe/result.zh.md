# Phase 010 Result: selectWithinDistance production probe

## 执行范围

本阶段按 `plan.zh.md` 把 `SampleConsensusModelCircle3D<PointT>::selectWithinDistance` 的 Phase 000 正向候选接入真实 production（生产源码）公开入口。当前有效验证边界是 `selectWithinDistance`、direct indexed `indices_`、exact `pcl::PointXYZ`、signed 32-bit `pcl::index_t`、65536 点、200 次计时迭代、20 次 warm-up、Milkv-Jupiter 板卡。Phase 020 的点类型扩展负向后，production gate 已从 traits-gated xyz AoS `PointT` 收窄到 exact `pcl::PointXYZ`。

`countWithinDistance` 保持标量；`getDistancesToModel` 不在本阶段修改。当前 post-narrowing production-public（公开入口标量/RVV）10-run board 数据不支持采纳，因此不创建或保留正式 `doc-rvv` 主题文档。

## 动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| Phase 010 plan | done | `doc/phases/010-selectwithin-production-probe/plan.zh.md` | 生产接入范围、fallback、doctor、registry 和板卡预算已冻结。 |
| RED asm gate | done | 接入前 `make -C test-rvv/sample_consensus/sac_model_circle3d check_select_production_asm` 失败：`selectWithinDistanceRVVCircle3D: symbol not found`。 | gate 能捕获生产 RVV 符号缺失。 |
| 生产接入 | done | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` | 公开入口保持“模型有效性检查 -> RVV 短路 -> Std fallback”形态；不改 public API。 |
| QEMU correctness | done | `make -C test-rvv/sample_consensus/sac_model_circle3d run_test_compare` | Std/RVV 均 2/2 通过；QEMU 只证明正确性和路径，不作为性能证据。 |
| QEMU bench log-shape smoke | done | `make -C test-rvv/sample_consensus/sac_model_circle3d run_bench_rvv BENCH_ARGS='1024 2 1'` | RVV binary 的 public select 与 candidate select 均可输出，作为日志形状检查。 |
| production asm attribution | historical evidence | `make -C test-rvv/sample_consensus/sac_model_circle3d clean_bench_rvv && make -C test-rvv/sample_consensus/sac_model_circle3d check_select_production_asm` | 该 helper 已随回滚移除；此处仅保留历史 asm 归属证据。 |
| board repeated | done | `make -C test-rvv/sample_consensus/sac_model_circle3d collect_select_production_repeated_board_evidence record_select_production_board_evidence_state` | post-narrowing 10-run 中 5/10 退化，checksum 一致。 |
| Evidence Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_circle3d record_select_production_board_evidence_state` | 生成并登记 production manifest / doctor；doctor Errors=1，Warnings=1，Suggestions=0。 |

## 当前生产证据路径

| artifact | path | role |
| --- | --- | --- |
| production manifest（生产证据清单） | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/010-selectwithin-production-probe/select-production-repeated-evidence-manifest.json` | 10-run production-public board summary。 |
| Evidence Doctor Markdown | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/010-selectwithin-production-probe/select-production-repeated-evidence-doctor.md` | reviewer 可读异常信号摘要。 |
| Evidence Doctor JSON | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/010-selectwithin-production-probe/select-production-repeated-evidence-doctor.json` | 可登记 summary artifact。 |
| evidence registry（证据登记表） | `test-rvv/sample_consensus/sac_model_circle3d/log/evidence_registry.json` | 当前 summary artifact freshness 登记。 |
| raw board logs | `test-rvv/sample_consensus/sac_model_circle3d/log/board/repeated-20260828-phase010-circle3d-select-production/run-*/` | 本机 raw logs，默认不提交。 |

## 板卡性能摘要

B/A = Std public ms / RVV public ms，大于 1 表示生产 RVV 路径更快。

| case | B/A values | mean | median | min / max | decision bucket |
| --- | --- | ---: | ---: | ---: | --- |
| `selectWithinDistance` production-public `PointXYZ` 65536 | 1.0562, 0.9069, 1.0370, 0.9924, 1.2843, 1.0706, 1.0058, 0.9655, 0.8429, 0.5866 | 0.9748 | 0.9991 | 0.5866 / 1.2843 | negative / unstable，5/10 退化。 |

平均 Std public select 为 21.1399 ms/iter，平均 RVV public select 为 22.4800 ms/iter。Std/RVV checksum 均为 `3.99061e+08`。

## Evidence Doctor 结果

当前 doctor 结果：Errors=1，Warnings=1，Suggestions=0。

- `ba_degradation_frequency`：10-run 中 5 次 B/A 低于 1。处理动作是把生产证据降级为不支持采纳；不能只看个别正向 run 或早期历史 run。
- `long_tail_or_variance`：min/max 比为 2.19。处理动作是保留 min/median/max，不先验剔除负向尾部；该 warning 与 Error 一起阻塞 clean adoption。

板卡日志持续出现 `Clock skew detected`。它说明远端时间戳异常，可能影响 make 的增量判断；本阶段每次 board run 都重新部署/重编二进制，unit test 和 checksum 均通过，因此不作为 correctness failure。

## Diagnostic 到 Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 010 为 production-public。 |
| A/B boundary | Std binary public overload vs RVV binary public overload；两侧 wrapper、row source、timer boundary 和 checksum policy 一致。 |
| 当前决策问题 | `selectWithinDistance` 的生产 RVV dispatch 是否值得保留。 |
| diagnostic 是否可外推到 production | Phase 000 不能直接外推；本阶段以接入后的 10-run production-public 数据作为采用依据。 |
| comparison-boundary / baseline mismatch 风险 | 低；manifest 仅允许 `gate` 和 `reduction` 不同，因为它们正是标量与 RVV 生产路径的目标变量。 |
| 弱 / 负 / 中性 / 不稳定时是否允许继续 | 当前为 negative / unstable；不建议保留 production patch。是否回滚当前源码补丁需要用户确认。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前 public boundary 没有已有 adopted RVV family，决策是 RVV-vs-scalar；未来若新增 RVV family，需要同一 production boundary 内 detail A/B。 |

## EvidenceDecision

本阶段 decision 已刷新为 `rollback/no-production`：

- `selectWithinDistance` 的生产 RVV path 曾接入测试工作区，但接入后的板卡证据不支持采纳，且现已回滚。当前有效 production-public 10-run：mean 0.9748、median 0.9991、5/10 退化，Evidence Doctor 有 1 个 Error。
- `countWithinDistance` 不接入；Phase 000 projection count candidate 已 `rejected with evidence`。
- `getDistancesToModel` 暂缓；需要先做 lambda sign audit 和独立 same-chain correctness，但不属于当前已结束 topic 的继续推进方向。

## Continue / Stop Decision

`continue_stop_decision = turn_stop_deferred with stop_condition_hit`

停止条件：接入后的 production-public board evidence 有 Evidence Doctor Error，且平均值、中位数和退化频率不支持采纳。当前补丁已回滚，topic 关闭为 no-production。

- `point_type_expansion_queue`：Phase 020 已执行，`PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` 均负向并被收窄为 fallback。
- `count-specific formula`：当前不建议继续，除非先提出避免 sqrt 或改变阈值比较的独立数值预算；Phase 000 已证明现有 projection count RVV 形态负向。
- `getDistancesToModel`：可另开语义审计 phase，但它属于另一个入口，不应混入当前未采纳的 select production patch。
