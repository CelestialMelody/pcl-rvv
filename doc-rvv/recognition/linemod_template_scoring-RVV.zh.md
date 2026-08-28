# LINEMOD Template Scoring RVV 生产接入说明

## 当前状态

`recognition/src/linemod.cpp` 当前采用 `linearized-map-copy-rvv`。该路径接管默认宏配置下 `pcl::LINEMOD::matchTemplates`、`pcl::LINEMOD::detectTemplates` 和 `pcl::LINEMOD::detectTemplatesSemiScaleInvariant` 中 `EnergyMaps -> LinearizedMaps` 的 8x8 offset map 拷贝；public API（公开接口）、template 管理、energy map generation（能量图生成）、score accumulation（分数累加）、threshold scan（阈值扫描）、detection 输出、NMS（非极大值抑制）和 averaged detection（邻域加权检测位置）保持原有形态。

采纳依据是 Phase 070 / 080 production-public（真实公开入口）板卡 repeated evidence（重复板卡证据）。Std/RVV 两侧都链接当前源码，并真实调用公开入口：

| case | 入口 | runs | median speedup | range | B/A < 1 | checksum |
| --- | --- | ---: | ---: | --- | ---: | --- |
| `LINEMOD production matchTemplates total` | `LINEMOD::matchTemplates()` | 5 | `1.138x` | `1.133x` - `1.173x` | `0/5` | Std/RVV 一致：`5083644600596343113` |
| `LINEMOD production detectTemplates total` | `LINEMOD::detectTemplates()` | 5 | `1.129x` | `1.111x` - `1.141x` | `0/5` | Std/RVV 一致：`5083644600596343113` |
| `LINEMOD production semi-scale detectTemplates total` | `LINEMOD::detectTemplatesSemiScaleInvariant()` | 5 | `1.136x` | `1.130x` - `1.141x` | `0/5` | Std/RVV 一致：`5083644600596343113` |

证据路径：

- production-public summary: `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-summary.md`
- Evidence Doctor（证据体检）: `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-evidence-doctor.md`
- Phase 070 result: `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/result.zh.md`
- semi-scale production-public summary: `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-summary.md`
- semi-scale manifest: `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-manifest.json`
- semi-scale Evidence Doctor: `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-doctor.md`
- semi-scale Evidence Doctor JSON: `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-doctor.json`
- Phase 080 result: `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/result.zh.md`
- 函数级评估: `test-rvv/recognition/linemod_template_scoring/doc/linemod_template_scoring-evaluation.zh.md`

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论只来自板卡 repeated summary。

## 函数语义和标量路径

LINEMOD 的默认匹配链路先让每个 modality（模态，提供量化图的输入通道）生成 8 个 energy map。随后源码把每个 energy map 按 `step_size=8` 拆成 64 个 linearized map。每个 linearized map 对应一个 `(map_col, map_row)` offset；后续 score accumulation 通过 template feature 的 `x/y` offset 直接读取连续内存位置。

原标量拷贝对每个 bin 执行：

1. 遍历 `map_row` 和 `map_col`，选择一个 output linearized map。
2. 对 output 的 `lin_height * lin_width` 元素逐个计算源位置：
   `source_row = row_index * step_size + map_row`，
   `source_col = col_index * step_size + map_col`。
3. 从 `energy_map[source_row * width + source_col]` 读取一个 `unsigned char`，写入连续的 `linearized_map[row_index * lin_width + col_index]`。

这段拷贝本身不改变分数、不做阈值判断、不构造 detection；它只是把后续打分要访问的 8x8 offset 数据重排为连续布局。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| production dispatch（生产分流） | adopted | `linearizeEnergyMap()` 在 `__RVV10__` 且 `<riscv_vector.h>` 可用时调用 RVV helper，否则调用 Std helper | production-direct tests、asm、board | 公开入口只替换默认 copy loop |
| 标量 fallback（回退路径） | adopted | `linearizeEnergyMapStd()` 保留原三层循环语义，非 RVV 构建自然回到标量 | Std build correctness | 不需要修改 public header |
| RVV stride copy（跨步拷贝） | adopted | 对每个 output row 用 `vlse8.v` 按 `step_size` 跨步读取 source，再用 `vse8.v` 连续写回 target | Phase 070 board median `1.138x` / `1.129x`；Phase 080 semi-scale median `1.136x` | 只处理默认单套 energy map |
| score accumulation | attempted-negative | Phase 020 isolated board median `0.862x` 且 5/5 退化 | topic-local optimization evidence | 当前不接 production |
| threshold / max scan | attempted-negative | Phase 010 scan median `0.601x`，combined median `0.879x`，均 5/5 退化 | topic-local optimization evidence | 若重试需新 compress / index staging 方案 |
| energy map generation | attempted-negative | Phase 030 median `0.954x`，5/5 退化 | topic-local optimization evidence | 当前不接 production |
| semi-scale | adopted | 复用同一 copy helper，不改变后续 scaled feature offset 读取语义；Phase 080 已补 production direct 证据 | Phase 080 board median `1.136x` | 只覆盖默认单套 energy map |
| separate-energy | deferred | 默认宏未启用，四套 energy map 编译分支需要单独对拍 | roadmap | 只有实际 build 需要该宏时另起 phase |

