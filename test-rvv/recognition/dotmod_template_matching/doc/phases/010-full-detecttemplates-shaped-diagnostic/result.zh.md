# Phase 010 Result: full-detecttemplates-shaped-diagnostic

## 阶段结论

Phase 010 已完成。`full-detecttemplates-direct-window-rvv` 在 full
`detectTemplates()` shaped diagnostic（完整检测入口形态诊断）边界内为
`positive`：板卡 5-run 的 `dotmod_full_detecttemplates_shaped_total` median
speedup 为 `2.124x`，范围为 `2.074x - 2.209x`，`B/A < 1` 为 `0/5`。

这仍不是 production direct（真实生产路径证据）。它没有调用真实
`pcl::DOTMOD::detectTemplates()`，但已经覆盖 row/col/template 顺序、多
modality（多模态）累加、`responses` 分配、strict threshold（严格阈值）和
detection（检测结果）输出顺序。因此下一阶段可以进入 production integration
loop（生产接入闭环），把同一 direct-window RVV 计数接入
`recognition/src/dotmod.cpp` 后重跑生产直连证据。

## 计划动作回填

| action | status | evidence |
| --- | --- | --- |
| RED/GREEN full-shaped helper | done | `src/test_dotmod_template_matching.cpp` 增加 full-shaped detection order / score / strict threshold 测试，`include/impl/dotmod_template_matching_candidates.hpp` 增加 `detectTemplatesViaSubMapStd`、`detectTemplatesDirectStd`、`detectTemplatesDirectRVV` |
| correctness | done | `make -C test-rvv/recognition/dotmod_template_matching run_test_compare` 通过 Std/RVV 两侧 4 个 gtest |
| QEMU smoke | done | `run_bench_rvv` 只作为日志形状和可运行性 smoke，不作为性能结论 |
| asm attribution | done | `make -C test-rvv/recognition/dotmod_template_matching check_dotmod_direct_window_rvv_asm` 命中 `vle8`、`vand`、`vmsne`、`vcpop` |
| board repeated | done | `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make -C test-rvv/recognition/dotmod_template_matching collect_full_detecttemplates_repeated_board BENCH_ARGS='256 192 24 16 100 5 8 2 0.9'` |
| Evidence Doctor | done | `make -C test-rvv/recognition/dotmod_template_matching record_phase010_evidence_state` 生成 summary、manifest、doctor 并登记 registry |

## 证据摘要

| case | role | median | min | max | B/A < 1 | decision |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `dotmod_submap_baseline_window_score` | control only | `1.018x` | `0.974x` | `1.062x` | `1/5` | neutral control，不参与候选采纳 |
| `dotmod_direct_window_score` | supporting candidate | `2.442x` | `2.412x` | `2.561x` | `0/5` | positive，用于确认窗口计分核仍稳定 |
| `dotmod_full_detecttemplates_shaped_total` | candidate | `2.124x` | `2.074x` | `2.209x` | `0/5` | positive |

证据路径：

- Summary：`test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase010_full_detecttemplates/summary.md`
- Manifest：`test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase010_full_detecttemplates/evidence_manifest.json`
- Evidence Doctor：`test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase010_full_detecttemplates/evidence_doctor.md`
- Registry：`test-rvv/recognition/dotmod_template_matching/log/evidence_registry.json`

## Evidence Doctor

`Errors=0`，`Warnings=0`，`Suggestions=2`。

两个 suggestion 不阻塞 production probe（生产探针）：

- `environment_metadata_missing`：summary 缺少 taskset、governor、freq、temperature。当前 5-run 方向稳定，production direct repeated board 应继续记录 device / VLEN，并把缺失环境字段作为边界说明。
- `binary_identity_missing`：summary 缺少 binary hash。若 production direct 出现方向反转或长尾，优先补二进制身份后重跑。

## Diagnostic To Production Mismatch Audit

| question | result |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` / `bench wrapper` |
| 当前决策问题 | `RVV-vs-scalar` 和 `implementation-shape` |
| diagnostic 是否可外推到 production | 不能直接外推；它仍不调用真实 `DOTMOD` 私有模板状态和真实 `DOTModality` 对象。但它已覆盖完整检测循环形态，因此足以支持有界 production probe。 |
| comparison-boundary / baseline mismatch 风险 | 存在；synthetic byte maps/templates 不等于真实训练模板分布，production direct 必须重新跑真实 `createAndAddTemplate()` + `detectTemplates()`。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段实际为 positive；允许进入窄范围生产探针。若 production direct 退化，本阶段诊断证据不能替代生产证据。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已采纳 DOTMOD RVV family；若只接入 direct-window 计数，production public Std/RVV 正向即可按本轮用户授权采纳。 |

## 阶段反思

full-shaped 结果说明 direct-window RVV 计数没有被 `responses` 分配、多模板 / 多模态循环和输出容器成本完全稀释。仍可继续观察 `response-buffer-reuse`，但它是另一个实现族：只有 production direct 显示 direct-window 接入收益偏弱或仍存在明显分配瓶颈时，才需要同一 production boundary 内继续 A/B。

## Continue / Stop Decision

`continue_stop_decision`: continue.

`stop_condition_hit`: none.

`next_phase_default`: `020-production-integration-direct-window`。下一阶段进入
PI1-PI5：先写生产接入计划，再给 `recognition/src/dotmod.cpp` 增加最小 RVV
dispatch（分流逻辑），并通过 production direct correctness、asm、板卡 repeated
和 Evidence Doctor 后决定是否按本轮用户授权采纳。
