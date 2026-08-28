# Phase 000 Result: current-state and CGM scaffold

## 执行范围

本阶段实际完成 `sobel-selected-angle-quantize-rvv` 的 test-rvv scaffold、
same-chain correctness（同构链路正确性）、QEMU 日志形状 smoke、反汇编门、
5-run board repeated（板卡重复性能测试）、Evidence Doctor（证据体检）和
evidence registry（证据登记）。production 源码
`recognition/include/pcl/recognition/color_gradient_modality.h` 未修改。

阶段存在已记录偏差：RED 测试资产先于 `plan.zh.md` 写入，因此
`phase_plan_written_before_edits=partial`。本 result 不把该门禁写成 pass。

## 动作回填

| action | status | evidence |
| --- | --- | --- |
| RED gtest | done | `run_test_rvv` 曾因 RVV build 返回 `ScalarFallback` 失败，证明 path-hit 测试有效 |
| GREEN candidate | done | `make -C test-rvv/recognition/color_gradient_modality run_test_compare`，Std 2/2、RVV 2/2 |
| bench smoke | done | `run_bench_rvv BENCH_ARGS="--case-filter sobel_quantize_320x240 --iterations 1 --warmup-iterations 1"`；仅作 QEMU 日志形状和可运行性 |
| asm | done | `make -C test-rvv/recognition/color_gradient_modality check_cgm_rvv_asm` 通过，filtered asm 命中 vector load/convert/math 和 `atan2_RVV` |
| board repeated | done | `make -C test-rvv/recognition/color_gradient_modality board_repeated`，5 runs，20 iterations，3 warmup |
| Evidence Doctor | done | `log/board/repeated_phase000_sobel_quantize/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=4 |
| evidence registry | done | `make -C test-rvv/recognition/color_gradient_modality record_evidence_state_repeated` 登记 summary / manifest / doctor / doctor json |

## 板卡结果

证据路径：

- summary: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase000_sobel_quantize/summary.md`
- manifest: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase000_sobel_quantize/evidence_manifest.json`
- doctor: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase000_sobel_quantize/evidence_doctor.md`
- registry: `test-rvv/recognition/color_gradient_modality/log/evidence_registry.json`

| case | runs | median speedup | min | max | B/A < 1 | checksum |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `sobel_quantize_320x240` | 5 | `2.230x` | `2.220x` | `2.280x` | `0/5` | Std/RVV 一致：`3118309285021054092` |
| `sobel_quantize_641x481_tail` | 5 | `2.420x` | `2.400x` | `2.440x` | `0/5` | Std/RVV 一致：`10554169072401616548` |

Decision bucket（决策桶）为 `positive`。远端 make 每轮出现 clock skew warning
（板卡文件时间戳偏移警告）；本阶段未观察到 checksum、方向或长尾异常，因此记录为
环境风险，不降级当前 diagnostic 结论。若后续出现方向反转，应优先修正板卡时间或记录
binary hash（编译产物指纹）后重跑。

## Evidence Doctor

`log/board/repeated_phase000_sobel_quantize/evidence_doctor.md` 结果：

- Errors: 0
- Warnings: 0
- Suggestions: 4

Suggestions 为两个 case 分别缺少 `taskset / governor / freq / temperature`
环境字段，以及缺少 `binary_hash`。处理策略：不阻塞 Phase 000，因为 repeated direction
稳定、checksum 一致、run_count 达到计划预算；后续 production-shaped full-chain 或
production direct 阶段应补环境摘要和 binary identity。

## 诊断到 production 错配审计回填

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper `computeSobelQuantizedCandidate` vs scalar reference |
| 当前决策问题 | RVV-vs-scalar diagnostic；不做 production adoption |
| diagnostic 是否可外推到 production | 不能直接外推；它覆盖 Sobel+quantize 子链路，但不包含 Gaussian smoothing、dominant filter、spread、feature extraction 或 public entry |
| comparison-boundary / baseline mismatch 风险 | 有；bench wrapper 不是 `ColorGradientModality::processInputData()` |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段结果为 positive；若后续子链路弱/负，也必须先完成 full-chain mismatch audit，不能直接 no-production |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；当前没有 production patch，也没有 production direct evidence |

## 阶段决策

`sobel-selected-angle-quantize-rvv` 标为 `attempted-positive diagnostic`：
当前 helper 在板卡上对两个规模均有稳定正向信号，值得作为后续 full-chain candidate
的组成部分保留；但由于 Sobel stencil 仍标量 staging，且 production 未接入，本阶段不进入
production integration loop。

`continue_stop_decision`: continue。

`stop_condition_hit`: none。

`next_phase_default`: `010-dominant-filter-rvv`。

理由：roadmap 和 matrix 仍有当前授权范围内的 high-priority unblocked action。
下一阶段应先闭合 `filterQuantizedColorGradients()` 的 3x3 dominant filter 语义，再决定
是否进入 full Sobel RGB stencil 或 full `processInputData()` production-shaped diagnostic。
