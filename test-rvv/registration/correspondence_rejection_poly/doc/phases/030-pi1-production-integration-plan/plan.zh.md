# Phase 030 PI1 production integration plan

## 阶段意图和边界

本阶段进入 production integration loop（生产接入闭环）的 PI1-PI5。PI1 的 gate（准入条件）是把 Phase 020 的 `edge_gather_staging` 诊断候选收窄成真实 `CorrespondenceRejectorPoly::getRemainingCorrespondences` 入口里的可维护 RVV 分流，而不是扩大到整个类的所有潜在优化。

本阶段允许修改：

- `registration/include/pcl/registration/correspondence_rejection_poly.h`
- `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`
- `test-rvv/registration/correspondence_rejection_poly/**`
- `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md`
- `tmp/rvv-work-logs/registration/correspondence_rejection_poly/**`

本阶段不可扩大范围：

- 不修改 public API（公开接口）的函数签名。
- 不把 `thresholdPolygon`、`thresholdEdgeLength` 或其它 protected helper 改成公共接口。
- 不接入 `accept_rate_filter`、histogram（直方图）或 Otsu threshold（Otsu 阈值）RVV 路径。
- 不为 `Scalar=double`、normal 字段、非法 correspondence index、其它 registration topic 或其它模块做生产承诺。
- 不把 QEMU timing（QEMU 计时）写成性能结论。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| 标量入口 | `getRemainingCorrespondences` 仍是完整标量主体，尚未拆出 `Standard` helper。 | `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp` |
| 诊断候选 | `edge_gather_staging` 已覆盖 correspondence index 读点、squared distance staging（平方距离暂存）和 RVV edge formula（边公式）。 | `test-rvv/registration/correspondence_rejection_poly/include/impl/correspondence_rejection_poly_candidates.hpp` |
| 正确性 | QEMU / board smoke 当前 7 个测试通过。 | `test-rvv/registration/correspondence_rejection_poly/log/qemu/run_test_*.log`、`log/board/test_smoke/run_test.log` |
| 板卡性能 | `edge_gather_staging` 5-run board repeated 为 `weak_positive`：64K median 1.100x，256K median 1.119x，0/5 degradation。 | `test-rvv/registration/correspondence_rejection_poly/log/board/edge_gather_staging_repeated/summary.md` |
| Evidence Doctor | `edge_gather_staging` board doctor clean：Errors=0，Warnings=0，Suggestions=0。 | `test-rvv/registration/correspondence_rejection_poly/log/board/edge_gather_staging_repeated/evidence_doctor.md` |
| 生产状态 | 尚未修改 production，当前 Handoff 写作 `diagnostic/no-production`。 | `tmp/rvv-work-logs/registration/correspondence_rejection_poly/current-handoff/current-handoff.zh.md` |

## 标量路径和 RVV 覆盖范围

当前标量流程：

1. public entry（公开入口）先把 `remaining_correspondences` 设成输入副本。
2. 检查 source / target 是否存在、`cardinality_`、correspondence 数量和 similarity threshold（相似度阈值）。
3. 重新计算 `similarity_threshold_squared_`。
4. `iterations_` 次循环中用 `std::rand()` 采样 `cardinality_` 个唯一 correspondence 下标。
5. `thresholdPolygon` 调用 `thresholdEdgeLength`，比较 source edge squared distance（源边平方距离）和 target edge squared distance（目标边平方距离）的 `min/max` ratio。
6. 根据 polygon 是否通过更新 `num_samples` 和 `num_accepted`。
7. 标量计算 accept rate（接受率）、histogram、Otsu cut（Otsu 切分点）。
8. 只保留 `accept_rate[i] > cut` 的原始 correspondence，并保持原输入顺序。

本阶段只让 RVV 覆盖第 4-6 步中的 edge-length predicate（边长谓词）批量计算：

- 采样仍由原 `getUniqueRandomIndices` 负责，保持 `std::rand()` 调用顺序。
- `num_samples` 更新仍按采样结果逐项执行，保持原语义。
- RVV 批量评估每个 sampled polygon 的边长相似度 mask（掩码）。
- polygon 是否 accepted 的归约和 `num_accepted` 更新仍由标量 tail（标量尾段）完成。
- accept rate、histogram、Otsu 和最终输出保持标量。

