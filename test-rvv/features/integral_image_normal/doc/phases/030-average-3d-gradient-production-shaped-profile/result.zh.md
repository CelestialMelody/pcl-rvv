# Phase 030: Average 3D Gradient Production-Shaped Profile Result

## 当前结论

本阶段新增 AVERAGE_3D_GRADIENT 的 production-shaped profile（生产形态剖析，测试代码尽量模拟生产阶段边界）和 component ablation（组件消融，拆分 diff-buffer、积分图构建和 normal query 三段耗时）。生产源码仍未修改。

EvidenceDecision（证据决策）：`negative/unstable-production-shaped-diagnostic / no-production-now`。

Phase 020 的 diff_x / diff_y buffer（差分缓冲区）局部收益在完整 profile total（剖析总链路）中被积分图构建和 normal query 成本稀释。`avg3d_profile_641x481_tail` 5-run mean 为 `0.98x`、median 为 `1.00x`，且 2/5 run 退化；Evidence Doctor（证据体检）给出 Error。当前不建议把 diff-buffer 接入 production，也不建议仅凭该候选申请 PI2。

map-prep 候选仍稳定 positive，但它属于 Phase 010 的生产接入门禁；修改 `features/include/pcl/features/impl/integral_image_normal.hpp` 仍需要用户显式确认。

## 执行范围回填

| planned action | status | evidence | result / gap |
| --- | --- | --- | --- |
| 扩展 bench profile harness | done | `src/bench_integral_image_normal.cpp` | 新增 `avg3d_profile_*` total 和 `profile_component_*` 三段计时 / checksum。 |
| 扩展 manifest wrapper | done | `script/generate_integral_image_normal_evidence_manifest.py` | 新增 profile total 和 component group；summary 说明 production-shaped diagnostic 边界。 |
| 更新 Makefile evidence refs | done | `Makefile` | repeated label / run label 更新到 Phase 030；registry doc-ref 包含本 result。 |
| QEMU correctness / log-shape | done | `make run_test_compare`、`run_bench_std BENCH_ARGS=1`、`run_bench_rvv BENCH_ARGS=1` | gtest 仍通过；QEMU bench 只作日志形状检查，不作性能结论。 |
| asm check | done | `make dump_bench_rvv` + `rg` | bench RVV binary 包含 map-prep 和 avg3d diff-buffer 相关 RVV 指令；不是 production symbol attribution。 |
| board repeated | done | `make run_board_integral_image_normal_repeated` | 5-run 完成，summary / manifest / doctor / registry 已刷新。 |
| Evidence Doctor + registry | done with downgrade | `log/board/evidence_doctor.md`、`log/evidence_registry.json` | Errors=5，Warnings=12，Suggestions=4；Errors 来自 profile/component 退化频率，处理为降级结论。 |

## 正确性和日志形状

`run_test_compare` 在 QEMU（仿真器）下分别运行 Std 和 RVV 构建；两侧各 4 个 gtest 全部通过。新增 profile harness 不改变 correctness test（正确性测试）矩阵，它复用 Phase 020 已验证的 diff-buffer helper，并用 checksum 检查 profile 输出形状。

QEMU bench smoke（小型日志形状检查）使用 `BENCH_ARGS=1`，确认新 case label 和 checksum 行可被 manifest wrapper 解析。QEMU 计时不进入性能判断。

## 反汇编证据

`make dump_bench_rvv` 生成 `build/asm/riscv/bench_integral_image_normal_rvv.asm`。其中可见 `vlse32.v`、`vfsub.vv`、`vle32.v`、masked `vse8.v`、`vmseq`、`vmerge` 和 `vsetvli` 等指令。该证据只说明 bench / test helper binary 的 RVV path（RVV 链路）存在，不说明 production `initAverage3DGradientMethod()` 或 `computeFeature()` 已命中 RVV。

## 板卡 repeated summary

当前 repeated board summary 路径为 `log/board/repeated-summary.md`。性能结论只来自板卡，不使用 QEMU timing（QEMU 计时）。

| case | min speedup | median speedup | mean speedup | max speedup | `B/A < 1` | checksum | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `avg3d_profile_320x240` | 0.9365x | 1.0589x | 1.0498x | 1.1413x | 1/5 | `-5.85432e+07` | weak / unstable |
| `avg3d_profile_641x481_tail` | 0.8762x | 1.0043x | 0.9824x | 1.0715x | 2/5 | `-5.6621e+07` | negative / unstable |
| `profile_component_diff_320x240` | 1.0017x | 1.0198x | 1.0300x | 1.0612x | 0/5 | `20440.8` | weak |
| `profile_component_diff_641x481_tail` | 1.0709x | 1.1213x | 1.1097x | 1.1545x | 0/5 | `150825` | weak-positive diagnostic |
| `profile_component_integral_320x240` | 0.6825x | 0.9921x | 0.9020x | 1.0595x | 4/5 | `8.29818e+06` | negative / unstable |
| `profile_component_integral_641x481_tail` | 0.7860x | 0.9039x | 0.9333x | 1.1016x | 4/5 | `3.49725e+07` | negative / unstable |
| `profile_component_query_320x240` | 0.8809x | 0.9982x | 0.9886x | 1.0711x | 3/5 | `-5.85432e+07` | neutral / unstable |
| `profile_component_query_641x481_tail` | 0.8868x | 1.0063x | 1.0041x | 1.0866x | 2/5 | `-5.6621e+07` | neutral / unstable |

