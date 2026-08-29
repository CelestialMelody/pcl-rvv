# Phase 020 Result: production-integration-direct-window

## 阶段结论

Phase 020 已完成 PI1-PI5 production integration loop（生产接入闭环）。本阶段把
`dotmodScoreWindowDirectRVV()` 接入 `recognition/src/dotmod.cpp` 的
`pcl::DOTMOD::detectTemplates()`，在 RVV 构建中直接读取原 `QuantizedMap` 窗口并计数；
非 RVV 构建保留原 `getSubMap()` 标量路径。

本阶段首次 production-public（真实公开入口）板卡证据为 positive。该阶段随后被 Phase 030 的
response-buffer-reuse 复跑覆盖；当前长期结论以 Phase 030 result 和
`log/board/repeated_phase020_production_direct/summary.md` 中的 post-Phase030 数据为准。

## 计划动作回填

| action | status | evidence |
| --- | --- | --- |
| production-direct test helper | done | `include/impl/dotmod_template_matching_production_direct.hpp` 通过 `createAndAddTemplate()` 和 `detectTemplates()` 构造真实生产入口 |
| RED asm gate | done | production patch 前 `dump_production_direct_bench_rvv` 找不到 `dotmodScoreWindowDirectRVV`，证明 gate 能捕捉未接入状态 |
| production patch | done | `recognition/src/dotmod.cpp` 新增 `__RVV10__ && __riscv_vector` include、internal RVV helper 和 RVV 分支 |
| correctness | done | `make -C test-rvv/recognition/dotmod_template_matching run_production_direct_test_compare` 通过 Std/RVV 两侧各 2 个 gtest |
| asm | done | `make -C test-rvv/recognition/dotmod_template_matching dump_production_direct_bench_rvv` 命中 `dotmodScoreWindowDirectRVV`、`vle8`、`vand`、`vmsne`、`vcpop` |
| board repeated | done | `collect_production_direct_repeated_board BENCH_ARGS='256 192 24 16 100 5 8 2 0.9 1'` 完成 5-run；Phase 030 后同一路径被刷新为当前证据 |
| Evidence Doctor / registry | done | `record_production_direct_evidence_state` 生成 summary、manifest、doctor，并登记 registry |

## Production Boundary

| item | result |
| --- | --- |
| public entry | `pcl::DOTMOD::detectTemplates()` |
| adopted mechanism at Phase 020 | RVV direct-window score helper |
| non-RVV behavior | 原 `getSubMap()` + scalar byte loop |
| public API | unchanged |
| point type expansion | not applicable；输入是量化 byte map |
| unvalidated scope | 真实 RGB-D workload 分布、其它模板尺寸、其它 `bin_size`、DOTMOD modality preprocessing |

## Evidence Decision

Phase 020 decision 为 `production-ready`，并在用户本轮授权下允许继续采纳判定。由于源码中仍存在每窗口
`responses` vector 重复构造，本阶段没有直接 closeout；它按 phase loop 继续进入 Phase 030。

`diagnostic-to-production mismatch audit`: Phase 010 只支持进入 production probe；Phase 020 使用真实
`DOTMOD::detectTemplates()` 的 production-public evidence，不依赖诊断外推完成最终采纳。

`production-public vs family selection`: 当前 DOTMOD 没有既有 adopted RVV family。Phase 020 的
Std/RVV positive 只证明当前 public RVV path 快于当前 public scalar path；Phase 030 再检查一个实现形态小改。

## Continue / Stop Decision

`continue_stop_decision`: continue to `030-response-buffer-reuse`。

`stop_condition_hit`: none。

`next_phase_default`: `030-response-buffer-reuse`。该阶段只允许复用 `responses` 缓冲并重跑同一 production
direct 证据，不能扩大到 modality preprocessing 或其它 API。
