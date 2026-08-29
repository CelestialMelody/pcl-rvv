# DOTMOD Template Matching Evaluation

## 范围和结论

目标源码是 `recognition/src/dotmod.cpp`，目标入口是
`pcl::DOTMOD::detectTemplates()`。当前结论为 adopted production behavior（已采纳生产行为）：
RVV（RISC-V Vector，可变长度向量扩展）构建下直接从 `QuantizedMap` 原图窗口读取并计数，
同时复用 `responses` 缓冲；非 RVV 构建保持原 `getSubMap()` 标量路径。

当前 production-public（真实公开入口）板卡证据为 `positive`：5-run median speedup
`3.298x`，range `3.276x - 3.304x`，`B/A < 1 = 0/5`。Evidence Doctor（证据体检）
为 `Errors=0`、`Warnings=0`、`Suggestions=2`。

## 函数语义

`DOTMOD::detectTemplates()` 从每个 `DOTModality` 读取 dominant quantized map（主量化图），
随后对每个 row/col 滑窗、每个 modality（模态）和每个 template（模板）执行 byte map 匹配。
原标量路径先调用 `QuantizedMap::getSubMap()` 形成窗口副本，再遍历
`image_data[data_index] & template_data[data_index]`。命中数累加到 `responses[template_index]`，
乘以 `1 / (template_bins_x * template_bins_y)` 后与 threshold（阈值）比较，超过阈值时按原顺序
追加 `DOTMODDetection`。

## 标量路径与 RVV 路径对照

| 阶段 | 标量路径 | RVV 路径 | 证据 |
| --- | --- | --- | --- |
| map 获取 | 每个 modality 读取 `getDominantQuantizedMap()` | 相同 | production direct gtest |
| 窗口读取 | 非 RVV 构建每个 modality/window 调用 `getSubMap()` | RVV 构建直接用原图 row-major 数据、window x/y 和 image width 计算窗口行地址 | Phase 020/030 production patch |
| 窗口计分 | 标量逐 byte 做 `&` 和非零计数 | 每个 VL chunk 连续加载 image/template byte，`vand` 后 `vmsne` 生成 mask，再用 `vcpop` 计数 | asm 和 board summary |
| response 缓冲 | `responses` 在窗口循环外分配，每个窗口 `std::fill` 清零 | 相同 | Phase 030 correctness |
| threshold / output | 标量比较 `response > threshold` 并按 template 顺序追加 detection | 相同，保持标量 tail（标量尾段） | production direct gtest 和 checksum |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `pcl::DOTMOD::detectTemplates()` | production public entry | 真实检测入口，承载 RVV/标量分流 | DOTMOD caller | `dotmodScoreWindowDirectRVV()` 或原 `getSubMap()` 路径 | production boundary | `recognition/src/dotmod.cpp` |
| `dotmodScoreWindowDirectRVV()` | production RVV helper | 对一个窗口和一个模板做 byte AND + popcount | `detectTemplates()` RVV 分支 | RVV intrinsic | asm attribution | `recognition/src/dotmod.cpp` |
| `FixedDOTModality` | production direct test support | 提供稳定 `QuantizedMap`，仍走真实 `createAndAddTemplate()` | production direct gtest / bench | `DOTMOD::createAndAddTemplate()` | correctness gate | `test-rvv/recognition/dotmod_template_matching/include/impl/dotmod_template_matching_production_direct.hpp` |
| `run_production_direct_test_compare` | correctness target | 对拍 Std/RVV 真实入口输出 | Makefile | QEMU / run command | production direct correctness | `test-rvv/recognition/dotmod_template_matching/Makefile` |
| `bench_dotmod_template_matching_production_direct.cpp` | bench wrapper | 计时真实 `detectTemplates()` total | board repeated target | summary/manifest script | production-public performance | `test-rvv/recognition/dotmod_template_matching/src/bench_dotmod_template_matching_production_direct.cpp` |
| `generate_dotmod_template_matching_evidence_manifest.py` | analysis script | 从 board logs 生成 summary 和 manifest | evidence targets | Evidence Doctor / registry | evidence manifest | `test-rvv/recognition/dotmod_template_matching/script/generate_dotmod_template_matching_evidence_manifest.py` |
| `summary.md` | evidence output summary | 保存 production-public 5-run speedup | board collect target | evaluation / doc-rvv / Handoff | board performance | `test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase020_production_direct/summary.md` |
| `dotmod_template_matching-RVV.zh.md` | production topic doc | 维护 adopted production 行为和证据链 | reviewer / maintainer | 源码、测试和证据路径 | long-term production docs | `doc-rvv/recognition/dotmod_template_matching-RVV.zh.md` |

## 测试与证据计划

