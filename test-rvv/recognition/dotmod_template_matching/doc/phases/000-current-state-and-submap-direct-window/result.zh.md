# Phase 000 Result: current-state-and-submap-direct-window

## 阶段结论

Phase 000 已完成。`submap-direct-window-score-rvv` 在单窗口、单模板、单
modality（模态）的 production-shaped diagnostic（生产形态诊断）边界内为
`positive`：板卡 5-run 的 `dotmod_direct_window_score` median speedup
为 `2.440x`，范围为 `2.438x - 2.547x`，`B/A < 1` 为 `0/5`。

这不是 production direct（真实生产路径）证据。它只说明把窗口内
`image_data & template_data` 计数从临时 submap 改成直接窗口读取并用 RVV
（RISC-V Vector，可变长度向量扩展）计数，在当前 synthetic helper 边界下值得继续。
下一阶段必须把 helper 放回完整 `detectTemplates()` 形态，覆盖多 template、
多 modality、`responses` 分配、threshold（阈值）和 detection（检测结果）输出顺序。

## 计划动作回填

| action | status | evidence |
| --- | --- | --- |
| RED gtest | done | `make -C test-rvv/recognition/dotmod_template_matching run_test_rvv` 曾因 helper 缺失失败 |
| GREEN helper | done | `include/impl/dotmod_template_matching_candidates.hpp` 增加 `scoreWindowViaSubMapStd`、`scoreWindowDirectStd` 和 `scoreWindowDirectRVV` |
| correctness | done | `make -C test-rvv/recognition/dotmod_template_matching run_test_compare` 通过 Std/RVV 两侧 2 个 gtest |
| QEMU smoke | done | `make -C test-rvv/recognition/dotmod_template_matching run_bench_rvv BENCH_ARGS='64 48 11 7 1 1'` 仅作为日志形状和可运行性 smoke，不作为性能结论 |
| asm attribution | done | `make -C test-rvv/recognition/dotmod_template_matching check_dotmod_direct_window_rvv_asm` 命中 `vle8`、`vand`、`vmsne`、`vcpop` |
| board repeated | done | `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make -C test-rvv/recognition/dotmod_template_matching collect_submap_direct_window_repeated_board BENCH_ARGS='256 192 24 16 200 5'` |
| Evidence Doctor | done | `make -C test-rvv/recognition/dotmod_template_matching record_phase000_evidence_state` 生成 summary、manifest、doctor 并登记 registry |

## 证据摘要

| case | role | median | min | max | B/A < 1 | decision |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `dotmod_submap_baseline_window_score` | control only | `0.999x` | `0.961x` | `0.999x` | `5/5` | neutral control，不参与候选采纳 |
| `dotmod_direct_window_score` | candidate | `2.440x` | `2.438x` | `2.547x` | `0/5` | positive |

证据路径：

- Summary：`test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase000_submap_direct_window/summary.md`
- Manifest：`test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase000_submap_direct_window/evidence_manifest.json`
- Evidence Doctor：`test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase000_submap_direct_window/evidence_doctor.md`
- Registry：`test-rvv/recognition/dotmod_template_matching/log/evidence_registry.json`

## Evidence Doctor

`Errors=0`，`Warnings=0`，`Suggestions=2`。

两个 suggestion 都不阻塞 Phase 000：

- `environment_metadata_missing`：summary 记录了 device 和 VLEN 说明，但未记录 taskset、governor、freq、temperature。当前 5-run 方向稳定，下一轮 production-shaped 或 production direct repeated board 可补环境字段。
- `binary_identity_missing`：未记录 binary hash。当前命令、路径和 checksum 可复现，若后续出现方向反转或长尾，优先补 binary hash 后重跑。

## Diagnostic To Production Mismatch Audit

| question | result |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper`；Std/RVV 两侧都在 `src/bench_dotmod_template_matching.cpp` 中计时 |
| 当前决策问题 | `RVV-vs-scalar` 和 `implementation-shape` |
| diagnostic 是否可外推到 production | 不能直接外推。Phase 000 不调用 `DOTMOD::detectTemplates()`，不覆盖多 template、多 modality、`responses` 分配和 detection 输出顺序 |
| comparison-boundary / baseline mismatch 风险 | 存在。候选比较的是 direct-window helper；control row 的 submap baseline 只看构建漂移，不参与候选采纳 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不允许；必须先完成 full detectTemplates-shaped diagnostic |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已采纳 RVV family；若后续同时尝试 response-buffer reuse 或 fused loop，需要同边界 A/B |

## 阶段反思

Phase 000 证明窗口计分本身有足够收益，但完整生产路径还包含两类可能稀释或放大收益的成本：

- 每个窗口的 `responses` vector（响应数组）分配仍在内层循环中，可能是独立优化候选。
- 多 template、多 modality 循环会反复读取同一窗口；如果 direct-window helper 在 full chain 中仍正向，后续可比较“只替换窗口计分”和“复用 response buffer”的生产接入价值。

## Continue / Stop Decision

`continue_stop_decision`: continue.

`stop_condition_hit`: none.

`next_phase_default`: `010-full-detecttemplates-shaped-diagnostic`。Phase 010 应在 topic-local helper 中模拟完整
`DOTMOD::detectTemplates()` 链路：多 template、多 modality、threshold、detection 顺序和 checksum，
先通过 correctness，再做 asm、QEMU smoke 和 board repeated。
