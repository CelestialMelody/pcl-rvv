# color_gradient_modality 函数级评估

## S2 函数级评估

目标源码是 `recognition/include/pcl/recognition/color_gradient_modality.h`。
公开入口 `ColorGradientModality<PointInT>::processInputData()` 从 organized RGB
输入点云开始，先复制 `r/g/b` 到 `pcl::RGB` 云，经过 Gaussian convolution（高斯平滑）
后调用 `computeMaxColorGradientsSobel()`、`quantizeColorGradients()`、
`filterQuantizedColorGradients()` 和 `QuantizedMap::spreadQuantizedMap()`。
`extractFeatures()` 后续再根据 mask（掩码）、gradient magnitude（梯度幅值）、
filtered quantized map 和 list/distance selection 生成 LINEMOD template features。

当前可 RVV 化的主片段是 organized 图像内区的 RGB 3x3 Sobel stencil（邻域模板）、
`sqrt`、`atan2` 和 8 方向量化。3x3 dominant filter 也可继续评估；feature extraction
含 list sort（链表排序）和距离约束，首阶段不作为 RVV 主目标。

## 初步判断

当前判断为 adopted production behavior（已采纳生产行为）。Phase 050 已完成 RGB Sobel
stencil production direct（真实生产路径）证据闭环；用户确认“接入后板卡有收益即可采纳”后，
正式长期主题文档刷新为
`doc-rvv/recognition/color_gradient_modality-RVV.zh.md`。
早期 production-shaped diagnostic（生产形态诊断）给出接入依据；Phase 030 首次把候选接到
`processInputData()` 的真实公开入口，Phase 050 进一步把 RGB Sobel stencil 接入当前生产 helper。
理由是：

- 像素级循环规模来自 `width * height`，具备批量处理价值。
- 现有 `pcl::atan2_RVV_f32m2` 可作为候选，但它是有限输入域快速近似，不是 strict
  libm replacement（严格 libm 替换）。
- 角度量化会把连续角度压成 8 个 bin（方向桶），near-boundary（靠近边界）样本可能
  因近似误差跨 bin；必须先用 same-chain correctness 和对抗样本验证。
- 完整 `processInputData()` 包含 Gaussian convolution、dominant filter 和 spread；
  Phase 050 production direct bench 已覆盖这些步骤，但仍不包含后续 feature extraction 计时。

Phase 040/050 已尝试并接入 `full-sobel-rgb-stencil-rvv`。接入后 production direct
板卡 median speedup 为 `1.620x` / `1.590x`，checksum 一致，Evidence Doctor
无 Error / Warning。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ColorGradientModality::processInputData` | production public entry | 完整 modality 预处理入口 | LINEMOD template / detection 输入准备 | Sobel、quantize、filter、spread | adopted production boundary | `recognition/include/pcl/recognition/color_gradient_modality.h` |
| `computeMaxColorGradientsSobel` | production helper | RGB 3x3 Sobel 并选择最大通道梯度 | `processInputData` | `quantizeColorGradients` | scalar path source of truth（标量事实来源） | `recognition/include/pcl/recognition/color_gradient_modality.h` |
| `computeSobelQuantizedScalar` | diagnostic reference | 复刻 Sobel+quantize 子链路 | `test_cgm`, `bench_cgm` | correctness oracle | correctness reference | `test-rvv/recognition/color_gradient_modality/include/impl/cgm_sobel_quantize.hpp` |
| `computeSobelQuantizedCandidate` | candidate helper | RVV build 下尝试向量 angle/quantize | `test_cgm`, `bench_cgm` | output checksum / gtest | production-shaped diagnostic | `test-rvv/recognition/color_gradient_modality/include/impl/cgm_sobel_quantize.hpp` |
| `computeSobelQuantizedStencilCandidate` | candidate helper | RVV build 下尝试 RGB byte stride-load Sobel stencil、量化和 filter | `test_cgm`, `bench_cgm` | output checksum / gtest | production-shaped diagnostic / Phase 050 production input | `test-rvv/recognition/color_gradient_modality/include/impl/cgm_sobel_quantize.hpp` |
| `filterQuantizedGradientsCandidate` | candidate helper | RVV build 下尝试 3x3 dominant filter | `test_cgm`, `bench_cgm` | filtered map checksum / gtest | production-shaped diagnostic | `test-rvv/recognition/color_gradient_modality/include/impl/cgm_sobel_quantize.hpp` |
| `computeSobelQuantizedFilteredCandidate` | candidate helper | 串接 Sobel+quantize 与 dominant filter | `test_cgm`, `bench_cgm` | filtered map checksum / gtest | production-shaped diagnostic / partial-production-candidate | `test-rvv/recognition/color_gradient_modality/include/impl/cgm_sobel_quantize.hpp` |
| `computeColorGradientPipelineStd` | production helper | Gaussian 后的标量 Sobel+quantize+filter 链路 | `processInputData` | spread | production fallback source of truth | `recognition/include/pcl/recognition/color_gradient_modality.h` |
| `computeColorGradientPipelineRVV` | production helper | Gaussian 后的 RVV RGB Sobel stencil + quantize + filter 链路 | `processInputData` | spread | adopted production RVV path | `recognition/include/pcl/recognition/color_gradient_modality.h` |
| `test_cgm` | correctness target | 验证边框、阈值、RVV path、量化一致性、public entry forced scalar/RVV 对拍和 feature 输出一致 | Makefile `run_test_compare` | QEMU output | correctness gate | `test-rvv/recognition/color_gradient_modality/src/test_cgm.cpp` |
| `bench_cgm` | bench wrapper | 生成可解析 bench 输出；`production_process_*` 调用真实 `processInputData()` | Makefile `run_bench_*` | board summary / Evidence Doctor | diagnostic + production direct | `test-rvv/recognition/color_gradient_modality/src/bench_cgm.cpp` |

## 诊断证据链

Phase 000 已完成 Sobel+quantize production-shaped diagnostic：

| evidence | result | boundary |
| --- | --- | --- |
| RED | `run_test_rvv` 曾因 RVV build 返回 `ScalarFallback` 失败 | path-hit 测试有效 |
| correctness | `make -C test-rvv/recognition/color_gradient_modality run_test_compare` 通过，Std/RVV 均 2/2 | QEMU correctness，不含 production dispatch |
| QEMU smoke | `run_bench_rvv --case-filter sobel_quantize_320x240 --iterations 1 --warmup-iterations 1` 可运行 | 日志形状，不作性能结论 |
| asm | `make -C test-rvv/recognition/color_gradient_modality check_cgm_rvv_asm` 通过 | `bench_cgm_rvv` filtered asm 命中 RVV math / convert / `atan2_RVV` |
| board repeated | `cgm_phase000_sobel_quantize_repeated` 5 runs，20 iterations，3 warmup | production-shaped diagnostic only |
| Evidence Doctor | `log/board/repeated_phase000_sobel_quantize/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=4 | suggestions 为环境字段和 binary hash 缺失 |

