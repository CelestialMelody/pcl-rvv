# 030 production-direct-probe

## 阶段意图和边界

本阶段承接 `020-pi1-production-integration-plan`，在用户已要求继续推进的前提下进入 PI2-PI5。目标是用最小 production patch（生产补丁）验证真实 `DifferenceOfNormalsEstimation::compute()` 公开入口能否在 `PointNT=pcl::Normal`、`PointOutT=pcl::Normal`、`Scalar=float` 的窄范围内命中 RVV path（RVV 执行链路），并重新采集 production direct（真实生产路径证据）。

本阶段不扩大到 normal-like 泛型点类型、不覆盖 `Scalar=double`、不修改 public API（公开接口），也不触碰前置 normal estimation。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| 生产源码 | `computeFeature()` 仍是单体标量循环 | `features/include/pcl/features/impl/don.hpp` |
| PI1 范围 | 已冻结 exact `pcl::Normal` / float / ordered normal cloud | `test-rvv/features/don/doc/phases/020-pi1-production-integration-plan/plan.zh.md` |
| 诊断正确性 | QEMU Std/RVV 3 个 gtest 通过 | `test-rvv/features/don/log/qemu/run_test_std.log`, `test-rvv/features/don/log/qemu/run_test_rvv.log` |
| 诊断板卡性能 | 5-run helper-only median `1.087x`，decision bucket 为 `weak_positive` | `test-rvv/features/don/log/board/repeated_phase010_diagnostic/summary.md` |
| Evidence Doctor | repeated diagnostic Errors=0, Warnings=0, Suggestions=0 | `test-rvv/features/don/log/board/repeated_phase010_diagnostic/evidence_doctor.md` |
| registry | 当前 diagnostic evidence 为 fresh | `test-rvv/features/don/log/evidence_registry.json` |

## PI2 Scope（生产补丁范围）

| 维度 | 冻结值 |
| --- | --- |
| production 文件 | `features/include/pcl/features/impl/don.hpp` |
| public entry | `DifferenceOfNormalsEstimation<PointInT, PointNT, PointOutT>::computeFeature(PointCloudOut&)` |
| covered point type | exact `PointNT=pcl::Normal` and `PointOutT=pcl::Normal` |
| covered scalar | `float` normal fields and curvature |
| row source | ordered normal cloud，按 `input_->size()` 一一对应 |
| min size gate | 当前阶段先不设小规模阈值；若证据显示小规模不稳，再回填 gate |
| forbidden expansion | 不新增 public API，不扩大到 traits-gated normal-like 点型，不改变其它模板实例语义 |

## Fallback Matrix（回退矩阵）

| 条件 | 预期行为 | 证据 |
| --- | --- | --- |
| 非 RVV build | 只编译 / 运行 Std helper | QEMU Std test |
| `PointNT` 非 exact `pcl::Normal` | Std fallback | production fallback gtest |
| `PointOutT` 非 exact `pcl::Normal` | Std fallback | production fallback gtest |
| normal 差产生 NaN / Inf | 输出 normal 三分量置零，curvature 为 0 | production direct correctness |
| input / normal 尺寸不匹配 | 仍由 `initCompute()` 返回失败，不进入 RVV helper | 既有 production 语义，不在本阶段扩展 |

## TDD 红灯计划

先新增一个 production-direct bench（真实生产入口性能测试）和一个 asm gate（反汇编验收条件）：

| action | 产物 | 预期红灯 |
| --- | --- | --- |
| 新增 `bench_don_production.cpp` | 只通过 `DifferenceOfNormalsEstimation::compute()` 计时，不调用 test-only helper | 当前生产源码下可编译运行，但 asm gate 找不到 production RVV 指令组合 |
| 新增 `check_production_rvv_asm` | dump production bench 的 RVV build 反汇编并检查 `vlse32/vfsub/vmerge/vfsqrt/vsse32` | 当前生产源码下失败 |
| 新增 fallback/direct gtest | 检查 exact normal 输出语义和非 exact 输出自然 fallback | 输出语义应先保持通过；路径命中由 asm gate 承担 |

