# Phase 010 Plan: production-integration

## 阶段意图和边界

本阶段把 Phase 000 的 `depth-quantize-rvv` 从 production-shaped diagnostic
（生产形态诊断）推进到 production direct（真实生产路径）证据。范围只覆盖
`SurfaceNormalModality<PointInT>::processInputData()` 中的
`computeAndQuantizeSurfaceNormals2()` 子链路；`filterQuantizedSurfaceNormals()`、
`spreadQuantizedMap()`、`extractFeatures()` 和 `processInputDataFromFiltered()` 保持标量或既有路径。

validated_scope：organized depth cloud、`PointXYZ` / `PointXYZRGBA` 代表点型、`z` 字段为单个
`float` 的连续 AoS（结构数组）输入、320x240 与 641x481 tail 规模、QEMU correctness
（QEMU 正确性验证）、production asm attribution（生产反汇编归属）、production direct board
repeated（真实生产路径板卡重复性能测试）和 Evidence Doctor（证据体检）。

unvalidated_scope：任意用户自定义 `PointInT`、`Scalar=double`、不含 traits 注册 `z` 字段的点型、
filter 5x5 RVV、spread RVV、feature extraction RVV、完整 LINEMOD RGB-D 上游训练 / 检测耗时。

point_type_expansion_queue：若本阶段 production direct 结果正向，后续可把 gate 从代表点型扩展到
`RVVFloatFieldLayout<PointInT, pcl::fields::z>` 能证明的其它 xyz-like / rgba-like 点型；扩展必须补
dedicated correctness、fallback、bench、asm、board 和 Evidence Doctor，不能由本阶段自动外推。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| Phase 000 correctness | `run_test_compare` 已通过 Std/RVV 2/2。 |
| Phase 000 asm | `check_snm_rvv_asm` 已命中 RVV 指令。 |
| Phase 000 board | `snm_phase000_depth_quantize_repeated` 5-run，两个 case median `1.100x` / `1.110x`，checksum 一致，decision bucket 为 `weak_positive`。 |
| Evidence Doctor | Phase 000 Errors=0 / Warnings=0 / Suggestions=4；Suggestion 为环境 metadata 与 binary hash 缺失，不阻塞继续。 |
| production source | `recognition/include/pcl/recognition/surface_normal_modality.h` 尚未接入 RVV dispatch。 |
| prompt override | 本轮用户明确允许：生产接入后板卡结果有收益即可自动采纳，并创建正式 `doc-rvv` 文档。 |

## 实现和测试动作

| action | artifact / command | completion |
| --- | --- | --- |
| RED production direct test | `src/test_snm.cpp` 新增真实 `SurfaceNormalModality` public entry path-hit / forced scalar 对拍 | RVV build 在 production hook 或 RVV path 缺失时失败。 |
| PI2 production patch | `surface_normal_modality.h` 抽出 `computeAndQuantizeSurfaceNormals2Std()`；新增 `computeAndQuantizeSurfaceNormals2RVV()` 和 hook | 非 RVV 构建只走 Std；RVV gate 失败时自然 fallback。 |
| PI3 correctness | `make -C test-rvv/recognition/surface_normal_modality run_test_compare` | Std/RVV 生产直连测试通过，forced scalar/RVV map 与 orientation 一致。 |
| PI3 bench | `src/bench_snm.cpp` 新增 `production_process_320x240` / `production_process_641x481_tail` | bench label 调用真实 `processInputData()`。 |
| PI4 asm | `make -C test-rvv/recognition/surface_normal_modality check_snm_production_rvv_asm` | 反汇编能归属 production helper 或 production case。 |
| PI4 board | `board_repeated` 覆盖 production case，run label `snm_phase010_production_direct_repeated` | 5-run、20 iterations、3 warmup，生成 summary / manifest / doctor / registry。 |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 `production-shaped diagnostic`；本阶段新增 `production_direct`。 |
| A/B boundary | Phase 000 是 test helper；本阶段必须走 public overload（公开入口）`processInputData()`。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path。 |
| diagnostic 是否可外推到 production | 只能作为生产探针信号；最终采纳以本阶段 production direct 证据为准。 |
| comparison-boundary / baseline mismatch 风险 | 存在；public entry 额外包含 5x5 filter 和 spread，收益可能被稀释。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本轮 weak-positive，但实现小、fallback 明确、用户授权生产探针，因此允许本阶段 bounded probe。 |
| clean adoption 是否需要同一 production boundary 内的证据 | 需要。本轮用 production direct Std/RVV repeated board 作为采纳门槛；若无收益或不稳定，则不采纳并保留交接。 |

## 板卡复跑预算和决策桶

默认 5-run repeated，bench 参数为 `--case-filter production_process_320x240,production_process_641x481_tail --iterations 20 --warmup-iterations 3`。
`positive` 要求 median speedup >= 1.20x 且 `B/A < 1` 为 0/5；`weak_positive` 为 median >= 1.05x
且退化频率不超过 1/5；低于 1.05x 或退化频率高时判为 neutral / negative。预算耗尽仍摇摆时标为
unstable（不稳定），不自动采纳。

## 继续 / 停止条件

若 production direct correctness、asm 和板卡 repeated 均闭合，且板卡 summary 显示 positive 或
weak_positive，本轮按用户授权自动采纳，并创建 `doc-rvv/recognition/surface_normal_modality-RVV.zh.md`。
若 correctness 失败、asm 无法归属、板卡不可达、checksum mismatch 或 production direct 结果 neutral /
negative / unstable，则停在当前 production patch 状态并写 Handoff，不自行回滚。
