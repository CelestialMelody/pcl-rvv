# Phase 020: cylinder production integration

## 阶段意图和边界

本阶段连续推进 PI2-PI5 production integration loop（生产接入闭环）：把 Phase 000 已验证的
count/select diagnostic candidate（诊断候选）接入真实 `SampleConsensusModelCylinder` 公开入口，
再用 production direct（真实生产路径证据）重新跑 correctness（正确性）、QEMU（仿真器）日志形状、
反汇编归属、板卡 repeated（重复板卡测试）和 Evidence Doctor（证据体检）。

本阶段允许修改 production 文件：

- `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp`

不修改 public API（公开接口）声明，不改 `getDistancesToModel`、`optimizeModelCoefficients`、
`projectPoints` 或 `doSamplesVerifyModel` 的生产行为。若实现需要扩大到其它入口、其它模块或公共
load/store API，立即停止并回到 Handoff。

## 当前状态清单

| area | 当前事实 | 本阶段动作 |
| --- | --- | --- |
| Phase 000 diagnostic | `countWithinDistanceCandidateRVV` 和 `selectWithinDistanceCandidateRVV` 在 `PointXYZ + Normal` direct indexed `indices_` 下 correctness、asm、5-run board repeated 已闭合。count median `7.22x`，select median `6.32x`。 | 只迁移同一实现族，不新增 family selection（实现族选择）问题。 |
| Phase 010 PI1 | PI1 已冻结 scope、fallback、证据计划，但因当时缺少 production 修改授权停在 PI2 前。 | 用户本轮说明“接入后板卡有收益即可采纳”，解除 PI2 前授权阻塞；PI5 仍要保留证据和 diff。 |
| production source | `countWithinDistance` / `selectWithinDistance` 当前是纯标量模板公开入口。 | 抽成 Standard helper（标量 helper）和 RVV detail helper（内部 RVV helper），公开入口只做 `isModelValid` 与 dispatch。 |
| test / bench | topic 已有 public companion 与 diagnostic candidate labels。 | 接入后 public label 变成 production-public 证据；保留 diagnostic candidate 作为 historical/cross-check。 |

## Phase scope 与扩展队列

`validated_scope`：本阶段只准备证明 `countWithinDistance` / `selectWithinDistance` 的 public overload，
direct indexed `indices_`，`PointXYZ + Normal`，`Eigen::VectorXf` coefficients，`double threshold`
但 RVV 内部按 float 公式，输入点和法线 AoS（结构数组）字段满足 RVV gate。

`unvalidated_scope`：`getDistancesToModel`、其它 production 入口、`PointXYZI` / RGB / RGBA / normal 复合点型、
自定义点型、`Scalar=double`、非 direct indexed row source（行来源）、超大点云 32-bit byte offset 边界的真实内存 case。

`point_type_expansion_queue`：若本阶段 production-public board positive，后续 phase 可做 PointXYZ-like /
Normal-like traits 扩展，至少补 representative point type correctness、fallback、asm、5-run board 和 Doctor。
本阶段的 `PointXYZ + Normal` 板卡数字不能外推成完整模板泛型结论。

## 候选实现形态