RVV helper 的核心流程：

```cpp
// dispatch: non-RVV build -> linearizeEnergyMapStd; RVV build -> linearizeEnergyMapRVV.
// RVV chunk: for each map offset and output row, vlse8.v loads every step_size byte.
// Store: vse8.v writes the chunk into the contiguous linearized output row.
// Tail: vsetvl chooses the remaining lane count, so non-multiple row widths need no scalar tail.
```

`step_size` 当前由公开入口固定为 8。helper 仍按参数计算 `lin_width` 和 `lin_height`，但正式证据只批准默认入口使用方式。

## 覆盖范围与 fallback

| 范围 | 当前状态 | 证据 / 原因 |
| --- | --- | --- |
| `LINEMOD::matchTemplates()` 默认宏路径 | adopted | Phase 070 production-public board median `1.138x` |
| `LINEMOD::detectTemplates()` 默认宏路径 | adopted | Phase 070 production-public board median `1.129x` |
| `LINEMOD::detectTemplatesSemiScaleInvariant()` 默认宏路径 | adopted | Phase 080 production-public board median `1.136x` |
| `__RVV10__` 未启用或无 `<riscv_vector.h>` | scalar fallback | 条件编译只保留 Std helper |
| `LINEMOD_USE_SEPARATE_ENERGY_MAPS` | scalar-only for this patch | 四套 energy / linearized maps 不在 Phase 070 / 080 范围 |
| NMS / averaged detection | unchanged | RVV helper 不触碰 detection 后处理 |
| PCL 点类型 / `Scalar` | not_applicable with evidence | LINEMOD 此段处理 `unsigned char` map，不是点云模板计算 |

## 正确性与高效性证据链

| 证据层 | 当前结果 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness | `run_production_direct_test_compare` 中 Std/RVV 两侧 production-direct tests 通过 | 当前源码公开入口可运行，tail 规模下 detection 输出稳定 | 不证明板卡性能 |
| QEMU path | QEMU 日志只作为构建、路径和输出形状证据 | RVV / Std 二进制均可执行 | QEMU timing 不作为性能结论 |
| asm（反汇编） | `dump_production_direct_bench_rvv` 定位到 `linearizeEnergyMapRVV`，并看到 `vlse8.v` / `vse8.v` | production-linked RVV binary 命中预期跨步拷贝指令 | 不能单独证明端到端收益 |
| board repeated | `matchTemplates 1.138x`、`detectTemplates 1.129x`、`semi-scale 1.136x`，均 0/5 退化 | 当前 public RVV path 快于当前 public scalar path | 不覆盖 separate-energy 或新 RVV family |
| Evidence Doctor | `Errors=0 / Warnings=0 / Suggestions=0` | 当前 summary 无脚本发现的阻塞异常 | reviewer 仍需按源码和文档边界复核 |

## Bench case 说明

Phase 070 production-direct bench 使用测试专用 `FixedQuantizableModality` 构造稳定 quantized map，并通过 `makeProductionLinemod()` 建立一个模板。计时边界包含公开入口里的 energy map generation、linearized copy、score accumulation、threshold scan 和 detection 构造。它故意不暴露内部 linearized checksum，因为正式判断要回答公开入口 wall time 是否有收益。

