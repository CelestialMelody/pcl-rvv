# Phase 020 Plan: filter-5x5-rvv

## 阶段意图和边界

本阶段只证明 `SurfaceNormalModality<PointInT>::filterQuantizedSurfaceNormals()` 的 5x5 histogram
filter（直方图滤波）可以接入 RVV，并在当前 production direct（真实生产路径）边界内保持与
标量一致。范围不包括 `QuantizedMap::spreadQuantizedMap()`、`extractFeatures()`、其它点类型、
其它 `Scalar`、更大的 public API 重构或新的 production family 选择。

validated_scope（本阶段准备证明的范围）：organized `PointXYZRGBA` public entry、
`SurfaceNormalModality::processInputData()`、5x5 filter、`Scalar=float`、连续 AoS（结构数组）、
320x240 与 641x481 tail、QEMU correctness、RVV 反汇编、板卡 repeated benchmark 和
Evidence Doctor。

unvalidated_scope（仍未验证范围）：`QuantizedMap::spreadQuantizedMap()`、`extractFeatures()`、
其它模板实例、非 organized 输入、其它 row source / point type / layout、`Scalar=double`、
filter 之外的 family 选择。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| queue | `doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md` 已完成 `surface_normal_modality` 的 production direct 初始闭环；下一候选是 filter。 |
| production source | `recognition/include/pcl/recognition/surface_normal_modality.h` 已有 `computeAndQuantizeSurfaceNormals2()` 的 RVV 分流，`filterQuantizedSurfaceNormals()` 仍为标量。 |
| test assets | `test-rvv/recognition/surface_normal_modality` 已具备 public entry correctness、bench、board 和 registry 基础。 |
| doc-rvv | 已存在 adopted production behavior 的长期文档；本阶段只在其下追加 filter 方向事实，不改前一阶段结论。 |
| board availability | 已确认可用，前一轮 repeated board 与 Evidence Doctor 正常。 |

## 假设与候选族

| candidate family | hypothesis | risk / unknown |
| --- | --- | --- |
| `filter-5x5-rvv` | 5x5 byte histogram 的 25 个邻域累加可以按 VL chunk 批量化，减少 filter 主循环开销。 | 直方图更新、tie-break 和边界零填充必须与标量逐像素一致。 |
| `filter-public-direct` | 直接沿用 `processInputData()` 公共入口做 production direct 更容易保持证据边界清晰。 | 需要区分 quantize 与 filter 两段 path hook，避免把前一阶段的 RVV 命中误当 filter 证据。 |
| `spread-rvv` | 如果 filter 继续正向，spread 可能作为后续独立 phase。 | spread 半径和写回成本会改变计时边界，不能与本阶段混淆。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `filter-5x5-rvv` | organized quantized map | `PointXYZRGBA` / float / contiguous AoS | `SurfaceNormalModality<PointInT>::processInputData()` 中的 `filterQuantizedSurfaceNormals()` | public entry forced scalar / RVV 对拍，filter path hook 命中 | `bench_snm --case-filter production_process_*` | 5-run repeated board，先看现有 board target 再刷新 | `check_snm_production_rvv_asm` | `Errors=0 / Warnings=0 / Suggestions` 可解释 | planned | 写失败测试并实现 filter RVV |
| `spread-rvv` | spreaded map | byte map | `QuantizedMap::spreadQuantizedMap()` | not covered | not covered | not covered | not covered | not covered | deferred | 另开后续 phase |

## 实现和测试动作

| action | artifact / command | completion |
| --- | --- | --- |
| RED | `test-rvv/recognition/surface_normal_modality/src/test_snm.cpp` | 新增 filter path hook 对拍测试，在当前代码下失败。 |
| GREEN | `recognition/include/pcl/recognition/surface_normal_modality.h` | `filterQuantizedSurfaceNormals()` 在 `__RVV10__` 下出现可记录的 RVV 分流，并与标量结果一致。 |
| correctness | `make -C test-rvv/recognition/surface_normal_modality run_test_compare` | public entry forced scalar / RVV 对拍通过。 |
| asm | `make -C test-rvv/recognition/surface_normal_modality check_snm_production_rvv_asm` | 生产 helper 的 RVV 归属与指令集合可见。 |
| board | `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make -C test-rvv/recognition/surface_normal_modality board_repeated record_evidence_state_repeated` | 5-run repeated board、manifest、Evidence Doctor、registry 刷新。 |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_direct` |
| A/B boundary | `public_overload`，通过 `processInputData()` 直连 |
| 当前决策问题 | 当前 public RVV path 是否继续快于当前 public scalar path |
| diagnostic 是否可外推到 production | 不需要外推；本阶段直接用 production direct 证据闭合 |
| comparison-boundary / baseline mismatch 风险 | 受控；同一公开入口、同一输入、同一计时边界 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许，但必须保留 filter-only 范围和回退标量 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 本阶段不是 family selection；只需 RVV-vs-scalar |

## 板卡复跑预算和决策桶

默认 5-run repeated，沿用 `--iterations 20 --warmup-iterations 3`。`weak_positive` 需要 median
speedup 在 1.05x 以上且退化频率不高；若 board 结果掉到 neutral / negative / unstable，则先停在
filter 结论，不自动扩大到 spread。

## 继续 / 停止条件

继续条件：filter 路径 hook 能在 RVV build 命中，forced scalar / RVV 输出一致，asm 能归属到
filter 相关 RVV helper，板卡重复测试保持正向。停止条件：filter 正确性无法闭合、board 不可达、
或本阶段发现 spread / feature extraction 才是主收益点。

## 文档更新清单

本阶段完成后更新 `doc/phases/020-filter-5x5-rvv/result.zh.md`、`doc/optimization-roadmap.zh.md`、
`doc/phases/optimization-matrix.zh.md`、`doc/surface_normal_modality-evaluation.zh.md`、
`README.zh.md`、current Handoff，以及必要时的 `doc-rvv/recognition/surface_normal_modality-RVV.zh.md`
中的“当前采用的优化方式”和证据链小节。