## 用户关于测试数据随机性的反馈处理

当前已有测试主要是 deterministic corpus（确定性样本集）：固定 synthetic cloud（合成点云）、identity correspondences（同下标对应关系）、固定 edge pairs（边对）和固定 `std::srand` public-entry smoke。它们可重复，适合保护已知边界，但不足以覆盖更多随机输入分布。

本阶段新增 seeded random stress（固定种子随机压力样本），用 `std::mt19937` 生成点云扰动、乱序 correspondence、不同 seed / size / cardinality / threshold 组合，并在每个 case 中分别重置 `std::srand(seed)` 对拍 reference path（参考链路）和真实 production public entry。随机样本必须固定 seed、固定参数并写入文档，避免不可复现。

## 生产实现计划

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| PI2-A：拆分标量主体 | `getRemainingCorrespondencesStandard(...)` 声明和定义 | `getRemainingCorrespondences` public entry 只做 RVV dispatch / fallback，不保留大段标量主体。 |
| PI2-B：新增 RVV helper | `getRemainingCorrespondencesRVV(...)`，只在 `#if defined(__RVV10__)` 下存在 | unsupported gate 返回 `false` 并自然 fallback；命中时完整生成 `remaining_correspondences`。 |
| PI2-C：edge work staging | production 内部局部结构或向量：source/target 端点 index、polygon 映射、polygon edge count | 保持 `std::rand()` 采样顺序和 `num_samples` 更新顺序。 |
| PI2-D：RVV edge predicate | 使用 `pcl::rvv_load::indexed_load3_fields_f32m2` 与 `pcl::rvv::RVVXYZAoSFloatLayout` | source / target 分别 gate，32-bit byte offset 规模 gate 闭合，`0/0` NaN 拒绝语义保持。 |
| PI2-E：生产注释 | 仅解释 dispatch、fallback、layout 和 staging 边界 | 不写逐行 intrinsic 教程。 |

## Fallback 矩阵

| 条件 | 预期行为 | 测试 / 证据 |
| --- | --- | --- |
| 非 RVV 构建 | 只编译并执行 `Standard` helper。 | Std build `run_test_compare`。 |
| `__RVV10__` 关闭 | public entry 自然调用 `getRemainingCorrespondencesStandard`。 | Std build 编译和测试。 |
| 点类型不满足 `RVVXYZAoSFloatLayout` | fallback 到 `Standard`。 | production direct fallback case，使用不满足布局 gate 的点型或编译期 Std build 边界。 |
| source / target size 超过 32-bit byte offset gate | fallback 到 `Standard`。 | 文档说明；实际构造极大云不作为本阶段必测项。 |
| `cardinality_ < 2`、`cardinality_ >= nr_correspondences`、invalid threshold、缺 source/target | 保持原 PCL_ERROR 和返回输入副本语义。 | 现有 guard test + production direct 回归。 |
| `edge_count == 0` 或规模过小 | fallback 到 `Standard` 或 RVV helper 返回 false；不得改变输出。 | 小规模 fallback test。 |
| histogram / Otsu / output append | 保持标量，不属于 RVV gate。 | fixed corpus + seeded random public entry 对拍。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `production_edge_batch_rvv` | correspondences | traits-gated xyz AoS / `float` source+target | real public entry `getRemainingCorrespondences` | planned: fixed corpus、seeded random、guard/fallback | planned: `production-direct` bench case | planned: repeated board | planned: production symbol or inlined hot region attribution | planned | PI5 decision pending |
| `accept_rate_filter` | contiguous counters | `int` counters -> `float` rates | test-only candidate | already pass | already neutral | neutral / no-production | partial | warning in confirm | rejected for this phase |
| `histogram_otsu_scalar` | accept-rate values | scalar | production scalar tail | existing pass | no bench needed | not_applicable | not_applicable | manual | adopted scalar |

