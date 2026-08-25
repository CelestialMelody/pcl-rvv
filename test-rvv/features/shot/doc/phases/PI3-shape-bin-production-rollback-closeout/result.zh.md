# PI3 result: shape-bin production rollback closeout

## 当前结论

本阶段按用户确认回滚了 `features/include/pcl/features/impl/shot.hpp` 中的 PI2 shape-bin indexed gather production patch（生产补丁）。当前 `shot.hpp` 不包含 SHOT RVV production helper（生产 RVV helper）或 dispatch（分流逻辑），`createBinDistanceShape` 回到原标量路径。

最终 EvidenceDecision（证据决策）为 `rollback/no-production`。本 topic 不创建 `doc-rvv/features/shot-RVV.zh.md`，因为当前没有 adopted production behavior（已采用生产行为）。

## 实际执行范围

| item | planned | actual |
| --- | --- | --- |
| production rollback | 移除 PI2 `__RVV10__` include、normal AoS gate、RVV helper、Std helper 和 dispatch。 | 已完成；`git diff -- features/include/pcl/features/impl/shot.hpp` 为空。 |
| historical evidence | 保留 PI2 tests、bench、manifest 和 phase result。 | 已保留；它们是 historical production probe（历史生产探针）证据，不代表当前 production path。 |
| docs closeout | 文档从 `pending_user_confirmation_rollback` 同步到 `rollback/no-production`。 | README、evaluation、benchmark/evidence、optimization evidence、code map、testing overview、phase index、matrix、roadmap、queue row 已同步。 |
| doc-rvv action | 不创建 production 长期主题文档。 | `doc-rvv/features/shot-RVV.zh.md` 仍不适用。 |

## 验证结果

| command | result | boundary |
| --- | --- | --- |
| `git diff -- features/include/pcl/features/impl/shot.hpp` | empty diff | 证明 PI2 production patch 已从 `shot.hpp` 撤回。 |
| `make -C test-rvv/features/shot run_test_shape_bin` | Std/RVV 各 6/6 pass | shape-bin diagnostic helper 和 historical production-detail tests 仍通过。 |
| `make -C test-rvv/features/shot run_test_compare` | Std/RVV 各 15/15 pass | public fixed-LRF、所有 component diagnostic 和 historical production-detail tests 仍通过。 |
| `make -C test-rvv/features/shot check_evidence_doc_refs` | evidence registry check fresh | 登记的 side-run summary / manifest / doctor 均有文档引用。 |
| `make -C test-rvv/features/shot evidence_status` | evidence registry check fresh | 当前登记证据没有未登记覆盖或未登记 run-label 文件。 |
| `git diff --check -- ...` + topic doc whitespace scan | pass | 已检查 production、queue、Handoff diff 和 untracked topic 文档尾随空白。 |

## Production diff 摘要

PI3 撤回了 PI2 对 `shot.hpp` 的所有 production 改动：

- 移除 `pcl/rvv_point_load.h` 和 `pcl/rvv_point_traits.h` 的 `__RVV10__` include。
- 移除 `SHOTRVVNormalAoSLayout`、`shotCreateBinDistanceShapeRVV` 和抽出的 `shotCreateBinDistanceShapeStd`。
- `createBinDistanceShape` 恢复为原来的局部标量循环和 `PCL_WARN` 行为。

当前 production API 和 runtime behavior（运行时行为）与 PI2 前一致。

## 保留的历史证据

| run label | evidence role | case | Std ms | RVV ms | speedup | Doctor |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `board-shot-production-shape-bin-direct-pi2` | production-detail historical | `production_shape_bin_direct` | 4.48743 | 4.17598 | 1.07x | Errors=0, Warnings=1 |
| `board-shot-public-shot352-production-pi2` | production-public historical | `public_shot352_fixed_lrf` | 0.122331 | 0.125424 | 0.98x | Errors=1, Warnings=1 |
| `board-shot-public-shot1344-production-pi2` | production-public historical | `public_shot1344_fixed_lrf` | 0.268388 | 0.269739 | 0.99x | Errors=1, Warnings=1 |
| `board-shot-production-shape-bin-direct-side-20260825-once` | production-detail historical side-run | `production_shape_bin_direct` | 4.38712 | 4.64276 | 0.94x | Errors=1, Warnings=1 |
| `board-shot-public-shot352-side-20260825-once` | production-public historical side-run | `public_shot352_fixed_lrf` | 0.122204 | 0.125002 | 0.98x | Errors=1, Warnings=1 |
| `board-shot-public-shot1344-side-20260825-once` | production-public historical side-run | `public_shot1344_fixed_lrf` | 0.268613 | 0.270401 | 0.99x | Errors=1, Warnings=1 |

