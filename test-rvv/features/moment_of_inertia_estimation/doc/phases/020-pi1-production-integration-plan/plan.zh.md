# Phase 020 plan: PI1 production integration plan

## 阶段意图和边界

本阶段只写 PI1 production integration plan（生产接入计划），不修改 production（生产源码）。目标是把 phase 000 / phase 010 的 positive diagnostic（正向诊断）证据转成可审查的生产接入范围、fallback（回退路径）矩阵、traits gate（点类型字段门控）和 PI2-PI5 证据计划。

本阶段不得触碰：

- `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp`
- `features/include/pcl/features/moment_of_inertia_estimation.h`
- 公共 API（公开接口）或跨 topic 公共 helper

PI2 production patch（生产补丁）需要用户明确授权后才能开始。

## 当前证据输入

| evidence | status | path | production implication |
| --- | --- | --- | --- |
| phase 000 fused xyz reductions | positive diagnostic；board median 2.082x | `doc/phases/000-current-state-and-reduction-diagnostic/result.zh.md` | 支持考虑把 mean/AABB、covariance、single-axis moment、OBB extrema 的逐点规约合并进生产 helper |
| phase 010 projected covariance fusion | positive diagnostic；board median 1.247x | `doc/phases/010-projected-covariance-fusion-diagnostic/result.zh.md` | 支持考虑在 angle scan 内绕开 materialized projected cloud（实体化投影点云） |
| Evidence Doctor | phase 000/010 均 Errors=0、Warnings=0、Suggestions=1 | `log/board/repeated_phase000_reduction_diagnostic/evidence_doctor.md`；`log/board/repeated_phase010_projected_covariance_diagnostic/evidence_doctor.md` | binary identity missing（缺二进制身份字段）需在 production rerun 前补强 |
| production scalar shape | `compute()` 串联 mean/covariance/eigen/angle scan/eccentricity/OBB | `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` | production patch 必须保持 public entry 语义和最终输出顺序 |

## 候选接入范围

推荐 PI2 窄范围候选：

| scope item | PI1 decision |
| --- | --- |
| entry | `MomentOfInertiaEstimation<PointT>::compute()` 内部调用的 private helpers；不改 public API |
| point type | 首选 `RVVXYZAoSFloatLayout<PointT>` 这类 PointXYZ-like traits gate（类似 PointXYZ 的 xyz 单 float AoS 布局门控）；若生产实现无法同轮安全复用公共 traits，则先停，不改成 exact `PointXYZ` clean adoption |
| scalar type | 当前 production helper 使用 `float` 和 `Eigen::Vector3f`；不扩大到 `double` |
| row source | `indices_` 指向 `input_` 的 indexed cloud；不覆盖其它外部 row source |
| layout | AoS indexed gather，32-bit byte offset gate 必须证明 `input_->size() <= UINT32_MAX / sizeof(PointT)` 或等价安全上界 |
| finite input | `initCompute()` 之后沿用 PCLBase 输入有效性；若不能证明 dense / finite 语义，RVV helper 内部应保留标量 fallback 或不接入该片段 |
| production family | 可先接入 fused xyz reductions；projected covariance fusion 是否同轮接入由 PI1 实现复杂度和 fallback 隔离决定 |

## Fallback 矩阵

| condition | required behavior | planned evidence |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 自然只编译 / 调用标量路径 | native cross build 或 QEMU Std build |
| 点类型不满足 xyz 单 float AoS layout | 标量路径 | compile-time fallback test，至少覆盖非兼容点型或 traits gate static path |
| `input_` 为空或 `indices_` 为空 | 保持现有标量语义和除零保护 | production direct gtest |
| `input_->size()` 超过 32-bit byte offset helper 上界 | 标量路径 | unit test 或 code review gate；超大输入可用 synthetic gate test |
| `indices_` 非连续 / 重排 | RVV gather path 仍正确，或 fallback | production direct correctness 覆盖 shuffled indices |
| 小规模输入 | 若 RVV 启动成本不划算，可 fallback 标量 | size sweep 或明确定义阈值 |
| projected covariance fusion 未接入或 gate 失败 | 继续使用 `getProjectedCloud()` + projected `computeCovarianceMatrix()` | production direct smoke 对比完整输出 |

## 实现形态要求

PI2 如果获准修改 production，公开入口不应堆叠大段 RVV 主体。推荐形态：

1. 保留 `compute()` 的初始化、normalize、角度扫描和输出 push 顺序。
2. 将可复用逐点规约拆成 `*_Std` / `*_RVV` private 或邻近 helper；若需要修改 public header 声明，优先评估是否能用 `impl` 邻近 internal helper 避免扩大 ABI / API 表面。
3. RVV helper 返回 `bool` 或结构化结果，让非覆盖路径自然回到现有标量 helper。
4. projected covariance fusion 若接入，必须让 `computeEccentricity()` 仍接收等价 covariance matrix；不得改变 eccentricity 输出定义。

## Production direct 证据计划

| gate | command / artifact | completion criteria |
| --- | --- | --- |
| public direct correctness | 新增 test-rvv production-shaped gtest，真实实例化 `MomentOfInertiaEstimation<pcl::PointXYZ>`，比较 getters 输出 | Std/RVV 完整输出在容差内一致 |
| fallback correctness | 非 RVV build、非覆盖 point type、小输入 / 空输入、shuffled indices | 每个 fallback 原因至少一个隔离测试或明确 not_applicable 证据 |
| asm attribution | `dump_test_rvv` / `dump_bench_rvv` 或 production direct binary objdump | RVV 指令归属到 production helper 符号或内联范围 |
| board production bench | 新增 public-entry-shaped bench case，不复用 helper-only timing | repeated board bucket 稳定，Evidence Doctor 无 Error / Warning |
| evidence freshness | `evidence_status` 或 phase020/PI evidence registry target | summary / manifest / doctor 登记 fresh |

## 暂停条件

PI1 后必须暂停并等待用户确认，如果：

- PI2 需要修改 production 源码。
- traits gate 不能安全表达泛型 PointXYZ-like 范围。
- 需要新增跨 topic 公共 helper 或 public API。
- fallback 矩阵无法隔离，可能让非覆盖路径误命中 RVV。
- production direct 证据计划无法在现有 test-rvv harness 内复现。

## continue / stop conditions

本阶段完成条件是产出上述 PI1 计划并更新 roadmap / matrix / evaluation 的默认恢复入口。完成后 `next_phase_default` 应写成 `PI2 production_patch pending explicit user authorization`，除非用户在同一轮明确授权修改 production。