板卡 repeated summary 显示：

- `sobel_quantize_320x240`: median speedup `2.230x`，range `2.220x` - `2.280x`，`B/A < 1` 为 `0/5`，checksum 一致。
- `sobel_quantize_641x481_tail`: median speedup `2.420x`，range `2.400x` - `2.440x`，`B/A < 1` 为 `0/5`，checksum 一致。

当前 EvidenceDecision（证据决策）：`sobel-selected-angle-quantize-rvv` 是正向
diagnostic signal（诊断信号），可继续作为 full-chain candidate 的组成部分；不接入
production。下一阶段默认进入 `010-dominant-filter-rvv`，闭合
`filterQuantizedColorGradients()` 的 3x3 dominant-bin 语义。

Phase 010 完成 dominant filter production-shaped diagnostic：

| evidence | result | boundary |
| --- | --- | --- |
| correctness | `make -C test-rvv/recognition/color_gradient_modality run_test_compare` 通过，Std/RVV 均 3/3 | QEMU correctness，不含 production dispatch |
| asm | `make -C test-rvv/recognition/color_gradient_modality check_cgm_filter_rvv_asm` 通过 | `bench_cgm_rvv` filtered asm 命中 byte load / compare / merge / store |
| board repeated | `cgm_phase010_dominant_filter_repeated` 5 runs，20 iterations，3 warmup | production-shaped diagnostic only |
| Evidence Doctor | `log/board/repeated_phase010_dominant_filter/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=2 | suggestions 为环境字段和 binary hash 缺失 |

板卡 repeated summary 显示 `filter_dominant_320x240` median speedup `3.480x`，
range `3.470x` - `3.510x`，`B/A < 1` 为 `0/5`，checksum 一致。

Phase 020 完成 Sobel+quantize+filter full-chain production-shaped diagnostic：

| evidence | result | boundary |
| --- | --- | --- |
| RED | `run_test_rvv` 曾因缺少 full-chain helper 和 `RvvFullChain` path 编译失败 | path-hit 测试有效 |
| correctness | `make -C test-rvv/recognition/color_gradient_modality run_test_compare` 通过，Std/RVV 均 4/4 | QEMU correctness，不含 production dispatch |
| QEMU smoke | `run_bench_rvv` 使用 `full_chain_*` case-filter 可运行 | 日志形状，不作性能结论 |
| asm | `make -C test-rvv/recognition/color_gradient_modality check_cgm_full_chain_rvv_asm` 通过 | filtered asm 同时命中 RVV math 和 byte filter 指令 |
| board repeated | `cgm_phase020_full_chain_repeated` 5 runs，20 iterations，3 warmup | production-shaped diagnostic only |
| Evidence Doctor | `log/board/repeated_phase020_full_chain/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=4 | suggestions 为环境字段和 binary hash 缺失 |

板卡 repeated summary 显示：

- `full_chain_320x240`: median speedup `2.320x`，range `2.280x` - `2.340x`，`B/A < 1` 为 `0/5`，checksum 一致。
- `full_chain_641x481_tail`: median speedup `2.480x`，range `2.460x` - `2.490x`，`B/A < 1` 为 `0/5`，checksum 一致。

