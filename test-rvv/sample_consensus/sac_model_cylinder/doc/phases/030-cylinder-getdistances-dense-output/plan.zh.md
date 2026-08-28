# Phase 030: cylinder getDistances dense output 计划

## 阶段意图和边界

本阶段验证 `SampleConsensusModelCylinder::getDistancesToModel` 是否值得接入 RVV（RISC-V Vector，可变长向量）
生产路径。它复用 Phase 020 已采纳的 cylinder distance kernel（圆柱距离核）：按 `indices_` 离散加载 point xyz
和 normal xyz，计算 radial distance（径向距离）与 normal angle（法线夹角），再把每个点的最终距离
dense store（连续写回）到 `std::vector<double>`。

本阶段只覆盖：

- public entry（公开入口）：`getDistancesToModel`。
- row source（行来源）：direct indexed `indices_`。
- point type / layout（点型 / 布局）：`PointXYZ + pcl::Normal` 代表样本；production gate 使用 traits-gated
  xyz + normal AoS（结构数组）布局，不把代表性能外推到所有点型。
- `Scalar`：当前 PCL 入口的 `Eigen::VectorXf` / float model coefficients；输出仍是 `double` vector。
- bench case：65536 点 shuffled adjacent pairs，200 iterations，5 warm-up。

不覆盖 `optimizeModelCoefficients`、`projectPoints`、`doSamplesVerifyModel`、其它 sample_consensus model、
`Scalar=double`、自定义点型性能、真实上游 RANSAC 分布或 identity-index 专门优化。

## 当前状态清单

| 来源 | 当前事实 | 对 Phase 030 的影响 |
| --- | --- | --- |
| Phase 020 production closeout | count/select public RVV 已接入并在 fresh production repeated board 上正向：count median `5.0328x`，select median `4.6756x`，Doctor 0/0/0。 | 可复用 traits gate、distance vector helper、indexed load 和 normal angle RVV 代码形态。 |
| 当前源码 | `getDistancesToModel` 仍是原标量循环，逐点写 `distances[i]`，没有 early gate。 | 可作为独立入口补 production helper 和 public-vs-Standard correctness。 |
| 诊断经验 | circle/line/stick 的 dense double store 已证明 `vfwcvt + vse64` 可以成为正向模式。 | 作为候选来源，不替代本 topic 的 correctness / asm / board。 |
| 风险 | `getDistancesToModel` 对所有点都读取 normal，不像 count/select 有 early gate；RVV 会完整计算每个 lane。 | 需要独立板卡 repeated，不能从 count/select speedup 外推。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| getDistances full-RVV dense double store | 逐点公式与 count/select 相同，去掉 `vcompress` 后可用 `vfwcvt.f.f.v + vse64.v` 连续写回，减少标量 `sqrt/acos` 与循环开销。 | 没有 early gate，全部 lane 都要跑 normal angle；若 `acos` helper 或 double 写回成本过高，收益可能低于 count/select。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| getDistances dense output | direct indexed `indices_` | `PointXYZ + Normal`, float xyz/normal AoS, double output vector | public `getDistancesToModel` | 新增 public-vs-Standard gtest；normal coverage fallback | bench 增加 `public getDistancesToModel` 行 | 5-run production repeated，包含 getDistances item | `getDistancesToModelRVVCylinder` 命中 `vfsqrt.v` / `vfwcvt.f.f.v` / `vse64.v` | production manifest / doctor 重新生成 | planned |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 生产 helper | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` | 抽出 `getDistancesToModelStandardCylinder`，新增 `getDistancesToModelRVVCylinder`，公开入口 RVV 成功则 return，失败 fallback。 |
| correctness | `src/test_sac_model_cylinder.cpp` | `run_test_compare` Std/RVV 全部通过；新增 dense distance 与 Standard helper 对拍。 |
| bench 输出 | `src/bench_sac_model_cylinder.cpp`、manifest wrapper | `public getDistancesToModel` 有 checksum 和 timing，production manifest 可选中 `getdistances`。 |
| asm | `script/check_cylinder_production_asm.py` | 新 helper 归属到 RVV 指令并包含 dense double store 关键指令。 |
| board repeated | `collect_production_repeated_board_evidence` | 5-run 都通过 gtest；getDistances speedup bucket 稳定。 |
| Evidence Doctor / registry | `record_production_board_evidence_state`、`production_evidence_status` | Doctor Errors=0；Warning 必须解释；registry fresh。 |

## Evidence Doctor 和 registry 规则

production manifest 必须使用 `production_direct` evidence role（证据角色）和 public overload A/B boundary（公开入口对比边界）。
若 checksum 不一致或 Doctor 出现 Error，停止并修复 correctness；若 getDistances 为 weak / neutral / negative，
不得回滚 Phase 020 已采纳的 count/select，只把 Phase 030 candidate 写成 attempted / rejected。

## 板卡复跑预算和决策桶

- run count：5。
- warm-up：5。
- iterations：200。
- positive：median speedup >= `1.20x` 且 5/5 run > `1.0x`。
- weak-positive：median `1.05x`-`1.20x` 且无 correctness / Doctor Error。
- neutral / negative：median <= `1.05x` 或退化 run 过多。
- unstable：复跑预算用完后 decision bucket 摇摆或 Doctor 指出未解释长尾。

## Diagnostic 到 Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-direct，最终只使用接入后的 public entry Std/RVV board repeated。 |
| A/B boundary | Std build public `getDistancesToModel` vs RVV build 同一 public `getDistancesToModel`。 |
| 当前决策问题 | 新增 getDistances RVV path 是否值得接入；不重新选择 count/select family。 |
| diagnostic 是否可外推到 production | 不使用 diagnostic performance 外推；Phase 020 只提供 code shape 来源。 |
| comparison-boundary / baseline mismatch 风险 | 通过同一 public overload、同一 bench case 和 checksum 消除主要 mismatch。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段就是 bounded production probe；若结果不正向，保留证据并等待用户确认是否回滚 getDistances patch。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有既有 getDistances RVV family；不需要 family-selection A/B。 |

## 继续 / 停止条件

若 correctness、asm、5-run board repeated 和 Evidence Doctor 都闭合，且用户给出的“板卡有收益即可采纳”准则成立，
则把 getDistances 写成同 topic 内新增 adopted production behavior，并刷新正式 `doc-rvv`。
若性能不支持采纳，停止在 PI5 getDistances review point，保留 patch 和证据，等待用户决定回滚或继续验证。

Phase 030 结束后仍可能存在 `040-point-type-expansion`，但它是覆盖面扩展，不是当前 getDistances 阶段内动作。
