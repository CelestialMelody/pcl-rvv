# Phase 010 Plan: Production Integration

## 阶段意图和边界

本阶段把 Phase 000 中 positive 的 f64 scatter diagnostic（双精度散布矩阵诊断候选）做成 bounded production probe（有界生产探针）。production 入口是 `ISSKeypoint3D::getScatterMatrix`，公开 API 不变。RVV path（RVV 路径）只覆盖 `searchForNeighbors` 已返回足够邻域后的 3x3 scatter 累加；`searchForNeighbors`、BoundaryEstimation、Eigen EVD、NMS 和输出写入保持原生产逻辑。

本轮用户明确允许：如果 production direct board（真实生产路径板卡）结果显示有收益，可以自行采纳 production patch，并创建 `doc-rvv/keypoints/iss_3d-RVV.zh.md`。如果 production direct 证据不支持收益，不能写成 adopted；阶段结果需要记录不采纳理由，并把生产源码恢复到标量当前状态。

## PI2 Scope

| field | value |
| --- | --- |
| entry | `keypoints/include/pcl/keypoints/impl/iss_3d.hpp::ISSKeypoint3D::getScatterMatrix` |
| point type gate | `pcl::rvv::kRVVXYZAoSPointCompatible<PointInT>`，即注册的单 float `x/y/z` AoS 布局；不硬编码 `PointXYZ`。 |
| scalar type | production matrix remains `Eigen::Matrix3d`; RVV 只从 float xyz widen（拓宽）到 double 后累加。 |
| row source | indexed neighbor list returned by `searchForNeighbors`。 |
| size gate | `n_neighbors >= 16` and `input_->size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointInT>()`。 |
| forbidden expansion | 不改 public API，不改 search / EVD / NMS，不扩大到 normal 字段、其它模块或其它 keypoints topic。 |

## Fallback Matrix

| fallback case | expected behavior | evidence |
| --- | --- | --- |
| non-RVV build | 编译期没有 `__RVV10__` 时只保留原标量 helper。 | `run_test_compare` 的 Std binary。 |
| layout gate false | 点型不满足 xyz AoS float traits 时编译期保留标量 loop。 | 文档和 compile gate；若后续扩点型需新增测试。 |
| small neighbor count | `n_neighbors < 16` 走标量 loop，避免短邻域 RVV overhead。 | `CandidateHandlesTailNeighborCount` 和 production scatter probe。 |
| min neighbors not met | 保持原 `n_neighbors < min_neighbors_` early return。 | production scatter probe / public compute correctness。 |
| large cloud offset | `input_->size()` 超过 32-bit byte offset 可表达范围时回退。 | code gate + Handoff 风险说明。 |

## Production Direct Tests And Bench

| action | command / target | expected result |
| --- | --- | --- |
| T1: 先补生产 asm gate 并验证 RED。 | `make -C test-rvv/keypoints/iss_3d check_iss_3d_production_rvv_asm` before production patch | 失败：找不到 production RVV scatter helper。 |
| T2: 补 production scatter correctness probe。 | `make -C test-rvv/keypoints/iss_3d run_test_compare` | Std/RVV 都通过；RVV binary 通过 protected `getScatterMatrix` 与独立 reference 对拍。 |
| T3: production asm。 | `make -C test-rvv/keypoints/iss_3d check_iss_3d_production_rvv_asm` after patch | 通过：production helper 符号内存在 `vluxei32` 和 `vfred*.vs`。 |
| T4: production public board smoke/repeated。 | `run_board_bench_compare` / `board_repeated_production` with `--mode public` | 判断真实 `compute()` 边界是否 positive。 |
| T5: Evidence Doctor / registry。 | `record_evidence_state_production` | Errors=0；Warnings 必须解释或降级。 |

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 010 使用 production_public（真实生产公开入口）和 production_detail asm。 |
| A/B boundary | public `compute()` bench plus production detail helper asm；Phase 000 diagnostic 只作为进入 probe 的依据。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path；不是 RVV-family-selection。 |
| diagnostic 是否可外推到 production | 不直接外推。只有 production board bench 正向才采纳。 |
| comparison-boundary / baseline mismatch 风险 | 有：production 包含 search / Eigen / NMS，局部 scatter speedup 可能被稀释。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | Phase 000 rerun1 为 positive，允许本阶段 probe；首批 unstable 作为历史污染风险保留。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要；当前 production 尚无已采纳 ISS RVV family。 |

## 板卡复跑预算和决策桶

production repeated 默认 5-run；若出现单次离群，最多补 1 次完整 repeated，保留旧 run 为 historical evidence。采纳阈值：public case median speedup > 1.03x、无低于 1.0x 的高频退化、checksum 一致或输出差异经 correctness test 解释。

## 文档更新清单

若 production public board 为 positive，本阶段结束后更新 topic-local doc suite、Phase 010 result、optimization matrix、roadmap、Handoff，并创建 `doc-rvv/keypoints/iss_3d-RVV.zh.md`。若不是 positive，不创建长期 production doc；production patch 撤回后只把历史 probe 状态和不采纳理由写入 Handoff。
