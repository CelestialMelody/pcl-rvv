# Phase 040 Plan: production-integration-plan

## 阶段意图和边界

本阶段进入 PI1 production integration plan（生产接入计划）。目标是把 Phase 030 的 combined production-shaped diagnostic（组合生产形态诊断）转成有界 production patch（生产补丁）合同：先为真实 `ROPSEstimation::rotateCloud()` 与 `getDistributionMatrix()` 增加可失败 production-direct tests（真实生产路径测试），再把已验证的 RVV helper 以窄 gate 接入 `features/include/pcl/features/impl/rops_estimation.hpp`。

PI1/PI2 不改公开 API。PI5 是用户检查点；无论 production direct 证据支持保留还是回滚，都必须先报告 production diff、测试和板卡证据，等待用户确认。

| 维度 | 本阶段范围 |
| --- | --- |
| production 边界 | 修改 `features/include/pcl/features/impl/rops_estimation.hpp` 的 private helper 实现，不修改 `rops_estimation.h` public API。 |
| 候选入口 | `rotateCloud()` 和 `getDistributionMatrix()`，由 `computeFeature()` 现有循环自然调用。 |
| 点型 / Scalar | 初始生产候选收窄到 exact `pcl::PointXYZ`、float、AoS（结构数组）字段布局；其它 `PointInT` fallback。 |
| 运行时 gate | cloud 非空、点数达到 RVV 阈值、`cloud.is_dense == true`、所有点有限、bins 非零且 projection 为 0/1/2。 |
| 不覆盖范围 | LRF、KdTree local surface、central moments、descriptor normalization、完整 `computeFeature()` public benchmark、其它点型、非 dense 或非有限点。 |
| 证据角色 | Phase 030 是 pre-production diagnostic；PI4 后才可写 production-direct performance evidence（真实生产路径性能证据）。 |

## 当前状态清单

| 项 | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| diagnostic correctness | pass | `run_test_compare`：Std/RVV 各 7 个 gtest 通过。 |
| combined board evidence | positive | `log/board/phase030_combined_rotate_distribution_repeated/summary.md`：5-run median 1.630x，0/5 退化，checksum match。 |
| Evidence Doctor | pass with suggestions | `log/board/phase030_combined_rotate_distribution_repeated/evidence_doctor.md`：0 Error / 0 Warning / 2 Suggestion。 |
| asm attribution | pass for diagnostic | `build/asm/riscv/bench_rops_estimation_rvv.full.asm`：combined boundary 含 rotate 和 distribution RVV 指令。 |
| production源码 | unchanged at PI1 entry | `features/include/pcl/features/impl/rops_estimation.hpp` 尚未有本 topic production diff。 |
| generic point type strategy | loaded | `doc-rvv/rvv/RVV Generic Point Type Strategy.zh.md` 已读取；PI1 选择 exact `PointXYZ` 初始 gate。 |

## PI1 候选范围

### 采用的窄范围

PI2 production patch 只允许尝试下面的模板实例形态：

```text
ROPSEstimation<pcl::PointXYZ, pcl::Histogram<135>>::rotateCloud(...)
ROPSEstimation<pcl::PointXYZ, pcl::Histogram<135>>::getDistributionMatrix(...)
```

实现形态：

1. 在 `__RVV10__` 下包含 `pcl/rvv_point_load.h`、`pcl/rvv_point_store.h`、`riscv_vector.h` 和必要 traits 头。
2. 新增 `rotateCloudRVV()` 返回 `bool`：gate 不满足时返回 `false`，满足时写 rotated cloud 和 AABB。
3. 新增 `getDistributionMatrixRVV()` 返回 `bool`：gate 不满足时返回 `false`，满足时批量生成 row/col staging，再保留标量 scatter。
4. `rotateCloud()` 和 `getDistributionMatrix()` 在 `__RVV10__` 下先尝试 RVV helper，成功则 return，失败自然执行原标量主体。
5. 生产注释只解释 gate / fallback / 数据布局边界，不写逐行 intrinsic 教材。

公共 load/store 选择：优先使用 `pcl::rvv_load::strided_load3_f32m2` 和 `pcl::rvv_store::strided_store3_f32m2`，由 exact `PointXYZ` 阶段性 gate 保证字段布局。后续若扩到 traits-gated generic point types（按字段特征门控的泛型点类型），再改用 `RVVXYZAoSFloatLayout<PointInT>` 做独立 phase。

### 有意不采用的范围

| 范围 | PI1 决策 | 理由 / 恢复条件 |
| --- | --- | --- |
| traits-gated generic `PointInT` | deferred | Phase 030 只覆盖 `PointXYZ`。扩大到 `PointXYZI` / `PointXYZRGB` / 自定义点型需要 traits offset、layout tests、fallback tests 和代表性板卡证据。 |
| `PointOutT` 变化 | not_applicable in PI1 | 本阶段只优化 private helper，输出 feature 维度由既有 `computeFeature()` 处理。 |
| 非 dense 或非有限输入 | fallback | 当前 RVV helper 不重建每点有限性语义；生产 path 先保留标量。 |
| 小规模 local cloud | fallback | component evidence 来自较大 synthetic local cloud；小规模收益不确定且回退成本低。 |
| central moments RVV | fallback / deferred | Phase 000 只 correctness-only；不在 PI1 接入。 |
| descriptor normalization RVV | deferred | 长度 135，需独立证据；不混入 PI1。 |

## Fallback Matrix

