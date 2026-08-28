# Phase 040: RGB Sobel stencil RVV ablation plan

## 阶段意图和边界

本阶段尝试 `full-sobel-rgb-stencil-rvv`，目标是验证 RGB 3x3 Sobel stencil（邻域模板）
本身是否值得进一步 RVV 化。当前已采纳的 production family 是
`post-gaussian-production-rvv`：它仍用标量循环生成 `selected_dx/selected_dy/selected_sqr_mag`，
再用 RVV 完成 `sqrt/atan2/quantize` 和 dominant filter。

Phase 040 不直接修改 production helper，不扩大 public API，也不替换已采纳 production 路径。
本阶段只在 `test-rvv/recognition/color_gradient_modality/include/impl/cgm_sobel_quantize.hpp`
新增候选 helper，并在 `src/test_cgm.cpp`、`src/bench_cgm.cpp`、Makefile 和 manifest
中接入同边界诊断证据。只有板卡 repeated 显示相对当前 production-shaped full-chain candidate
仍有明确收益，后续才考虑单独的 production integration phase。

## 当前状态清单

| 项 | 当前状态 |
| --- | --- |
| production | Phase 030 已采纳 `computeColorGradientPipelineRVV()`，板卡 production direct median `1.490x` / `1.540x` |
| 现有 full-chain diagnostic | `computeSobelQuantizedFilteredCandidate()` 为标量 Sobel staging + RVV math/filter，Phase 020 median `2.320x` / `2.480x` |
| 主要剩余候选 | `full-sobel-rgb-stencil-rvv`，尝试把 RGB byte load、widen、Sobel 差分、max-channel selection 也放进 RVV |
| 风险 | byte stride load 和 widen 可能比标量 staging 更慢；channel tie-break 必须严格保持 production 规则 |

## 假设与候选族

| candidate family | 假设 | 风险 / unknown |
| --- | --- | --- |
| `full-sobel-rgb-stencil-rvv` | 用 `vlse8` 按 `pcl::RGB` 的 B/G/R/A 字节布局跨像素加载邻域，RVV 计算 RGB 三通道 Sobel 和最大通道选择，可减少 scalar staging 成本 | `pcl::RGB` stride 是 4 字节；9 邻域 x 3 通道加载较多，可能增加内存压力；signed int 算术和 tie-break 要与标量一致 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `full-sobel-rgb-stencil-rvv` | organized RGB image | `pcl::RGB`, B/G/R/A 4-byte stride, float output | test helper only | add gtest path-hit + scalar equivalence | add `full_chain_stencil_*` case | run repeated board with 5 runs if QEMU/asm pass | add `check_cgm_stencil_rvv_asm` | manifest + doctor required | planned |

## 实现和测试动作

1. RED：新增 gtest，要求 RVV build 命中 `ExecutionPath::RvvFullStencilChain`，当前应因枚举 / helper 缺失而失败。
2. GREEN：新增 `computeSobelQuantizedStencilCandidate()`，在 `__RVV10__` 下用 RVV 计算内区 Sobel selected dx/dy/sqr_mag，再复用现有 RVV math/filter。
3. Bench：新增 `full_chain_stencil_320x240` 和 `full_chain_stencil_641x481_tail`。
4. ASM：新增 `check_cgm_stencil_rvv_asm`，至少检查 `vlse8`、widen、integer arithmetic、`vfsqrt`、`atan2_RVV` 和 byte filter 指令。
5. Board：若 correctness、QEMU smoke 和 asm 通过，运行 5 次 repeated board；case-filter 只包含 `full_chain_*` 与 `full_chain_stencil_*` 或只包含 stencil case，并在结果中说明 A/B 边界。
6. Docs：回填 Phase 040 result、optimization matrix、roadmap、evaluation 和 Handoff。

## Evidence Doctor 和 registry

本阶段使用 topic-local `script/generate_cgm_evidence_manifest.py` 生成 summary / manifest，
再运行 `record_evidence_state_repeated` 登记。若新增 case label，manifest metadata 必须说明：

- evidence role: `production_shaped_diagnostic`
- boundary: `test_helper`
- timer boundary: RGB Sobel stencil RVV + math quantize + dominant filter
- checksum policy: filtered one-hot map checksum

Evidence Doctor Error 必须修复或降级；Warning 必须解释；Suggestion 可不阻塞但要记录。

## 板卡复跑预算和决策桶

- repeated runs: `5`
- per run: `20` iterations, `3` warmup
- positive: median speedup `>= 1.20x` 且 `B/A < 1` 为 `0/5`
- weak-positive: median `1.05x`-`1.20x` 且 `B/A < 1` 为 `0/5`
- neutral: median `0.95x`-`1.05x`
- negative: median `< 0.95x` 或多数退化
- unstable: 方向摇摆或 checksum 不一致

## 继续 / 停止条件

若 stencil candidate 相对当前 production-shaped full-chain baseline 没有至少 weak-positive，
本阶段停止并记录 `rejected with evidence`，不接 production。若结果 positive，再开新的
production integration phase；该 phase 需要同一 production boundary 内 RVV-vs-RVV detail A/B，
不能用 Phase 030 Std/RVV positive 直接替代。

## 文档更新清单

- `doc/phases/040-rgb-stencil-rvv-ablation/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/color_gradient_modality-evaluation.zh.md`
- `tmp/rvv-work-logs/recognition/color_gradient_modality/current-handoff/current-handoff.zh.md`
- 若生产行为不变，`doc-rvv/recognition/color_gradient_modality-RVV.zh.md` 只引用 Phase 040 的暂缓 / 拒绝状态，不复制诊断流水。
