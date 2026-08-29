# Phase 010 Result: production-integration

## 阶段结论

本阶段已完成 PI1-PI5：`ZBuffering<ModelT, SceneT>::filter(model, indices, thres)` 的
RVV production path 已接入并采纳。用户本轮明确允许“板卡上的测试结果如果显示有收益即可采纳”，
因此 Phase 010 的 positive board repeated 结果直接进入 `adopted production behavior`。

当前证据只关闭 `filter(model, indices)` 的 production-public 边界，不外推到 smooth depth window、
`copyPointCloud` 端到端成本、其它点类型布局或其它公开 free-function overload。

## 实际执行范围

| 项目 | 计划 | 实际结果 |
| --- | --- | --- |
| production helper / dispatch | `__RVV10__` + xyz AoS gate 走 RVV，其他回退 | done |
| `computeDepthMap()` 矩形 stride 修复 | 纳入 production patch 并用 regression 保护 | done |
| correctness | Std/RVV 都通过，RVV build 命中 production hook | done |
| QEMU smoke | 只证明 correctness / path shape | done |
| asm | 检查生产 bench hot path 的 RVV 指令 | done |
| board repeated | 5-run production direct | done，decision bucket 为 `positive` |
| Evidence Doctor / registry | 生成 summary / manifest / doctor 并登记 | done |
| `doc-rvv` closeout | 生产证据正向后创建长期文档 | done |

## Production Direct 证据

| 证据 | 路径 / 命令 | 结果 | 边界 |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/recognition/occlusion_reasoning run_test_compare` | pass | Std/RVV 输出一致，RVV build 命中 production hook |
| QEMU smoke | `make -C test-rvv/recognition/occlusion_reasoning run_qemu_smoke` | pass | 只证明构建和正确性 |
| asm attribution | `make -C test-rvv/recognition/occlusion_reasoning check_occlusion_filter_rvv_asm` | pass | bench RVV asm 中有 `vlse32` / `vlsseg3e32`、`vfdiv`、`vfcvt` |
| board repeated | `log/board/repeated_phase010_production_direct/summary.md` | median `1.270x`，min/max `1.260x/1.270x`，`B/A < 1 = 0/5` | production-public performance |
| Evidence Doctor | `log/board/repeated_phase010_production_direct/evidence_doctor.md` | `Errors=0`，`Warnings=0`，`Suggestions=1` | 环境 metadata 建议不阻塞 |
| registry | `log/evidence_registry.json` | fresh | Phase 010 summary / manifest / doctor 已登记 |

## Diagnostic To Production Mismatch Audit 回填

| question | answer |
| --- | --- |
| evidence role | production-public |
| A/B boundary | public template member `ZBuffering::filter(model, indices, thres)` |
| 当前决策问题 | RVV-vs-scalar production adoption |
| diagnostic 是否可外推到 production | Phase 000 diagnostic 只作为 PI1 依据；最终采纳只使用 Phase 010 production direct |
| comparison-boundary / baseline mismatch 风险 | 已缩窄；timer 包含真实 `computeDepthMap()` 预构造后的成员状态，但不含 `copyPointCloud` |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；本阶段 production direct 为 positive |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有既有 adopted RVV family；不需要 RVV-vs-RVV 选择证据 |

## Evidence Doctor 和 Registry

Evidence Doctor 没有 Error 或 Warning。唯一 Suggestion 是下次采集补充 `taskset`、`governor`、
`freq`、`temperature` 和 device / VLEN metadata。由于本轮 5 次方向稳定、checksum 一致、
没有退化 run，这个 suggestion 不降级 production adoption。

registry 已记录 Phase 010 的 summary、manifest、doctor Markdown 和 doctor JSON。提交前仍应运行
`make -C test-rvv/recognition/occlusion_reasoning check_evidence_freshness` 复核是否出现未登记覆盖。

## Optimization Matrix 更新

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `production-projection-filter-rvv` | model point stream + `depth_` buffer | `ModelT` xyz AoS，`float` fields；已测 `PointXYZ` | `ZBuffering::filter(indices)` | `run_test_compare` pass | `bench_occlusion_reasoning` production direct | positive `1.270x` | pass | Errors=0 / Warnings=0 / Suggestions=1 | adopted | none |
| `compute-depth-map-semantics-fix` | scene point stream | production `SceneT`，internal `depth_` | `computeDepthMap()` | rectangular regression pass | included in production direct setup | indirect support | not_applicable | manual audit | adopted correctness fix | none |
| `smooth-window-min-rvv` | depth grid neighborhood | `float` depth grid | `smooth=true` path | not covered | not covered | not covered | not covered | not covered | deferred | separate phase/topic only with caller evidence |

## Doc Suite Parity 审计

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已指向 Phase 010、evaluation、`doc-rvv` 和常用 target | 入口导航应展示当前 truth 和证据白名单 | adopted | `README.zh.md` | none |
| testing overview | 已列 correctness、QEMU、asm、board、doctor、registry、freshness | target 粒度审计应来自当前 Makefile / board.mk | adopted | `doc/testing-overview.zh.md` | none |
| correctness tests | 已列 diagnostic、regression、production direct TEST | 每个 TEST 写输入、路径、断言和不能证明范围 | adopted | `doc/correctness-tests.zh.md` | none |
| benchmark/evidence | 已列 production direct bench、summary、doctor、registry | 性能结论只来自 board repeated | adopted | `doc/benchmark-and-evidence.zh.md` | none |
| optimization evidence | 已列 adopted / deferred candidate | adopted 和 deferred 不混写 | adopted | `doc/optimization-evidence.zh.md` | none |
| test support map | 已列 include、impl、src、script、Makefile | 代码和证据位置可追踪 | adopted | `doc/test-support-code-map.zh.md` | none |
| evaluation | 已升级为 production evaluation | S11 需要 production patch scope 和 final evidence | adopted | `doc/occlusion_reasoning-evaluation.zh.md` | none |
| production topic doc | 已创建长期 production 文档 | adopted behavior 需要 `doc-rvv` | adopted | `doc-rvv/recognition/occlusion_reasoning-RVV.zh.md` | none |
| phase suite | Phase index、matrix、010 result 已同步 | phase loop 可恢复 | adopted | `doc/phases/**` | none |
| artifact tracking | 当前 topic 文件仍为 untracked，需要提交阶段纳入 topic commit | README / evaluation 引用的文件必须进入 to-be-staged 集合 | adopted with commit-boundary note | `git status --short --untracked-files=all -- test-rvv/recognition/occlusion_reasoning doc-rvv/recognition/occlusion_reasoning-RVV.zh.md` | commit phase 精确选择 |

## 继续 / 停止判断

`continue_stop_decision`：当前 topic 停止。停止条件命中：Phase 010 矩阵闭合，roadmap 没有当前 topic
授权范围内的高优先级未阻塞优化动作。`depth-gather-rvv`、`smooth-window-min-rvv` 和更宽点类型扩展都需要
新的 evidence boundary（证据边界），应另开 phase 或 follow-up topic。

`next_phase_default`：`ready_for_review`。
