# Normal-plane Normal Layout 扩展 Phase Plan

## 阶段意图和边界

本阶段关闭 normal layout expansion（法线点类型布局扩展）的 correctness（正确性）与 fallback（回退路径）证据缺口。当前 production gate（生产分流准入）已经要求 source 点型满足 `RVVXYZAoSFloatLayout<PointT>`，normal 点型满足 registered single-float `normal_x/y/z/curvature`、standard-layout 和 float 对齐。本阶段只验证这个 gate 对更多 `PointNT` normal-like layout（法线类布局）是否可用。

| 维度 | 本阶段验证范围 |
| --- | --- |
| public entry | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` |
| row source | ordered `indices_`，索引按 `indices_` gather source 和 normals |
| source 点型 | `PointXYZ`，source 扩展已由 Phase 040/050 覆盖 |
| normal 点型 | `Normal`、`PointNormal`、`PointXYZINormal` 作为 AoS-compatible normal 代表点型 |
| fallback | registered single-float normal/curvature 但非 standard-layout 的 normal 点型；curvature 非 float 的旧 fallback 继续保留 |
| Scalar | `float` model coefficients，公开入口仍输出 double distances |
| 不证明 | 泛型 normal 点类型全集、source 点型全集、`Scalar=double`、公开入口性能、RVV family selection（RVV 实现族选择） |

## 当前状态清单

| 项 | 当前事实 | 路径 |
| --- | --- | --- |
| production helper | `NormalPlaneRVVNormalAoSLayout<PointNT>` 已使用 normal/curvature field gate 和 standard-layout/alignment gate；三条公开入口只在 source/normal 均兼容且点云规模满足 32-bit byte offset 上界时走 RVV。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` |
| 已有 correctness | `PointXYZ + Normal` public-vs-direct RVV、`PointXYZI` / `PointXYZINormal` source correctness、non-AoS source fallback、curvature double fallback 已通过。 | `src/test_sample_consensus_plane_models.cpp` |
| 已有 board evidence | Phase 030/050 repeated board performance 均 positive-stable；Phase 040 board public alias 6/6 通过。 | `log/board/normal-plane-phase030-repeated-board/summary.md`、`log/board/normal-plane-phase050-representative-aos-source-performance/summary.md` |
| phase 050 doctor | Errors=0、Warnings=5、Suggestions=0；Warnings 不改变 positive-stable bucket，但禁止按组均值外推。 | `log/board/normal-plane-phase050-representative-aos-source-performance/evidence-doctor.md` |
| 未关闭项 | roadmap 仍有 `other normal-like layouts`，Phase 040 result 写明只覆盖 `Normal` 和 double-curvature fallback。 | `doc/optimization-roadmap.zh.md`、`doc/phases/040-normal-plane-aospoint-gate-expansion/result.zh.md` |

## 假设与候选族

候选族是 `normal AoS layout expansion`：production 源码不新增算法路径，只用现有 `NormalPlaneRVVNormalAoSLayout<PointNT>` 支持更多 normal cloud 点型。`PointNT` 只读取 `normal_x/y/z/curvature`，额外的 `x/y/z`、`intensity` 或 label 字段不参与 normal-plane 距离公式；只要字段注册为单个 float 且 AoS byte-offset 前提成立，公开入口应该命中 RVV 并与 direct RVV helper 一致。

负向候选是 `non-AoS normal fallback`：如果 normal 点型注册了单 float normal/curvature，但类型不是 standard-layout，公开入口必须回退 Standard helper，避免 byte-offset gather helper 的 static assertion（编译期断言）或未定义布局访问。

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | correctness / fallback target | bench / board | asm | doctor | decision 预期 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| normal AoS layout expansion | ordered `indices_` | `PointXYZ + PointNormal`, float, AoS-compatible normal/curvature | 新增 public-vs-direct RVV test | board public alias 必要时复跑 | helper asm 不应改变 | registry freshness check | adopted for correctness if pass |
| normal AoS layout expansion | ordered `indices_` | `PointXYZ + PointXYZINormal`, float, AoS-compatible normal/curvature | 新增 public-vs-direct RVV test | board public alias 必要时复跑 | helper asm 不应改变 | registry freshness check | adopted for correctness if pass |
| non-AoS normal fallback | ordered `indices_` | `PointXYZ + NonAoSRegisteredNormal`, float fields but non-standard-layout | 新增 public-vs-standard fallback test | board public alias 必要时复跑 | not_applicable | registry freshness check | adopted fallback if pass |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 扩测试 fixture | `src/test_sample_consensus_plane_models.cpp` | 增加 AoS normal copy helper、`PointNormal` / `PointXYZINormal` public-vs-direct RVV 测试、non-AoS normal fallback 测试和 traits static asserts。 |
| 更新 target filter | `Makefile` | `NORMAL_PLANE_PUBLIC_FILTER` 覆盖新增 normal layout tests，board public alias 可运行。 |
| 本地 QEMU correctness | `run_normal_plane_public_tests`、`run_test_compare` | RVV 构建下新增测试通过；完整 compare 仍 25+N/25+N 通过。 |
| board public alias | `run_board_normal_plane_public_tests` | 当前板卡可用时命令级注入 SSH agent socket；新增 public alias 在板卡 RVV build 通过。 |
| 文档同步 | phase result、README、testing/correctness/optimization/evaluation、matrix、roadmap、长期 production doc 和队列表 | 记录本阶段已验证 normal layout、未验证范围、EvidenceDecision 和下一阶段默认入口。 |

