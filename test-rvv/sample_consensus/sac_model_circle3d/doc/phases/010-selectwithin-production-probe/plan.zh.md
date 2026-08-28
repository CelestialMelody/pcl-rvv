# Phase 010 Plan: selectWithinDistance production probe

## 阶段意图和边界

本阶段把 Phase 000 中板卡正向的 `selectWithinDistance` 投影 RVV 候选推进到 production integration loop（生产接入闭环）。目标不是采纳 Phase 000 的 test-only（仅测试使用）数据，而是在 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` 的真实公开入口中接入窄范围 RVV dispatch（分流逻辑），再用 production direct（真实生产入口直连）correctness、fallback、asm attribution（反汇编归属）、repeated board（重复板卡性能测试）和 Evidence Doctor（证据体检）重新判断是否值得保留。

本阶段只覆盖 `SampleConsensusModelCircle3D<PointT>::selectWithinDistance`。`countWithinDistance` 在 Phase 000 的当前候选上 5/5 退化，保持标量；`getDistancesToModel` 存在公式符号审计缺口，本阶段不触碰。

## 范围冻结

| scope item | 本阶段状态 |
| --- | --- |
| validated_scope | `selectWithinDistance`，direct indexed `indices_`，traits-gated xyz AoS `PointT`，`pcl::index_t` 为 signed 32-bit，`Scalar=float` model coefficients，非退化投影，65536 点生产 bench。 |
| unvalidated_scope | `countWithinDistance`、`getDistancesToModel`、`Scalar=double`、非 xyz AoS layout、非 32-bit index、超出 u32 byte offset 的大点云、退化投影点、其它规模和其它输入分布。 |
| point_type_expansion_queue | Phase 010 使用 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 泛型 gate；生产直连测试先覆盖 `PointXYZ`。若后续扩大到 `PointXYZI`、`PointXYZRGB/RGBA` 或 normal 复合点型，需要新增 phase，补 fallback/correctness/asm/board/Evidence Doctor。 |
| phase_closeout_boundary | 只能关闭当前 gate 下的 `selectWithinDistance` production-public 证据；不能把结论外推到所有模板实例、其它入口或其它候选 family。 |

## 当前状态清单

| item | evidence / path | 状态 |
| --- | --- | --- |
| Phase 000 result | `doc/phases/000-circle3d-projection-component-ablation/result.zh.md` | select candidate B/A mean 1.1294，5/5 positive；count candidate B/A mean 0.5713，5/5 degrade。 |
| test-only candidate | `include/impl/sac_model_circle3d_candidates.hpp` | 已有 select RVV formula、mask、`vcompress` 输出和退化投影 fallback。 |
| production source | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` | 尚未接入本 topic RVV dispatch。 |
| topic docs | README、evaluation、testing overview、benchmark/evidence、optimization evidence、code map、matrix、roadmap | 已记录 Phase 000 诊断边界；本阶段完成后必须刷新为 production direct 证据。 |
| board availability | 当前用户说明板卡可用，topic 已有 `board_smoke` 和 repeated collect 入口。 | 本阶段必须继续跑到 repeated board + doctor + registry。 |

## 候选实现

本阶段采用 select-only RVV family：公开入口先执行既有 `isModelValid` 检查，再在 `__RVV10__` 下尝试内部 `selectWithinDistanceRVVCircle3D` helper；任一 gate 不满足时调用命名清楚的 `selectWithinDistanceStdCircle3D` 标量 helper。RVV helper 使用公共 `pcl::rvv_load::indexed_load3_f32m2`，由 `RVVXYZAoSFloatLayout<PointT>` 取得当前点类型的 xyz offset；`vcompress` 负责按 mask 保序压缩 inlier index，并把 float 平方距离拓宽写入 `error_sqr_dists_`。

保守 fallback gate：

- 非 RVV 构建自然只编译标量 helper。
- `PointT` 不满足 xyz 单 float AoS layout 时回退。
- `pcl::index_t` 不是 signed 32-bit 时回退。
- 点云规模超过 u32 byte offset 可表达范围时回退。
- normal norm 非法或任一参与点投影到圆心附近时回退，保留 Eigen `normalized()` 的既有退化语义。

