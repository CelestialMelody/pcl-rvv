# Phase 030 Plan: energy-map-generation

## 阶段意图和边界

本阶段建立 `detectTemplates` / `detectTemplatesSemiScaleInvariant` 默认路径里的 energy map generation（能量图生成）production-shaped diagnostic（生产形态诊断）。输入是一段 `QuantizedMap` 等价的 `u8` 字节流，输出是 8 个 bin（量化方向桶）的 `u8` energy map；每个输出元素按 `val0`、`val1`、`val2`、`val3` 四个 mask（掩码）命中次数累计，取值范围是 0..4。

本阶段不修改 `recognition/src/linemod.cpp`，不覆盖 `LINEMOD_USE_SEPARATE_ENERGY_MAPS` 编译分支，不证明 `matchTemplates` 中优先级敏感的旧 mask 表达式，也不证明 `LinearizedMaps` 拷贝、score accumulation（分数累加）、threshold scan（阈值扫描）、NMS（非极大值抑制）、averaged detection（邻域加权检测）或真实 production dispatch（生产分流）。

## 当前状态清单

| item | current state |
| --- | --- |
| source | `recognition/src/linemod.cpp` 的 `detectTemplates` 和 `detectTemplatesSemiScaleInvariant` 使用 `%8` 生成 `val0..val3`，然后对 `width * height * 8` 做字节 mask 命中累计 |
| previous phases | Phase 010 / 020 已证明 conservative score scan 和 accumulation-only helper 在板卡上均为 attempted-negative |
| topic assets | `test-rvv/recognition/linemod_template_scoring` 已有 `src/`、`include/impl`、phase docs、manifest wrapper 和 evidence registry |
| board availability | 当前 prompt 明确板卡可用；correctness、asm 和日志形状通过后继续 5-run repeated board |
| production state | 当前 phase 不接入 production；若本阶段正向，后续仍需 production integration loop |

## 假设与候选族

| hypothesis | expected signal | evidence needed |
| --- | --- | --- |
| `u8` mask-to-energy RVV 有收益 | 每个 bin 对完整字节流做向量按位与、非零比较并累计 0/1/2/3/4，减少逐元素四分支开销 | same-chain correctness、非整向量 tail、asm 中 `vand` / compare / add / store、5-run board repeated、Evidence Doctor |
| 8-bin 多次扫图会被内存流量或 `vsetvl` 抵消 | correctness 通过但 board median speedup < 0.97，或 Evidence Doctor 报稳定退化 | repeated summary、doctor、asm 归因 |
| full-chain 需要先做成本拆分 | energy map 子核若中性或负向，继续到 linearized map copy / profile，而不是直接 no-production | roadmap / matrix 更新 |

## 优化矩阵

| candidate | scope | correctness | bench | asm | board | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| energy map generation RVV | `u8` quantized map -> 8 个 `u8` energy maps，默认非 separate-energy 语义，tail included | planned RED/GREEN | planned dedicated timing item | planned dump | planned 5-run budget | planned | planned |

## 实现和测试动作

| action | artifact | expected evidence | completion |
| --- | --- | --- | --- |
| RED：新增 energy map correctness | `src/test_linemod_template_scoring.cpp` | `buildEnergyMapsStd/RVV` 缺失导致编译失败 | failure observed before helper edit |
| GREEN：实现标量 / RVV helper | `include/impl/linemod_template_scoring_candidates.hpp` | 8 bins、空 map、tail、所有 bit pattern 与标量 reference 一致 | `run_test_compare` pass |
| 扩展 bench harness | `src/bench_linemod_template_scoring.cpp` | 输出 `LINEMOD energy map generation` timing 和 energy checksum；默认 bench 仍保留旧三项输出 | parseable logs |
| 增加 Phase 030 Make targets | `Makefile` | collect / manifest / doctor / registry / freshness target 指向 Phase 030 证据路径 | target exists and runs |
| 扩展 manifest wrapper | `script/generate_linemod_template_scoring_evidence_manifest.py` | 可解析 energy timing、energy checksum 和新的 boundary metadata | summary + manifest |
| 反汇编检查 | `make dump_energy_map_bench_rvv` | RVV 指令能归属到 energy helper 或 inline candidate | asm summary |
| 板卡 repeated | 5-run budget | 判断 positive / weak_positive / neutral / negative / unstable | summary + doctor |