## Evidence Doctor 和 Registry 规则

本阶段不生成新的性能 summary，因此不新增 phase 060 manifest。开始和结束时仍运行 `evidence_status`、`repeated_evidence_status` 和 `phase050_evidence_status`，确认已有 Phase 000/030/050 summary 与文档引用仍 fresh；若 registry 报告 `unregistered_change` 或 `stale_doc_pending_refresh`，先降级当前性能结论并修复引用。

## 板卡复跑预算和决策桶

本阶段的板卡动作是 public correctness alias，不是 repeated performance。预算为 1 次 `run_board_normal_plane_public_tests`；若 SSH、rsync 或远端 make 失败，先按输出判断是环境问题还是测试失败。性能决策桶沿用 Phase 030/050，不因本阶段 correctness smoke 改写。

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-public correctness（真实公开入口正确性）和 fallback correctness |
| A/B boundary | public overload 对 direct RVV helper，fallback 对 Standard helper |
| 当前决策问题 | implementation-shape 和 fallback correctness |
| diagnostic 是否可外推到 production | 本阶段测试直接调用真实 public entry，因此 correctness 可作为当前 production dispatch 证据；不外推性能。 |
| comparison-boundary / baseline mismatch 风险 | 低；public-vs-direct RVV 只判断是否命中 RVV 分流，public-vs-standard fallback 只判断不兼容 layout 不误入 RVV。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；生产源码已存在，失败时先修 gate 或降级该 normal layout 条目。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；本阶段不是 RVV family selection，也不改 helper 算法。 |

## Phase Scope 与扩展队列

`validated_scope` 预期为 `PointXYZ + PointNormal`、`PointXYZ + PointXYZINormal` normal cloud 的 public RVV correctness，以及 non-AoS registered normal fallback。`unvalidated_scope` 仍包括完整用户自定义 normal 点型全集、其它 source 点型与其它 normal 点型的交叉组合、`Scalar=double` 和公开入口性能。

`point_type_expansion_queue`：

| queue item | 恢复条件 | 所需证据 |
| --- | --- | --- |
| more PCL normal-like point types | 用户要求或本阶段发现 `PointNormal` / `PointXYZINormal` 不足以代表 normal field layout | traits audit、public direct/fallback test、QEMU、board alias |
| source × normal cross product | 需要证明 `PointXYZI`/`PointXYZINormal` source 与非 `Normal` normal cloud 组合 | 组合 public direct tests；必要时 dedicated helper bench |
| `Scalar=double` | 生产源码准备支持 double model coefficients 或 helper 重写 | 独立设计、数值预算、QEMU、asm、board evidence |

## 继续 / 停止条件

本阶段完成后，如果 correctness、board alias 和 registry freshness 均通过，normal layout expansion 可写为 adopted for representative normal layouts。若 roadmap 仍只有完整泛型全集、`Scalar=double` 或公开入口性能这类需要扩大范围或用户选择的方向，本轮可在 Handoff 中暂停并报告下一候选；若发现新增测试失败或 gate 与源码不一致，则停在 blocked / repair phase。

## 文档更新清单

- 新增 `doc/phases/060-normal-plane-normal-layout-expansion/result.zh.md`。
- 更新 `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`。
- 更新 topic-local `correctness-tests.zh.md`、`testing-overview.zh.md`、`optimization-evidence.zh.md`、`sac_model_normal_plane-evaluation.zh.md` 和 README。
- 更新适用的长期 `doc-rvv/sample_consensus/selectWithinDistance_getDistancesToModel_RVV.zh.md`，仅记录当前 adopted production behavior 和证据边界。
- 更新 `doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md` 的 normal-plane 状态。

## Roadmap 同步动作

若本阶段通过，`other normal-like layouts` 从 deferred 改为 closed representative correctness；`full generic point-type adoption` 仍 deferred，恢复条件改为用户明确要求完整泛型全集或新增调用点需要更宽点型组合。若本阶段失败，roadmap 新增 `normal layout gate repair`，优先级高于继续扩大点型。
