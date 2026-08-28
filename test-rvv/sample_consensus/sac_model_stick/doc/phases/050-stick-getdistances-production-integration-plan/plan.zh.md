# Phase 050: stick getDistances production integration plan

## PI1 计划边界

本文件只记录 `SampleConsensusModelStick<PointT>::getDistancesToModel` 的 PI1 production integration plan（生产接入计划）。它不是 production patch（生产补丁），不授权修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp`，也不把 Phase 040 的 test-only diagnostic（测试专用诊断）提升为 production direct evidence（真实生产路径证据）。

进入 PI2 前必须得到用户明确授权。

## 候选范围

| 维度 | 冻结范围 |
| --- | --- |
| production entry | `SampleConsensusModelStick<PointT>::getDistancesToModel` |
| first production scope | traits-gated（字段特征门禁）float xyz AoS direct indexed `indices_`，以 Phase 040 已验证的 `PointXYZ` 为首轮证据 |
| coefficient semantics | 保持当前源码语义：`model_coefficients[0..2]` 是 line point（线上的点），`model_coefficients[3..5]` 是 line direction（线方向），不是第二端点 |
| algorithm family | indexed xyz gather + cross3/squaredNorm + `vfsqrt` + scalar double dense store；`sqr_distance >= radius_max_^2` 时写 `2 * sqrt(sqr_distance)` |
| fallback | 非 RVV 构建、非支持布局、offset 超出 `u32` byte offset、其它未证明点型或 layout 均落回原标量语义 |
| excluded | `countWithinDistance` / `selectWithinDistance` production patch、identity fast path、三入口合并重构、泛型点型最终结论、真实 RANSAC 上游性能 |

## 建议的 production 结构

如果进入 PI2，建议先把原公开入口主体抽成 `getDistancesToModelStd` 或邻近等价 internal helper，再新增 `getDistancesToModelRVV`。公开入口保持“模型有效性检查 -> RVV 短路 -> Std fallback”的短结构。若 deprecated class（已废弃类）的模板声明让成员 helper 修改面过大，可以使用同文件 `detail` free helper，但 Handoff Packet（交接数据包）必须说明选择原因。

RVV helper 优先复用公共 xyz indexed load wrapper（索引离散加载封装）和 traits gate：

- `pcl/common/rvv_point_traits.h`
- `pcl/common/rvv_point_load.h`
- `pcl/common/impl/rvv_point_load.hpp`

新增 production 注释只解释范围、fallback、32-bit offset gate、direction coefficient 语义和 `radius_max_` penalty，不逐行解释 intrinsic（内建函数）。

## fallback / dispatch 计划

| fallback 条件 | 预期行为 | 验证方式 |
| --- | --- | --- |
| 非 RVV 构建或未定义 `__RVV10__` | 编译时只保留 Std helper | Std build gtest 通过，反汇编无 RVV 分流要求 |
| 点类型不满足 `RVVXYZAoSFloatLayout<PointT>` | public entry 落回 Std helper | 新增非覆盖点型或 traits-gate 编译 / 运行测试 |
| cloud 太大导致 32-bit byte offset 不可表达 | RVV helper 返回 false 或入口落回 Std | 构造 gate 单元测试或 helper-level fallback test |
| model invalid | 保持当前公开入口输出语义 | 复用或新增 public invalid model test |
| `indices_` 为空 | 输出空 `distances`，不触发越界 | 新增 explicit empty indices correctness |
| `radius_max_` penalty | 平方距离不小于 `radius_max_^2` 的点输出双倍距离 | production direct case 覆盖内外阈值两侧 |
| `countWithinDistance` / `selectWithinDistance` | 保持原标量或各自独立生产计划 | 不修改对应入口；diff 和测试确认 |

## PI2-PI5 证据计划

| step | 需要产物 | 验收 |
| --- | --- | --- |
| PI2 production patch | `sac_model_stick.h/.hpp` 中窄范围 helper / dispatch，保留清晰 Std fallback | 公开 API 不变；公开入口不保留大段重复标量主体；注释只说明 gate / fallback / direction coefficient / penalty 边界。 |
| PI3 correctness | production direct Std/RVV tests | 公开入口与 Std helper 对拍，覆盖方向系数、dense 输出顺序、penalty、空 indices 和 fallback。 |
| PI4 static evidence | `dump_bench_rvv` 或 dedicated asm target | production helper / public path 可归属 `vfmacc.vv`、`vfsqrt.v`、mask 或 store；若内联，manifest 记录真实承载边界。 |
| PI4 board evidence | production public repeated board | 5-run bounded budget；性能结论只来自板卡或目标硬件。 |
| PI4 doctor / registry | production manifest、Evidence Doctor、registry | Errors 必须修正或降级；Warnings / Suggestions 必须在 result 和 Handoff 解释。 |
| PI5 user checkpoint | 保留 patch，报告 diff、命令、证据和拟议下一步 | 无论 positive、weak、negative 或 unstable，都暂停等待用户确认采纳或回滚。 |

## diagnostic-to-production mismatch audit

Phase 040 candidate 在 test-only helper 中达到 positive-stable：median/min/max 为 `2.5553x / 2.5416x / 2.7849x`。它支持 bounded production probe，但不证明 production dispatch 已经存在。Phase 040 public `getDistancesToModel` 行触发 Evidence Doctor Error，处理结果是降级为未接 RVV 的 negative cross-check；这不会否定 candidate，但要求 PI2 后在同一 production public boundary（生产公开边界）内重跑 correctness、asm、board repeated 和 Evidence Doctor。

## 当前停止条件

`pending_user_authorization_for_PI2`。没有用户明确授权前，worker 不修改 production 源码，不创建长期 `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md`，不把本计划写成 adopted production behavior。