红灯通过后才写 production patch。若红灯意外通过，必须先解释是否由 compiler auto-vectorization（编译器自动向量化）导致，并调整 gate 归属。

## 实现动作

| action | 文件 | 完成判据 |
| --- | --- | --- |
| PI3 test-first | `test-rvv/features/don/src/test_don.cpp`, `src/bench_don_production.cpp`, `Makefile` | `check_production_rvv_asm` 在生产补丁前失败 |
| PI2 production patch | `features/include/pcl/features/impl/don.hpp` | public entry 呈现 `RVV short-circuit -> Std fallback`，原标量主体抽成 Std helper |
| PI3 correctness | topic gtest | QEMU Std/RVV compare 通过，fallback case 单独通过 |
| PI4 asm | production bench dump | RVV 指令归属到 production direct bench 路径 |
| PI4 board | production direct repeated board | 生成 repeated summary、Evidence Doctor 和 registry |
| PI5 decision | phase result / evaluation / matrix | 基于 production direct 证据给出 `pending_user_confirmation_adopt_production` 或 `pending_user_confirmation_rollback` |

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | 进入本阶段前的证据是 `diagnostic`；本阶段目标证据是 `production-public` |
| A/B boundary | 进入本阶段前为 `test helper`；本阶段为真实 public overload（公开入口）经 `Feature::compute()` 调用 |
| 当前决策问题 | `RVV-vs-scalar`，判断当前 public RVV path 是否快于当前 public scalar path |
| diagnostic 是否可外推到 production | 只能作为 bounded production probe（有界生产探针）触发条件，不能替代 production direct |
| comparison-boundary / baseline mismatch 风险 | 有。helper-only 不包含 `Feature::compute()` 输出准备、对象状态和 dispatch |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前为稳定 `weak_positive`，允许 exact `pcl::Normal` 的有界生产探针；若 production direct 失败则停在 PI5 用户确认点 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已有 adopted RVV family，因此先做 production-public Std/RVV；若后续有多 RVV family 再补 detail A/B |

## 板卡复跑预算和决策桶

| 项目 | 值 |
| --- | --- |
| run budget | 5 runs |
| warmup / iterations | 沿用 topic `BENCH_ARGS`，默认由 Makefile / board target 传入 |
| positive | median speedup >= `1.20x` 且 `B/A < 1 = 0` |
| weak_positive | median speedup >= `1.05x` 且 `B/A < 1` 不超过 1/5 |
| neutral | `0.95x <= median < 1.05x` |
| negative | median < `0.95x` |
| unstable | budget 用完后 bucket 仍摇摆或 Evidence Doctor 暴露未解释 Error |

## 继续 / 停止条件

继续条件：红灯失败符合预期、exact gate 能隔离、production patch 不改 public API、QEMU / asm / board target 可运行。

停止条件：fallback gate 无法隔离、生产补丁需要扩大到泛型 traits 或 public API、QEMU correctness 失败、asm 无法归属、板卡 target 不可用、Evidence Doctor Error 未修正，或 PI5 到达需要用户确认采纳 / 回滚的边界。

## 文档更新清单

本阶段结束前更新：

- `test-rvv/features/don/doc/phases/030-production-direct-probe/result.zh.md`
- `test-rvv/features/don/doc/phases/README.zh.md`
- `test-rvv/features/don/doc/phases/optimization-matrix.zh.md`
- `test-rvv/features/don/doc/optimization-roadmap.zh.md`
- `test-rvv/features/don/doc/don-evaluation.zh.md`
- `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md`

`doc-rvv/features/don-RVV.zh.md` 只有在 PI5 证据闭合且用户明确确认采纳后才进入 adopted production closeout；本阶段到 PI5 时只输出用户确认点。