上下文 case 同时刷新：

| case | median speedup | mean speedup | decision |
| --- | ---: | ---: | --- |
| `map_prep_320x240` | 5.4998x | 5.4998x | positive |
| `map_prep_641x481_tail` | 5.1720x | 5.1871x | positive |
| `avg3d_diff_320x240` | 1.0305x | 1.0228x | weak / near-threshold |
| `avg3d_diff_641x481_tail` | 1.3412x | 1.3529x | positive for diff-only diagnostic |

## Evidence Doctor 和异常处理

Evidence Doctor 输出：Errors=5，Warnings=12，Suggestions=4。

| severity | finding group | 解释 | 处理动作 |
| --- | --- | --- | --- |
| Error | `avg3d_profile_641x481_tail` 2/5 退化 | 完整 profile total 在大图 tail 上 mean < 1，median 贴近 1，不能支撑 diff-buffer production probe。 | 降级为 `negative/unstable-production-shaped-diagnostic`；不追加自动复跑，因为当前决策已经是不生产化 diff-buffer。 |
| Error | `profile_component_integral_*` 退化频率高 | component 行是同一 harness 下的 cross-build 诊断，不是单独 RVV 候选；它说明积分图构建成本占比大且 profile 对编译 / 运行波动敏感。 | 只作为瓶颈线索，不写成 integral RVV 负向或正向采纳结论。 |
| Error / Warning | `profile_component_query_*` 和 profile total 长尾 | query loop 成本占比较大，且部分 run 方向反转。 | 记录为后续 full profile / exact PCL IntegralImage2D 审计的恢复条件；不从 diff-buffer 外推 production。 |
| Warning / Suggestion | `avg3d_diff_320x240` near-threshold | 与 Phase 020 一致，小图 diff-buffer 仍接近 1.0。 | 维持 Phase 020 的 no-production-now。 |

这些 Error 不是 checksum 错误，也不是脚本解析失败；它们是“不足以支持生产化”的证据。当前阶段用降级 EvidenceDecision 处理，不把 profile 表写成 strict production performance。

registry 状态由 `record_board_evidence_state` 刷新，run label 为 `board-integral-image-normal-diagnostic-repeated-phase030`。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper / production-shaped helper |
| 计时边界 | `avg3d_profile_*` 包含 diff-buffer、测试专用积分图构建和 normal query；`profile_component_*` 拆分三段。 |
| 当前决策问题 | implementation-shape and bottleneck localization |
| diagnostic 是否可外推到 production | no。它使用测试专用 `XYZPadPoint` 和自包含 profile harness，不覆盖真实 `IntegralImage2D` 对象状态和 public dispatch。 |
| comparison-boundary / baseline mismatch 风险 | medium。Std/RVV 同 wrapper 可比，但 component integral / query 两侧没有新增 RVV candidate，主要用于揭示成本占比和噪声。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | diff-buffer 不允许；map-prep 仍可在用户授权 PI2 后按 Phase 010 进入 bounded production probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。当前没有 production boundary，不能 clean-adopt。 |

## Optimization matrix update

| candidate family | row source policy | point type / Scalar / layout | correctness | bench / board | asm | doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| average 3D gradient production-shaped profile | ordered-organized-image | `XYZPadPoint`, float, 4-float stride | reuse Phase 020 correctness | total weak / unstable; large tail mean 0.98x | pass for bench binary | Errors=5, Warnings=12, Suggestions=4 | `negative/unstable-production-shaped-diagnostic / no-production-now` | do not production-patch diff-buffer; only resume with exact production profile or new algorithm proof |

## 继续 / 停止判断

stop_condition_hit：是。继续当前最高优先级候选需要修改 production source（map-prep PI2）或提出新的积分图 / query 算法证明；前者需要用户显式授权，后者当前没有正向 RVV 候选证据。Phase 030 已完成用户要求的非生产 full-profile / component ablation 恢复动作。

micro_stop_guard：未把单个 bench 当终点；本阶段已完成 plan、bench 扩展、manifest、QEMU correctness/log-shape、asm、5-run board、Evidence Doctor、registry 和文档回填。当前没有应继续自动推进的 diff-buffer production path。

next_phase_default：回到 Phase 010 map-prep PI2 用户授权门禁。若用户明确要求继续非生产探索，建议另开 Phase 040，先做 exact PCL `IntegralImage2D::setInput()` boundary profile（精确 PCL 积分图边界剖析），再决定是否存在可设计的 RVV prefix / query 候选。
