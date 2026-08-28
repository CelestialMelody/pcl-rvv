# Phase 020 Result: full-chain production-shaped diagnostic

## 执行范围

本阶段完成 `full-sobel-quantize-filter-rvv` 的 production-shaped diagnostic
（生产形态诊断）：输入 organized RGB，经 Sobel+quantize 子链路生成 quantized map，
再经 dominant filter 输出 one-hot filtered map。production 源码未修改；本阶段不包含
Gaussian smoothing、`QuantizedMap::spreadQuantizedMap()`、`extractFeatures()` 或真实
`ColorGradientModality::processInputData()` public entry（公开入口）。

## 动作回填

| action | status | evidence |
| --- | --- | --- |
| RED gtest | done | `run_test_rvv` 曾因缺少 `computeSobelQuantizedFilteredScalar`、`computeSobelQuantizedFilteredCandidate` 和 `RvvFullChain` 编译失败，证明测试先于实现命中新入口 |
| GREEN helper | done | `computeSobelQuantizedFilteredScalar` 串接标量 Sobel+quantize 与标量 filter；`computeSobelQuantizedFilteredCandidate` 串接 RVV Sobel+quantize 与 RVV filter |
| correctness | done | `make -C test-rvv/recognition/color_gradient_modality run_test_compare`，Std/RVV 均 4/4 |
| QEMU smoke | done | `run_bench_rvv BENCH_ARGS="--case-filter full_chain_320x240,full_chain_641x481_tail --iterations 1 --warmup-iterations 1"`；仅验证日志形状，不作为性能结论 |
| asm | done | `make -C test-rvv/recognition/color_gradient_modality check_cgm_full_chain_rvv_asm` 通过，filtered asm 同时命中 RVV float math 和 byte filter 指令 |
| board repeated | done | `cgm_phase020_full_chain_repeated`，5 runs，20 iterations，3 warmup |
| Evidence Doctor / registry | done | `record_evidence_state_repeated` 生成 summary / manifest / doctor 并登记 registry |

## 板卡结果

证据路径：

- summary: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase020_full_chain/summary.md`
- manifest: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase020_full_chain/evidence_manifest.json`
- doctor: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase020_full_chain/evidence_doctor.md`
- registry: `test-rvv/recognition/color_gradient_modality/log/evidence_registry.json`

| case | runs | median speedup | min | max | B/A < 1 | checksum |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `full_chain_320x240` | 5 | `2.320x` | `2.280x` | `2.340x` | `0/5` | Std/RVV 一致：`18061343419196392793` |
| `full_chain_641x481_tail` | 5 | `2.480x` | `2.460x` | `2.490x` | `0/5` | Std/RVV 一致：`11024222116537967177` |

Decision bucket（决策桶）为 `positive`。远端 make 每轮仍出现 clock skew warning
（板卡文件时间戳偏移警告）；当前没有 checksum、方向或长尾异常，因此记录为环境风险，
不降级本阶段 production-shaped diagnostic 结论。

## Evidence Doctor

`log/board/repeated_phase020_full_chain/evidence_doctor.md` 结果：

- Errors: 0
- Warnings: 0
- Suggestions: 4

Suggestions 为两个 case 分别缺少 `taskset / governor / freq / temperature`
环境字段，以及缺少 `binary_hash`。处理策略：不阻塞 Phase 020，因为 repeated direction
稳定、checksum 一致、run_count 达到计划预算；若进入 production direct 阶段，应补环境摘要、
binary identity，并优先修正远端 clock skew。

## 诊断到 production 错配审计回填

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper `computeSobelQuantizedFilteredCandidate` vs scalar full-subchain reference |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断组合子链路是否支持进入 production integration plan |
| diagnostic 是否可外推到 production | 不能直接外推；它覆盖 Sobel+quantize+filter 连续片段，但缺 Gaussian、spread、对象状态、真实 public entry 和 fallback dispatch |
| comparison-boundary / baseline mismatch 风险 | 有；Std/RVV 两侧共享 test helper wrapper，但 wrapper 不等于 production public overload |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段结果为 positive；若后续 production direct 变弱或退化，应按同一 production boundary 复核，而不是直接回写 no-production |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；当前没有 production patch，也没有 production direct evidence |

## 阶段反思

组合子链路仍保持 Phase 000/010 的正向方向，说明中间 quantized map 提取和 filter 串接没有吞掉
主要收益。但当前 helper 仍用 `GradientCell` vector 暂存 magnitude/angle/quantized，再单独抽取
quantized byte map；如果 production direct 收益低于 diagnostic，优先消融 buffer layout（缓冲布局）和
Gaussian / spread 边界。

QEMU smoke 中早期低迭代 checksum 曾看起来低熵；完整 board repeated 的 Std/RVV checksum 一致，
且两个规模有不同 fingerprint。该信号不影响 correctness，但后续若要证明真实 feature quality，
应补更接近真实图像的 input distribution（输入分布）或 production direct test。

## 阶段决策

`full-sobel-quantize-filter-rvv` 标为 `partial-production-candidate`。当前证据支持进入
PI1 production integration plan（生产接入计划），但不支持直接修改或采纳 production：
还缺真实 `processInputData()` dispatch、fallback、production direct correctness、production asm
attribution（生产反汇编归属）和 board production benchmark。

`continue_stop_decision`: stop at production authorization gate。

`stop_condition_hit`: 继续到 PI2 需要修改 production 源码，当前 prompt 未明确授权直接进入
production integration loop；按仓库规则停在 PI1 计划 / 用户检查点。

`next_phase_default`: `030-production-integration-plan`；若用户明确授权推进 production integration
loop，则按该 plan 先冻结 PI1 gate，再连续执行 PI2-PI5，并在 PI5 后等待用户确认采纳或回滚。
