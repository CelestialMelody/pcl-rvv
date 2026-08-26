# Phase 040 Production RVV Probe Result

## 当前状态

Phase 040 已完成 PI2-PI5 production integration loop（生产接入闭环）。`features/include/pcl/features/impl/vfh.hpp`
新增 `__RVV10__` 下的 `pcl::detail::computeVFHSignatureRVV()` 及内部 helper，并在
`VFHEstimation::computeFeature()` 默认公开入口中先尝试 RVV（RISC-V Vector，可变长度向量）路径；不满足 gate
（会失败的准入条件）时继续执行原标量主体。

本阶段只覆盖默认 VFH（Viewpoint Feature Histogram，视点特征直方图）公开入口：
`PointInT` 为 xyz 单 `float` AoS（结构数组）布局，`PointNT` 为 normal 单 `float` AoS 布局，输出为
`pcl::VFHSignature308`，dense full-cloud sequential indices（全点云顺序索引），
`normalize_distances=false`、`size_component=false`，且没有给定 centroid（质心）或 normal（法线）。
CVFH / OUR-CVFH 的给定 centroid / normal 路径、子集 indices、非 dense normals、`size_component` 和
`normalize_distances` 均保持标量 fallback（回退路径）。

## 计划动作回填

| action | 状态 | 证据 / 结果 |
| --- | --- | --- |
| PI2-RED / PI2-GREEN | done | 生产 helper 已接入；`src/test_vfh.cpp` 增加 `VFHProductionRVV.DefaultPublicBoundaryHelperMatchesPublicCompute` 和 `HelperRejectsSizeComponentFallbackBoundary`。 |
| PI3-BENCH | done | `src/bench_vfh.cpp` 增加 `production_vfh_compute_default`，计时边界是真实 `VFHEstimation::compute()` 公开路径。 |
| PI4-ASM | done | `make -B -C test-rvv/features/vfh dump_bench_rvv` 证明 production helper 符号和 RVV 指令存在。 |
| PI4-BOARD | done | `log/board/repeated-production-phase040` 5-run checksum 一致，production case mean speedup `1.36195x`。 |
| PI5-DOCTOR | done | `log/board/repeated-production-phase040/evidence_doctor.md` 为 `0E/1W/11S`。唯一 Warning 属于历史 diagnostic near-threshold，不阻塞 production case。 |
| DOC | partial_then_superseded | Phase 040 结果被 Phase 050/060 继续优化取代；正式 closeout 以 Phase 060 生产数据为准。 |

## 证据分层

| 证据层 | 结果 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness（正确性） | `run_test_compare` Std/RVV 各 7 个测试通过。 | production helper 与标量 fallback 在默认边界下输出一致，且 `size_component` 会回退。 | 不能证明 CVFH / OUR-CVFH、非 dense、子集 indices 或其它点型组合。 |
| QEMU（仿真器） | 用于 correctness 和日志形状。 | 证明构建、测试和路径可运行。 | 不作为性能结论。 |
| asm（反汇编） | production helper 和 `computeFeature()` 调用可归属。 | 证明不是只命中 test-only candidate。 | 不证明目标硬件收益大小。 |
| board（板卡） | `production_vfh_compute_default` mean `1.36195x`。 | 证明接入后的真实公开入口快于标量公开入口。 | 不证明 Phase 040 是最优实现形态。 |
| Evidence Doctor（证据体检） | `0E/1W/11S`。 | 无 Error；Warning 已解释为非 production case。 | 仍建议补环境 metadata 和 binary identity。 |

## Diagnostic-to-production mismatch audit

| question | result |
| --- | --- |
| evidence role | `production-public`。 |
| A/B boundary | 真实 `VFHEstimation::compute()` public overload，Std build 对 RVV build。 |
| 当前决策问题 | `RVV-vs-scalar`：接入后的公开 RVV 路径是否优于公开标量路径。 |
| diagnostic 是否可外推到 production | 不外推；Phase 040 使用 production direct board 数据重新判断。 |
| comparison-boundary / baseline mismatch 风险 | 已用 production bench label 降低错配风险；Phase 000-030 只保留为候选来源。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | Phase 040 只回答 RVV-vs-scalar。后续 Phase 050/060 已继续做同一 production boundary 内实现形态优化。 |

## 继续 / 停止决策

Phase 040 是 positive production probe（正向生产探针），但源码中 O(N) whole-cloud staging（整云暂存）仍有可见内存流量和 heap allocation（堆分配）成本。当前未命中停止条件，默认继续到 Phase 050 的 chunk-local staging（分块局部暂存）优化。