| case | 数据 | 计时边界 | 证明点 |
| --- | --- | --- | --- |
| `LINEMOD production matchTemplates total` | `mem_size=4096`、`nr_features=96`、单 modality synthetic quantized map | `matchTemplates()` 完整公开入口 | 单最佳 detection 公开入口仍受益 |
| `LINEMOD production detectTemplates total` | 同上 | `detectTemplates()` 默认完整公开入口 | threshold scan 后多 detection 输出公开入口仍受益 |
| `LINEMOD production semi-scale detectTemplates total` | 同上，scale 参数为 `1.0f, 2.0f, 2.0f` | `detectTemplatesSemiScaleInvariant()` 默认完整公开入口 | 半尺度不变入口在逐 scale scalar accumulation / scan 保持不变时仍受益 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `LINEMOD::matchTemplates` | production public entry | 默认匹配入口 | 用户代码 / LINEMOD wrapper | `EnergyMaps`、`LinearizedMaps`、score accumulation | production boundary | `recognition/src/linemod.cpp` |
| `LINEMOD::detectTemplates` | production public entry | 默认检测入口 | 用户代码 / LINEMOD wrapper | `EnergyMaps`、`LinearizedMaps`、threshold scan | production boundary | `recognition/src/linemod.cpp` |
| `LINEMOD::detectTemplatesSemiScaleInvariant` | production public entry | 半尺度不变检测入口 | 用户代码 / LINEMOD wrapper | `EnergyMaps`、`LinearizedMaps`、scale offset scan | production boundary | `recognition/src/linemod.cpp` |
| `linearizeEnergyMapStd` | production Std helper | 原 offset copy 标量事实来源 | `linearizeEnergyMap` fallback | `LinearizedMaps` | fallback source of truth | `recognition/src/linemod.cpp` |
| `linearizeEnergyMapRVV` | production RVV helper | 跨步加载、连续写回 | `linearizeEnergyMap` dispatch | `LinearizedMaps` | adopted production RVV path | `recognition/src/linemod.cpp` |
| `test_linemod_template_scoring_production_direct.cpp` | correctness source | 真实公开入口 smoke | `run_production_direct_test_compare` | gtest assertions | correctness gate | `test-rvv/recognition/linemod_template_scoring/src/test_linemod_template_scoring_production_direct.cpp` |
| `bench_linemod_template_scoring_production_direct.cpp` | bench wrapper | Phase 070 / 080 公开入口计时 | board repeated target | summary / manifest | production-public performance input | `test-rvv/recognition/linemod_template_scoring/src/bench_linemod_template_scoring_production_direct.cpp` |
| `generate_linemod_template_scoring_evidence_manifest.py` | analysis script | 生成 Phase 070 / 080 summary / manifest | `record_production_direct_evidence_state` / `record_semiscale_production_direct_evidence_state` | Evidence Doctor / registry | evidence metadata | `test-rvv/recognition/linemod_template_scoring/script/generate_linemod_template_scoring_evidence_manifest.py` |
| Phase 070 summary | evidence output summary | 生产接入后板卡统计 | board repeated target | evaluation / 本文 | board performance | `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-summary.md` |
| Phase 070 result | phase result | 生产接入闭环审计 | PI2-PI5 | evaluation / 本文 | adoption audit | `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/result.zh.md` |
| Phase 080 summary | evidence output summary | semi-scale 生产接入后板卡统计 | board repeated target | evaluation / 本文 | board performance | `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-summary.md` |
| Phase 080 result | phase result | semi-scale 扩展审计 | Phase 080 plan | evaluation / 本文 | adoption audit | `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/result.zh.md` |

## Production Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production patch | adopted | 用户确认接入后板卡有收益即可采纳；Phase 070 / 080 public entry board positive |
| public API | unchanged | 只修改 `recognition/src/linemod.cpp` 内部 helper 和条件编译 |
| fallback | adopted | 非 RVV 构建调用 `linearizeEnergyMapStd()` |
| 文档归属 | adopted | 本文保存长期 production 行为；topic-local evaluation / phase docs 保存候选取舍和阶段审计 |
| 回滚边界 | simple source-local revert | 若后续证据反转，回滚 `linemod.cpp` 内部 helper 和两处默认 copy loop 调用即可恢复标量结构 |

## 遗留风险与后续条件

- `LINEMOD_USE_SEPARATE_ENERGY_MAPS` 默认未启用。若目标 build 使用该宏，需要单独证明四套 energy / linearized maps 的语义和性能。
- 当前 production-public positive 只证明当前 RVV path 快于当前 scalar path。若新增多 bin fused copy、separate-energy 特化或其它 RVV family，需要同一 production boundary 内的 RVV-vs-RVV A/B。