1. 在 `pcl::detail` 下新增 `CylinderRVVNormalAoSLayout` 和 `kCylinderRVVLayoutCompatible`，复用
   `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 与 normal field gate（法线字段准入）。
2. 把 `countWithinDistance` / `selectWithinDistance` 的原标量主体抽到
   `countWithinDistanceStandardCylinder` / `selectWithinDistanceStandardCylinder` free helper。
3. 在 `__RVV10__` 下新增 `countWithinDistanceRVVCylinder` / `selectWithinDistanceRVVCylinder`，
   使用 indexed xyz/normal gather（按索引离散加载）、radial norm、acute normal angle（锐角法线夹角）、
   `vcpop.m` 和 `vcompress.vm`。
4. RVV helper 以 `bool` 返回是否命中；点云规模超过 `rvvMaxU32ByteOffsetElements` 时返回 `false`，
   public entry 使用 Standard fallback。

## Fallback 矩阵

| gate | 期望行为 | 验证方式 |
| --- | --- | --- |
| 非 RVV 构建 | 编译时没有 RVV helper，public entry 走 Standard helper。 | `make run_test_std`。 |
| 点型 / 法线布局不满足 | `if constexpr` 不实例化 RVV helper，走 Standard helper。 | 代码审查；本阶段不强造 custom point type。 |
| `pcl::index_t` 不是 signed int32 | `select` 不进入 RVV，避免压缩索引写回类型不匹配。 | compile-time gate 审查。 |
| cloud / normal 超过 32-bit byte offset | RVV helper 返回 `false`，public entry 走 Standard helper。 | 代码审查；不分配超大内存。 |
| invalid model | 保持现有语义：count 返回 0，select 清空输出。 | gtest public entry 覆盖。 |
| 小规模 / tail | RVV 或 Standard 都必须输出一致。 | `run_test_compare` 的小输入覆盖。 |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| PI2 production patch | 修改 `impl/sac_model_cylinder.hpp` | public entry 有 Standard/RVV dispatch，非 RVV 构建仍可编译。 |
| PI3 production direct correctness | 更新 `src/test_sac_model_cylinder.cpp` | Std/RVV public entry、diagnostic cross-check、invalid model、select stale state 全通过。 |
| PI4 asm attribution | 新增或扩展 `check_production_asm` | production helper 或可接受 inline boundary 内出现 `vfsqrt.v`、`vcpop.m`、`vcompress.vm`。 |
| PI4 board repeated | 更新 bench label、manifest、Makefile target，运行 5-run board repeated | public count/select Std/RVV repeated 为 positive 或按桶降级。 |
| PI5 EvidenceDecision | 运行 Evidence Doctor、registry freshness、更新文档 | Errors=0；Warnings 必须解释；若 production evidence positive，则进入用户允许的采纳 closeout。 |

## Evidence Doctor 和 registry 规则

本阶段新增 run label：`cylinder-phase020-production-repeated-board`。manifest 路径：

- `doc/phases/020-cylinder-production-integration/production-repeated-evidence-manifest.json`
- `doc/phases/020-cylinder-production-integration/production-repeated-evidence-doctor.md`
- `doc/phases/020-cylinder-production-integration/production-repeated-evidence-doctor.json`

registry（证据登记表）写入 `log/evidence_registry.json`。若 registry 检查显示本阶段 summary /
manifest / doctor 未登记或被覆盖，不能关闭 PI5。

## 板卡复跑预算和决策桶

默认 5-run production repeated。若 Doctor 暴露长尾、方向接近阈值或与 Phase 000 diagnostic 方向冲突，
最多追加一次同边界确认复跑；用户未要求扩大预算时不无限复跑。

decision bucket（决策桶）：

- median speedup >= `1.20x` 且无 Error：positive。
- `1.05x <= median < 1.20x`：weak_positive，可接入但必须说明维护成本和风险。
- `0.95x <= median < 1.05x`：neutral。
- median < `0.95x`：negative。
- 跨桶摇摆或 Error 未解：unstable / blocked。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 `production_shaped_diagnostic`；本阶段必须生成 `production-public`。 |
| A/B boundary | Phase 000 是 test helper；本阶段是 public overload，必要时辅以 production detail helper asm。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path，并且是否值得保留 production patch。 |
| diagnostic 是否可外推到 production | 只外推公式和实现族，不外推真实 performance；最终以本阶段 production-public board 为准。 |
| comparison-boundary / baseline mismatch 风险 | 存在；因此本阶段必须重跑 public Std/RVV repeated。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | Phase 000 已 positive；若本阶段弱 / 负 / 中性 / 不稳定，停在 PI5 等待人工决定回滚或继续验证。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前 cylinder 没有已采纳 RVV family，本阶段只迁移 Phase 000 family；不需要 RVV-vs-RVV family A/B。 |

## 文档更新清单

本阶段若 production-public evidence positive，更新：

- `doc/phases/020-cylinder-production-integration/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/sac_model_cylinder-evaluation.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md`

若 production evidence 不成立，保留 production patch 并停在 PI5，等待用户确认回滚。
