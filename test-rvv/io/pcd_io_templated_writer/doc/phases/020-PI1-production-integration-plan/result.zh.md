# Phase 020 / PI1 结果：production integration plan

## 实际执行范围

本阶段完成 PI1 production integration plan（生产接入计划），没有修改 production（生产源码）。
实际新增文件为 `doc/phases/020-PI1-production-integration-plan/plan.zh.md`，用于冻结 PI2
production patch（生产补丁）前的范围、fallback（回退路径）、production direct test（真实生产路径测试）
和板卡证据计划。

## PI1 Gate 回填

| gate | status | evidence | conclusion |
| --- | --- | --- | --- |
| pi2_scope | done | `plan.zh.md` 的“候选范围” | 只覆盖 `writeBinaryCompressed<PointT>(file_name, cloud)` 的 4 字节字段 pack helper。 |
| forbidden_expansion | done | `plan.zh.md` 的“不接入范围” | 不碰 indices overload、ASCII writer、PCLPointCloud2 path、public API 或跨 topic common API。 |
| fallback_matrix | done | `plan.zh.md` 的“Fallback 矩阵” | 非 RVV、空 cloud、非 4 字节字段、未对齐、overflow、indices 均有 fallback 要求。 |
| entry_structure | done | `plan.zh.md` 的“生产 helper 形态候选” | PI2 应抽出 Std helper，并让 public entry 只做准备、RVV 短路和 Std fallback。 |
| generic point type strategy | done | `plan.zh.md` 的“Generic point type 策略” | 不用 exact `PointXYZ` gate；按 `pcl::getFields<PointT>()` 的 runtime field metadata gate 收窄到 4 字节字段。 |
| production_direct_test_plan | done | `plan.zh.md` 的“Production direct test 计划” | 已列 public compressed writer hit、Std/RVV file equivalence、fallback non-4-byte field、non-RVV build。 |
| evidence_commands | partial | `plan.zh.md` 的“Bench / asm / board 计划” | 命令族已定义；具体 Make target 需要 PI2/PI3 实现后补齐。 |

## 当前决策

current_decision：`PI1-plan-complete / pending-production-authorization`。

证据基础：

- Phase 010 production-shaped diagnostic 为 positive：`log/board/production_shaped_repeat_5/summary.md`
  三个 compressed case 的 mean speedup 为 `1.3904x`、`1.3526x`、`1.3493x`。
- Evidence Doctor `log/board/production_shaped_repeat_5/evidence_doctor.md` 为
  Errors=0，Warnings=0，Suggestions=0。
- registry `log/evidence_registry.json` 已登记 Phase 010 summary、manifest 和 Doctor。

production_decision（生产判断）：

- 当前仍未修改 `io/include/pcl/io/impl/pcd_io.hpp`。
- 若用户明确授权进入 production integration loop（生产接入闭环），下一阶段可按本 PI1 计划进入
  PI2 production patch。
- 未授权前不得把诊断收益写成 production-ready，也不得自行修改 production。

## Continue / Stop Decision

stop_condition_hit：继续到 PI2 会扩大到 production 源码，需要用户明确授权。

next_phase_default：

- 若用户授权 production integration loop：`PI2 production_patch`，范围严格限于本 PI1 计划。
- 若用户不授权 production：保持 `partial-production-candidate`，可另开 topic-local doc-suite parity
  或 binary writer component ablation，但不进入生产补丁。

unblocked_next_actions：

- `turn_stop_deferred with stop_condition_hit`：PI2 production patch，需要用户授权。
- `phase_deferred + unblocked`：若当前 topic 继续但不进 production，可补 topic-local doc suite parity；
  该动作不改变生产判断。