这些数据说明：shape-bin indexed gather 的 component positive（组件正向）没有转化成 public entry speedup（公开入口收益）。Production-detail 1.07x 只是 weak-positive（弱正向）局部信号，后续 side-run 的 detail 0.94x 又显示该局部信号不稳定；两组 public case 都没有正向收益，不能覆盖 public case 的退化 Error。

## Optimization matrix 更新

| candidate family | decision | reason | unblocked next action |
| --- | --- | --- | --- |
| shape-bin indexed production probe | rollback/no-production | production-public 0.98x / 0.99x，两个 public Doctor 均有退化 Error；用户已确认回滚。 | none。 |
| interpolation geometry staging | rejected / not recommended | Phase 050 修正后仍 0.97x。 | 仅当 production profile 显示 geometry projection 是独立主瓶颈时恢复。 |
| interpolation bin-selection scalar-tail staging | rejected / unstable | Phase 080 三次 run 跨 0.84x / 1.12x / 1.17x，Phase 100 代表 alias 为 0.83x。 | 仅当重新定义 histogram scatter 或 direct profile 问题时恢复。 |
| color RGB/LUT indexed staging | rejected / neutral-weak | Phase 070 为 1.02x，staging 吞掉 LAB arithmetic 收益。 | 仅当 direct RGB/LUT gather 消融明显正向时恢复。 |
| descriptor normalization production probe | deferred with external trigger | component 1.49x-1.63x，但没有 production profile 证明它能突破 public entry 稀释。 | 需要新的 production profile 或用户明确授权独立 PI1。 |

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | PI2 是 production-detail + production-public；PI3 是 rollback closeout。 |
| A/B boundary | PI2 已覆盖 production detail helper 和 public overload；PI3 不新增 A/B。 |
| 当前决策问题 | 是否保留 PI2 production patch。 |
| diagnostic 是否可外推到 production | 否。Phase 040 indexed component positive 被 PI2 production-public negative 纠正。 |
| comparison-boundary / baseline mismatch 风险 | 已通过 PI2 暴露；detail 1.07x 不能替代 public 0.98x / 0.99x。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已允许并执行；结果不支持采纳。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不适用；当前没有 adopted RVV family，且 public Std/RVV 已负向。 |

## Evidence Doctor 处理

本阶段不新增 board run，不新增 Evidence Doctor 输入。PI2 public Doctor Error 已作为回滚主证据保留；补充 side-run 已在 benchmark/evidence 和 evaluation 文档中引用，`evidence_status` 和 `check_evidence_doc_refs` 当前均为 fresh。

## Continue / stop decision

`continue_stop_decision`: stop。

`stop_condition_hit`: 当前矩阵和 roadmap 没有授权、未阻塞且值得默认继续推进的 SHOT descriptor RVV candidate。

停止理由：

- shape-bin indexed production probe 已接入实测，public entry negative / neutral-negative，且已回滚；
- interpolation geometry、interpolation bin-selection 和 color RGB/LUT staging 已有负向、弱中性或不稳定证据；
- normalization component 虽有稳定局部收益，但缺少 production profile（生产路径性能剖析）证明它是独立主瓶颈，继续会变成新的 production probe，需要独立 PI1 授权和新证据预算；
- topic-local doc suite、target alias 和 evidence registry 已在 Phase 090 / 100 闭合；
- 当前没有 adopted production behavior，因此 production 长期 `doc-rvv` 不适用。

`next_phase_default`: none for current authorization。若未来重启 SHOT descriptor，需要先提供 production profile 或明确授权新的独立 PI1 production probe，而不是从已拒绝的 staging / shape-bin patch 继续叠加。
