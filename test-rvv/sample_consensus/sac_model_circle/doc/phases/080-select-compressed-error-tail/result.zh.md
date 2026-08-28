# Phase 080 Result: select compressed error tail

## 执行范围

本阶段按计划只新增 test-only candidate（仅测试使用候选），不改 production dispatch（生产分流）。候选入口为 `SampleConsensusModelCircleAccess::selectWithinDistanceFullRVVErrorTailCandidate`，baseline（基线）为当前已采纳 production `selectWithinDistanceRVV`，比较问题是 RVV-family-selection（RVV 实现族选择）。

validated_scope：`SampleConsensusModelCircle2D<PointT>::selectWithinDistance`，direct indexed `indices_`，`PointXYZ` board performance，float x/y AoS（结构数组）字段布局，signed 32-bit `pcl::index_t`，u32 byte offset gate。

unvalidated_scope：更多 PointXYZ-like 点型 dedicated board、`Scalar=double`、自定义 x/y layout、其它 SAC 模型、其它 row source 和真实 production 替换后的 public evidence。

## 动作回填

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| 新增候选 helper | done | `test-rvv/sample_consensus/sac_model_circle/include/impl/sac_model_circle_candidates.hpp` 新增 `selectWithinDistanceFullRVVErrorTailCandidate` 和 RVV helper。 |
| 新增 correctness gate | done | `make -C test-rvv/sample_consensus/sac_model_circle run_circle_select_error_tail_candidate_test` 通过。 |
| 全量对拍 | done | `make -C test-rvv/sample_consensus/sac_model_circle run_test_compare`：Std/RVV 各 8/8 通过。 |
| 新增 bench row | done | `bench_sac_model_circle` 输出 `diagnostic select full-rvv error tail`，checksum 与 public select 一致。 |
| asm attribution | done | `make -C test-rvv/sample_consensus/sac_model_circle check_select_error_tail_asm` 通过；候选符号含 `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v`。 |
| board repeated | done | `make -C test-rvv/sample_consensus/sac_model_circle collect_select_error_tail_repeated_board_evidence` 完成 5 run。 |
| Evidence Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_circle record_select_error_tail_board_evidence_state` 生成 manifest、doctor 并登记 registry。 |

## 证据结论

Phase 080 是 strict RVV-vs-RVV detail A/B：baseline 是 RVV build 内的 public `selectWithinDistance` adopted path，candidate 是同一对象状态下的 test-only full-RVV error tail helper。5-run board B/A 为：

```text
1.5774, 1.6127, 1.6138, 1.6078, 1.6059
```

median `1.6078x`，min/max `1.5774x / 1.6138x`，`B/A < 1` 为 0/5。baseline 平均 `1.672869 ms/iter`，candidate 平均 `1.043296 ms/iter`。Evidence Doctor（证据体检）为 Errors=0，Warnings=0，Suggestions=0。证据路径：

- manifest：`test-rvv/sample_consensus/sac_model_circle/doc/phases/080-select-compressed-error-tail/select-error-tail-repeated-evidence-manifest.json`
- doctor markdown：`test-rvv/sample_consensus/sac_model_circle/doc/phases/080-select-compressed-error-tail/select-error-tail-repeated-evidence-doctor.md`
- doctor json：`test-rvv/sample_consensus/sac_model_circle/doc/phases/080-select-compressed-error-tail/select-error-tail-repeated-evidence-doctor.json`

候选比当前 adopted select RVV 稳定更快，原因与源码形态一致：当前 production `selectWithinDistanceRVV` 在 `vcompress` 后把 compressed squared distances（压缩后的平方距离）写到临时 float buffer，再逐 active lane（有效向量通道）调用标量 `std::sqrt` 和 double store；Phase 080 candidate 在 RVV 寄存器内继续做 `vfsqrt`、abs、`vfwcvt` 和 `vse64`，只保留 `vcompress` 写 inlier index 的必要步骤。

board log 中仍出现 `Clock skew detected`，这是远端文件时间提示；它没有导致 gtest、bench、manifest 或 Evidence Doctor 失败，但仍作为环境提示保留。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `strict_ab`，production-detail test-only A/B。 |
| A/B boundary | 同一 RVV binary 内 public adopted select baseline vs test-only candidate helper。 |
| 当前决策问题 | RVV-family-selection：是否值得用 full-RVV error tail 替换当前 adopted select RVV family。 |
| 是否可直接外推到 production | 不能直接 clean-adopt，因为 candidate 还未接入真实 `selectWithinDistanceRVV`。 |
| comparison-boundary / baseline mismatch 风险 | 有；candidate helper 与 public entry wrapper 不同，但数据流、gate、row source、点型和 bench 输入一致。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本轮为 positive-stable；若弱或负则不会进入 production probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。Phase 080 已给出 test-only detail A/B；后续 Phase 090 仍需接入后 public production evidence。 |

## Optimization matrix 更新

`select compressed error full-RVV tail` 从 planned 更新为 `production probe candidate`。这不是 adopted production behavior；它只说明该实现族值得进入 bounded production probe（有界生产探针）。

## continue_stop_decision

continue。Phase 080 未命中停止条件，因为当前 topic 内存在明确、正向、未阻塞的 production probe 动作：把 candidate 逻辑接入 production `selectWithinDistanceRVV`，再跑 production direct correctness、asm、board repeated 和 Evidence Doctor。

next_phase_default：`090-select-error-tail-production-probe`。
