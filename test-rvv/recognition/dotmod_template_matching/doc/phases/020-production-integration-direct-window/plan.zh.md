# Phase 020 Plan: production-integration-direct-window

## 阶段意图和边界

本阶段进入 production integration loop（生产接入闭环），把 Phase 010 已证明正向的
direct-window RVV score（直接窗口 RVV 计分）接入
`recognition/src/dotmod.cpp` 的 `pcl::DOTMOD::detectTemplates()`。

本阶段只证明一个窄范围：

- production public entry（真实公开入口）：`DOTMOD::detectTemplates()`。
- 数据布局：`QuantizedMap` 的 row-major `uint8_t` map 和
  `DenseQuantizedMultiModTemplate::features` 连续数组。
- 候选机制：在 `__RVV10__ && __riscv_vector` 构建中直接从原图窗口按行读取，
  用 byte AND + nonzero mask + `vcpop` 计数；非 RVV 构建保留原 `getSubMap()`
  标量路径。
- 不修改 public API（公开接口）、训练入口、序列化、`DOTModality` 层次或
  `QuantizedMap::getSubMap()`。

用户本轮 prompt 覆盖默认 PI5 暂停规则：production direct 板卡结果若显示稳定收益，
worker 可以自行把 production patch 视为 adopted production behavior（已采纳生产行为），
并创建正式 `doc-rvv/recognition/dotmod_template_matching-RVV.zh.md`。若生产证据退化、
不稳定或 Evidence Doctor 出现阻塞 Error，则保留 patch 状态并整理为需要人工判断，不自行回滚。

## 当前状态清单

| item | current state |
| --- | --- |
| Phase 000 | 单窗口 direct-window 计分板卡 median `2.440x`，Evidence Doctor `Errors=0` / `Warnings=0` |
| Phase 010 | full-shaped diagnostic 板卡 median `2.124x`，Evidence Doctor `Errors=0` / `Warnings=0` |
| production source | `recognition/src/dotmod.cpp` 仍为原始 `getSubMap()` 标量路径 |
| build strategy | topic production-direct 二进制直接编译 `recognition/src/dotmod.cpp`、`quantizable_modality.cpp`、`mask_map.cpp`，不依赖已安装 `libpcl_recognition` |
| evidence registry | Phase 000 / Phase 010 summary、manifest、doctor 已登记；Phase 020 将新增 production-public 记录 |

## 假设与候选族

| candidate family | hypothesis | risk / unknown | status |
| --- | --- | --- | --- |
| `production-direct-window-rvv` | 真实 `DOTMOD::detectTemplates()` 中去掉每窗口 `getSubMap()` 拷贝并用 RVV 计数后，production public Std/RVV 仍正向 | 真实 `DOTModality` 子类、模板创建、detection 输出和 `responses` 分配可能改变收益幅度 | planned |
| `response-buffer-reuse` | 将 `responses` vector 移出窗口内层可能进一步降低分配成本 | 会改变生产代码形态，需与 direct-window 接入做 RVV-vs-RVV detail A/B | deferred until production direct result |

## Phase Scope 与扩展队列

| field | scope |
| --- | --- |
| `validated_scope` | `DOTMOD::createAndAddTemplate()` 创建真实 `templates_` 后，由固定测试 `DOTModality` 调用 `DOTMOD::detectTemplates()`；`uint8_t` organized quantized maps；多模板、多模态；threshold 输出 |
| `unvalidated_scope` | 真实 RGB-D workload 的模板分布、`ColorGradientDOTModality::computeInvariantQuantizedMap()` 生成的所有输入分布、`template_width_` 不能整除 `bin_size` 的语义外推、其它 DOTMOD 上游调用组合 |
| `point_type_expansion_queue` | not applicable；DOTMOD template matching 使用量化 byte map，不是模板点类型入口 |
| `phase_closeout_boundary` | 只能关闭 `DOTMOD::detectTemplates()` direct-window RVV 生产接入；不能关闭 DOTMOD color gradient modality、`QuantizedMap::getSubMap()` standalone 或 response-buffer-reuse |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `production-direct-window-rvv` | organized quantized byte map sliding windows | `uint8_t` row-major image/template layout | real `DOTMOD::detectTemplates()` public entry | `run_production_direct_test_compare` | `bench_dotmod_template_matching_production_direct_*` | 5-run `collect_production_direct_repeated_board` | production `dotmodScoreWindowDirectRVV` / inlined RVV instructions | production-public manifest + doctor | planned | TDD RED asm gate, production patch, validation |