## 实现和测试动作

1. 写生产补丁：按 public entry dispatch -> `getRemainingCorrespondencesRVV` -> `getRemainingCorrespondencesStandard` 的形态接入。
2. 补 production direct correctness（真实生产路径正确性）：固定 corpus、固定 seed 随机压力样本、cardinality 2/3/4、threshold 边界、小规模 fallback、guard 回归。
3. 补 bench case：新增或改造 `production-direct` case，计时真实 public entry；Std build 是 `Standard`，RVV build 命中 production RVV dispatch。
4. 补 QEMU smoke / manifest / doctor target：QEMU 只验证构建、correctness、日志形状和证据合同。
5. dump asm：检查 `vluxei32`、`vfmin`、`vfmax`、`vfdiv`、`vmfge`、mask store 等关键 RVV 指令是否可归到 production helper 或其内联热区。
6. 跑 board correctness 和 repeated board production-direct bench。板卡可用，本阶段不以“缺板卡”停止。
7. 更新 phase result、optimization matrix、roadmap、evaluation、适用的 `doc-rvv` 长期文档和 current Handoff。

## Evidence Doctor 和 registry 规则

- production direct board summary 必须生成 `summary.md`、`evidence_manifest.json` 和 `evidence_doctor.md`。
- Evidence Doctor（证据体检）Errors 必须修复或降级，不能进入 PI5 严格性能结论。
- Warnings 必须解释、必要时重跑或降级边界。
- `log/evidence_registry.json` 必须登记新的 QEMU / board / asm / doctor summary；raw logs 默认 local-only。

## 板卡复跑预算和决策桶

| 项目 | 预算 | 判断口径 |
| --- | --- | --- |
| production-direct repeated board | 默认 5-run；若 bucket 因 1 次异常摇摆，最多 1 次 5-run 确认复跑 | median 全部 >=1.05 且 degradation fraction <=20% 为 `weak_positive`；全部 >=1.20 且无退化为 `positive`；median <0.95 或退化比例 >20% 为 `negative`；其它为 `neutral`。 |
| board correctness | 至少 1 次 smoke | 必须全测试通过。 |
| asm attribution | 至少 1 次 RVV bench dump | 指令存在但归属不闭合时降级为 partial，不能写 production-ready。 |

## 阶段完成条件

进入 `production-ready` 的最低条件：

- production public entry 已按 `Standard` / `RVV` 分层。
- Std build 和 RVV build correctness 都通过。
- 固定 seed 随机压力样本与 reference 输出一致。
- fallback matrix 中本阶段可执行项已覆盖或有明确不适用理由。
- production-direct repeated board bucket 至少 `weak_positive`，且 Evidence Doctor 无 Error。
- asm 归因能证明 RVV 指令属于 production direct 热路径或其内联区域。

转入 rollback / no-production 的条件：

- correctness 或 fallback 不能闭合。
- production-direct board bucket 为 `neutral` 或 `negative`，且无可接受维护理由。
- Evidence Doctor Error 不能修复。
- asm 不能证明 production direct 命中 RVV，且无法通过 target 或符号调整复核。

暂停条件：

- 继续需要修改 public API 或跨 topic 公共封装。
- 需要扩大到 `Scalar=double`、normal 字段、非 xyz AoS 点型或其它 row source。
- 发现 production direct 与 Phase 020 诊断方向矛盾，需要用户决定是否回退。

## 文档更新清单

- `doc/phases/030-pi1-production-integration-plan/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/correspondence_rejection_poly-evaluation.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md`，仅在 production patch 和 PI5 证据闭合后作为 production 长期主题文档更新。
- `tmp/rvv-work-logs/registration/correspondence_rejection_poly/current-handoff/current-handoff.zh.md`

## roadmap 同步动作

若 production direct 成立，将 `production_edge_batch_rvv` 标为 adopted，并把 generic point type / `Scalar=double` 扩展放入后续可选路线。若不成立，将 Phase 020 `edge_gather_staging` 降级为 diagnostic historical evidence，并把 production 路径回收到 no-production closeout。
