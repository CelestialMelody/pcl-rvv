# Phase 030: production integration result

## 当前状态

本阶段已从 PI1 计划推进到 PI5 production evidence decision（生产证据决策）。
当前 production patch 已按用户确认视为 adopted production behavior（已采纳生产行为）。
用户确认策略为：接入后板卡 production direct（真实生产路径）测试显示有收益即可采纳。

## 实际执行范围

本阶段只接入 `ColorGradientModality<PointXYZRGB>::processInputData()` 中 Gaussian
convolution（高斯卷积）之后、`QuantizedMap::spreadQuantizedMap()` 之前的内部链路：

1. `computeMaxColorGradientsSobel(smoothed_input_)`
2. `quantizeColorGradients()`
3. `filterQuantizedColorGradients()`

未修改 public API（公开接口）、Gaussian、spread、feature extraction，也未扩大到直接
RVV 读取模板 `PointInT` 的 RGB 字段。

## 生产补丁摘要

| 文件 | 变更 | 边界 |
| --- | --- | --- |
| `recognition/include/pcl/recognition/color_gradient_modality.h` | 抽出 `computeColorGradientPipelineStd()`；在 `__RVV10__` 下新增 `computeColorGradientPipelineRVV()`；`processInputData()` 在 Gaussian 后尝试 RVV，失败时回退 Std | production candidate；非 RVV 构建保留标量路径 |
| `test-rvv/recognition/color_gradient_modality/src/test_cgm.cpp` | 新增 production direct gtest，覆盖 public entry path-hit、forced scalar/RVV 同进程对拍、feature 输出一致 | QEMU correctness（正确性）和 fallback gate |
| `test-rvv/recognition/color_gradient_modality/src/bench_cgm.cpp` | 新增 `production_process_320x240` / `production_process_641x481_tail` case，通过真实 `processInputData()` 计时 | production direct board bench |
| `test-rvv/recognition/color_gradient_modality/Makefile` | 新增 `check_cgm_production_rvv_asm`；`EVIDENCE_ROLE_REPEATED` 可配置；freshness scan 覆盖 Phase 030 evidence | asm / registry gate |
| `script/generate_cgm_evidence_manifest.py` | 识别 production direct case，生成 `production_direct` manifest metadata | Evidence Doctor 输入 |

## 正确性和 fallback 结果

| evidence | result | boundary |
| --- | --- | --- |
| production direct correctness | `make -C test-rvv/recognition/color_gradient_modality run_test_compare` 通过，Std/RVV 构建均 7/7 | QEMU correctness；包含 public entry、path-hit、forced scalar fallback |
| forced scalar vs RVV | RVV build 中同一输入的 quantized map、spreaded map、`GradientXY` 容差和 `extractFeatures()` 输出均一致 | public entry 语义对拍 |
| small shape fallback | `computeColorGradientPipelineRVV()` 对 `width < 3 || height < 3` 返回 false，`processInputData()` 自然调用 Std helper | 小图保持标量语义 |
| non-RVV build | `__RVV10__` 未启用时不会编译 RVV helper，`processInputData()` 调用 Std helper | 编译期 fallback |

`GradientXY::angle` 使用 `atan2_RVV_f32m2` 近似；它不是 strict libm replacement
（严格 libm 替换）。本阶段把浮点角度放在 gtest 容差门禁中检查；board checksum
只覆盖 production 后续 feature path 直接消费的离散 `getQuantizedMap()` 和
`getSpreadedQuantizedMap()`。这样避免把容差内浮点角度差异误判为离散输出不一致。

## QEMU 和反汇编

| evidence | result |
| --- | --- |
| QEMU smoke | `make -C test-rvv/recognition/color_gradient_modality run_bench_rvv BENCH_ARGS="--case-filter production_process_320x240,production_process_641x481_tail --iterations 1 --warmup-iterations 1"` 通过；只验证日志形状和可运行性，不作性能结论 |
| asm | `make -C test-rvv/recognition/color_gradient_modality check_cgm_rvv_asm check_cgm_filter_rvv_asm check_cgm_full_chain_rvv_asm check_cgm_production_rvv_asm` 通过 |

