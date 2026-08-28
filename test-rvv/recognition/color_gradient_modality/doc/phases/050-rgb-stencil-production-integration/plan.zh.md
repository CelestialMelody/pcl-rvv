# Phase 050: RGB Sobel stencil production integration plan

## 阶段意图和边界

Phase 050 将 Phase 040 的 RGB Sobel stencil RVV candidate 接入真实
`ColorGradientModality<PointInT>::processInputData()` 路径。接入位置仍然是
Gaussian convolution（高斯卷积）之后、spread 之前的 `computeColorGradientPipelineRVV()`。

本阶段不修改 public API（公开接口），不直接 RVV 读取模板 `PointInT` 的 RGB 字段，
不修改 Gaussian、spread 或 `extractFeatures()`。若接入后的 production direct 板卡收益不成立，
需要保留当前 Phase 030 adopted production family 或回滚到它，不把 Phase 040 诊断收益外推成生产结论。

## 当前状态清单

| 项 | 当前状态 |
| --- | --- |
| 已采纳 production | Phase 030 `post-gaussian-production-rvv`，production direct median `1.490x` / `1.540x` |
| 新 candidate | Phase 040 `full-sobel-rgb-stencil-rvv`，diagnostic median `5.040x` / `4.680x` |
| 正确性 | Phase 040 `run_test_compare` Std/RVV 8/8 |
| 反汇编 | `check_cgm_stencil_rvv_asm` 通过 |
| 生产缺口 | 还未证明真实 public entry 接入 stencil 后仍有收益 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `rgb-stencil-production-rvv` | public `processInputData()` after Gaussian output | `PointXYZRGB` public input copied to `pcl::RGB`, B/G/R/A 4-byte internal layout | production helper `computeColorGradientPipelineRVV` | production direct forced scalar/RVV 对拍 | `production_process_*` | planned 5-run repeated | production helper must show `vlse8` and stencil arithmetic | required | planned |

## 实现和测试动作

1. Production patch：把 `computeColorGradientPipelineRVV()` 的 scalar Sobel staging 换成 RVV stride-load Sobel stencil，同时继续写 `GradientXY`、quantized map 和 filtered map。
2. Correctness：运行 `make -C test-rvv/recognition/color_gradient_modality run_test_compare`，要求 Std/RVV 8/8 通过。
3. QEMU smoke：运行 production case-filter 极小迭代，确认日志形状和 checksum。
4. ASM：加强 `check_cgm_production_rvv_asm` 或新增 production stencil gate，要求 production helper 反汇编可见 `vlse8`、integer arithmetic、`vfsqrt`、`atan2_RVV` 和 byte filter 指令。
5. Board：运行 production direct 5-run repeated，case-filter 使用 `production_process_320x240,production_process_641x481_tail`。
6. Docs：回填 Phase 050 result、长期 `doc-rvv`、evaluation、roadmap、matrix 和 Handoff。

## Evidence Doctor 和 registry

production direct repeated 生成新的 summary / manifest / doctor，并登记到
`test-rvv/recognition/color_gradient_modality/log/evidence_registry.json`。
正式长期文档中的生产数据以 Phase 050 production direct board summary 为准；Phase 030
转为 adopted historical baseline（历史已采纳基线）。

## 板卡复跑预算和决策桶

- repeated runs: `5`
- per run: `20` iterations, `3` warmup
- positive: median speedup `>= 1.20x` 且 `B/A < 1` 为 `0/5`
- weak-positive: median `1.05x`-`1.20x` 且 `B/A < 1` 为 `0/5`
- neutral / negative / unstable：按 Phase 040 口径处理

## 继续 / 停止条件

若 production direct positive，按用户本轮确认的“板卡有收益即可采纳”采纳 Phase 050，
但仍需要明确列出接入后数据。若 production direct 不成立，
停止并保留 Phase 030 production family，记录 Phase 040 diagnostic-to-production mismatch。
