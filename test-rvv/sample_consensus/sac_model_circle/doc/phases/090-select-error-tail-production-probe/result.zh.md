# Phase 090 Result: select error-tail production probe

## 执行范围

本阶段把 Phase 080 正向的 `selectWithinDistance` full-RVV error tail（完整 RVV 误差尾段）候选接入 production `selectWithinDistanceRVV`。接入后重新运行 correctness（正确性）、asm attribution（反汇编归属）、5-run board repeated benchmark（重复板卡性能测试）、Evidence Doctor（证据体检）和 registry（证据登记）。

validated_scope：`SampleConsensusModelCircle2D<PointT>::selectWithinDistance`，direct indexed `indices_`，`PointXYZ` board performance，float x/y AoS（结构数组）字段布局，signed 32-bit `pcl::index_t`，u32 byte offset gate。

unvalidated_scope：更多 PointXYZ-like 点型 dedicated board、`Scalar=double`、自定义 x/y layout、其它 SAC 模型、其它 row source 和其它硬件。

## 动作回填

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| production patch | done | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` 的 `selectWithinDistanceRVV` 删除 `vcompress` 后逐 active lane 标量 `std::sqrt`，改为 compressed lanes 上的 `vfsqrt`、abs、`vfwcvt` 和 `vse64` 写回 `error_sqr_dists_`。 |
| correctness | done | `make -C test-rvv/sample_consensus/sac_model_circle run_test_compare` 通过，Std/RVV 各 8/8。 |
| asm attribution | done | `make -C test-rvv/sample_consensus/sac_model_circle check_select_production_error_tail_asm` 通过；production `selectWithinDistanceRVV` 符号包含 `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v`。 |
| board repeated | done | `make -C test-rvv/sample_consensus/sac_model_circle collect_select_error_tail_production_repeated_board_evidence` 完成 5 run，每轮 board gtest 8/8 通过。 |
| Evidence Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_circle record_select_error_tail_production_board_evidence_state` 生成 manifest、doctor 并登记 registry。 |
| 文档同步 | done | README、evaluation、benchmark/evidence、optimization evidence、roadmap、matrix、正式 `doc-rvv`、队列表和 Handoff 已刷新为 Phase 090 adopted。 |

## 接入后证据结论

Phase 090 是 production direct（真实生产入口直连）证据。`public selectWithinDistance` 的 5-run board B/A 为：

```text
2.5412, 2.6515, 2.5700, 2.6770, 2.5357
```

median `2.5700x`，min/max `2.5357x / 2.6770x`，`B/A < 1` 为 0/5。Std 平均 `2.722169 ms/iter`，RVV 平均 `1.048910 ms/iter`。`public selectWithinDistance` 的 item checksum（单项输出校验和）在 Std/RVV 间均为 `2133435400977669312`。

Evidence Doctor 结果为 Errors=0，Warnings=0，Suggestions=0。证据路径：

- manifest：`test-rvv/sample_consensus/sac_model_circle/doc/phases/090-select-error-tail-production-probe/select-error-tail-production-repeated-evidence-manifest.json`
- doctor markdown：`test-rvv/sample_consensus/sac_model_circle/doc/phases/090-select-error-tail-production-probe/select-error-tail-production-repeated-evidence-doctor.md`
- doctor json：`test-rvv/sample_consensus/sac_model_circle/doc/phases/090-select-error-tail-production-probe/select-error-tail-production-repeated-evidence-doctor.json`

接入后 `public countWithinDistance` 作为同一 binary 的回归行也保持正向：B/A median `1.3982x`，min/max `1.3909x / 1.4073x`，Evidence Doctor 0/0/0。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_direct`。 |
| A/B boundary | public `selectWithinDistance` Std build vs RVV build，真实 production dispatch。 |
| 当前决策问题 | production adoption after probe：Phase 080 的 test-only RVV family 接入后是否仍值得采纳。 |
| diagnostic 是否可外推到 production | Phase 080 只支持进入 probe；最终采纳以本阶段 Phase 090 production direct 数据为准。 |
| comparison-boundary / baseline mismatch 风险 | 已闭合到 public overload，同一 row source、点型、layout、threshold 和 board 数据集。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive-stable；若为负或不稳定，应停在 PI5 用户检查点并保留 patch 等待判断。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | Phase 080 已先完成 RVV-vs-RVV detail A/B，本阶段又完成接入后 public Std/RVV production direct。 |

## Optimization matrix 更新

`select full-RVV error tail production` 从 production probe 更新为 adopted production behavior。Phase 000 的 select gather + scalar error tail 数据保留为 historical baseline（历史基线）；正式 `doc-rvv` 的当前 select 数据改用 Phase 090 接入后板卡数据。

## continue_stop_decision

stop：当前授权的同边界高优先级优化动作已经闭合。`selectWithinDistance`、`countWithinDistance` 和 `getDistancesToModel` 在当前 `PointXYZ` / direct indexed / float x-y AoS / u32 byte offset 范围内均为 adopted production behavior；identity-index strided load 仍为 rejected。更多 PointXYZ-like 点型 dedicated board、`Scalar=double`、自定义 layout、其它 SAC 模型或新的 identity/load 组织都需要新 scope 或新 topic。