| gate | 触发条件 | 期望行为 | PI3 证据 |
| --- | --- | --- | --- |
| non-RVV build | 未定义 `__RVV10__` | 编译和行为完全走原标量主体。 | Std build `run_test_compare`。 |
| exact 点型不匹配 | `PointInT` 不是 `pcl::PointXYZ` | 不尝试 RVV，继续原标量。 | `PointXYZI` 或等价 compile / correctness fallback test。 |
| cloud 过小 | `cloud.size() < kRopsRVVMinPoints` | 不尝试 RVV，继续原标量。 | 小规模 trace-hit test。 |
| 空 cloud | `cloud.empty()` | 不尝试 RVV，保持原标量结果。 | 空输入 fallback test 或现有 helper oracle。 |
| 非 dense | `cloud.is_dense == false` | 不尝试 RVV，继续原标量。 | 非 dense trace-hit test。 |
| 非有限点 | `pcl::isFinite(pt) == false` | 不尝试 RVV，继续原标量。 | NaN / Inf trace-hit test。 |
| projection 非 0/1/2 或 bins 为 0 | 输入不满足 helper 语义 | 不尝试 RVV。 | 只在 production call site 保持合法 projection；异常输入不作为扩大范围。 |

## 实现和测试动作

| id | 动作 | 产物 | 完成判据 |
| --- | --- | --- | --- |
| PI2-A1 | 新增 production trace 宏下的命中计数测试。 | `src/test_rops_estimation.cpp`、Makefile RVV test extra flag | RED：RVV build 期望 `rotateCloud()` / `getDistributionMatrix()` production helper 命中 trace，当前 production 未接入时失败。 |
| PI2-A2 | 新增 production RVV helper 和短路分流。 | `features/include/pcl/features/impl/rops_estimation.hpp` | 无 public API 变更；非 RVV / gate 失败自然 fallback；测试通过。 |
| PI3-A1 | 补 fallback trace tests。 | `src/test_rops_estimation.cpp` | 小规模、非 dense、非有限或非 `PointXYZ` 不增加 trace hit，输出仍与生产 oracle 一致。 |
| PI4-A1 | 新增 production-direct bench case。 | `src/bench_rops_estimation.cpp`、summary script、Makefile target | bench label 区分 production-direct，不复用 diagnostic label。 |
| PI4-A2 | 运行 production direct QEMU smoke、asm、board repeated 和 Evidence Doctor。 | `log/board/phase040_production_direct_repeated/**` | Doctor Errors=0；Warnings 必须解释或降级。 |
| PI5-A1 | 写 result 和 EvidenceDecision。 | phase result、evaluation、roadmap、matrix、Handoff | 停在用户检查点，等待保留 / 回滚确认。 |

## Evidence Doctor 和 Registry 规则

PI4 需要新增 production-direct manifest，不复用 Phase 030 的 diagnostic manifest。推荐输出：

```text
test-rvv/features/rops_estimation/log/board/phase040_production_direct_repeated/summary.md
test-rvv/features/rops_estimation/log/board/phase040_production_direct_repeated/evidence_manifest.json
test-rvv/features/rops_estimation/log/board/phase040_production_direct_repeated/evidence_doctor.md
```

`evidence_role` 使用 `production_detail` 或 `production_direct`，A/B boundary 写真实 production private helper / public-derived `computeFeature()` 调用关系。若只测 private helper，必须降级为 production-detail，不可写完整 public production-ready。

## Board 复跑预算和决策桶

PI4 初始 run budget：

| 参数 | 计划值 |
| --- | --- |
| run count | 5 |
| points | 65536 |
| iterations | 20 |
| warm-up iterations | 3 |
| repeat | 8 |
| case-filter | `rops_production_rotate_distribution_pipeline` |

决策桶：

- `positive`：median >= 1.15 且 0/5 退化。
- `weak_positive`：median >= 1.05 且退化不超过 1/5。
- `neutral`：median 0.95-1.05。
- `negative`：median < 0.95。
- `unstable`：方向跨桶、checksum 不一致、Doctor Error 或长尾明显；最多一次同边界确认复跑。

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / board target | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| production private helper dispatch | transformed local cloud -> rotated local cloud -> bins | exact `pcl::PointXYZ`, float, dense finite AoS | planned PI2/PI3 production-direct + fallback tests | planned PI4 production-detail repeated board | planned production helper / bench boundary attribution | planned production manifest + doctor | PI1 planned |
| generic `PointInT` dispatch | transformed local cloud -> rotated cloud -> bins | traits-gated xyz AoS | not_yet_covered | not_yet_covered | not_run | not_run | deferred |
| full `computeFeature()` descriptor | mesh local surface -> descriptor | public-derived entry | not_yet_covered | not_yet_covered | not_run | not_run | deferred after private helper production detail |

## 阶段完成条件

PI1/PI2/PI3 完成条件：

- RED test 先失败，production helper 接入后通过。
- `run_test_compare` 在 Std/RVV 下通过，RVV trace hit 证明生产 helper 命中，fallback tests 证明未覆盖范围不命中。
- production diff 不改 public API，不扩大到泛型点型。
- 若进入 PI4，production-direct bench、asm、board repeated、Evidence Doctor 和文档全部同步。

## Continue / Stop 条件

`continue_stop_decision`：Phase 040 不因 PI1 plan 完成而停止；当前用户已授权持续推进，下一步执行 PI2/PI3。PI5 后必须停在用户检查点。

`stop_condition_hit`：仅当 production diff 需要修改 public API、泛型点型、公共 RVV API、其它 topic，或 board / tool / Evidence Doctor 出现真实 blocker 时停止。

`next_phase_default`：PI2 production patch + PI3 production-direct/fallback correctness。

## 文档更新清单

本阶段更新：

- `doc/phases/040-production-integration-plan/plan.zh.md`
- `doc/phases/040-production-integration-plan/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/rops_estimation-evaluation.zh.md`
- 当前 Handoff Packet

PI5 用户确认保留前不创建 `doc-rvv/features/rops_estimation-RVV.zh.md`。
