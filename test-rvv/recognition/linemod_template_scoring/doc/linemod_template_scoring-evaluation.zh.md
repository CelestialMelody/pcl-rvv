# LINEMOD Template Scoring 函数级评估

## S2 函数级评估

`pcl::LINEMOD::matchTemplates`、`detectTemplates` 和 `detectTemplatesSemiScaleInvariant` 都会先从每个 modality（模态，提供量化图的输入通道）生成 8 个 energy map（能量图），再把 8x8 step 的像素布局转换成 linearized map（线性化图，用于按模板偏移直接扫连续内存）。打分主循环按 template feature（模板特征）和 bin（量化方向位）选择对应 linearized map，把每个候选位置的 `unsigned char` 分数累加到 `unsigned short score_sums`。

Phase 000 覆盖了 score accumulation（分数累加）局部核，但后续同边界复跑改变了证据方向。Phase 010 增加 threshold scan（阈值扫描）后，score accumulation、score scan 和 accumulate+scan 三项在板卡上均为 5/5 退化；Phase 020 又用编译期隔离的 accumulation-only bench 复核，score accumulation 仍为 5/5 退化。Phase 030 覆盖默认合并 energy map generation（能量图生成），board repeated median `0.954x` 且 5/5 退化。Phase 040 覆盖 8x8 linearized map 拷贝，board repeated median `2.160x` 且 0/5 退化。Phase 050 把该 copy-only 候选放回 full-chain split（完整链路分段）后，full-chain total median `1.565x`、linearized copy median `1.790x`，二者均 0/5 退化且 Evidence Doctor（证据体检）为 `Errors=0 / Warnings=0 / Suggestions=0`。non-max suppression（非极大值抑制）和 averaged detection（邻域加权检测位置）仍未进入 production 接入。

Phase 070 已完成 production integration loop（生产接入闭环）：默认宏路径下 `matchTemplates` 和 `detectTemplates` 的 `EnergyMaps -> LinearizedMaps` copy loop 已接入 `linearizeEnergyMapRVV`。接入后的 production-public（真实公开入口）板卡 5-run 结果为 positive：`matchTemplates` median `1.138x`，`detectTemplates` median `1.129x`，两项均 0/5 退化，Evidence Doctor 为 `Errors=0 / Warnings=0 / Suggestions=0`。

Phase 080 继续覆盖默认宏路径下 `detectTemplatesSemiScaleInvariant` 的同类 copy loop，并复用同一个 production helper。接入后的 semi-scale production-public 板卡 5-run 结果为 positive：median `1.136x`，range `1.130x` - `1.141x`，0/5 退化，Evidence Doctor 为 `Errors=0 / Warnings=0 / Suggestions=0`。

## 初步判断

