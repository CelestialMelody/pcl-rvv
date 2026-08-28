# Phase 040: cylinder point type expansion 计划

## 阶段意图和边界

本阶段验证已采纳的 cylinder production RVV（生产 RVV）路径是否可以从 `PointXYZ + pcl::Normal`
代表数据扩展到更多常见 traits-gated（字段特征门控）点型组合。当前 production 源码已经使用
`RVVXYZAoSFloatLayout<PointT>` 和 normal AoS layout gate，不是 exact type（具体类型）门控；因此仅有
`PointXYZ + Normal` 证据不足以说明其它会命中 gate 的模板实例语义和性能也成立。

本阶段只覆盖：

- public entry（公开入口）：`countWithinDistance`、`selectWithinDistance`、`getDistancesToModel`。
- row source（行来源）：direct indexed `indices_`。
- point type / layout（点型 / 布局）：新增常见代表组合：
  - `PointXYZI + Normal`：source xyz 后有 intensity，验证 source stride 扩展。
  - `PointXYZRGB + Normal`：source xyz 后有 packed rgb，验证常见彩色点。
  - `PointXYZ + PointNormal`：normal cloud 同时含 xyz 和 normal，验证 normal field offset 扩展。
- `Scalar`：仍是当前公开入口的 `Eigen::VectorXf`。
- bench case：默认 65536 点 shuffled adjacent pairs，200 iterations，5 warm-up；每个代表组合单独输出 production-public timing。

不覆盖自定义点型全集、`Scalar=double`、identity-index 专门优化、超大 offset 真实内存、NaN/Inf 或完整 RANSAC / MSAC / MLESAC 工作负载。

## 当前状态清单

| 来源 | 当前事实 | 对 Phase 040 的影响 |
| --- | --- | --- |
| production source | 三入口 RVV helper 已按 traits gate 接入。 | 可新增测试/bench，不需要改 production 算法。 |
| Phase 020 / 030 | `PointXYZ + Normal` 三入口 production direct board 均 positive，Doctor 0/0/0。 | 当前实现族值得扩展覆盖面。 |
| RVV traits | `RVVXYZAoSFloatLayout` 和 `RVVNormalFloatLayout` 支持 registered single-float xyz / normal fields。 | `PointXYZI`、`PointXYZRGB`、`PointNormal` 是合理代表组合。 |
| 风险 | 点型 stride / field offset 改变后 gather 成本可能不同；normal offset 不同可能影响 `vluxseg3ei32` / fields path。 | 需要独立 correctness、asm 和 board repeated，不能只靠源码 gate。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| point-type expansion without production algorithm change | 当前 RVV load wrapper 使用 offset 和 stride，常见 AoS 点型会保持语义且仍有正向收益。 | 更宽 stride 或非紧邻 normal 字段可能降低收益；某些点型只通过 correctness，不一定可写成性能采纳。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| source stride expansion | direct indexed `indices_` | `PointXYZI + Normal`, `PointXYZRGB + Normal` | 三条 public entries | 新增 public-vs-Standard typed gtest | typed bench labels | 5-run production repeated | 三个 production helper 仍命中 RVV | typed manifest / Doctor | planned |
| normal layout expansion | direct indexed `indices_` | `PointXYZ + PointNormal` | 三条 public entries | 新增 public-vs-Standard typed gtest | typed bench labels | 5-run production repeated | 三个 production helper 仍命中 RVV | typed manifest / Doctor | planned |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| typed fixtures | `src/test_sac_model_cylinder.cpp`、`src/bench_sac_model_cylinder.cpp` | 同一几何输入可生成不同 source / normal 点型。 |
| typed correctness | `src/test_sac_model_cylinder.cpp` | 三个代表组合的 count/select/getDistances public entry 均与 Standard helper 对齐。 |
| typed bench labels | `src/bench_sac_model_cylinder.cpp`、manifest wrapper | 每个代表组合输出 count/select/getDistances timing 和 checksum。 |
| asm | `check_production_asm` | production helper RVV 归属继续闭合。 |
| board repeated | `collect_production_repeated_board_evidence` | 5-run board 包含 typed labels，decision bucket 稳定。 |
| Doctor / registry | `record_production_board_evidence_state`、`production_evidence_status` | Doctor Errors=0；registry fresh。 |

## Evidence Doctor 和 registry 规则

manifest 必须保留 `production_direct` evidence role（证据角色），并把 typed comparison 的 point type 写清。
若 typed checksum 不一致或 Doctor Error 出现，先修 correctness / manifest；若某个点型 weak / neutral / negative，
只拒绝该点型性能扩展，不影响 Phase 020 / 030 的 `PointXYZ + Normal` 采纳结论。

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
| evidence role | production-direct；只使用接入后的 public entry Std/RVV typed board repeated。 |
| A/B boundary | Std build public entry vs RVV build 同一 public entry。 |
| 当前决策问题 | 常见点型组合是否能从 representative correctness / board evidence 扩展为 adopted typed boundary。 |
| diagnostic 是否可外推到 production | 不使用 diagnostic performance 外推；当前生产源码已接入，直接测 public entry。 |
| comparison-boundary / baseline mismatch 风险 | 通过同一 public overload、同一点型、同一 bench case 和 checksum 控制。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段就是 bounded production probe；不正向时只降级对应点型扩展。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不选择新 RVV family，只验证同一 production family 的点型范围；不需要 RVV-vs-RVV A/B。 |

## 继续 / 停止条件

若 typed correctness、asm、5-run board repeated 和 Evidence Doctor 都闭合，更新 Phase 040 result、matrix、roadmap、
topic-local docs 和 `doc-rvv` 范围表。若某点型不正向，把该组合写成 attempted / rejected，不回滚已采纳三入口。

Phase 040 后若仍继续，优先级较高的是 cone 新 topic；cylinder 内的 identity-index A/B 和 optimize staging 需要 profile 或用户点名。
