# Phase 020 Plan: public-inline-filter-production-integration

## 阶段意图和边界

本阶段把 `recognition/include/pcl/recognition/hv/occlusion_reasoning.h` 中的 public inline
`filter(scene, model, f, threshold)` 与 `getOccludedCloud(scene, model, f, threshold)` 接入
production direct（真实生产路径）证据链，确认 RVV（RISC-V Vector，可变长度向量扩展）
分流、fallback（回退）和 `copyPointCloud` wrapper（包装层）边界是否值得采纳。

本阶段只覆盖 `PointXYZ / float / AoS（数组结构） / dense` 的公共入口，不扩大到其它点型、
`Scalar != float`、非 dense 输入、其它 layout 或 `ZBuffering` 的新生产细节。`getOccludedCloud()`
和 public `filter()` 共用同一 dispatch；性能证据以 public `filter()` 返回 filtered cloud 的
board repeated（重复板卡测试）为准，正确性则同时核对两条 public inline 路径。

## 当前状态清单

| item | current state | path |
| --- | --- | --- |
| Phase 010 production direct | `ZBuffering::filter(model, indices, thres)` 已采纳 | `doc/phases/010-production-integration/result.zh.md` |
| public inline implementation | `occlusionReasoningFilterDispatch`、Std/RVV helper 和 test hook 已在 public header 中 | `recognition/include/pcl/recognition/hv/occlusion_reasoning.h` |
| correctness tests | public inline filter / getOccludedCloud 线已补 gtest | `src/test_occlusion_reasoning.cpp` |
| bench harness | 新增 public inline bench，计时 wrapper 和 `copyPointCloud` | `src/bench_occlusion_reasoning_public_inline.cpp` |
| board access | 已验证可用，`SSH_AUTH_SOCK` 可注入 | current shell |
| topic-local docs | 需补 phase 020 / matrix / roadmap / evaluation / code map / evidence docs | `doc/**` |

## Phase Scope 与扩展队列

- `validated_scope`：`pcl::occlusion_reasoning::filter(scene, model, f, threshold)` 与
  `getOccludedCloud(scene, model, f, threshold)`，`PointXYZ / float / AoS / dense`。
- `unvalidated_scope`：其它点型、`Scalar != float`、非 dense、非 xyz AoS、`ZBuffering::filter(model, filtered)`
  的类包装层成本、`depth-gather-rvv`、`smooth-window-min-rvv`。
- `point_type_expansion_queue`：若后续继续，只能另开 point-type 扩展 phase，优先补
  `PointXYZRGB` / `PointXYZI` 之类的独立布局验证。
- `phase_closeout_boundary`：只能关闭 public inline filter/getOccludedCloud 的 production-direct 条目。

## 诊断到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-public |
| A/B boundary | public inline free function `filter(scene, model, f, threshold)` |
| 当前决策问题 | RVV-vs-scalar production adoption |
| diagnostic 是否可外推到 production | 只能作为进入本阶段的参考；最终采纳必须看 public inline board repeated |
| comparison-boundary / baseline mismatch 风险 | yes；public inline bench 包含 `copyPointCloud`，和 Phase 010 indices-only 入口不同 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 board positive 且 checksum 一致，可采纳；否则保留为 deferred |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有既有 adopted RVV family 竞争，不需要 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `public-inline-filter-rvv` | organized scene depth + model point stream | `PointXYZ / float / AoS / dense` | `filter(scene, model, f, threshold)` / `getOccludedCloud(scene, model, f, threshold)` | `run_test_compare` pass；RVV hook 命中；Std fallback pass | `bench_occlusion_reasoning_public_inline` production direct | planned 5-run board repeated | `check_inline_filter_rvv_asm` | planned | planned | write inline RED / positive path tests |
| `production-projection-filter-rvv` | model point stream + member `depth_` buffer | `ModelT` xyz AoS, float fields | `ZBuffering::filter(model, indices, thres)` | 已通过 | `bench_occlusion_reasoning` production direct | `repeated_phase010_production_direct` positive | `check_occlusion_filter_rvv_asm` pass | clean enough | adopted | none |
| `depth-gather-rvv` | model point stream + member `depth_` buffer | same as adopted public inline / ZBuffering scope | production detail helper | not started | not started | not started | not started | not started | deferred | separate phase / topic only |
| `smooth-window-min-rvv` | depth grid neighborhood | `float` depth grid | `computeDepthMap(smooth=true)` | not covered | not covered | not covered | not covered | not covered | deferred | needs caller evidence |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED path test | `src/test_occlusion_reasoning.cpp` | RVV build 在 public inline hook 下失败或命中不对，迫使候选实现补齐 |
| public inline bench | `src/bench_occlusion_reasoning_public_inline.cpp` | 计时 public inline wrapper 且包含 `copyPointCloud` |
| asm | `make -C test-rvv/recognition/occlusion_reasoning check_inline_filter_rvv_asm` | RVV 指令归属到 public inline hot path |
| board repeated | `inline_board_repeated` | 5-run decision bucket 稳定、checksum 一致 |
| Evidence Doctor / registry | `record_inline_evidence_state_repeated` | Error=0；Warnings/Suggestions 已解释 |
| docs | phase result / evaluation / `doc-rvv` / roadmap / matrix / Handoff | 当前事实可恢复、可追踪 |

## 板卡复跑预算和决策桶

默认 5-run，`INLINE_BENCH_ARGS="65536 150 150 200 5"`。`positive` 的判断口径为
median speedup `>= 1.20x` 且 `B/A < 1 = 0/5`。若结果落入 `weak_positive`、`neutral`、
`negative` 或 `unstable`，最多只追加 1 组同边界复跑；预算耗尽仍摇摆时降级为 `unstable`。

## 继续 / 停止条件

若 public inline board repeated 为 positive，且 checksum 一致、Evidence Doctor 无 Error，
则进入文档 closeout：更新 phase result、evaluation、topic-local docs、long-term `doc-rvv`
和 Handoff。若本轮再要继续优化，只能从 `depth-gather-rvv` 或其它新边界另开 phase；当前
topic scope 内没有更高优先级未阻塞动作时停止。
