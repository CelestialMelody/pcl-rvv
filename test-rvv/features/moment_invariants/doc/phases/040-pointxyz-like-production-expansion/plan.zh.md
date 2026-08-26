# Phase 040 PointXYZ-like production expansion plan

## 阶段意图和边界

Phase 030 已在真实 `MomentInvariantsEstimation<PointXYZ, MomentInvariants>::compute`
production-public（生产公开入口）边界上取得板卡 weak-positive（弱正向）收益，并且用户确认“板卡上有收益即可采纳”。
本阶段继续关闭 Phase 030 留下的 exact `PointXYZ` 阶段性门控，把同一个 indexed neighbor moment accumulation
RVV path（索引邻域矩累加 RVV 路径）扩展到 `RVVXYZAoSFloatLayout<PointT>` 可证明的常见 PointXYZ-like
点型。

本阶段不改变 public API（公开接口），不改变输出类型，仍只覆盖 `PointOutT == pcl::MomentInvariants`。
不接 full-cloud overload，不修改 `compute3DCentroid`、`searchForNeighbors` 或 KdTree，不覆盖
`Scalar=double`、非 xyz 单 `float` layout、自定义未验证点型或其它输出点类型。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| Phase 030 production evidence | `mi_production_compute_feature,points=4096` 板卡 5-run median `1.067x`，min `1.023x`，0/5 退化，bucket `weak_positive`。 | `log/board/repeated_phase030_production_compute_feature/summary.md` |
| Phase 030 correctness | QEMU Std/RVV 各 5/5 pass；板卡 RVV gtest 5/5 pass。 | `make run_test_compare`、`make run_board_test` |
| Phase 030 asm | production `detail` helper 和 public `computePointMomentInvariants` 符号范围内有 `vlux*ei32` 与 `vfred*sum`。 | `make check_production_rvv_asm` |
| 当前 production gate | `PointInT` exact `pcl::PointXYZ` + `PointOutT` exact `pcl::MomentInvariants`。 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| 公共 layout gate | `RVVXYZAoSFloatLayout<PointT>` 证明 PCL traits 注册 xyz 单 `float`、POD standard-layout、`sizeof(PointT)==sizeof(POD)` 和 float 对齐。 | `common/include/pcl/rvv_point_traits.h` |

## Phase scope 与扩展队列

| 字段 | 本阶段冻结 |
| --- | --- |
| `validated_scope` | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 的 public `computeFeature` indexed neighbor path；`Scalar=float`；AoS layout；`PointOutT=pcl::MomentInvariants`；邻域规模至少 16。 |
| `still_unvalidated_scope` | `PointXYZRGBNormal`、`PointXYZINormal`、自定义点型、非 xyz 单 float、full-cloud overload、其它输出类型、`Scalar=double`、search/KdTree 优化。 |
| `phase_closeout_boundary` | 只关闭常见 PointXYZ-like 输入点型的 production dispatch、correctness、asm 和 board evidence；不关闭所有 PCL_XYZ_POINT_TYPES 或用户自定义点型。 |

## 候选实现

| candidate family | 设计 | 预期收益 | 风险 / 未知 |
| --- | --- | --- | --- |
| traits-gated indexed moment accumulation RVV | 删除 exact `PointXYZ` 限制，保留 `RVVXYZAoSFloatLayout<PointT>`、u32 byte offset、规模阈值和 `PointOutT` gate。 | 常见带额外字段的 xyz 点型复用同一 RVV gather/reduction；因读取 stride 变大，收益可能不同。 | 更大 `sizeof(PointT)` 会影响 gather cache locality；typed production asm 需要单独归因；板卡收益不能从 PointXYZ 外推。 |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| RED asm gate | `check_production_pointxyzi_rvv_asm` | production patch 前 exact gate 下失败，证明 gate 能观察 typed RVV 缺失。 |
| typed correctness | `src/test_moment_invariants.cpp` | QEMU Std/RVV 通过，覆盖 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` output 与独立标量 reference。 |
| typed bench cases | `src/bench_moment_invariants.cpp`、`Makefile` | 能按 case-filter 跑 `mi_production_compute_feature_pointxyzi` 等 typed public cases。 |
| production gate expansion | `moment_invariants.hpp` | `RVVXYZAoSFloatLayout<PointT>` 命中即可走 RVV；其它路径自然 fallback。 |
| typed asm / board evidence | Make targets + summary/manifest/doctor/registry | 至少 `PointXYZI` 完成 production asm 和 board repeated；若 `PointXYZRGB/RGBA` 同轮可行则继续补齐。 |

## Evidence Doctor 与 registry

typed repeated summary 继续使用 `script/generate_mi_repeated_summary.py`。每个 typed case 必须写明：
`evidence_role=production-public`、`A/B boundary=public overload`、`row_source=kd_tree_k_neighbor_query`、
`timer_boundary=public_compute_feature_with_kdtree_search_and_output_write` 和具体 point type。若 summary
出现 Error，禁止采纳该点型；Warning / Suggestion 必须在 phase result 和 Handoff 中解释。

## 板卡复跑预算和决策桶

每个 typed case 初始 5 run、warm-up 2、iterations 8、points 4096。`weak_positive` 或 `positive` 可按用户当前授权采纳；
`neutral/negative/unstable` 不采纳该点型，并保留 fallback。若某点型结果近阈值但 0/5 退化，可作为 weak-positive
采纳，并把 metadata suggestion 作为证据卫生风险记录。

## 继续 / 停止条件

默认先推进 `PointXYZI`，若 correctness、asm、board 均正向，再继续 `PointXYZRGB` 和 `PointXYZRGBA`。
如果 typed 点型出现退化、asm 无法归因或 fallback 无法隔离，停止扩展该点型并记录 rollback / narrow-gate 方案。
若三种常见点型均闭合，后续不继续自动扩展到 normal 复合或自定义点型，除非新 phase 单独冻结范围。
