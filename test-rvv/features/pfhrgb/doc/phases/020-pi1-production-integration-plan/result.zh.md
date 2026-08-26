# Phase 020 Production Integration Result

## 执行摘要

Phase 020 已完成 production integration loop（生产接入闭环）并进入 adopted production behavior（已采用生产行为）。
生产源码 `features/include/pcl/features/impl/pfhrgb.hpp` 现在包含 exact-gated（精确门控）
`PointXYZRGBNormal -> PointXYZRGBNormal -> PFHRGBSignature250` RVV 路径；其它模板实例、非 RVV 构建和
不满足 gate 的情况继续走标量 fallback（回退路径）。

接入后的 production-public（生产公开入口）板卡结果为正：`public_pfhrgb_k` 在 Milkv-Jupiter 5-run repeated
board（重复板卡测试）中 B/A 为 `1.28, 1.27, 1.28, 1.27, 1.27`，median `1.27x`，0/5 低于 1，checksum 一致。
用户本轮已明确说明“接入后板卡有收益即可采纳”，因此本阶段结论是采纳当前窄范围 production patch。

## 计划回填

| action | 状态 | 产物 / 证据 | 结论 |
| --- | --- | --- | --- |
| PI1 scope freeze | done | `plan.zh.md`、本 result | 范围冻结为 exact `PointXYZRGBNormal`、`PFHRGBSignature250`、`nr_split=5`、KSearch public boundary。 |
| PI2 production patch | done | `features/include/pcl/features/impl/pfhrgb.hpp` | 新增 Std helper、RVV helper、workspace 和 `computeFeature` 中的 workspace 复用分流；公开 API 不变。 |
| PI3 production direct tests | done | `src/test_pfhrgb.cpp` | `run_test_compare` Std/RVV 两侧各 6 个 gtest 通过，包含 exact public path 和非 exact source fallback 语义。 |
| PI4 production evidence rerun | done | `log/board/repeated/evidence_manifest.json`、`evidence_doctor.md` | `public_pfhrgb_k` 作为 production-public case 稳定正向；helper-only Error 已降级。 |
| PI5 evidence decision | done / adopted | `doc-rvv/features/pfhrgb-RVV.zh.md`、matrix、roadmap、evaluation | 接入后板卡 positive，按用户授权采纳；不需要等待额外 PI5 人工确认。 |

## Production Patch 范围

| 位置 | 作用 | 边界 |
| --- | --- | --- |
| `pcl::pfhrgb_rvv_detail::computePointPFHRGBSignatureStd` | 抽出的标量 helper。 | 保持原 `computePointPFHRGBSignature` 语义，供 fallback 使用。 |
| `PFHRGBPairBatchWorkspace` / staging helpers | 跨点复用 SoA staging 和 tuple buffers。 | 只在 `__RVV10__` 下参与 RVV path。 |
| `computePointPFHRGBSignatureRVV` | 批量计算几何 tuple 和 RGB ratio。 | 仅 `nr_split==5` 且邻域规模不少于 4 时返回 true；histogram scatter 保持标量顺序。 |
| `computePointPFHRGBSignature` / `computeFeature` | 分流入口。 | exact `PointXYZRGBNormal` / `PFHRGBSignature250` 尝试 RVV；失败自然回到 Std。 |

## Correctness（正确性）

`make -B -C test-rvv/features/pfhrgb run_test_compare` 已通过。Std 与 RVV 两侧均运行 6 个 gtest：

| test family | 覆盖 |
| --- | --- |
| reference | 公开 estimator 第一个 descriptor 与测试专用 scalar reference 对拍。 |
| production exact path | exact `PointXYZRGBNormal` 公开入口每个 descriptor 与 scalar reference 对拍。 |
| fallback boundary | `PointXYZRGB` source + `PointXYZRGBNormal` normals 的非 exact source 模板实例保持标量语义。 |
| candidate diagnostics | pair-batch candidate、public-shaped wrapper、reusable workspace 仍保持数值一致。 |

