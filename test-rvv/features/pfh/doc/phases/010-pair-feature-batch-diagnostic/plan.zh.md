# Phase 010 Pair Feature Batch Diagnostic Plan

## 阶段意图和边界

本阶段尝试 test-only pair feature batch RVV（测试专用成对特征批处理 RVV）候选，目标是回答：PFH 的 O(k^2) pair math 若使用现有 `acos_RVV_f32m2` / `atan2_RVV_f32m2` 和 RVV lane-level helper（单段向量 helper）是否能在 component boundary（组件边界）产生足够正向信号。

本阶段仍不修改 `features/include/pcl/features/impl/pfh.hpp`。候选放在 `test-rvv/features/pfh/include/impl/pfh_pair_batch_candidate.hpp`，非 `__RVV10__` 构建回到 reference（参考链路）。它会先用 scalar staging（标量暂存）把 pair 坐标/法线整理成连续数组，再用 RVV 计算 tuple，最后按原 pair order 标量 scatter 到 histogram。这个设计只测试数学链路潜力，不是 production-ready shape。

## 当前状态清单

| 项目 | 当前事实 |
| --- | --- |
| Phase 000 baseline | `component_pfh_signature` 和 `public_pfh_k` 5-run repeated 都是 neutral；Doctor Errors=2，均为退化频率。 |
| 数学 helper | `common/include/pcl/common/impl/rvv_math.hpp` 已存在 `acos_RVV_f32m2` 和 `atan2_RVV_f32m2`。 |
| 公共 load/store | `common/include/pcl/rvv_point_load.h` 存在 xyz wrapper；normal 字段和 pair staging 的 production gate 本阶段不闭合。 |
| correctness baseline | `computePointPFHReference` 与 production `computePointPFHSignature` 对拍通过。 |

## 候选假设

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `pfh-pair-feature-staged-rvv` | Staging 后 vector math 可抵消 `atan2`、sqrt、dot/cross 的标量成本。 | staging 成本高、histogram scatter 仍标量、approx `atan2` 可能跨 bin、`acos(abs())` branch 需等价替换为 `abs(angle1) < abs(angle2)`。 |
| `pfh-direct-point-load-rvv` | 后续可用 point load wrapper 直接从 AoS 读取，减少 staging。 | normal field gate、indexed pair gather 和 histogram conflict 尚未闭合；本阶段不做。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `pfh-pair-feature-staged-rvv` | fixed neighborhood indices | `PointNormal -> PFHSignature125`, float, staged SoA arrays | test-only `computePointPFHSignaturePairBatchRVV` | new gtest vs production/reference | `candidate_pfh_pair_batch_rvv` | 5-run repeated if correctness + smoke pass | `dump_bench_rvv`, look for `atan2_RVV_f32m2`/vector math in candidate | repeated Doctor | planned | write failing test, implement candidate |

## 实现和测试动作

| action | 产物 | 命令 / 验收 |
| --- | --- | --- |
| RED-010 | 新增 gtest 调用尚不存在的 `computePointPFHSignaturePairBatchRVV`。 | `make -B -C test-rvv/features/pfh run_test_rvv` 先因候选缺失失败。 |
| GREEN-010 | 新增 `pfh_pair_batch_candidate.hpp` 并更新 aggregator。 | `run_test_compare` 通过；candidate 与 production histogram 误差阈值暂定 `2e-3`，若失败必须分析 bin crossing。 |
| BENCH-010 | `bench_pfh.cpp` 新增 `candidate_pfh_pair_batch_rvv`。 | `dump_bench_rvv` 生成可归属 RVV 指令；QEMU 不做性能结论。 |
| BOARD-010 | 5-run repeated board。 | 若 candidate >1.15 且退化频率 0，可进入 PI1 候选；若 1.03-1.15 为 weak；否则 attempted/rejected 或继续 direct-load candidate。 |

## 数值预算

候选使用 `atan2_RVV_f32m2` 近似，可能导致 f1 落入相邻 bin。correctness gate 首先比较完整 125-bin histogram，阈值为每 bin `2e-3`；若失败，补充 tuple-level 或 bin-level diagnostic，判断是否是近边界跨 bin。`acos(abs(angle1)) > acos(abs(angle2))` 用 `abs(angle1) < abs(angle2)` 等价替换，避免实际调用 `acos` 做分支。

## Evidence Doctor 和 registry

沿用 `script/generate_pfh_evidence_manifest.py`。Phase 010 repeated 输出仍放 `log/board/repeated`，若覆盖 Phase 000 baseline，必须在 result 中把旧 run 标为 historical 或刷新 registry。Doctor Error 未解决前不能写 production-ready。

## 继续 / 停止条件

默认继续到 correctness、asm、board repeated 和 Doctor。若候选 correctness 因数学近似不可接受而失败，停止本候选并把 `pfh-direct-point-load-rvv` 或数学 helper 精度专项列为下一动作；若性能为 neutral/negative，拒绝 staged candidate，但仍可继续审计 direct point-load / histogram-copy 方向。