该 topic 是 `doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md` 的第一条建议主题。当前判断为 `adopted production behavior`：score accumulation、conservative scan 和 energy map generation 不进入 production；linearized map copy 已在默认宏下接入 `matchTemplates` / `detectTemplates` / `detectTemplatesSemiScaleInvariant`，并由 Phase 070 / 080 production-public 板卡数据确认有收益。separate-energy、NMS 和 averaged detection 未被本次采纳。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 000-050 为 production-shaped diagnostic；Phase 070 / 080 为 production-public。 |
| A/B boundary | Phase 070 / 080 使用 `public_overload_current_source`，Std/RVV 两侧真实调用当前源码公开入口。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path，以及是否采纳 production patch。 |
| diagnostic 是否可外推到 production | 最终不依赖外推；Phase 070 接入后板卡证据是采纳依据。 |
| comparison-boundary / baseline mismatch 风险 | Phase 070 / 080 已降低默认入口错配风险；仍不覆盖 separate-energy、NMS 或 averaged detection。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已完成 bounded probe；结果为 positive。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前只有一个 production RVV family，不是 family selection；后续新增 family 时需要同边界 RVV-vs-RVV A/B。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `pcl::LINEMOD::detectTemplates` | production public entry | 真实检测入口，包含 energy map、score accumulation、threshold 和 detection 输出 | 用户代码 / `LineRGBD` wrapper | `EnergyMaps`、`LinearizedMaps`、score scan | production boundary | `recognition/src/linemod.cpp` |
| `pcl::LINEMOD::detectTemplatesSemiScaleInvariant` | production public entry | 半尺度不变检测入口，逐 scale 复用默认 energy / linearized map 形态 | 用户代码 | `EnergyMaps`、`LinearizedMaps`、scale offset scan | production boundary | `recognition/src/linemod.cpp` |
| `linearizeEnergyMapStd` | production Std helper | 原 `EnergyMaps -> LinearizedMaps` 标量拷贝事实来源 | `matchTemplates` / `detectTemplates` / `detectTemplatesSemiScaleInvariant` 默认路径 | `LinearizedMaps` | fallback source of truth | `recognition/src/linemod.cpp` |
| `linearizeEnergyMapRVV` | production RVV helper | `vlse8.v` 跨步加载 energy map，`vse8.v` 连续写回 linearized map | `linearizeEnergyMap` dispatch | `LinearizedMaps` | adopted production RVV path | `recognition/src/linemod.cpp` |
| `linemod_score_accumulate_*` / `scanScores*` | candidate helper | 测试专用 score accumulation 与 threshold scan 标量 / RVV 对拍 | `src/test_linemod_template_scoring.cpp`、bench | 内部 RVV intrinsic 或标量循环 | production-shaped diagnostic correctness | `include/impl/linemod_template_scoring_candidates.hpp` |
| `LINEMODTemplateScoring.*` tests | correctness gate | 验证 u8 map 累加到 u16、尾段、max tie-break、threshold strict 和候选 index 顺序 | `make run_test_compare` | helper | QEMU correctness / 本地构建证据 | `src/test_linemod_template_scoring.cpp` |
| `generate_linemod_template_scoring_evidence_manifest.py` | analysis script | 把 repeated board logs 转成 Evidence Doctor manifest 和 summary，支持三项 bench 和 scan checksum 字段 | `make run_score_scan_evidence_doctor`、`make run_accumulation_ablation_evidence_doctor` | `test-rvv/script/evidence_doctor.py`、`evidence_registry.py` | evidence boundary / freshness | `script/generate_linemod_template_scoring_evidence_manifest.py` |
| Phase 010 summary / doctor | evidence output summary | 保存三项 score bench 的 5-run 负向统计和 doctor finding | evaluation / Handoff / matrix | reviewer | production-shaped diagnostic board evidence | `doc/phases/010-threshold-scan-and-detection-order/score-scan-repeated-summary.md` |
| Phase 020 summary / doctor | evidence output summary | 保存 accumulation-only 消融的 5-run 负向统计和 doctor finding | evaluation / Handoff / matrix | reviewer | production-shaped diagnostic board evidence | `doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-summary.md` |
| Phase 030 summary / doctor | evidence output summary | 保存默认 energy map generation 的 5-run 负向统计和 doctor finding | evaluation / Handoff / matrix | reviewer | production-shaped diagnostic board evidence | `doc/phases/030-energy-map-generation/energy-map-repeated-summary.md` |
| Phase 040 summary / doctor | evidence output summary | 保存 linearized map copy 的 5-run 正向统计和 doctor finding | evaluation / Handoff / matrix | reviewer | production-shaped diagnostic board evidence | `doc/phases/040-linearized-map-copy-ablation/linearized-map-copy-repeated-summary.md` |
| Phase 050 summary / doctor | evidence output summary | 保存 full-chain split 中 linearized-copy-only 候选的 5-run 正向统计和 doctor finding | evaluation / Handoff / matrix | reviewer | production-shaped diagnostic board evidence for PI1 | `doc/phases/050-full-chain-timing-and-production-eligibility/full-chain-split-repeated-summary.md` |
| Phase 060 PI1 plan | phase plan | 冻结默认宏下 `matchTemplates` / `detectTemplates` 的 production patch 候选范围和 PI2-PI5 证据计划 | 下一轮 worker / reviewer | production source only after authorization | production integration planning, no production evidence yet | `doc/phases/060-pi1-production-integration-plan/plan.zh.md` |
| Phase 070 summary / doctor | evidence output summary | 保存接入后真实公开入口的 5-run 板卡统计和 doctor 结果 | evaluation / Handoff / matrix / 正式 `doc-rvv` | reviewer | production-public board evidence | `doc/phases/070-production-integration-loop/production-direct-repeated-summary.md` |
| Phase 080 summary / doctor | evidence output summary | 保存 semi-scale 接入后真实公开入口的 5-run 板卡统计和 doctor 结果 | evaluation / Handoff / matrix / 正式 `doc-rvv` | reviewer | production-public board evidence | `doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-summary.md` |

## 诊断证据链

当前 production-shaped diagnostic 证据链：