当前 EvidenceDecision：`full-sobel-quantize-filter-rvv` 是
`partial-production-candidate`（局部生产候选）。证据支持进入 PI1 production
integration plan，但不支持直接采纳：真实 `processInputData()` dispatch、fallback、
production direct correctness、production asm attribution 和 board production benchmark
仍缺失。继续到 PI2 会修改 production 源码，当前停在用户授权门禁。

## Phase 030 production direct evidence

Phase 030 已完成 PI2-PI4，并在 PI5 停止等待用户确认。候选生产补丁只接管
`processInputData()` 中 Gaussian 后、spread 前的内部链路；非 RVV 构建或小图仍走
`computeColorGradientPipelineStd()`。

| evidence | result | boundary |
| --- | --- | --- |
| correctness | `make -C test-rvv/recognition/color_gradient_modality run_test_compare` 通过，Std/RVV 均 7/7 | QEMU correctness；含 public entry、path-hit、forced scalar/RVV 同进程对拍 |
| output semantics | forced scalar/RVV 的 quantized map、spreaded map、`GradientXY` 容差和 `extractFeatures()` 输出一致 | public entry 语义 |
| QEMU smoke | `run_bench_rvv --case-filter production_process_320x240,production_process_641x481_tail --iterations 1 --warmup-iterations 1` 可运行 | 日志形状，不作性能结论 |
| asm | `check_cgm_production_rvv_asm` 通过 | production helper 命中 RVV math / convert / byte filter 指令 |
| board repeated | `cgm_phase030_production_direct_repeated` 5 runs，20 iterations，3 warmup | production direct |
| Evidence Doctor | `log/board/repeated_phase030_production_direct/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=4 | suggestions 为环境字段和 binary hash 缺失 |

板卡 production direct summary 显示：

- `production_process_320x240`: median speedup `1.490x`，range `1.450x` - `1.510x`，`B/A < 1` 为 `0/5`，checksum 一致。
- `production_process_641x481_tail`: median speedup `1.540x`，range `1.530x` - `1.570x`，`B/A < 1` 为 `0/5`，checksum 一致。

首轮 production direct board 曾出现 checksum mismatch；调试确认 root cause（根因）是
bench checksum 把 `GradientXY::angle` 按千分位纳入严格哈希，而 RVV `atan2_RVV_f32m2`
是近似 helper。当前证据拆分为：离散 maps 使用 strict checksum；`GradientXY` 浮点输出和
feature 输出由 gtest 容差 / 等价断言覆盖。

EvidenceDecision：Phase 030 是历史 `adopted production behavior`，后来被 Phase 050
RGB Sobel stencil production path 取代。正式长期主题文档仍为
`doc-rvv/recognition/color_gradient_modality-RVV.zh.md`，当前数据采用 Phase 050
production direct summary；提交 commit 仍需要用户单独要求。

## Phase 040/050 RGB stencil evidence

Phase 040 先在 test-rvv helper 中验证 RGB Sobel stencil RVV candidate；Phase 050 再把同一
code shape 接入真实 `computeColorGradientPipelineRVV()`。

| evidence | result | boundary |
| --- | --- | --- |
| Phase 040 correctness | `make -C test-rvv/recognition/color_gradient_modality run_test_compare` 通过，Std/RVV 均 8/8 | production-shaped diagnostic；不调用真实 production dispatch |
| Phase 040 board | `full_chain_stencil_320x240` median `5.040x`，`full_chain_stencil_641x481_tail` median `4.680x`，checksum 一致 | candidate family screening |
| Phase 050 correctness | `make -C test-rvv/recognition/color_gradient_modality run_test_compare` 通过，Std/RVV 均 8/8 | production direct；含 forced scalar/RVV public entry 对拍 |
| Phase 050 QEMU smoke | production case-filter 极小迭代可运行 | 日志形状，不作性能结论 |
| Phase 050 asm | `check_cgm_production_rvv_asm` 和 `check_cgm_stencil_rvv_asm` 通过 | production helper 命中 `vlse8`、widen / integer Sobel、`vfsqrt`、`atan2_RVV` 和 byte filter |
| Phase 050 board | `cgm_phase050_rgb_stencil_production_direct_repeated` 5 runs | production direct |
| Phase 050 Evidence Doctor | `log/board/repeated_phase050_rgb_stencil_production_direct/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=4 | suggestions 为环境字段和 binary hash 缺失 |

板卡 production direct summary 显示：

- `production_process_320x240`: median speedup `1.620x`，range `1.610x` - `1.680x`，`B/A < 1` 为 `0/5`，checksum 一致。
- `production_process_641x481_tail`: median speedup `1.590x`，range `1.580x` - `1.660x`，`B/A < 1` 为 `0/5`，checksum 一致。

EvidenceDecision：Phase 050 `rgb-stencil-production-rvv` 为当前
`adopted production behavior`。它替换 Phase 030 的标量 Sobel staging production family；
正式长期主题文档数据采用 Phase 050 production direct board summary。
