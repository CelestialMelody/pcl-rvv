# Phase 020 Result: public-inline-filter-production-integration

## 阶段结论

本阶段已完成 public inline `pcl::occlusion_reasoning::filter(scene, model, f, threshold)` 与
`getOccludedCloud(scene, model, f, threshold)` 的 production direct（真实生产路径）接入和采纳。
用户已允许“板卡上的测试结果如果显示有收益即可采纳”，因此 board repeated 为 positive 且
checksum 稳定时，可直接把当前 patch 视为 adopted production behavior。

本阶段的性能边界包含 public wrapper 的 `copyPointCloud` 成本，但不包含 scene/model setup；
它证明的是当前 public RVV path 是否快于当前 public scalar path，不外推到其它点型、
`Scalar != float`、`ZBuffering` 的新生产细节或 `depth-gather-rvv`。

## 实际执行范围

| 项目 | 计划 | 实际结果 |
| --- | --- | --- |
| public inline dispatch / fallback | `__RVV10__` + xyz AoS gate 走 RVV，否则回退标量 | done |
| public inline correctness | `run_test_compare` 命中 public inline RVV hook | done |
| public inline asm | RVV hot path 出现 `vlse32` / `vlsseg3e32`、`vfdiv`、`vfcvt` | done |
| public inline board repeated | 5-run production direct，包含 `copyPointCloud` | done，decision bucket 为 `positive` |
| Evidence Doctor / registry | 生成 summary / manifest / doctor 并登记 | done |
| topic-local / long-term docs | 期望把 public inline 证据同步到 phase、evaluation 和 `doc-rvv` | done |

## Production Direct 证据