| evidence | result | path |
| --- | --- | --- |
| QEMU correctness | Std/RVV 3 个 gtest 通过 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| asm attribution | RVV bench 反汇编含 `vle8.v`、`vzext.vf2`、`vadd.vv`、`vse16.v` | `build/asm/riscv/bench_linemod_template_scoring_rvv.full.asm` |
| Phase 010 board repeated | score accumulation median `0.842x`，score scan median `0.601x`，combined median `0.879x`，三项均 5/5 退化，checksum 一致 | `test-rvv/recognition/linemod_template_scoring/doc/phases/010-threshold-scan-and-detection-order/score-scan-repeated-summary.md` |
| Phase 010 Evidence Doctor | Errors=3，Warnings=0，Suggestions=0；Error 均为 5/5 退化频率 | `test-rvv/recognition/linemod_template_scoring/doc/phases/010-threshold-scan-and-detection-order/score-scan-repeated-evidence-doctor.md` |
| Phase 020 board repeated | accumulation-only median `0.862x`，5/5 退化，checksum 一致 | `test-rvv/recognition/linemod_template_scoring/doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-summary.md` |
| Phase 020 Evidence Doctor | Errors=1，Warnings=0，Suggestions=0；Error 为 accumulation 5/5 退化频率 | `test-rvv/recognition/linemod_template_scoring/doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-evidence-doctor.md` |
| Phase 030 board repeated | energy map generation median `0.954x`，5/5 退化，checksum / energy_checksum 一致 | `test-rvv/recognition/linemod_template_scoring/doc/phases/030-energy-map-generation/energy-map-repeated-summary.md` |
| Phase 030 Evidence Doctor | Errors=1，Warnings=0，Suggestions=0；Error 为 energy map 5/5 退化频率 | `test-rvv/recognition/linemod_template_scoring/doc/phases/030-energy-map-generation/energy-map-repeated-evidence-doctor.md` |
| Phase 040 board repeated | linearized map copy median `2.160x`，0/5 退化，checksum / energy_checksum / linearized_checksum 一致 | `test-rvv/recognition/linemod_template_scoring/doc/phases/040-linearized-map-copy-ablation/linearized-map-copy-repeated-summary.md` |
| Phase 040 Evidence Doctor | Errors=0，Warnings=0，Suggestions=0 | `test-rvv/recognition/linemod_template_scoring/doc/phases/040-linearized-map-copy-ablation/linearized-map-copy-repeated-evidence-doctor.md` |
| Phase 050 board repeated | full-chain total median `1.565x`，linearized copy median `1.790x`，二者均 0/5 退化，checksum / energy_checksum / linearized_checksum 一致 | `test-rvv/recognition/linemod_template_scoring/doc/phases/050-full-chain-timing-and-production-eligibility/full-chain-split-repeated-summary.md` |
| Phase 050 Evidence Doctor | Errors=0，Warnings=0，Suggestions=0 | `test-rvv/recognition/linemod_template_scoring/doc/phases/050-full-chain-timing-and-production-eligibility/full-chain-split-repeated-evidence-doctor.md` |
| Phase 060 PI1 plan | 已冻结默认宏下 `matchTemplates` / `detectTemplates` 的 production patch 范围；未新增性能证据 | `test-rvv/recognition/linemod_template_scoring/doc/phases/060-pi1-production-integration-plan/plan.zh.md` |
| Phase 000 historical board repeated | old accumulation-only median `1.404x`，0/5 退化；已被 Phase 010/020 当前复跑 supersede | `test-rvv/recognition/linemod_template_scoring/doc/phases/000-current-state-and-score-accumulation/score-accumulation-repeated-summary.md` |

QEMU timing 不参与性能结论。Phase 010/020/030 的负向 diagnostic 和 Phase 040/050 的正向 diagnostic 都不能直接替代真实 production evidence；最终采用 Phase 070 / 080 接入后的 production-public 结果作为当前 truth。

## S11 Production Closeout

| 项 | 当前状态 | 证据 |
| --- | --- | --- |
| production patch | adopted | `recognition/src/linemod.cpp` 内部新增 Std/RVV helper 和 dispatch，公开 API 不变。 |
| production direct correctness | passed | `run_production_direct_test_compare` 中 Std/RVV 两侧 production-direct tests 通过。 |
| asm attribution | passed | `dump_production_direct_bench_rvv` 定位 `linearizeEnergyMapRVV`，并出现 `vlse8.v` / `vse8.v`。 |
| board performance | positive | Phase 070 `matchTemplates 1.138x`、`detectTemplates 1.129x`，均 0/5 退化。 |
| Evidence Doctor | clean | `doc/phases/070-production-integration-loop/production-direct-repeated-evidence-doctor.md`：Errors=0 / Warnings=0 / Suggestions=0。 |
| semi-scale extension | adopted | Phase 080 `detectTemplatesSemiScaleInvariant 1.136x`，0/5 退化，`doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-doctor.md`：Errors=0 / Warnings=0 / Suggestions=0。 |
| production topic doc | applicable | `doc-rvv/recognition/linemod_template_scoring-RVV.zh.md` 已创建，数据采用 Phase 070 接入后板卡结果。 |

## 后续方向判断

当前 adopted patch 的默认宏公开入口范围已经闭合。仍能继续尝试的方向是 `LINEMOD_USE_SEPARATE_ENERGY_MAPS` helper phase，但它会扩大到非默认编译宏和四套 energy / linearized map 语义，不能由 Phase 070 / 080 证据直接采纳。默认建议是完成 final verification 后暂停在 ready-for-review；除非后续有实际 build 需要该宏或新的 profile 证明它值得投入，否则不建议继续扩大当前 topic。
