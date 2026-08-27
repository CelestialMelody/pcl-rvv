# Phase 000 Result: score-update diagnostic

## 当前结论

EvidenceDecision：`partial-production-candidate`。

Phase 000 证明了 `updatedScoresAccordingToNeighborValues` 形态的连续 float score image（分数图像）3x3 邻域传播适合 RVV candidate（候选实现）：QEMU correctness 通过、板卡 gtest 通过、反汇编能归因到 RVV 指令、5 次板卡 repeated benchmark 为稳定正向。该证据仍是 test helper diagnostic（测试辅助函数诊断），不能替代 production evidence（生产证据）。

## 计划回填

| action | result | evidence |
| --- | --- | --- |
| RED test | done | 曾将 `updateScoresRVV()` 临时改为全零输出后运行 `make run_test_rvv`，出现 `actual=0` vs scalar expected mismatch，测试能捕获候选错误 |
| GREEN candidate | done | `make run_test_compare`：Std/RVV QEMU gtest 均 1/1 passed |
| asm check | done | `make dump_test_rvv`、`make dump_bench_rvv`；filtered asm 有 `vle32.v`、`vse32.v`、`vfadd.vv`、`vfmul.*`、`vfsub.*`、`vfabs.v`、`vmflt.vf`、`vmerge.vvm`、`vsetvli` |
| board smoke | done | `make run_board_test fetch_board_logs`：板卡 gtest 1/1 passed |
| repeated board bench | done | `make board_repeated`：5 次 `score_update_641x481_tail`，median speedup `2.560x`，min `2.520x`，max `2.590x`，0/5 低于 1 |
| Evidence Doctor | done | `make evidence_doctor_repeated`：`Errors=0, Warnings=0, Suggestions=1` |

## Board repeated summary

| case | runs | Std mean ms | RVV mean ms | median speedup | values | checksum |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `score_update_641x481_tail` | 5 | 531.1158 | 207.6578 | `2.560x` | `2.560x, 2.590x, 2.520x, 2.560x, 2.570x` | `8.74573e+08` both sides |

Evidence paths are local-only unless the user explicitly asks to submit evidence logs:

- `log/board/repeated_phase000_score_update/summary.md`
- `log/board/repeated_phase000_score_update/evidence_manifest.json`
- `log/board/repeated_phase000_score_update/evidence_doctor.md`
- `log/evidence_registry.json`

## Evidence Doctor

`Errors=0`、`Warnings=0`、`Suggestions=1`。剩余 suggestion 为 `environment_metadata_missing`：缺少 taskset、governor、freq、temperature。该缺口不阻塞当前 diagnostic positive，因为 5 次 repeated 方向一致、没有低于 1 的 run、checksum 一致；但它阻止把这批数据升级为 strict production performance（严格生产性能）证据。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic；bench wrapper 和 helper 接近 production score-update 数据形态，但仍未接生产入口 |
| A/B boundary | test helper；Std build 走 `updateScoresStd`，RVV build 走 `updateScoresRVV` |
| 当前决策问题 | RVV-vs-scalar；是否值得进入更贴近 public pipeline 的下一阶段 |
| diagnostic 是否可外推到 production | 部分可外推到 score-update 内部循环；不能外推到完整 `computeFeature()` |
| comparison-boundary / baseline mismatch 风险 | helper 内低；完整 production 高，因为不包含 `RangeImage::get1dPointAverage`、`LocalSurface`、shadow / veil 和 output construction |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive；若下一阶段转弱或负，仍需先做 public-shaped mismatch audit，不能直接 no-production |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；若后续进入 PI1-PI5，必须补 production direct correctness、fallback、asm、board repeated 和用户确认 |

## 结构和文档审计

| area | current shape scan | decision | next action |
| --- | --- | --- | --- |
| test/bench source layout | `src/test_*.cpp`、`src/bench_*.cpp` 已存在 | adopted | Phase 010 继续沿用 |
| aggregator and internal helpers | `include/range_image_border_extractor.h` 聚合，`include/impl/*score_update.hpp` 内部 helper | adopted | Phase 010 若新增 RangeImage fixture，再按职责拆分 |
| script and bench registry | topic-local manifest wrapper 已存在；`log/evidence_registry.json` 已记录本次 local-only evidence | adopted | 若提交 summary evidence，再补脱敏/提交边界 |
| target granularity | correctness、QEMU smoke、board smoke、board repeated、doctor target 已存在；production direct target 未适用 | adopted for Phase 000 | Phase 010 新增 public-shaped target |
| topic-local docs | README、testing overview、correctness tests、benchmark/evidence、optimization evidence、code map、evaluation、phase suite 已补齐 | adopted | 后续 phase 更新对应 role |
| production topic doc | 未接 production、未完成 PI1-PI5 | not_applicable with evidence | 不创建 `doc-rvv/features/range_image_border_extractor-RVV.zh.md` |

## 下一阶段

默认继续到 Phase 010：`production-shaped-score-pipeline`。目标是在 test-rvv 内构造更接近 public path 的 RangeImage fixture / 四方向 score pipeline 诊断，验证 Phase 000 的 helper-level 正向是否仍能穿透 production-shaped boundary。停止条件：需要修改 production、oracle 无法闭合、板卡不可达、Evidence Doctor Error 无法降级，或 dirty isolation 变得不安全。
