# Phase 030 Plan: spread-quantized-map-rvv

## 阶段意图和边界

本阶段只证明 `SurfaceNormalModality<PointInT>::processInputData()` 最后的 spread step
（扩散步骤，把 filter 后的 bit mask 按窗口传播到邻域）是否值得在当前 topic 内接入 RVV。

`QuantizedMap::spreadQuantizedMap()` 是 recognition 公共 helper，同时被 color modality 和
color-gradient modality 使用。本阶段不直接修改这个公共 helper，避免把 surface normal topic 的
授权扩大到其它 modality。候选实现放在 `SurfaceNormalModality` 私有路径内：保留
`processInputDataFromFiltered()` 走公共标量 helper，只让 `processInputData()` 在 Phase 020 已采纳
的 production direct 边界内尝试 surface-normal 专用 RVV spread helper。

validated_scope（本阶段准备证明的范围）：organized `PointXYZRGBA` public entry、
`SurfaceNormalModality::processInputData()`、`Scalar=float`、连续 AoS（结构数组）、
默认 `spreading_size_=8`、320x240 与 641x481 tail、QEMU correctness、RVV 反汇编、
板卡 repeated benchmark 和 Evidence Doctor。

unvalidated_scope（仍未验证范围）：公共 `QuantizedMap::spreadQuantizedMap()` 的其它调用方、
`processInputDataFromFiltered()`、非默认 spreading size、其它模板实例、非 organized 输入、
其它 layout、`extractFeatures()`。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| production source | `surface_normal_modality.h` 已采纳 depth-to-normal / quantize 和 5x5 filter RVV；spread 仍调用公共标量 helper。 |
| public helper | `recognition/src/quantizable_modality.cpp` 中的 `QuantizedMap::spreadQuantizedMap()` 是全 recognition 共享函数，本阶段不修改。 |
| test assets | `test_snm.cpp` 已能区分 scalar、quantize RVV 和 filter RVV hook，需要新增 spread hook。 |
| board evidence | Phase 020 public entry median `1.420x` / `1.420x`，作为本阶段生产基线，不直接证明 spread。 |
| doc-rvv | 已使用 Phase 020 数据记录 adopted production behavior；本阶段若采纳，需要刷新为 Phase 030 数据。 |

## 假设与候选族

| candidate family | hypothesis | risk / unknown |
| --- | --- | --- |
| `snm-spread-rvv-2pass` | 公共 spread 是两次 byte OR 滑窗：横向写 `tmp_map`，纵向写 `output_map`。RVV 可按 VL chunk 同时处理多个列，减少内层 `spreading_size=8` 的标量 OR。 | 当前写入位置有半窗口偏移；必须完全匹配公共 helper 的边界和未写区域初值。 |
| `generic-quantized-map-spread-rvv` | 公共 helper 也可 RVV 化，惠及 color/color-gradient modality。 | 会影响多个 production 调用方，超出当前 topic 授权；本阶段只记录为后续独立 topic。 |
| `spread-radius-specialized-8` | surface normal 默认 `spreading_size_=8`，可以先针对 8 做窄路径。 | 非默认 spreading size 必须 fallback 到公共标量 helper。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `snm-spread-rvv-2pass` | spreaded quantized map | byte map / default spread 8 | `SurfaceNormalModality<PointXYZRGBA>::processInputData()` | public entry forced scalar / RVV 对拍，spread path hook 命中，非默认 spread fallback | `bench_snm --case-filter production_process_*` | 5-run repeated board，run label `snm_phase030_spread_rvv_repeated` | `check_snm_production_rvv_asm` 增加 spread 符号和 byte OR/store 指令 | `Errors=0 / Warnings=0 / Suggestions` 可解释 | planned | 写失败测试并实现 surface-normal 专用 spread RVV |
| `generic-quantized-map-spread-rvv` | shared QuantizedMap | byte map / all modalities | `QuantizedMap::spreadQuantizedMap()` | not covered | not covered | not covered | not covered | not covered | deferred | 另开跨 modality topic |

## 实现和测试动作

| action | artifact / command | completion |
| --- | --- | --- |
| RED | `test-rvv/recognition/surface_normal_modality/src/test_snm.cpp` | 新增 spread path hook 和非默认 spread fallback case，在当前代码下 RVV build 因未命中 spread hook 失败。 |
| GREEN | `recognition/include/pcl/recognition/surface_normal_modality.h` | 新增 `spreadFilteredQuantizedSurfaceNormals()`、`spreadFilteredQuantizedSurfaceNormalsStd()`、`spreadFilteredQuantizedSurfaceNormalsRVV()`；默认 spread 8 且规模足够时尝试 RVV，否则回公共标量 helper。 |
| correctness | `make -C test-rvv/recognition/surface_normal_modality run_test_rvv`、`run_test_compare` | public entry forced scalar / RVV 对拍通过；非默认 spread size 回退标量。 |
| asm | `make -B -C test-rvv/recognition/surface_normal_modality check_snm_production_rvv_asm` | 生产 asm 能归属到 spread helper 或 public entry，并可见 RVV byte OR / store。 |
| board | `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make -C test-rvv/recognition/surface_normal_modality board_repeated record_evidence_state_repeated REPEATED_BOARD_TAG=phase030_spread_rvv ...` | 5-run repeated board、manifest、Evidence Doctor、registry 刷新。 |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_direct` |
| A/B boundary | `public_overload`，通过 `processInputData()` 直连 |
| 当前决策问题 | 当前 public RVV path 是否继续快于当前 public scalar path，以及 spread 子链路是否值得保留 |
| diagnostic 是否可外推到 production | 不需要外推；本阶段直接用 production direct 证据闭合 |
| comparison-boundary / baseline mismatch 风险 | 受控；同一公开入口、同一输入、同一计时边界。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许，但必须保留 spread-only 范围；若 production board neutral / negative，则不采纳 spread RVV，保留 Phase 020。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 本阶段会把 Phase 020 作为源码基线；若 public Std/RVV positive 但难以说明 spread 增量，需增加 RVV-vs-RVV detail A/B 或降级为 explicit probe。 |

## 板卡复跑预算和决策桶

默认 5-run repeated，bench 参数为
`--case-filter production_process_320x240,production_process_641x481_tail --iterations 20 --warmup-iterations 3`。
若两个 case median 明显高于 Phase 020 且 `B/A < 1 = 0/5`，判为 positive；若仍高于标量但没有超过
Phase 020，需作为 “public RVV path positive but spread incremental unclear（公开入口正向但 spread 增量不清）”
处理，不能 clean-adopt spread。若任一 case 中性、退化或 Evidence Doctor 出现未解释 Warning，
最多做一次同边界复跑；预算耗尽仍摇摆则标成 unstable 并停在 Phase 020 adopted 状态。

## 继续 / 停止条件

继续条件：spread hook 能在 RVV build 命中，forced scalar / RVV 输出一致，非默认 spread size 回退标量，
asm 归属到 spread RVV，板卡 repeated 支持接入。停止条件：correctness 无法闭合、asm 无法归属、
board 不可达、或 Phase 030 相对 Phase 020 没有可解释增量收益。

## 文档更新清单

本阶段完成后更新 `doc/phases/030-spread-quantized-map-rvv/result.zh.md`、
`doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md`、
`doc/surface_normal_modality-evaluation.zh.md`、`README.zh.md`、current Handoff，以及若采纳则刷新
`doc-rvv/recognition/surface_normal_modality-RVV.zh.md` 和队列表。