## Evidence Doctor 和 Registry

本阶段 summary 主归属预计为 `doc/phases/030-energy-map-generation/energy-map-repeated-summary.md`，manifest 为 `energy-map-repeated-evidence-manifest.json`，Evidence Doctor 输出为 `energy-map-repeated-evidence-doctor.md/json`。正式性能结论只来自 board repeated；QEMU bench 仅允许作为日志形状 smoke（冒烟检查），不参与 speedup 判断。

registry 使用 `phase030-energy-map-repeated-20260828` run label 登记。若 Evidence Doctor 报 checksum mismatch、metadata missing 或稳定退化 Error，result 必须解释并降级当前候选边界，不能进入 production integration loop。

## 阶段完成条件

- RED 编译失败已观察，GREEN 后 Std/RVV correctness 在 QEMU 或当前后端通过。
- Bench 输出保持旧 score 三项兼容，并新增 energy map timing / checksum 字段。
- 反汇编能看到 energy map RVV 路径的向量 load、按位与、比较、加法和 store；若 helper 被 inline，应记录 inline boundary（内联边界）。
- 板卡 5-run 完成，Std/RVV energy checksum 一致，Evidence Doctor 已运行。
- result 回填本阶段动作表、诊断到 production 错配审计、matrix、roadmap、evaluation、README 和筛选队列。

## 板卡复跑预算和决策桶

默认 5-run。`median speedup >= 1.05` 为 `positive`，`1.00-1.05` 为 `weak_positive`，`0.97-1.00` 为 `neutral`，`<0.97` 为 `negative`；若方向跨越 1 或 Evidence Doctor 指出影响决策桶的长尾，最多再做一次同边界确认复跑。预算耗尽后仍跨桶摇摆则标为 `unstable`，交给 reviewer / 用户判断。

## 继续 / 停止条件

板卡可用时，本阶段不因“需要板卡验证”停止。若 correctness、构建、ssh/rsync、Evidence Doctor Error 无法修正、证据矛盾或 dirty isolation 不安全，则输出 Handoff。若 Phase 030 正向，下一步是 production integration eligibility audit（生产接入适用性审计），不是自动修改 production。若 Phase 030 中性或负向，默认继续到 `040-linearized-map-copy-ablation` 或 full-chain timing split，除非 roadmap / matrix 已无授权未阻塞动作。

## 文档更新清单

更新 `README.zh.md`、`doc/linemod_template_scoring-evaluation.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、本 phase result、筛选队列和 Handoff。没有 adopted production behavior 或 PI5 生产证据闭环，因此不创建 `doc-rvv/recognition/linemod_template_scoring-RVV.zh.md`。

## Roadmap 同步动作

Phase result 必须把 energy map candidate 写成 `attempted-positive`、`attempted-negative`、`neutral`、`unstable` 或 `blocked`。若正向，roadmap 新增 production probe 前置条件；若负向或中性，roadmap 默认提升 linearized map copy / full-chain timing split。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` |
| 当前决策问题 | `RVV-vs-scalar` for default energy map generation |
| diagnostic 是否可外推到 production | `unknown`；只覆盖 `QuantizedMap` 字节流到默认合并 energy map，不覆盖 modality 对象、linearized copy、score accumulation、NMS、averaging 或真实 dispatch |
| comparison-boundary / baseline mismatch 风险 | `yes`；bench 直接传入 synthetic quantized bytes，省略 `QuantizableModality::getSpreadedQuantizedMap` 和 `EnergyMaps` 类分配 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `yes`，但必须先有 production-shaped full-chain profile 证明 energy map 是真实入口热点，并且 production patch 能保持默认 / separate-energy 分支边界 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前无已采用 RVV family；若后续出现 table-lookup、single-pass multi-bin 或 separate-map 多实现族，需要同边界 RVV-vs-RVV A/B |
