# Phase 010：生产形态边界侦察计划

## 阶段意图和边界

Phase 000 已证明单个大 leaf helper 的 component ablation（组件消融）有局部正向信号，但 Evidence Doctor（证据体检）暴露 leaf size sensitivity（叶大小敏感性）和小规模 decode 波动。本阶段继续在 `test-rvv/io/point_coding` 内推进，不修改 production（生产源码），目标是用更细的 leaf-size sweep（叶大小扫描）判断局部收益是否可能穿透更真实的 octree point coder context（八叉树点编码上下文）。

validated_scope（计划验证范围）：

- `PointXYZ` / AoS（结构数组）布局。
- encode source-indexed leaf，新增小 leaf 到中 leaf 的 synthetic size sweep。
- decode contiguous output segment，新增小输出段 size sweep。
- test helper A/B boundary（测试 helper 对比边界），仍不是 public overload（公开重载）。

unvalidated_scope（未验证范围）：

- 真实 `OctreePointCloudCompression` traversal、entropy context（熵编码上下文）、完整 object state（对象状态）和 production fallback（生产回退路径）。
- 泛型 `PointT`、其它字段布局和 production direct（真实生产路径证据）。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| Phase 000 | completed；result / roadmap / matrix 已记录 diagnostic-positive 和 warning。 |
| bench case | 已有 1024 / 4096 / 16384 三个 encode 和 decode case。 |
| Evidence Doctor | repeated Phase 000 为 Errors=0、Warnings=6、Suggestions=0。 |
| production | 未修改；本阶段继续不触碰。 |

## 假设与候选族

| candidate family | 计划验证问题 | 风险 |
| --- | --- | --- |
| small-leaf encode sweep | 小 leaf 下 indexed gather + scalar quantize 是否仍正向。 | leaf 太小时 `vsetvli`、临时 store 和标量量化可能吞掉收益。 |
| small-segment decode sweep | 小输出段下 `vlse8` / `vsse32` decode 是否稳定。 | Phase 000 的 `decode_contiguous_1024` 已有 1/5 退化，继续缩小可能负向。 |
| quantize safety scout | 文档化 full f32 RVV quantize 暂缓条件，不在本阶段重新引入未闭合公式。 | 若只加更多 bench 而不处理语义风险，不能进入 production。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| small-leaf encode sweep | source-indexed leaf | `PointXYZ` / float coordinate + double reference / AoS | test helper | existing `run_test_compare` | new `encode_indexed_16/64/256` plus Phase 000 sizes | planned repeated board | existing encode asm | planned | planned | 添加 bench case 和 manifest metadata。 |
| small-segment decode sweep | contiguous output segment | `PointXYZ` / float output / AoS | test helper | existing `run_test_compare` | new `decode_contiguous_16/64/256` plus Phase 000 sizes | planned repeated board | existing decode asm | planned | planned | 添加 bench case 和 manifest metadata。 |
| full f32 quantize | source-indexed leaf | f32 formula | not implemented | not_run | not_run | not_run | not_run | not_run | deferred | 仅记录恢复条件，不在本阶段写 production。 |

## 实现和测试动作

1. 修改 `src/bench_point_coding.cpp`，新增 `encode_indexed_16/64/256` 与 `decode_contiguous_16/64/256`。
2. 修改 `script/generate_point_coding_evidence_manifest.py`，补新 case 的 metadata，确保 Evidence Doctor 能按 group 识别。
3. 更新 phase index / roadmap / matrix，记录 Phase 010 正在执行。
4. 运行 `make run_test_compare`，确认新增 bench 不破坏 correctness。
5. 运行 `make run_qemu_bench_smoke`，确认日志形状。
6. 板卡可用时运行 `make collect_board_repeated POINT_CODING_REPEATED_RUNS=5` 和 `make run_board_repeated_evidence_doctor`，用新的 all-case matrix 刷新 summary。
7. 写 `result.zh.md`，解释 small leaf / small segment 的 decision bucket、Evidence Doctor warning 和是否允许后续 production-shaped diagnostic。

## Evidence Doctor 和 registry 规则

使用 topic-local manifest wrapper 生成 `log/board/repeated_phase000/evidence_manifest.json`，再调用全局 Evidence Doctor。尽管目录名仍为 `repeated_phase000`，Phase 010 会把本次刷新后的 summary 视为 current evidence，并在 result 中说明它 supersedes（取代）Phase 000 的 6-case summary。当前没有 registry，仍用路径限定扫描人工检查。

## 板卡复跑预算和决策桶

本阶段沿用 5-run bounded rerun budget（有界复跑预算）。判断口径：

- median >= 1.20 且 min >= 1.05：positive。
- median 1.05 到 1.20：weak-positive。
- median 0.95 到 1.05：neutral。
- median < 0.95 或多次低于 1：negative / unstable。
- Evidence Doctor warning 不自动失败，但必须降级边界或列出下一步。

## 继续 / 停止条件

若新增小 leaf case 大量 negative，本阶段停止 production 探针，保留诊断资产并把 octree point/color combined context 降级。若小 leaf 仍正向但 full f32 quantize 语义仍未闭合，下一 phase 默认是 `020-encode-quantize-safety`，不是 production patch。

## 文档更新清单

更新 `result.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` 和最终 Handoff。仍不创建 `doc-rvv/io/point_coding-RVV.zh.md`。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic / component ablation，稍微靠近 production-shaped，但仍不是 production direct。 |
| A/B boundary | test helper。 |
| 当前决策问题 | small leaf 下局部 RVV-vs-scalar 是否仍可继续。 |
| diagnostic 是否可外推到 production | no；若小 leaf 全面退化，可作为不进入 production 的强风险；若正向，只能支持下一 production-shaped diagnostic。 |
| comparison-boundary / baseline mismatch 风险 | helper A/B 低；production 外推高。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若小 leaf negative 或 unstable，不允许；若正向但量化语义未闭合，也不允许直接 production patch。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段不 clean-adopt。 |
