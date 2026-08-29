# Phase 010 Plan: full-detecttemplates-shaped-diagnostic

## 阶段意图和边界

本阶段把 Phase 000 的 direct-window score helper 放回完整 `DOTMOD::detectTemplates()`
形态，建立 full detectTemplates-shaped diagnostic（完整检测入口形态诊断）。
本阶段仍不修改 production（生产源码），只在 `test-rvv/recognition/dotmod_template_matching`
中验证：

- 多 modality（多模态）输入下，每个窗口依次累加所有 template 的 response（响应分数）。
- threshold（阈值）比较使用 `>`，detection（检测结果）输出顺序保持 row、col、template 的原标量顺序。
- RVV 候选只替换窗口内 byte AND + count，不改变 `responses` 分配、模板遍历或输出容器。

## 当前状态清单

| item | current state |
| --- | --- |
| Phase 000 | `dotmod_direct_window_score` 板卡 5-run median `2.440x`，Evidence Doctor `Errors=0` |
| production source | `recognition/src/dotmod.cpp` 仍是原实现，每个 modality/window 调用 `getSubMap()` 后计数 |
| topic helper | 只有单窗口 score helper；尚无完整 detectTemplates-shaped helper |
| production state | 无 production patch |
| evidence registry | 已登记 Phase 000 summary / manifest / doctor |

## 假设与候选族

| candidate family | hypothesis | risk / unknown | status |
| --- | --- | --- | --- |
| `full-detecttemplates-direct-window-rvv` | 在完整多模板 / 多模态 / threshold 链路中复用 direct-window RVV 计数，仍能超过当前 submap 标量形态 | `responses` 分配、template loop 和 detection push 可能稀释 Phase 000 收益 | planned |
| `response-buffer-reuse` | 若 full-chain direct-window 收益被分配稀释，可把 `responses` 移出窗口内层再比较 | 这是另一实现族，本阶段只记录，不接入 | deferred |

## Phase Scope 与扩展队列

| field | scope |
| --- | --- |
| `validated_scope` | synthetic organized quantized byte maps；`uint8_t` row-major modality maps；多个 template；多个 modality；single `bin_size` derived window；threshold 输出 |
| `unvalidated_scope` | 真实 `DOTModality` 对象、`QuantizedMap` ownership、真实 `DOTMOD::templates_` 私有状态、production dispatch、序列化、训练路径 |
| `point_type_expansion_queue` | not applicable；DOTMOD template matching 使用量化 byte map，不是模板点类型入口 |
| `phase_closeout_boundary` | 只能关闭 full detectTemplates-shaped diagnostic；不能关闭 production direct 或 adopted production |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `full-detecttemplates-direct-window-rvv` | organized quantized byte map sliding windows | `uint8_t` image/template row-major layout | test helper matching full `detectTemplates()` loop shape | add RED/GREEN gtest then `run_test_compare` | full-chain bench with template/modalities parameters | 5-run board repeated if local evidence passes | RVV byte/mask/popcount still present in full-chain bench | summary + manifest + doctor | planned | implement helper by TDD |

## 实现和测试动作

| action | artifact / command | completion rule |
| --- | --- | --- |
| RED test | `src/test_dotmod_template_matching.cpp` references full-shaped helper before implementation | RVV build fails because helper/type is missing |
| GREEN helper | `include/impl/dotmod_template_matching_candidates.hpp` | helper returns same detections and checksum as submap baseline |
| bench extension | `src/bench_dotmod_template_matching.cpp` | output keeps Phase 000 rows and adds full-chain row |
| correctness | `make -C test-rvv/recognition/dotmod_template_matching run_test_compare` | Std/RVV both pass |
| QEMU smoke | `run_bench_rvv` with small full-chain args | only log shape / path evidence |
| asm | `check_dotmod_direct_window_rvv_asm` or extended target | key RVV byte/mask/popcount instructions still present |
| board repeated | new repeated board target with 5-run budget | median speedup bucket decides whether to enter PI1 |
| Evidence Doctor / registry | topic-local wrapper extended for Phase 010 | Errors must be 0 before EvidenceDecision |

## 板卡复跑预算和决策桶

默认 5-run。`median speedup >= 1.20` 且 `B/A < 1` 为 `0/5` 时判为
`positive`；`1.05-1.20` 为 `weak_positive`；`0.95-1.05` 为 `neutral`；
`<0.95` 为 `negative`。若 bucket 摇摆或 checksum 不一致，先修正或降级为
`unstable`，不无限复跑。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` / `bench wrapper` |
| 当前决策问题 | `RVV-vs-scalar` and `implementation-shape` |
| diagnostic 是否可外推到 production | 比 Phase 000 更接近，但仍不能直接外推；它不调用真实 `DOTMOD` 私有模板和 `DOTModality` 对象 |
| comparison-boundary / baseline mismatch 风险 | 有；synthetic templates/maps 必须在文档中写清，不等于真实训练模板分布 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 `weak_positive` 以上且 correctness / doctor 无 Error 时才考虑；否则先尝试 `response-buffer-reuse` 或停止 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 如果只存在一个 RVV family 且 production public Std/RVV 正向，不需要 RVV-vs-RVV；若同时接入 response reuse/fused family，则需要 |

## 继续 / 停止条件

若 Phase 010 positive，下一步进入 PI1 production integration plan（生产接入计划），冻结真实
`recognition/src/dotmod.cpp` 修改范围，并在本轮用户授权下可继续 PI2-PI5。若 Phase 010
neutral / negative / unstable，先判断是否还有 `response-buffer-reuse` 这种当前授权内的高价值方向；
没有时才停止并整理 no-production 或 diagnostic-only 结论。

## 文档更新清单

Phase 010 完成后更新：

- `doc/phases/010-full-detecttemplates-shaped-diagnostic/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/dotmod_template_matching-evaluation.zh.md`
- `README.zh.md`
- `tmp/rvv-work-logs/recognition/dotmod_template_matching/current-handoff/current-handoff.zh.md`
