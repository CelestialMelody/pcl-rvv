# Phase 050 / PI1 结果：binary writer production integration plan

## 实际执行范围

本阶段完成 `writeBinary<PointT>(file_name, cloud)` 的 PI1 production integration plan（生产接入计划），
没有修改 production（生产源码）。新增计划文件为
`doc/phases/050-binary-writer-PI1-production-integration-plan/plan.zh.md`，用于冻结后续 PI2
production patch（生产补丁）前的范围、fallback（回退路径）、production direct test（真实生产路径测试）
和板卡证据计划。

## PI1 Gate 回填

| gate | status | evidence | conclusion |
| --- | --- | --- | --- |
| pi2_scope | done | `plan.zh.md` 的“候选范围” | 只覆盖 `writeBinary<PointT>(file_name, cloud)` 的 contiguous cloud、4 字节字段 packed output helper。 |
| forbidden_expansion | done | `plan.zh.md` 的“不接入范围” | 不碰 indices overload、compressed writer、ASCII writer、PCLPointCloud2 path、public API 或跨 topic common API。 |
| fallback_matrix | done | `plan.zh.md` 的“Fallback 矩阵” | 非 RVV、空 cloud、非 4 字节字段、未对齐、indices 和文件错误路径均有 fallback / 保持原行为要求。 |
| entry_structure | done | `plan.zh.md` 的“生产 helper 形态候选” | PI2 应抽出 Std helper，并让 public entry 只做准备、mmap、RVV 短路和 Std fallback。 |
| generic point type strategy | done | `plan.zh.md` 的“Generic point type 策略” | 不用 exact `PointXYZ` gate；按 runtime field metadata gate 收窄到 4 字节字段。 |
| production_direct_test_plan | done | `plan.zh.md` 的“Production direct test 计划” | 已列 public binary writer hit、Std/RVV file equivalence、fallback non-4-byte field、indices unchanged、non-RVV build。 |
| evidence_commands | partial | `plan.zh.md` 的“Bench / asm / board 计划” | 命令族已定义；具体 Make target 需要 PI2/PI3 实现后补齐。 |

## 当前决策

current_decision：`PI1-plan-complete / pending-production-authorization`。

证据基础：

- Phase 040 binary component ablation 为 positive：`log/board/binary_component_repeat_5/summary.md`
  的 median speedup 为 `1.2738x`、`1.1842x`、`6.8125x`。
- Evidence Doctor `log/board/binary_component_repeat_5/evidence_doctor.md` 为
  Errors=0，Warnings=1，Suggestions=0。Warning 是 small case group outlier，已在 Phase 040 result
  中降级为 small smoke signal。
- registry `log/evidence_registry.json` 已登记 Phase 040 summary、manifest 和 Doctor。

production_decision（生产判断）：

- 当前仍未修改 `io/include/pcl/io/impl/pcd_io.hpp`。
- 若用户明确授权进入 production integration loop（生产接入闭环），后续可按本 PI1 计划进入
  binary writer PI2 production patch。
- compressed writer 的 Phase 020 PI1 计划仍是更成熟的 production 候选，因为它已有 pack+LZF
  production-shaped diagnostic；binary writer PI2 应由用户明确选择或在 compressed PI2 之后再排。

## Continue / Stop Decision

stop_condition_hit：继续到 binary writer PI2 会扩大到 production 源码，需要用户明确授权。

next_phase_default：

- 若用户授权 compressed writer production integration loop：按 Phase 020 进入 compressed writer PI2。
- 若用户授权 binary writer production integration loop：按本 Phase 050 计划进入 binary writer PI2。
- 若用户不授权 production：保持 topic-local 诊断和 PI1 计划，不修改 production。

unblocked_next_actions：

- `turn_stop_deferred with stop_condition_hit`：compressed writer 或 binary writer 的 PI2 production patch，都需要用户授权。