## Diagnostic 到 Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 component ablation（组件消融）；Phase 010 目标是 production-public（公开入口标量/RVV）证据。 |
| A/B boundary | Phase 000 是 public helper vs test-only helper；Phase 010 必须是 Std binary public overload vs RVV binary public overload。 |
| 当前决策问题 | 当前 select-only RVV production dispatch 是否比标量公开入口更快，且语义/fallback/归属可维护。 |
| diagnostic 是否可外推到 production | 不能直接外推；只作为进入有界生产探针的依据。正式 `doc-rvv` 只能使用 Phase 010 接入后的数据。 |
| comparison-boundary / baseline mismatch 风险 | Phase 000 存在 wrapper/timer mismatch；Phase 010 通过 production-public manifest 降低该风险。 |
| diagnostic 弱/负/中性/不稳定时是否允许 bounded production probe | 本轮已由用户授权；若 Phase 010 production-public 结果不再正向，则保留 patch 并暂停报告，不自行回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本 topic 当前没有已采用 RVV family；决策是 RVV-vs-scalar，不是 RVV-family-selection。若未来出现替代 RVV family，再补 detail A/B。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / board target | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| select-only production RVV | direct indexed `indices_` | traits-gated xyz AoS `PointT` / `Scalar=float` coefficients / `PointXYZ` first evidence | `selectWithinDistance` public entry | `run_test_compare`；新增 production direct/fallback tests；非 RVV build fallback | 新增 `collect_select_production_repeated_board_evidence`，production-public Std/RVV | 新增 `check_select_production_asm`，生产 helper 符号内必须有 gather/FMA/sqrt/vcompress | 新增 production manifest + doctor + registry | planned |
| count projection RVV | direct indexed `indices_` | `PointXYZ` / float xyz AoS | `countWithinDistance` | Phase 000 已通过 correctness 但性能负向 | Phase 000 B/A mean 0.5713 | Phase 000 asm 已闭合 | Phase 000 doctor Error | rejected with evidence |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 写生产直连 evidence target | `Makefile`、`generate_circle3d_board_evidence_manifest.py`、`check_circle3d_projection_asm.py` | 能区分 Phase 000 component manifest 和 Phase 010 production-public manifest。 |
| RED asm gate | `make -C test-rvv/sample_consensus/sac_model_circle3d check_select_production_asm` | 接生产前应因生产 RVV helper 符号缺失失败。 |
| 接生产源码 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` | public entry 保持“语义检查 -> RVV 短路 -> Std fallback”形态；不改 public API。 |
| correctness/fallback | `make -C test-rvv/sample_consensus/sac_model_circle3d run_test_compare` | Std/RVV 全部通过，包含退化投影 fallback。 |
| asm attribution | `make -C test-rvv/sample_consensus/sac_model_circle3d check_select_production_asm` | 生产 helper 符号中归属 `vlux*` 或 `vluxseg*`、`vfmacc`、`vfnmsac`、`vfsqrt.v`、`vcompress.vm`。 |
| board repeated | `make -C test-rvv/sample_consensus/sac_model_circle3d collect_select_production_repeated_board_evidence` | 5-run，默认 65536 点、200 iter、20 warm-up，Std/RVV 公开入口 checksum 一致。 |
| doctor / registry | `make -C test-rvv/sample_consensus/sac_model_circle3d record_select_production_board_evidence_state` 和 `select_production_evidence_status` | doctor 无阻塞 Error；registry fresh。 |
| 文档 closeout | Phase 010 result、matrix、roadmap、evaluation、topic docs、`doc-rvv/sample_consensus/sac_model_circle3d-RVV.zh.md` | 只引用 Phase 010 接入后的 production direct 数据作为正式采用数据。 |

## 板卡预算和决策桶

默认板卡预算为 5-run repeated，warm-up 20 次。B/A = Std public ms / RVV public ms；`> 1` 表示 RVV 更快。

| bucket | 判定 |
| --- | --- |
| positive | 5/5 B/A > 1，mean 和 median 均 >= 1.05，doctor 无阻塞 Error。 |
| weak-positive | 多数 B/A > 1，但 mean 或 median 落在 1.00 到 1.05；可保留为人工判断候选。 |
| neutral | mean/median 接近 1 或方向不足以覆盖维护成本。 |
| negative | 多数 B/A < 1 或 doctor 报退化频率 Error。 |
| unstable | 方向摇摆，预算内不能稳定归桶。 |

若 production-public bucket 为 positive，按用户本轮授权进入“建议保留 / 可采纳”文档 closeout；若 weak/neutral/negative/unstable，暂停并报告补丁状态，不自行回滚。

## 文档更新清单

本阶段完成后同步：

- `doc/phases/010-selectwithin-production-probe/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/sac_model_circle3d-evaluation.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/test-support-code-map.zh.md`
- `README.zh.md`
- `doc-rvv/sample_consensus/sac_model_circle3d-RVV.zh.md`，仅在 Phase 010 production evidence 支持保留时创建。

## 继续 / 停止条件

未命中停止条件时，本阶段持续推进到生产接入、测试、板卡、doctor、registry 和文档 closeout。停止条件只包括：生产接入会扩大到本计划未授权入口、板卡或工具不可用、Evidence Doctor 阻塞 Error 无法修复、dirty isolation 不安全、或 production-public 结果不支持保留且需要用户决定是否回滚。