production asm gate 命中 `computeColorGradientPipelineRVV` 或 production case 归属，并检查
`vfsqrt`、`vfcvt` / `vfwcvt`、`vmseq` / `vmsgtu` / `vmsltu` / `vmerge` 和
`atan2_RVV` / `vfdiv`。

## Board repeated 结果

当前生产直连证据路径：

- summary: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase030_production_direct/summary.md`
- manifest: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase030_production_direct/evidence_manifest.json`
- doctor: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase030_production_direct/evidence_doctor.md`
- registry: `test-rvv/recognition/color_gradient_modality/log/evidence_registry.json`

命令：

```bash
make -C test-rvv/recognition/color_gradient_modality board_repeated record_evidence_state_repeated \
  REPEATED_BOARD_TAG=phase030_production_direct \
  REPEATED_BOARD_REMOTE_TAG=phase030_production_direct \
  REPEATED_BOARD_TITLE="CGM production direct repeated board summary" \
  REPEATED_BOARD_RUN_LABEL=cgm_phase030_production_direct_repeated \
  EVIDENCE_ROLE_REPEATED=production_direct \
  EVIDENCE_DOC_REF_PRIMARY=doc/phases/030-production-integration-plan/result.zh.md \
  EVIDENCE_DOC_REF_SECONDARY=doc/color_gradient_modality-evaluation.zh.md \
  CGM_REPEATED_BENCH_ARGS="--case-filter production_process_320x240,production_process_641x481_tail --iterations 20 --warmup-iterations 3"
```

| case | runs | median speedup | range | B/A < 1 | checksum |
| --- | ---: | ---: | --- | ---: | --- |
| `production_process_320x240` | 5 | `1.490x` | `1.450x` - `1.510x` | `0/5` | Std/RVV 一致：`5189351474172833280` |
| `production_process_641x481_tail` | 5 | `1.540x` | `1.530x` - `1.570x` | `0/5` | Std/RVV 一致：`2369217414544299322` |

Decision bucket（决策桶）为 `positive`。QEMU timing 不参与该结论。

## Evidence Doctor

`log/board/repeated_phase030_production_direct/evidence_doctor.md` 结果：

- Errors: `0`
- Warnings: `0`
- Suggestions: `4`

Suggestions 是环境字段 `taskset/governor/freq/temperature` 和 binary hash 缺失。它们不阻塞
当前 positive 结论，但后续提交或更严格 production review 可以补二进制身份和环境摘要。

## EvidenceDecision

当前 production direct evidence 支持并已进入 `adopted production behavior`：

- public entry: 真实调用 `ColorGradientModality<PointXYZRGB>::processInputData()`。
- correctness: QEMU Std/RVV 均通过，RVV build 中 forced scalar 和 RVV 输出一致。
- performance: 板卡 5-run repeated 为稳定 positive，两个 case 都无 `B/A < 1`。
- asm: production helper 有 RVV 指令归属。
- doctor: 无 Error / Warning。

已创建正式长期文档
`doc-rvv/recognition/color_gradient_modality-RVV.zh.md`，数据采用本阶段 production direct
board summary。该文档只记录当前已采纳 production 行为；Phase 000-020 的诊断探索仍归属
topic-local evaluation / phase 文档。

## 继续 / 停止决策

- `continue_stop_decision`: PI5 adoption confirmed，进入 S11 production closeout 后继续
  Phase 040。
- `stop_condition_hit`: none。
- `next_phase_default`: `040-rgb-stencil-rvv-ablation`，尝试 RGB Sobel stencil RVV
  作为同边界 production-shaped diagnostic A/B。
- `unblocked_next_actions`: Phase 040 已有计划入口；若 stencil candidate 板卡不成立，则
  以 evidence rejected 关闭，不接 production。
