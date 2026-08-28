# Phase 010: stick count production integration plan

## 阶段意图和边界

本阶段是 PI1 production_integration_plan（生产接入计划）。它只把 Phase 000 的 `partial-production-candidate` 证据转成可审查的生产接入范围，不修改 production（生产源码）。进入 PI2 production patch（生产补丁）前需要用户明确授权。

计划候选只覆盖 `SampleConsensusModelStick<PointT>::countWithinDistance`。生产源码路径为 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp`。不接入 `selectWithinDistance`、`getDistancesToModel`、`optimizeModelCoefficients` 或 public API（公开接口）变更。

## 候选范围

| 维度 | PI1 冻结范围 |
| --- | --- |
| 入口 | `countWithinDistance` |
| row source | `indices_` direct indexed 路径；不新增 identity-index fast path |
| 点类型 | 优先按 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` traits gate（字段特征准入）覆盖 xyz 单 float AoS 点型 |
| Scalar / 系数 | `Eigen::VectorXf` model coefficients，`threshold` 仍按当前源码转成 float `threshold^2` |
| 布局 | PCL traits 注册的 x/y/z 单 float 字段，POD / standard-layout / stride / field offset 满足 AoS byte-offset helper 前提 |
| 规模 gate | `input_->size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()`，保证 32-bit indexed byte offset 可表达 |
| 目标硬件 | Phase 000 使用 Milkv-Jupiter；PI4 需重跑 production direct repeated board |

## 不接入范围

- `selectWithinDistance` 需要维护 `inliers` 保序写回和 `error_sqr_dists_`，应另建 `select-vcompress` phase。
- `getDistancesToModel` 当前源码把系数 3-5 当方向使用，并对 outlier 距离乘 2，不能复用 count/select 的端点语义。
- `Scalar=double` 不适用，因为当前 PCL 入口使用 `Eigen::VectorXf`，但生产文档仍需说明阈值和距离计算的 float 边界。
- 自定义非 AoS、非单 float xyz 或字段 offset 不满足公共 wrapper 前提的点型必须 fallback（回退路径）。
- 真实 RANSAC 上游性能不在 PI1/PI2 第一范围内；PI4 先证明公开入口 direct bench。

## 建议的 production 结构

如果进入 PI2，建议把原公开入口主体抽成 `countWithinDistanceStd` 或邻近等价 internal helper，再新增 `countWithinDistanceRVV`。公开入口保持“模型有效性检查 -> RVV 短路 -> Std fallback”的短结构。若模板声明或 deprecated class 结构让成员 helper 修改面过大，可以使用同文件 `detail` free helper，但 Handoff 必须说明选择原因。

RVV helper 复用公共 xyz indexed load wrapper（索引离散加载封装）：

- `pcl/common/rvv_point_traits.h`
- `pcl/common/rvv_point_load.h`
- `pcl/common/impl/rvv_point_load.hpp`

新增 production 注释只解释范围、fallback、32-bit offset gate 和 stick 双计数语义，不逐行解释 intrinsic（内建函数）。

## fallback / dispatch 计划

| fallback 条件 | 预期行为 | 验证方式 |
| --- | --- | --- |
| 非 RVV 构建或未定义 `__RVV10__` | 编译时只保留 Std helper | Std build gtest 通过，反汇编无 RVV 分流要求 |
| 点类型不满足 `RVVXYZAoSFloatLayout<PointT>` | public entry 落回 Std helper | 新增非覆盖点型或 traits-gate 编译 / 运行测试 |
| cloud 太大导致 32-bit byte offset 不可表达 | RVV helper 返回 false 或入口落回 Std | 构造 gate 单元测试或 helper-level fallback test |
| model invalid | 保持当前公开入口返回 0 和错误输出语义 | 复用或新增 public invalid model test |
| `indices_` 为空 | 返回 0，不触发越界 | 新增 explicit empty indices correctness |
| `selectWithinDistance` / `getDistancesToModel` | 保持原标量 | 不修改对应入口；diff 和测试确认 |

## production direct 证据计划

PI2 后至少运行：

```bash
make -C test-rvv/sample_consensus/sac_model_stick run_test_compare
make -C test-rvv/sample_consensus/sac_model_stick dump_bench_rvv
SSH_AUTH_SOCK=/run/user/$(id -u)/keyring/ssh make -C test-rvv/sample_consensus/sac_model_stick collect_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_stick record_repeated_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_stick repeated_evidence_status
```

PI3 还需要把 test-rvv 从测试派生类 candidate 改成或补成 production direct（真实生产路径）测试：公开 `countWithinDistance` 必须真实命中 RVV dispatch，并且 fallback case 单独证明不满足 gate 时回到 Std helper。PI4 需要重新生成 manifest，把 evidence role 从 `production_shaped_diagnostic` 改成 `production_public` 或补同一 production boundary 的 detail evidence。

## 暂停条件

命中以下任一条件时，停止并输出 Handoff，不进入 PI2 或 PI5 后采纳：

- 用户未授权修改 production。
- 生产 helper 需要改 public API、跨 topic 公共 API 或大范围模板声明。
- fallback gate 无法隔离，可能让非覆盖点型误命中 RVV。
- production direct correctness、fallback、asm attribution（反汇编归属）或 repeated board 证据失败。
- Evidence Doctor 出现未解决 Error，或 Warning 使 production decision bucket 不稳定。
- PI5 证据完成后等待用户确认采纳或回滚；不能自动进入 S11 adopted closeout。

## 当前状态

本 PI1 计划已创建，下一默认动作是等待用户授权 PI2 production patch。授权后不得扩大本文件冻结范围；若要覆盖泛型点型之外的其它入口、`selectWithinDistance`、`getDistancesToModel` 或 identity fast path，必须另建 phase。