| 证据 | 路径 / 命令 | 结果 | 边界 |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/recognition/occlusion_reasoning run_test_compare` | pass | Std/RVV 输出一致，RVV build 命中 public inline hook |
| QEMU smoke | `run_qemu_smoke`（同 correctness 路径） | pass | 只证明构建和路径，不证明性能 |
| asm attribution | `make -C test-rvv/recognition/occlusion_reasoning check_inline_filter_rvv_asm` | pass | public inline bench 中有 RVV 指令 |
| board repeated | `log/board/repeated_phase020_inline_filter_production_direct/summary.md` | median `1.250x`，min/max `1.240x/1.260x`，`B/A < 1 = 0/5` | production-public performance |
| Evidence Doctor | `log/board/repeated_phase020_inline_filter_production_direct/evidence_doctor.md` | `Errors=0`，`Warnings=0`，`Suggestions=1` | 仅缺环境 metadata |
| registry | `log/evidence_registry.json` | fresh | Phase 020 summary / manifest / doctor 已登记 |

## Diagnostic To Production Mismatch Audit 回填

| question | answer |
| --- | --- |
| evidence role | production-public |
| A/B boundary | public inline free function `filter(scene, model, f, threshold)` |
| 当前决策问题 | RVV-vs-scalar production adoption |
| diagnostic 是否可外推到 production | public inline diagnostic 已经是 production-shaped；最终采纳只使用 Phase 020 production direct |
| comparison-boundary / baseline mismatch 风险 | yes；public inline bench 包含 `copyPointCloud`，与 Phase 010 indices-only 边界不同 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive；若再做扩展，必须另起边界 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有既有 adopted RVV family 竞争，不需要 |

## Evidence Doctor 和 Registry

Evidence Doctor 没有 Error 或 Warning。唯一 Suggestion 是补 `taskset`、`governor`、`freq`、
`temperature` 和 device / VLEN metadata。由于 5 次方向稳定、checksum 一致、没有退化 run，
这个 suggestion 不阻塞 adopted 结论。

registry 已记录 Phase 020 的 summary、manifest、doctor Markdown 和 doctor JSON。提交前仍应
运行 `make -C test-rvv/recognition/occlusion_reasoning check_evidence_freshness` 复核是否出现未登记覆盖。

## Optimization Matrix 更新

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `public-inline-filter-rvv` | organized scene depth + model point stream | `PointXYZ / float / AoS / dense` | `filter(scene, model, f, threshold)` / `getOccludedCloud(scene, model, f, threshold)` | `run_test_compare` pass；RVV hook 命中；Std fallback pass | `bench_occlusion_reasoning_public_inline` production direct | positive `1.250x` | pass | Errors=0 / Warnings=0 / Suggestions=1 | adopted | none |
| `production-projection-filter-rvv` | model point stream + member `depth_` buffer | `ModelT` xyz AoS，`float` fields；已测 `PointXYZ` | `ZBuffering::filter(model, indices, thres)` | `run_test_compare` pass | `bench_occlusion_reasoning` production direct | positive `1.270x` | pass | Errors=0 / Warnings=0 / Suggestions=1 | adopted | none |
| `depth-gather-rvv` | model point stream + member `depth_` buffer | same as adopted public inline / ZBuffering scope | possible production detail helper | not_yet_covered | not_yet_covered | not_yet_covered | not_yet_covered | not_yet_covered | deferred | separate phase / topic only |
| `smooth-window-min-rvv` | depth grid neighborhood | `float` depth grid | `computeDepthMap(smooth=true)` | not_yet_covered | not_yet_covered | not_yet_covered | not_yet_covered | not_yet_covered | deferred | needs caller evidence |

## Doc Suite Parity 审计

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已指向 Phase 020、evaluation、`doc-rvv` 和常用 target | 入口导航应展示当前 truth 和证据白名单 | adopted | `README.zh.md` | none |
| testing overview | 已列 correctness、QEMU、asm、board、doctor、registry、freshness 和 inline board | target 粒度审计应来自当前 Makefile / board.mk | adopted | `doc/testing-overview.zh.md` | none |
| correctness tests | 已列 public inline 与 ZBuffering 两组 production direct TEST | 每个 TEST 写输入、路径、断言和不能证明范围 | adopted | `doc/correctness-tests.zh.md` | none |
| benchmark/evidence | 已列 public inline bench、summary、doctor、registry | 性能结论只来自 board repeated | adopted | `doc/benchmark-and-evidence.zh.md` | none |
| optimization evidence | 已列两条 adopted production path | adopted 和 deferred 不混写 | adopted | `doc/optimization-evidence.zh.md` | none |
| test support map | 已列 include、impl、src、script、Makefile | 代码和证据位置可追踪 | adopted | `doc/test-support-code-map.zh.md` | none |
| evaluation | 已升级为 production evaluation | S11 需要 production patch scope 和 final evidence | adopted | `doc/occlusion_reasoning-evaluation.zh.md` | none |
| production topic doc | 已创建长期 production 文档 | adopted behavior 需要 `doc-rvv` | adopted | `doc-rvv/recognition/occlusion_reasoning-RVV.zh.md` | none |
| phase suite | Phase index、matrix、020 result 已同步 | phase loop 可恢复 | adopted | `doc/phases/**` | none |
| artifact tracking | 当前 topic 文件仍需纳入提交边界 | README / evaluation / `doc-rvv` 引用的文件必须可追踪 | adopted with commit-boundary note | `git status --short --untracked-files=all -- test-rvv/recognition/occlusion_reasoning doc-rvv/recognition/occlusion_reasoning-RVV.zh.md` | commit phase 精确选择 |

## 继续 / 停止判断

`continue_stop_decision`：当前 topic 停止。停止条件命中：Phase 020 证据已把 public inline
filter/getOccludedCloud 的 production boundary 闭合，而 roadmap 中剩余的 `depth-gather-rvv`
和 `smooth-window-min-rvv` 都需要新的证据边界，不属于当前 topic 内的高优先级未阻塞动作。

`next_phase_default`：`ready_for_review`。