这证明当前窄范围 production path 的 descriptor 语义和 fallback 分流没有破坏；它不证明泛型 RGB traits、
radius search、其它 `PointOutT` 或真实业务数据集。

## ASM Attribution（反汇编归属）

`make -B -C test-rvv/features/pfhrgb check_production_rvv_symbol` 已通过。该 target 生成
`build/asm/riscv/bench_pfhrgb_rvv.full.asm` 并检查 `computePointPFHRGBSignatureRVV` 符号；同一 RVV bench
binary 中也包含 `vsetvli`、`vle32.v`、`vfmacc.vv`、`vfdiv.vv` 等 RVV 指令。该证据证明接入后的 RVV
helper 进入 benchmark binary，不外推到非 exact 模板实例。

## Board Evidence（板卡证据）

当前 summary path：`test-rvv/features/pfhrgb/log/board/repeated/evidence_manifest.json`。
Evidence Doctor path：`test-rvv/features/pfhrgb/log/board/repeated/evidence_doctor.md`。

| case | evidence role | B/A values | median | decision |
| --- | --- | --- | --- | --- |
| `public_pfhrgb_k` | production-public | `1.28, 1.27, 1.28, 1.27, 1.27` | `1.27x` | adopted evidence |
| `public_pfhrgb_k_with_candidate_reuse` | production-shaped diagnostic | `1.23, 1.23, 1.25, 1.25, 1.24` | `1.24x` | implementation-shape context |
| `public_pfhrgb_k_with_candidate` | production-shaped diagnostic | `1.21, 1.21, 1.22, 1.22, 1.21` | `1.21x` | implementation-shape context |
| `candidate_pfhrgb_pair_batch_rvv` | diagnostic | `0.98, 0.97, 0.99, 0.98, 0.97` | `0.98x` | helper-only negative |
| `component_pfhrgb_signature` | production-shaped diagnostic | `1.00, 1.00, 1.02, 1.01, 0.99` | `1.00x` | neutral-negative |

Evidence Doctor 当前结果为 Errors=1 / Warnings=1 / Suggestions=6。唯一 Error 是
`candidate_pfhrgb_pair_batch_rvv` 的退化频率；Warning 是 `component_pfhrgb_signature` 有 1/5 低于 1。
这些异常要求 helper-only / component claims 降级，但不阻塞 `public_pfhrgb_k` 的 production-public 结论。
Suggestions 主要是缺少 taskset、governor、freq、temperature 等环境 metadata；当前 B/A 桶稳定，记录为剩余风险。

## Diagnostic-To-Production Mismatch Audit 回填

| question | result |
| --- | --- |
| evidence role | 采纳依据是 `production_public`；接入前 public-shaped diagnostic 只作为实现形态背景。 |
| A/B boundary | `PFHRGBEstimation::compute` public overload（公开入口）+ `computePointPFHRGBSignatureRVV` production helper。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path。 |
| diagnostic 是否可外推到 production | 不外推；本阶段已用接入后的 production-public board evidence 替代。 |
| comparison-boundary / baseline mismatch 风险 | `public_pfhrgb_k` 消除了 topic-local wrapper mismatch；其它 diagnostic case 保持降级。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前 production 只有一个 RVV family，决策是 Std/RVV production-public；不需要同边界 RVV-vs-RVV family selection。 |

## Continue / Stop Decision

`continue_stop_decision`: stop after adoption closeout。

`stop_condition_hit`: 当前同边界接入已经通过 correctness、ASM、board repeated 和 Evidence Doctor 分层审计；剩余方向会扩大到泛型点型、
更多 fallback matrix、真实数据集或新 implementation family，属于新的 phase / topic 范围。helper-only case 当前为负向，
不支持继续做同一边界微优化。

`next_phase_default`: 若继续，建议新建 `050-point-type-expansion`，先按 PCL generic point type strategy
（泛型点类型策略）审计 RGB / normal traits、字段 offset、POD layout 和 fallback 测试，再决定是否扩大 production gate。
