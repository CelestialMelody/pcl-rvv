# Phase 040: RGB Sobel stencil RVV ablation result

## 当前状态

本阶段完成 `full-sobel-rgb-stencil-rvv` 的 test-rvv 诊断候选、正确性、QEMU smoke、
反汇编和板卡 repeated。结论为 `production-ready candidate family`：
它仍不是 adopted production behavior（已采纳生产行为），但证据支持进入下一阶段
production integration loop（生产接入闭环）。

## 实际执行范围

| 项 | 结果 |
| --- | --- |
| RED | `run_test_rvv` 先因缺少 `computeSobelQuantizedStencilCandidate` 和 `RvvFullStencilChain` 编译失败 |
| GREEN | 新增 test-rvv helper `computeSobelQuantizedStencilCandidate()`，在 RVV build 下命中 `RvvFullStencilChain` |
| correctness | `make -C test-rvv/recognition/color_gradient_modality run_test_compare` 通过，Std/RVV 均 8/8 |
| QEMU smoke | `full_chain_stencil_*` 极小迭代可运行，Std/RVV checksum 一致；QEMU timing 不作性能结论 |
| asm | `make -C test-rvv/recognition/color_gradient_modality check_cgm_stencil_rvv_asm` 通过 |
| board repeated | `cgm_phase040_rgb_stencil_repeated` 完成 5 runs |

## 设计结果

Phase 040 新 helper 在 `pcl::RGB` 的 B/G/R/A 4-byte packed layout（四字节打包布局）
上使用 `vlse8` 按通道 stride load（跨步加载），再 widen（拓宽）到 32-bit integer。
每个 VL chunk（可变向量长度分块）计算 RGB 三通道 Sobel `dx/dy/sqr_mag`，用与标量相同的
tie-break（并列处理）选择最大通道：

- red 只有在 `sqr_mag_r > sqr_mag_g && sqr_mag_r > sqr_mag_b` 时胜出；
- green 只有在 red 未胜出且 `sqr_mag_g > sqr_mag_b` 时胜出；
- 其它情况保留 blue。

后续 `sqrt`、`atan2_RVV_f32m2`、角度归一化、8-bin quantize 和 3x3 dominant filter
沿用已有 RVV 诊断链路。

## Board repeated 结果

证据路径：

- summary: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase040_rgb_stencil/summary.md`
- manifest: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase040_rgb_stencil/evidence_manifest.json`
- doctor: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase040_rgb_stencil/evidence_doctor.md`
- registry: `test-rvv/recognition/color_gradient_modality/log/evidence_registry.json`

| case | runs | median speedup | range | B/A < 1 | checksum |
| --- | ---: | ---: | --- | ---: | --- |
| `full_chain_320x240` | 5 | `2.340x` | `2.110x` - `2.360x` | `0/5` | Std/RVV 一致 |
| `full_chain_641x481_tail` | 5 | `2.470x` | `2.420x` - `2.490x` | `0/5` | Std/RVV 一致 |
| `full_chain_stencil_320x240` | 5 | `5.040x` | `4.980x` - `5.100x` | `0/5` | Std/RVV 一致 |
| `full_chain_stencil_641x481_tail` | 5 | `4.680x` | `4.660x` - `4.840x` | `0/5` | Std/RVV 一致 |

相对当前 full-chain diagnostic，stencil candidate 的 RVV 耗时明显更低；同一 repeated
采集中 `full_chain_stencil_320x240` 的 median speedup 从 `2.340x` 提升到 `5.040x`，
tail case 从 `2.470x` 提升到 `4.680x`。

## Evidence Doctor

`log/board/repeated_phase040_rgb_stencil/evidence_doctor.md`：

- Errors: `0`
- Warnings: `0`
- Suggestions: `8`

Suggestions 是四个 case 各自缺少环境字段和 binary hash。它们不阻塞当前 diagnostic
positive 结论，但 production direct 阶段仍应重新生成 production summary 和 doctor。

## EvidenceDecision

当前决策为 `production-ready candidate family`：

- correctness：Std/RVV 8/8 通过，stencil helper 与标量 full-chain 输出一致。
- asm：新增 gate 命中 stride byte load、widen / integer arithmetic、`vfsqrt`、
  `atan2_RVV` 和 byte filter 指令。
- board：5-run repeated 稳定 positive，checksum 一致。
- boundary：本阶段仍是 test helper diagnostic，不调用真实 `processInputData()`。

## 继续 / 停止决策

- `continue_stop_decision`: continue to Phase 050 production integration。
- `stop_condition_hit`: none。
- `next_phase_default`: `050-rgb-stencil-production-integration`。
- `unblocked_next_actions`: 把 RGB stencil RVV 接入 `computeColorGradientPipelineRVV()`，
  再重新运行 production direct correctness、asm、board repeated 和 Evidence Doctor。
