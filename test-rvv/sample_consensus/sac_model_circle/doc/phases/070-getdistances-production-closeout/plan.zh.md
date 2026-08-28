# Phase 070 Plan: getDistances production closeout

## 阶段意图和边界

本阶段把 Phase 060 的 `getDistancesToModel` full-RVV（完整 RVV）production probe（生产探针）在用户确认后收口为 adopted production behavior（已采用生产行为）。本阶段不改 production 源码；源码已经在 Phase 060 接入并完成接入后 production direct（真实生产入口直连）测试。

validated_scope：`SampleConsensusModelCircle2D<PointT>::getDistancesToModel`，direct indexed `indices_`，`PointXYZ` board performance，float x/y AoS（结构数组）字段布局，signed 32-bit `pcl::index_t`，u32 byte offset gate。

unvalidated_scope：更多 PointXYZ-like 点型 dedicated board、`Scalar=double`、自定义 x/y layout、其它 SAC 模型、`selectWithinDistance` 命中后 exact error scalar tail（标量尾段）改造。

## 当前状态清单

| 对象 | 当前证据 |
| --- | --- |
| production patch | `sample_consensus/include/pcl/sample_consensus/sac_model_circle.h` 与 `impl/sac_model_circle.hpp` 已新增 `getDistancesToModelStandard` / `getDistancesToModelRVV` 和 public dispatch。 |
| correctness（正确性） | `make -C test-rvv/sample_consensus/sac_model_circle run_test_compare`：Std/RVV 各 7/7 通过。 |
| asm attribution（反汇编归属） | `make -C test-rvv/sample_consensus/sac_model_circle check_getdistances_production_asm`：`getDistancesToModelRVV` 命中 `vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`。 |
| board performance（板卡性能） | Phase 060 production public Std/RVV 5-run B/A 为 `1.4697, 1.4737, 1.4962, 1.4755, 1.4687`，median `1.4737x`。 |
| Evidence Doctor（证据体检） | Phase 060 Errors=0，Warnings=0，Suggestions=0。 |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| production `doc-rvv` closeout | `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md` | `getDistancesToModel` 从 pending 改为 adopted，并使用 Phase 060 接入后板卡数据。 |
| topic-local 文档同步 | README、evaluation、optimization evidence、roadmap、matrix、benchmark/evidence、code map、phase index | 当前结论不再停在 PI5；历史 Phase 060 仍可保留当时 checkpoint 事实。 |
| Handoff 同步 | current handoff Markdown/YAML | 状态改为 S11 closeout completed，列出是否还有当前 topic 内未阻塞下一 phase。 |
| 验证 | correctness、asm、evidence status、diff check | 所有 scoped 检查通过，或写清阻塞。 |

## Evidence Doctor 和 registry 规则

本阶段不生成新板卡 raw log；沿用 Phase 060 接入后 production direct manifest 和 Evidence Doctor。必须复查 `getdistances_production_evidence_status`、`production_evidence_status` 和 `getdistances_full_rvv_evidence_status` 为 fresh。

## 继续 / 停止条件

若文档 closeout 和验证通过，`getDistancesToModel` 进入 adopted production behavior。随后扫描 roadmap：如果仍有当前 topic 授权范围内的高优先级未阻塞优化候选，则创建下一 phase plan 并继续；如果剩余方向需要新 scope、更多点型 dedicated board、其它模型 topic 或新的 production family 设计，则本 topic 可以停在 ready_for_review / submit boundary（提交前审查边界）。
