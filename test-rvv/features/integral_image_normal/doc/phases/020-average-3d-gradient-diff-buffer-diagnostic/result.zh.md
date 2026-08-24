# Phase 020: Average 3D Gradient Diff Buffer Diagnostic Result

## 当前结论

本阶段完成 `initAverage3DGradientMethod()` diff_x / diff_y buffer（差分缓冲区）的 test helper diagnostic（测试辅助入口诊断）。正确性、反汇编、板卡 repeated summary（重复板卡摘要）和 Evidence Doctor（证据体检）都已闭合，但性能信号是 size-dependent（依赖规模）：`641x481_tail` 大图稳定正向，`320x240` 接近 1.0 且出现 1/5 退化。

EvidenceDecision（证据决策）：`weak-size-dependent-diagnostic / no-production-now`。本阶段不支持进入 production patch（生产补丁），也不改变 Phase 010 map-prep production probe（生产探针）仍需用户授权的状态。

## 执行范围回填

| planned action | status | evidence | result / gap |
| --- | --- | --- | --- |
| RED correctness | done | `make run_test_compare` 首次失败，报 `buildAverage3DGradientDiffBuffersRVV` 不是 `iin` 成员 | RED 失败原因符合预期，证明新测试能抓到缺失 helper。 |
| GREEN helper | done | `include/integral_image_normal.h`、`include/impl/integral_image_normal_map_prep.hpp`；`make run_test_compare` | Std/RVV QEMU 各 4 个 gtest 全部通过。 |
| bench extension | done | `src/bench_integral_image_normal.cpp` | 新增 `avg3d_diff_320x240`、`avg3d_diff_641x481_tail`；raw log metadata 使用 `timer_boundary=mixed-by-case`，具体边界由 manifest 按 case 给出。 |
| manifest extension | done | `script/generate_integral_image_normal_evidence_manifest.py` | manifest 将 `map_prep_*` 和 `avg3d_diff_*` 分到不同 group、checksum policy 和 timer boundary。 |
| asm check | done | `make dump_test_rvv` + `rg 'vlse32\\.v|vsse32\\.v|vfsub\\.vv'` | RVV test binary 命中 `vlse32.v`、`vfsub.vv`、`vsse32.v`，归属为 test helper binary，不是 production symbol。 |
| board repeated | done | `make run_board_integral_image_normal_repeated` | 5-run completed；summary / manifest / doctor / registry 已刷新。 |
| Evidence Doctor + registry | done | `log/board/evidence_doctor.md`、`make evidence_status` | Errors=0，Warnings=1，Suggestions=1；registry check fresh。 |
| docs | done | 本 result、matrix、roadmap、evaluation、topic-local docs、Handoff | 当前结论写成 weak / size-dependent diagnostic，不写成 production-ready。 |

## 正确性和反汇编证据

`run_test_compare` 在 QEMU（仿真器）下分别运行 Std 和 RVV 构建：

- `IntegralImageNormalMapPrepRVV.*`：2 个旧 map-prep 测试通过。
- `IntegralImageNormalAverage3DGradientRVV.MatchesScalarDiffBuffersForInteriorAndBorders`：验证内圈 diff_x / diff_y 与标量参考逐元素一致，边界和第四通道保持 0。
- `IntegralImageNormalAverage3DGradientRVV.LeavesTooSmallImagesZeroInitialized`：验证小于有效内圈的输入安全保留零 buffer。

反汇编证据来自 `build/asm/riscv/test_integral_image_normal_rvv.asm`。本阶段关键指令包括 `vlse32.v`（跨步加载）、`vfsub.vv`（向量相减）和 `vsse32.v`（跨步存储）。这些指令证明 test binary 的 RVV path（RVV 链路）存在；它不能证明 production `initAverage3DGradientMethod()` 已经命中 RVV。

## 板卡 repeated summary

当前 repeated board summary 路径为 `log/board/repeated-summary.md`，由 `make run_board_integral_image_normal_repeated` 重新采集 5 run 后生成。性能结论只来自板卡，不使用 QEMU timing（QEMU 计时）。