| target / artifact | 层级 | 作用 | 当前结果 |
| --- | --- | --- | --- |
| `run_test_compare` | production-shaped diagnostic | 对拍 test-only direct-window 和 full-shaped helper | Std/RVV 4 个 gtest 通过 |
| `run_production_direct_test_compare` | production direct correctness | 真实 `DOTMOD::detectTemplates()` 输出和 strict threshold 语义 | Std/RVV 各 2 个 gtest 通过 |
| `dump_production_direct_bench_rvv` | asm attribution（反汇编归属） | 证明 production direct bench binary 含目标 RVV helper 和指令 | `dotmodScoreWindowDirectRVV`、`vle8`、`vand`、`vmsne`、`vcpop` present |
| `collect_production_direct_repeated_board` | board performance（板卡性能） | 5-run 真实入口性能采集 | median `3.298x`，`0/5` 退化 |
| `record_production_direct_evidence_state` | Evidence Doctor / registry | 生成 manifest、doctor 并登记当前 evidence | `Errors=0`、`Warnings=0`、`Suggestions=2` |

QEMU（仿真器）只用于 correctness、构建、路径和日志形状，不作为性能证据。

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | adopted | `__RVV10__ && __riscv_vector` 包住 RVV include、helper 和分支；非 RVV 构建自然走原标量路径 | 没有修改 public API |
| direct-window RVV score | adopted | Phase 020/030 correctness、asm、board | 只覆盖 row-major byte map 和 contiguous template features |
| response-buffer-reuse | adopted | Phase 030 correctness 和 board summary | 只改变窗口级 buffer 生命周期，不改变 threshold / output |
| threshold output tail | scalar-only | production direct gtest 锁住 `>` 阈值和 detection 字段范围 | 若真实 workload 中 detection 输出成为热点，可另起 phase |
| `QuantizedMap::getSubMap()` standalone | not_applicable for adopted path | RVV 构建已绕开该调用，非 RVV 构建保留 | 其它 caller 需要时另开 support-kernel topic |

## 生产接入后的最终证据更新

`production_patch_scope`: `recognition/src/dotmod.cpp` 新增 internal RVV helper，并在
`detectTemplates()` 的 modality/template 计分处用 RVV direct-window path 替代 RVV 构建下的
`getSubMap()` 路径；`responses` vector 移出窗口循环并用 `std::fill` 重置。

`covered_path`: 真实 `DOTMOD::createAndAddTemplate()` 写入模板状态后，真实
`DOTMOD::detectTemplates()` 处理 `uint8_t` organized quantized maps、多模板、多模态和 threshold 输出。

`fallback_matrix`: 非 RVV 构建、未定义 `__riscv_vector` 的构建、其它不经过本入口的 DOTMOD preprocessing
和 standalone `getSubMap()` caller 都不命中本 RVV helper。当前函数没有点类型模板或 `Scalar=double`
分支需要扩展。

`production_direct_tests`: `run_production_direct_test_compare` 通过 Std/RVV 两侧各 2 个 gtest。

`production_asm`: `dump_production_direct_bench_rvv` 和证据登记 target 均显示
`dotmodScoreWindowDirectRVV`，并命中 `vle8`、`vand`、`vmsne`、`vcpop`。

`production_board_bench`: `log/board/repeated_phase020_production_direct/summary.md`
记录当前 post-Phase030 5-run median `3.298x`。

`decision_delta`: Phase 000/010 的 diagnostic evidence（诊断证据）先证明值得进入生产探针；
Phase 020/030 的 production direct evidence（真实生产路径证据）确认收益并收窄为当前 adopted 范围。

## 正确性与高效性证据链

Correctness（正确性）：production direct tests 通过真实 `createAndAddTemplate()` 和
`detectTemplates()`，覆盖 detection 非空、字段范围、score 超阈值和 strict threshold `>` 边界。

Path / asm（路径 / 反汇编）：RVV bench binary 中可归属到 `dotmodScoreWindowDirectRVV()` 的调用和目标
RVV 指令存在。该证据证明路径命中，不代表性能。

Performance（性能）：性能结论只引用板卡 repeated summary。当前 `dotmod_production_detecttemplates_total`
median `3.298x`，所有 5 run 均为正向。

Boundary（边界）：结论只覆盖当前 synthetic fixed modality 构造的 production-public 输入边界和
`uint8_t` row-major 数据布局。它不外推到真实 RGB-D 模板分布、其它尺寸、其它 `bin_size` 组合或 modality 预处理。

Risk（风险）：Evidence Doctor suggestion 要求后续补 taskset/governor/freq/temperature 和 binary hash。
这些 metadata 缺失不会推翻当前稳定正向桶；若后续出现方向反转或长尾，应先补 metadata 后重跑。

## Continue / Stop Decision

`current_decision`: adopted production behavior。

`stop_condition_hit`: 当前 matrix 和 roadmap 在本 topic 授权边界内没有高优先级未阻塞动作。

`followup_options_for_user`: 默认进入 reviewer 审查或提交准备。若继续当前 topic，建议先提供真实 workload
profile 或新的尺寸矩阵；若优化 `QuantizedMap::getSubMap()`、threshold output staging 或 modality preprocessing，
建议另开 topic 或新 phase 并重新建立证据边界。
