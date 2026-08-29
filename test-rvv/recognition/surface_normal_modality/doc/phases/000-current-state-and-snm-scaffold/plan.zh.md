# Phase 000 Plan: current-state-and-snm-scaffold

## 阶段意图和边界

本阶段建立 `SurfaceNormalModality<PointInT>::processInputData()` 的首个 production-shaped diagnostic
（生产形态诊断）。范围只覆盖 `computeAndQuantizeSurfaceNormals2()` 中 depth-to-normal
（深度到法线）和 quantize（方向量化）链路，不覆盖 `filterQuantizedSurfaceNormals()`、
`QuantizedMap::spreadQuantizedMap()`、`extractFeatures()`、真实 production dispatch（生产分流）
或完整 LINEMOD RGB-D 模板训练入口。

validated_scope（本阶段准备证明的范围）：organized `PointXYZ` depth cloud、`Scalar=float`、
连续 AoS（结构数组）布局、320x240 与 641x481 tail 规模、QEMU correctness、RVV 反汇编、
板卡 repeated benchmark 和 Evidence Doctor。

unvalidated_scope（仍未验证范围）：模板 `PointInT` 泛型 traits、非 `PointXYZ` 点型、filter 5x5、
spread、feature extraction、真实 `processInputData()` production direct、上游 `LineRGBD` wrapper、
真实相机 focal length 配置和 `Scalar=double`。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| queue | `doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md` 第 3 条，状态为未启动。 |
| production source | `recognition/include/pcl/recognition/surface_normal_modality.h` 尚无 `__RVV10__` 分流。 |
| scalar path | `processInputData()` 调 `computeAndQuantizeSurfaceNormals2()`、`filterQuantizedSurfaceNormals()`、`spreadQuantizedMap()`。 |
| test assets | 本阶段创建 `test-rvv/recognition/surface_normal_modality` 主题目录。 |
| doc-rvv | 当前不适用；无 adopted production behavior（已采纳生产行为）。 |

## 假设与候选族

| candidate family | hypothesis | risk / unknown |
| --- | --- | --- |
| `depth-quantize-rvv` | depth 转毫米、有限值 mask、8 邻域整数累加和法线公式可以按 VL chunk（可变向量长度分块）批量处理。 | `atan2` 量化边界、`uint16_t` 转换、`std::isfinite` 与 NaN/Inf、`l_y/l_x` 内区边界必须同链路对拍。 |
| `filter-5x5-rvv` | 若首阶段正向，5x5 histogram filter（直方图滤波）可作为下一 phase。 | 25 个邻域 byte bin 的多数选择和 tie-break 需要单独 correctness。 |
| `production-process-rvv` | 若 diagnostic 正向，可把候选接入真实 `processInputData()`。 | 需要生产 fallback、公开入口 direct test、asm attribution（反汇编归属）和接入后板卡证据。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `depth-quantize-rvv` | organized depth image | `PointXYZ` / float / contiguous AoS | test helper shaped like `computeAndQuantizeSurfaceNormals2()` | `run_test_compare` | `bench_snm --case-filter depth_quantize_*` | planned 5-run repeated | `check_snm_rvv_asm` | planned | in_progress | run RED then implement candidate |
| `filter-5x5-rvv` | quantized map | byte map | `filterQuantizedSurfaceNormals()` | not yet created | not yet created | not covered | not covered | not covered | deferred | phase 010 if depth candidate supports continuing |

## 实现和测试动作

| action | artifact / command | completion |
| --- | --- | --- |
| TDD RED | `make -C test-rvv/recognition/surface_normal_modality run_test_rvv` | RVV build 因 path-hit 期望 `RvvDepthQuantize` 而失败。 |
| GREEN | `include/impl/snm_surface_normal.hpp` | `__RVV10__` candidate 命中 RVV path，并与标量量化输出一致。 |
| correctness | `make -C test-rvv/recognition/surface_normal_modality run_test_compare` | Std/RVV 两侧 gtest 通过。 |
| asm | `make -C test-rvv/recognition/surface_normal_modality check_snm_rvv_asm` | bench RVV 反汇编出现预期向量指令。 |
| board | board repeated target 或等价命令 | 5-run、warm-up、decision bucket 记录到 summary。 |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | 本阶段为 `production-shaped diagnostic`，不是 production direct。 |
| A/B boundary | `test helper`；bench 不调用真实 `processInputData()`。 |
| 当前决策问题 | `depth-quantize-rvv` 是否值得进入 production integration loop。 |
| diagnostic 是否可外推到 production | 只能作为 PI1 信号；最终采纳必须以后续 production direct 证据为准。 |
| comparison-boundary / baseline mismatch 风险 | 存在；当前 helper 排除了 filter、spread 和对象状态。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有实现范围小、fallback 明确、production direct test 可写且板卡可复跑时才允许。 |
| clean adoption 是否需要同一 production boundary 内的证据 | 需要；本轮用户授权 PI5 后若接入后板卡有收益即可采纳。 |

## 板卡复跑预算和决策桶

默认 5-run repeated，bench 参数为 `--iterations 20 --warmup-iterations 3`。`positive` 要求 median
speedup >= 1.20x 且 `B/A < 1` 为 0/5；`weak-positive` 为 1.05x 到 1.20x 且退化频率不超过 1/5；
低于 1.05x 或退化频率较高判为 neutral / negative。预算耗尽仍摇摆时标为 unstable（不稳定），不直接接 production。

## 继续 / 停止条件

继续条件：RED 能证明 path-hit 测试有效，GREEN correctness 通过，asm 能归属 RVV 指令，且板卡可用。
若板卡 repeated positive 或 weak-positive 且维护成本小，默认继续 PI1。停止条件：correctness 无法闭合、
asm 无法证明 RVV path、板卡不可达且 SSH_AUTH_SOCK 注入后仍失败、或板卡结果 negative / unstable。

## 文档更新清单

本阶段更新 `README.zh.md`、`doc/surface_normal_modality-evaluation.zh.md`、`doc/phases/README.zh.md`、
本 plan、后续 result、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` 和
current Handoff。正式 `doc-rvv/recognition/surface_normal_modality-RVV.zh.md` 仅在 PI5 生产证据闭环通过并采纳后创建。
