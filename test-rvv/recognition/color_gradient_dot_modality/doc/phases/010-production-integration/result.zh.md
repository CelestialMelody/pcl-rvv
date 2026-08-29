# Phase 010: production integration result

## 当前结论

`recognition/include/pcl/recognition/color_gradient_dot_modality.h` 已接入 production RVV path（生产 RVV 路径）。
`processInputData()` 在 RVV 构建下调用 `computeMaxColorGradientsRVV()`，非 RVV 构建保持原标量 helper；
`computeDominantQuantizedGradients()` 继续保持标量实现。基于本阶段 production direct 板卡证据和用户本轮授权，
该 production patch 采纳为 adopted production behavior（已采用生产行为）。

## production diff 摘要

| file | change | boundary |
| --- | --- | --- |
| `recognition/include/pcl/recognition/color_gradient_dot_modality.h` | 新增 `computeMaxColorGradientsRVV()`，使用 `vlse8` 跨步读取 `PointXYZRGB` 的 R/G/B 字段，RVV 计算通道差分、平方幅值、通道选择、`vfsqrt` 和 `atan2_RVV_f32m2`。 | 只接管 gradient map 生成；dominant bin scan 保持标量。 |
| `test-rvv/recognition/color_gradient_dot_modality/*` | 新增 correctness / bench / evidence wrapper 和 topic 文档。 | production direct 证据入口。 |

## 验证结果

| gate | result |
| --- | --- |
| correctness | `make -C test-rvv/recognition/color_gradient_dot_modality run_test_compare` 通过 Std/RVV 3 个 gtest。 |
| QEMU smoke | `process_input_320x240` 小迭代 smoke 通过；QEMU 只证明可运行和日志形状。 |
| asm | `check_cgdm_rvv_asm` 通过，`bench_cgdm_rvv` 中可见 RVV load / convert / `vfsqrt` / `atan2_RVV` 指令。 |
| board production direct | `log/board/repeated_phase010_production_direct/summary.md`。 |
| Evidence Doctor | `log/board/repeated_phase010_production_direct/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=4。Suggestions 为环境 metadata 和 binary identity 建议，不阻塞采纳。 |

## production direct 板卡结果

| case | runs | median speedup | p10 - p90 | B/A < 1 | checksum |
| --- | ---: | ---: | ---: | ---: | --- |
| `process_input_320x240` | 5 | `2.600x` | `2.568x` - `2.628x` | 0/5 | `16942614990468929485` |
| `process_input_641x481_tail` | 5 | `3.510x` | `3.484x` - `3.516x` | 0/5 | `6040139219709186475` |

## 未覆盖范围

本阶段 production evidence 只覆盖 organized `PointXYZRGB`、`float` magnitude/angle state、默认 AoS layout 和
`bin_size=4` 的两个代表规模。`computeInvariantQuantizedMap()` 的 region / mask 驱动路径仍未 RVV 化；
`recognition/src/dotmod.cpp` 的模板匹配 sliding-window scoring 是相邻独立 topic。

## continue / stop decision

当前 production path 已有稳定收益，可作为本 topic closeout。剩余 `cgdm-invariant-map-rvv` 有理论空间，但它临时改写
`color_gradients_` 并恢复，状态风险高，且本轮没有证明它是当前生产热点；因此不在本 topic closeout 前继续扩展。
后续只有在 template creation profile 指向 `computeInvariantQuantizedMap()` 时再开新 phase。