## 实现和测试动作

| action | artifact / command | completion rule |
| --- | --- | --- |
| production-direct test helper | `include/impl/dotmod_template_matching_production_direct.hpp` | 测试用 `DOTModality` 通过真实 `createAndAddTemplate()` 和 `detectTemplates()` 构造入口 |
| RED asm gate | `make -C test-rvv/recognition/dotmod_template_matching dump_production_direct_bench_rvv` before patch | 反汇编中找不到 production RVV helper / `vcpop`，证明测试能抓住未接入状态 |
| production patch | `recognition/src/dotmod.cpp` | `__RVV10__` 构建中窗口计分走 direct RVV；非 RVV 构建保留原标量路径 |
| correctness | `make -C test-rvv/recognition/dotmod_template_matching run_production_direct_test_compare` | Std/RVV 两侧 production direct gtest 全通过 |
| asm | `make -C test-rvv/recognition/dotmod_template_matching dump_production_direct_bench_rvv` | `vle8`、`vand`、`vmsne`、`vcpop` 可归属到 production direct bench binary |
| board repeated | `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make -C test-rvv/recognition/dotmod_template_matching collect_production_direct_repeated_board BENCH_ARGS='256 192 24 16 100 5 8 2 0.9'` | 5-run summary decision bucket 决定是否采纳 |
| Evidence Doctor / registry | `record_production_direct_evidence_state` | Errors 必须为 0；Warnings 必须解释；registry fresh |

## 板卡复跑预算和决策桶

默认 5-run。`median speedup >= 1.20` 且 `B/A < 1` 为 `0/5` 时判为
`positive`；`1.05-1.20` 且 `B/A < 1` 为 `0/5` 时判为 `weak_positive`；
`0.95-1.05` 为 `neutral`；`<0.95` 为 `negative`。若方向跨桶或 checksum 不一致，
先修正或降级为 `unstable`；本轮不无限复跑。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production-public` after Phase 020 board; Phase 010 is only `production_shaped_diagnostic` |
| A/B boundary | real public `DOTMOD::detectTemplates()` compiled from current production source |
| 当前决策问题 | `RVV-vs-scalar` and `implementation-shape` |
| diagnostic 是否可外推到 production | Phase 010 只能支持进入 probe；Phase 020 必须以 production direct 结果为当前 truth |
| comparison-boundary / baseline mismatch 风险 | Phase 020 通过同一 test helper、同一 production source、同一输入构造，只改变 std/RVV 编译宏，降低 mismatch 风险 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已由 Phase 010 positive 允许；若 Phase 020 退化或不稳定，不能用 Phase 010 强行采纳 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前 DOTMOD 无既有 adopted RVV family；若继续尝试 `response-buffer-reuse`，需要同边界 RVV-vs-RVV A/B |

## 继续 / 停止条件

若 Phase 020 production-public repeated board 为 `positive` 或可接受的
`weak_positive`，且 Evidence Doctor 无 Error，本轮按用户授权采纳 production patch，
进入 S11 production closeout（生产文档收尾）并创建正式 `doc-rvv`。若结果为
neutral / negative / unstable，先判断 `response-buffer-reuse` 是否仍值得作为下一 phase；
没有未阻塞高价值方向时停止并整理不采纳原因。

## 文档更新清单

Phase 020 完成后更新：

- `doc/phases/020-production-integration-direct-window/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/dotmod_template_matching-evaluation.zh.md`
- `README.zh.md`
- `doc-rvv/recognition/dotmod_template_matching-RVV.zh.md`，仅在 production direct 证据支持采纳时创建
- `doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md`
- `tmp/rvv-work-logs/recognition/dotmod_template_matching/current-handoff/current-handoff.zh.md`
