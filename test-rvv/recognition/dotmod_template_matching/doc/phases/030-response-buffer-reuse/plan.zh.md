# Phase 030 Plan: response-buffer-reuse

## 阶段意图和边界

本阶段在 Phase 020 已通过 production-public（真实公开入口）证据的基础上，
继续检查 `pcl::DOTMOD::detectTemplates()` 内层窗口循环里的
`responses` vector（响应数组）重复分配是否值得一并收敛。

本阶段只允许一个窄生产改动：把 `responses` 缓冲移出 row/col 窗口内层，
每个窗口用 `std::fill` 归零，再沿用 Phase 020 已接入的 direct-window RVV
计分。它不改变 public API（公开接口）、`DOTModality`、模板创建、序列化、
threshold（阈值）比较、detection（检测结果）输出顺序或非 RVV 构建的语义。

## 当前状态清单

| item | current state |
| --- | --- |
| Phase 020 production patch | `recognition/src/dotmod.cpp` 已在 `__RVV10__ && __riscv_vector` 下用 `dotmodScoreWindowDirectRVV` 取代每窗口 `getSubMap()` 计分 |
| Phase 020 production-public evidence | 5-run board median `3.164x`，range `3.160x - 3.183x`，`B/A < 1 = 0/5`，Evidence Doctor `Errors=0 / Warnings=0 / Suggestions=2` |
| remaining candidate | `responses` 仍在每个 row/col 窗口位置构造一次，可能造成与 RVV 计分无关的分配成本 |

## 假设与候选族

| candidate family | hypothesis | risk / unknown | status |
| --- | --- | --- | --- |
| `response-buffer-reuse` | 复用 `responses` 缓冲可降低窗口级分配成本，并且不改变每窗口从零开始累加的语义 | 如果分配成本不是主因，Std/RVV 绝对时间可能变化很小；`std::fill` 必须覆盖所有模板槽位 | planned |

## Phase Scope 与扩展队列

| field | scope |
| --- | --- |
| `validated_scope` | `DOTMOD::detectTemplates()` production direct；`uint8_t` organized quantized maps；多模板、多模态；threshold 输出；默认 case `256 192 24 16 100 5 8 2 0.9 1` |
| `unvalidated_scope` | 真实 RGB-D workload 模板分布、其它 `bin_size` / 模板尺寸组合、`QuantizedMap::getSubMap()` standalone、DOTMOD modality 预处理 |
| `point_type_expansion_queue` | not applicable；本 topic 处理量化 byte map，不是点类型模板入口 |
| `phase_closeout_boundary` | 只能判断 response 缓冲复用是否随 Phase 020 production patch 保留；不能证明其它分配或容器优化 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `response-buffer-reuse` | organized quantized byte map sliding windows | `uint8_t` row-major image/template layout | real `DOTMOD::detectTemplates()` public entry | `run_production_direct_test_compare` | `bench_dotmod_template_matching_production_direct_*` | 5-run `collect_production_direct_repeated_board` after patch | production `dotmodScoreWindowDirectRVV` still present | production-public manifest + doctor | planned | patch, correctness, asm, board, doctor |

## 实现和测试动作

| action | artifact / command | completion rule |
| --- | --- | --- |
| production patch | `recognition/src/dotmod.cpp` | `responses` 缓冲在窗口循环外分配，窗口内用 `std::fill` 清零；非 RVV 构建保持同一输出语义 |
| correctness | `make -C test-rvv/recognition/dotmod_template_matching run_production_direct_test_compare` | Std/RVV 两侧 production direct gtest 全通过 |
| asm | `make -C test-rvv/recognition/dotmod_template_matching dump_production_direct_bench_rvv` | `dotmodScoreWindowDirectRVV` 和 `vle8` / `vand` / `vmsne` / `vcpop` 仍可归属 |
| board repeated | `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make -C test-rvv/recognition/dotmod_template_matching collect_production_direct_repeated_board BENCH_ARGS='256 192 24 16 100 5 8 2 0.9 1'` | 5-run summary 与 Phase 020 比较，若 production-public 仍为 positive 且绝对 RVV 时间不退化，保留；若退化或不稳定，回到 Phase 020 形态 |
| Evidence Doctor / registry | `record_production_direct_evidence_state` | Errors 必须为 0；Warnings 必须解释；registry fresh |

## 板卡复跑预算和决策桶

沿用 Phase 020 的 5-run budget。若 median speedup 仍 `>= 1.20` 且 `B/A < 1`
为 `0/5`，并且 RVV median 绝对时间没有相对 Phase 020 明显退化，则把本阶段
作为 adopted production behavior（已采纳生产行为）的一部分。若 speedup positive
但绝对 RVV 时间明显变慢，优先回退本阶段改动而保留 Phase 020 direct-window RVV。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production-public` |
| A/B boundary | real public `DOTMOD::detectTemplates()` compiled from current production source |
| 当前决策问题 | `implementation-shape` and `RVV-vs-scalar` |
| diagnostic 是否可外推到 production | 不依赖诊断外推；本阶段只使用 production direct 结果 |
| comparison-boundary / baseline mismatch 风险 | Phase 030 与 Phase 020 是跨 run 比较，不能写成严格 RVV-vs-RVV；最终采纳仍以本阶段 Std/RVV production-public 为主，Phase 020 只作历史基线 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | not applicable |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前是同一 production patch 的实现形态小改；若证据显示绝对时间退化，则不采纳本阶段 |

## 继续 / 停止条件

若本阶段 production-public evidence（生产公开入口证据）仍为 positive，且 Evidence Doctor
无 Error，则进入 S11 production closeout，正式文档使用本阶段板卡数据作为最终数据。
若本阶段不如 Phase 020，回退 response-buffer-reuse，保留 direct-window RVV，并用 Phase 020
数据收口。若两种形态都显示收益但差异很小，不再继续扩大优化，避免把另一个微优化阶段当成
无界搜索。
