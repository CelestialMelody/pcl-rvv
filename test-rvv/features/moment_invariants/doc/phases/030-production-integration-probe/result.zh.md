# Phase 030 production integration probe result

## 执行范围

本阶段完成了 `features/include/pcl/features/impl/moment_invariants.hpp` 的 production integration loop（生产接入闭环）。真实公开入口是 `MomentInvariantsEstimation<PointXYZ, MomentInvariants>::computeFeature`：`computeFeature` 仍负责 KdTree nearestKSearch（近邻搜索）和输出写回，indexed `computePointMomentInvariants(cloud, indices, ...)` 在 centroid（质心）之后尝试 RVV path（RVV 链路）。

采用范围为 `PointXYZ / Scalar=float / dense AoS（结构数组）/ PointOutT=pcl::MomentInvariants / indexed neighbor list`。本阶段不覆盖 full-cloud overload、`Scalar=double`、其它输出类型、非 dense surface、非 xyz 单 float layout 或 KdTree/search 优化。

## 实现结果

| action | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| PI1 范围冻结 | done | `plan.zh.md` | 有界接入只替换 indexed moment accumulation（索引矩累加），public API（公开接口）不变。 |
| PI2 production patch | done | `features/include/pcl/features/impl/moment_invariants.hpp` | 抽出 `momentInvariantsIndexedMomentsStd`、`momentInvariantsFullMomentsStd` 和 `momentInvariantsIndexedMomentsRVV`；非 RVV 构建自然只保留标量。 |
| PI3 production direct tests | done | `src/test_moment_invariants.cpp` | 新增 public `computeFeature` 对拍和非 dense fallback case。 |
| PI4 evidence rerun | done | `make run_test_compare`、`make check_production_rvv_asm`、`make run_board_test`、`log/board/repeated_phase030_production_compute_feature/` | correctness、asm attribution（反汇编归属）和板卡 repeated evidence（重复板卡证据）闭合。 |
| PI5 decision | done | 本文件、matrix、evaluation | production-public 证据为 weak_positive；用户后续确认有收益即可采纳，因此进入 adopted production behavior（已采纳生产行为）。 |

## 生产路径边界

| gate / fallback | 行为 | 为什么这样做 |
| --- | --- | --- |
| `__RVV10__` 未启用 | 编译期没有 RVV helper，公开入口走标量 helper。 | 保持非 RVV 构建语义和可移植性。 |
| `PointOutT != pcl::MomentInvariants` | 走标量 helper。 | 当前只证明三值输出，不证明其它输出类型。 |
| `RVVXYZAoSFloatLayout<PointT>` 不满足 | 走标量 helper。 | RVV gather（离散加载）依赖 PCL traits（点类型字段特征）证明 xyz 单 float、POD layout 和字段 offset。 |
| `cloud.is_dense == false` | 走标量 helper。 | 保守避免 surface 邻域中非有限点直接进入无 mask RVV reduction。 |
| `indices.size() < 16` | 走标量 helper。 | 小邻域下 RVV setup 成本不划算。 |
| `cloud.size()` 超出 32-bit byte offset helper 范围 | 走标量 helper。 | 当前 indexed load helper 使用 u32 byte offset。 |

## 正确性、ASM 和板卡证据

| 证据 | 结果 | 边界 |
| --- | --- | --- |
| QEMU correctness（QEMU 正确性验证） | `make run_test_compare`：Std 5/5 pass，RVV 5/5 pass。 | QEMU 只证明正确性、构建和路径，不证明性能。 |
| board correctness（板卡正确性） | `make run_board_test`：RVV gtest 5/5 pass。 | 证明板卡可运行和输出容差，不是 repeated performance。 |
| production asm | `make check_production_rvv_asm` 通过。 | `PointXYZ` production helper / public symbol 范围内可归属 `vlux*ei32` 和 `vfred*sum`。 |
| production board summary | `log/board/repeated_phase030_production_compute_feature/summary.md` | `mi_production_compute_feature,points=4096` 5-run median `1.067x`，min `1.023x`，max `1.085x`，0/5 below 1，decision bucket `weak_positive`。 |
| Evidence Doctor（证据体检） | `log/board/repeated_phase030_production_compute_feature/evidence_doctor.md`：0 Error / 0 Warning / 2 Suggestion。 | 缺 taskset/governor/freq/temperature 和 binary hash；不阻塞当前结论，但后续复跑应补强 metadata（元数据）。 |
| Evidence registry（证据登记表） | `log/evidence_registry.json` 中 Phase 030 三个 summary artifact 均为 fresh。 | summary-only 证据可进入 review；raw `run-*` 不默认提交。 |

## EvidenceDecision

Phase 030 的结论为 `production-public weak_positive`。它证明当前 public RVV path 快于当前 public scalar path，且实现小、fallback 简单、公开 API 不变。该证据不证明新的 RVV family 优于其它未实现 family，也不证明其它点型或 full-cloud overload。

本阶段结束时的 `point_type_expansion_queue` 指向 Phase 040：用同一个 production dispatch 扩展到常见 PointXYZ-like 点型，并逐项补 correctness、asm、board repeated 和 Evidence Doctor。

## Continue / Stop

`stop_condition_hit=false`。用户确认板卡收益即可采纳，且 Phase 040 仍在当前 topic 的点类型扩展范围内，因此本阶段不停止在 PI5，而是继续到 `040-pointxyz-like-production-expansion`。
