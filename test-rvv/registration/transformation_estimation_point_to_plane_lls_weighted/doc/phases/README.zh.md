# transformation_estimation_point_to_plane_lls_weighted 阶段索引

本文是本 topic 的 phase loop（阶段循环）恢复入口。它只维护阶段索引、当前默认恢复点和文档归属；具体计划、矩阵、证据异常和阶段结果放在各阶段目录。

## 当前恢复入口

下一轮短 prompt 继续本 topic 时，默认先读：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/README.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/033-source-indexed-default-staged-and-workflow-guard/plan.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/033-source-indexed-default-staged-and-workflow-guard/result.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/032-source-indexed-production-diagnostic-boundary-root-cause/plan.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/032-source-indexed-production-diagnostic-boundary-root-cause/result.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/031-source-indexed-fused-production-probe/plan.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/031-source-indexed-fused-production-probe/result.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/030-source-indexed-bench-evidence-calibration/plan.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/030-source-indexed-bench-evidence-calibration/result.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/020-dual-indices-correspondences-family-carry-over/result.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/010-source-indexed-family-carry-over/result.zh.md
```

Phase 020 已完成并闭合。Phase 030 完成 source-indexed bench evidence calibration，并留下
`source_indexed_family_repeated` 的 diagnostic negative（诊断负向）：`block-fused-abcd-ilp`
full estimate median `0.90x`，Doctor 为 `3E / 6W / 13S`。Phase 031 按用户授权做真实 production
probe（生产探针），结果与 Phase 030 分叉：source-indexed public overload 接入 block-fused 后，
6 个代表 case 的 5-run median 均正向，Doctor 为 `0E / 9W / 12S`。因此 Phase 030 现在保留为
historical diagnostic 和 harness-risk signal（测试框架风险信号），不能再写成直接阻止 production probe。
Phase 032 进一步补了 production detail RVV-vs-RVV A/B：同一 production RVV binary 内比较
staged-gather 与 block-fused。结果显示 block-fused 相对 staged 是 mixed / negative，尤其 262144
规模有多项明显负向。因此 Phase 031 的 public Std/RVV 正向可信，但不能证明 block-fused 优于 staged；
当前结论仍不能升级为 clean adopted。Phase 033 已把 source-indexed production 默认路径退回
staged-gather / compressed-tail，并把 block-fused 保留为显式 probe / detail A/B helper。

## 阶段表

| phase | 状态 | 主目标 | 计划 | 结果 |
| --- | --- | --- | --- | --- |
| `000-current-state-and-gaps` | done | 恢复 full-cloud、source-indexed、dual-indices、correspondences 的当前状态；建立 implementation-family matrix；拆清接入生产前诊断和接入生产后 direct 证据；补 source-indexed production 和 row-source diagnostic 的 Evidence Doctor（证据体检）/ manifest（证据清单）边界。 | `000-current-state-and-gaps/plan.zh.md` | `000-current-state-and-gaps/result.zh.md` |
| `010-source-indexed-family-carry-over` | done | 在 test-rvv 中审计 source-indexed 是否需要迁移 full-cloud 已采纳的 block-reduction / A/B/C/N / fused-abcd-ilp family。 | `010-source-indexed-family-carry-over/plan.zh.md` | `010-source-indexed-family-carry-over/result.zh.md` |
| `020-dual-indices-correspondences-family-carry-over` | done | 为 dual-indices 和 correspondences 先补同 family candidate / test / bench / board smoke，再把结论收成 diagnostic negative，而不是直接 production。 | `020-dual-indices-correspondences-family-carry-over/plan.zh.md` | `020-dual-indices-correspondences-family-carry-over/result.zh.md` |
| `030-source-indexed-bench-evidence-calibration` | done | 校准 source-indexed-family bench 证据口径，补 warm-up / sink / Evidence Doctor 边界，并完成重复板卡 target / summary / doctor 审计。 | `030-source-indexed-bench-evidence-calibration/plan.zh.md` | `030-source-indexed-bench-evidence-calibration/result.zh.md` |
| `031-source-indexed-fused-production-probe` | done | 按用户授权把 source-indexed public overload 临时接到 `block-fused-abcd-ilp`，用 production direct correctness、repeated board 和 Evidence Doctor 验证 Phase 030 诊断负向是否复现。 | `031-source-indexed-fused-production-probe/plan.zh.md` | `031-source-indexed-fused-production-probe/result.zh.md` |
| `032-source-indexed-production-diagnostic-boundary-root-cause` | done | 解释 Phase 030 diagnostic negative 与 Phase 031 production public positive 的分歧，补 production detail RVV-vs-RVV A/B，并回答 full-cloud 对照。 | `032-source-indexed-production-diagnostic-boundary-root-cause/plan.zh.md` | `032-source-indexed-production-diagnostic-boundary-root-cause/result.zh.md` |
| `033-source-indexed-default-staged-and-workflow-guard` | done | 将 Phase 032 的 rejected-for-default 结论落实到 production 默认路径、gtest 护栏、agent 资产和 topic/doc-rvv 文档。 | `033-source-indexed-default-staged-and-workflow-guard/plan.zh.md` | `033-source-indexed-default-staged-and-workflow-guard/result.zh.md` |

## 文档归属

| 信息类型 | 主归属 |
| --- | --- |
| 阶段计划、optimization matrix（优化矩阵）、unblocked next action（未阻塞下一步动作） | `doc/phases/<phase>/plan.zh.md` |
| 阶段执行结果、Evidence Doctor 异常处理、continue / stop decision（继续 / 停止决策） | `doc/phases/<phase>/result.zh.md` |
| 测试类型、target、pre-production diagnostic（接入生产前诊断）/ post-production direct（接入生产后真实路径）分层 | `doc/testing-overview.zh.md` |
| bench label、case-filter、summary、manifest、Evidence Doctor 和提交边界 | `doc/benchmark-and-evidence.zh.md` |
| candidate family（候选实现族）到代码、test、bench、board、asm 和 decision 的索引 | `doc/optimization-evidence.zh.md` |
| EvidenceDecision（证据决策）、Traceability Map（可追踪性地图）和生产边界审计 | `doc/transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md` |
| 最终 production 行为、fallback 和长期维护说明 | `doc-rvv/registration/transformation_estimation_point_to_plane_lls_weighted-RVV.zh.md` |

## 当前早停规则

Phase 010 已完成 PointNormal-first source-indexed family 审计：correctness 通过，但旧 board smoke 和 Evidence Doctor 显示不能替换 production。Phase 020 已完成 dual-indices / correspondences 的同族 candidate、correctness、bench、board smoke 和 doctor 审计：结果仍是 diagnostic negative，不能替换 production。Phase 030 已把旧 no-warmup smoke 降级为 historical diagnostic，并用 `collect_board_source_indexed_family_repeated` 生成 5-run summary；该 diagnostic wrapper 里 `block-fused-abcd-ilp` full estimate median `0.90x` 且 `4/5` 低于 `1.0x`。Phase 031 继续做真实 production probe，`production_source_indices_block_fused_abcd_ilp_probe` summary 显示 6 个代表 case median `1.54x` 到 `1.71x`，Doctor 为 `0E / 9W / 12S`。Phase 032 补 `production_source_indices_detail_ba`，确认同一 production RVV binary 内 block-fused 对 staged 仍是 mixed / negative。Phase 033 已把 source-indexed 默认路径改为 staged-gather / compressed-tail，并补 gtest 防止默认 selector 回到 block-fused。默认早停规则：不能把 Phase 031 写成 clean adopted；若重新挑战 block-fused 默认优先 dispatch，必须先补 source-indexed-specific asm attribution、binary identity 和 extended-run detail A/B。
