# Phase 020：encode 量化语义安全计划

## 阶段意图和边界

Phase 010 证明 small leaf encode 仍有局部正向信号，当前阻塞 production-shaped diagnostic（生产形态诊断）的主因变成 encode quantize（量化）语义：production 标量表达式会把 float 坐标与 double reference 混合后做 double 除法，再 `static_cast<int>` 截断。本阶段继续只改 `test-rvv/io/point_coding`，尝试让 RVV candidate 复刻 double-semantics quantize（双精度语义量化），并通过现有 clamp 对抗测试验收。

本阶段不修改 production（生产源码），不进入 production integration loop（生产接入闭环），不声明完整 octree public entry（公开入口）收益。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| encode gather | `vluxseg3ei32.v` 已存在，small leaf 到 large leaf 均正向。 |
| encode quantize | 当前 RVV candidate gather 后回标量 same-chain quantize。 |
| correctness risk | 纯 f32 RVV 量化曾在边界样本差 1。 |
| decode | weak-positive / unstable，非本阶段主线。 |

## 假设与候选族

| candidate family | 计划验证问题 | 风险 |
| --- | --- | --- |
| double-semantics vector quantize | f32 gather 后 widen（拓宽）到 f64，按 double reference / resolution 做 RVV 除法和 trunc toward zero（向零截断）。 | f64 LMUL / convert 指令可能很重，小 leaf 收益可能下降。 |
| boundary fallback | 如果 double RVV intrinsic 不可用或性能过差，仅对边界 lane 回退标量。 | 复杂度上升，测试 helper 可能偏离 production-shaped 简洁性。 |
| scalar same-chain fallback | 若前两者不可行，保留 Phase 010 candidate。 | 不能支撑 production-shaped encode patch。 |

## 实现和测试动作

1. 用编译探针确认 GCC RVV intrinsic（内建函数）名称：f32 -> f64 widen、f64 div、f64 -> i32 trunc / narrow。
2. 若 intrinsic 可用，修改 `encodePointsRVV`，新增 double-semantics vector quantize path，保留 correctness gate。
3. 运行 `make run_test_compare`；若 clamp 对抗样本失败，立即回退到 scalar same-chain path 并把原因写入 result。
4. 运行 `make run_qemu_bench_smoke` 和 `make dump_bench_rvv`，确认路径可构建且反汇编包含量化相关 RVV 指令。
5. 若 correctness 通过且 QEMU / asm 成立，运行 5-run board repeated + Evidence Doctor，对比 Phase 010 current summary。
6. 写 `result.zh.md`，更新 roadmap / matrix / evaluation / Handoff。

## 板卡复跑预算和决策桶

本阶段最多 5-run repeated board。若 double-semantics path 让 encode 主要规模掉到 neutral / negative，候选标为 rejected，并保留 Phase 010 的 scalar same-chain gather 作为 diagnostic-only（仅诊断）资产。若 correctness 通过但性能弱正向，仍不能 production-adopt，只能作为 production-shaped scout 的输入。

## 继续 / 停止条件

继续条件：编译探针成功且 correctness 通过。停止条件：RVV intrinsic 不可用、correctness 边界失败且无简单 fallback、或 board 结果显示 double-semantics path 全面负向。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic / implementation-shape。 |
| A/B boundary | test helper。 |
| 当前决策问题 | implementation-shape：是否存在 same-chain vector quantize。 |
| diagnostic 是否可外推到 production | no；只能决定是否值得构造 production-shaped encode diagnostic。 |
| comparison-boundary / baseline mismatch 风险 | 若替换 candidate 后与 Phase 010 比较，是 same test helper boundary；仍非 production。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no；必须先有正向且语义闭合。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。 |