| case | min speedup | median speedup | mean speedup | max speedup | `B/A < 1` | checksum | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `avg3d_diff_320x240` | 0.9966x | 1.0068x | 1.0186x | 1.0793x | 1/5 | `20440.8` | weak / needs_review |
| `avg3d_diff_641x481_tail` | 1.3325x | 1.3368x | 1.3363x | 1.3398x | 0/5 | `150825` | positive for diagnostic boundary |
| `map_prep_320x240` | 5.4620x | 5.5150x | 5.5164x | 5.5628x | 0/5 | `1.7621e+07` | positive |
| `map_prep_641x481_tail` | 5.1455x | 5.2352x | 5.2180x | 5.2668x | 0/5 | `1.41569e+08` | positive |

`avg3d_diff_320x240` 的首轮 Std 侧偏慢导致 max 较高，后续 run 回到 1.0 附近，并有一次低于 1。因此当前不能把 diff-buffer 候选写成稳定加速；它只说明大图 tail case 可能受益。

## Evidence Doctor 和异常解释

Evidence Doctor 输出：Errors=0，Warnings=1，Suggestions=1。

| finding | case | 解释 | 处理动作 |
| --- | --- | --- | --- |
| `ba_degradation_frequency` warning | `avg3d_diff_320x240` | 5-run 中 1 次 `B/A < 1`，median 仅 1.01x，说明常规规模收益接近噪声边界。 | 本阶段降级为 weak diagnostic，不进入 production patch；若未来 profile 证明该段是主成本，可扩大 runs 或做同 production boundary A/B。 |
| `near_threshold_ba` suggestion | `avg3d_diff_320x240` | median 离 1.0 不足 0.05，容易被输入分布或频率波动反转。 | 写入 roadmap 的暂缓条件；不追加自动复跑，因为 decision bucket 已足以支持 no-production-now。 |

registry 状态：`make evidence_status` 输出 `evidence registry check: fresh`。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar for implementation-shape（实现形态） |
| diagnostic 是否可外推到 production | no。它只覆盖测试专用 4-float stride 点型和 diff buffer 写入，不包含真实 `PointInT` traits、对象状态和积分图构建。 |
| comparison-boundary / baseline mismatch 风险 | diagnostic 内部低；外推 production 风险高，因为 production 计时还包含 `integral_image_DX_ / DY_.setInput()` 和后续 normal solver。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 仅在 full profile 或 production-shaped ablation 证明 diff-buffer 是主成本，且 PI1 能冻结 traits / fallback / dispatch 时允许。当前不建议直接做。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段没有 production boundary，所以不 clean-adopt。 |

## Optimization matrix update

| candidate family | row source policy | point type / Scalar / layout | correctness | bench / board | asm | doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| average 3D gradient diff buffer RVV | organized image | `XYZPadPoint`, float, 4-float stride | pass | size-dependent：small weak / tail positive | pass for test binary | Error=0, Warning=1, Suggestion=1 | `weak-size-dependent-diagnostic / no-production-now` | do not enter production patch; resume only after profile or production-shaped ablation shows this slice dominates |

## 继续 / 停止判断

stop_condition_hit：继续把 diff-buffer 候选接入 production 需要扩大到未授权 production 文件，并且当前 evidence bucket 也不足以支撑生产探针。这里命中权限边界和证据边界两个停止条件。

micro_stop_guard：未把“新增一个 helper”当作终点；本阶段已经完成 correctness、asm、board repeated、Evidence Doctor、registry 和文档回填。当前仍有 Phase 010 map-prep production probe 的授权缺口，但它需要用户确认 PI2 才能继续；不因本阶段弱正向而绕过。

next_phase_default：回到 `010-pi1-production-integration-plan` 的生产授权门禁。若用户继续要求不改 production，则优先考虑 full-profile / component ablation（组件消融）来确认 distance transform、diff buffer 或 normal solver 哪一段是真正主成本，而不是直接重写生产路径。
