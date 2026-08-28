# Phase 090 Plan: select error-tail production probe

## 阶段意图和边界

本阶段把 Phase 080 正向的 `selectWithinDistance` full-RVV error tail candidate（完整 RVV 误差尾段候选）接入 production `selectWithinDistanceRVV`，形成 bounded production probe（有界生产探针）。接入后必须重新跑 public production evidence（真实公开入口生产证据），不能直接用 Phase 080 test-only A/B 采纳。

validated_scope：`SampleConsensusModelCircle2D<PointT>::selectWithinDistance`，direct indexed `indices_`，`PointXYZ` board performance，float x/y AoS（结构数组）字段布局，signed 32-bit `pcl::index_t`，u32 byte offset gate。

unvalidated_scope：更多 PointXYZ-like 点型 dedicated board、`Scalar=double`、自定义 x/y layout、其它 SAC 模型、其它 row source。

## 当前状态清单

| 对象 | 当前证据 |
| --- | --- |
| Phase 080 candidate | RVV-vs-RVV board B/A `1.5774, 1.6127, 1.6138, 1.6078, 1.6059`，median `1.6078x`，Evidence Doctor（证据体检）0/0/0。 |
| production baseline | 当前 `selectWithinDistanceRVV` 已 adopted，但 `vcompress` 后仍逐 active lane 调用标量 `std::sqrt` 写 `error_sqr_dists_`。 |
| correctness（正确性） | `run_test_compare` 已在 Phase 080 通过；接入后必须重跑。 |
| asm attribution（反汇编归属） | Phase 080 candidate 符号已确认 `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v`；接入后必须归属到 production `selectWithinDistanceRVV`。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| select full-RVV error tail production | direct indexed `indices_` | `PointXYZ` / float x-y AoS / active double error output | `run_test_compare`、`run_circle_public_tests` | public select row in Std/RVV build | 5-run repeated board planned | production `selectWithinDistanceRVV` must contain `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v` | required before decision | planned |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| production patch | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | `selectWithinDistanceRVV` 删除临时 float buffer 和逐 lane 标量 `sqrt`，改用 compressed lanes 的 RVV sqrt / double store。 |
| correctness | `run_test_compare`、`run_circle_public_tests` | Std/RVV gtest 全部通过，public select 继续和 Standard / direct RVV 对齐。 |
| asm | `check_select_production_error_tail_asm` 或等价命令 | production `selectWithinDistanceRVV` 符号中确认 `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v`。 |
| board repeated | `collect_production_repeated_board_evidence` 或 Phase 090 专用 target | 5-run public Std/RVV board，Evidence Doctor 0/0/0 或异常已解释。 |
| 文档同步 | result、matrix、roadmap、topic-local docs、Handoff；若采纳则刷新正式 `doc-rvv` | 文档必须使用接入后的 Phase 090 board 数据。 |

## Evidence Doctor 和 registry 规则

本阶段 evidence role 是 `production_direct`。若接入后 public select board median 稳定大于旧 Phase 000 public select，且 Std/RVV 仍正向、doctor 0/0/0，则可在用户确认规则下采纳并刷新正式 `doc-rvv`。若接入后 public select 退化或不稳定，保留 patch 并停在 PI5 checkpoint（用户检查点），不得自行回滚。

复跑预算为 5 run。`positive`：public select Std/RVV median 明显大于 1，且对比 Phase 000 adopted baseline 有解释性提升；`negative`：低于 Phase 000 adopted baseline 或 Evidence Doctor 报退化 Error；`unstable`：5-run 决策桶摇摆。

## 继续 / 停止条件

若 Phase 090 positive-stable，按用户本轮确认可进入 S11 production closeout，正式 `doc-rvv` 使用 Phase 090 接入后板卡数据。若 Phase 090 negative / unstable，停在 PI5，报告生产 diff、命令、Evidence Doctor 和拟议回滚 / 调整，不自行回滚。
