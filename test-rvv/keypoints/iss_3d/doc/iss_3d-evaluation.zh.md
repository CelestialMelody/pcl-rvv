# ISS 3D RVV Function Evaluation

## 范围和目标源码

目标源码是 `keypoints/include/pcl/keypoints/impl/iss_3d.hpp`。本 topic 来自 `doc-rvv/library-screening/keypoints/keypoints-retained-candidate-rescreen.zh.md` 的 retained candidate（保留候选）清单，关注 `ISSKeypoint3D::getScatterMatrix` 中按邻域索引累加 3x3 scatter matrix（散布矩阵）的局部热点。

## 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `detectKeypoints` | ISS 3D 主流程，计算边界、散布矩阵、特征值、非极大值抑制和输出点。 | 读 `input_`、search tree、半径和阈值；写 `third_eigen_value_`、`keypoints_indices_` 和 output。 | public `compute()` 调用的核心实现。 | 当前不整体向量化，search / EVD / NMS 成本会稀释局部收益。 |
| `getScatterMatrix` | 对一个中心点搜索 salient radius 邻域，累加 3x3 double scatter matrix。 | 读 `input_` 和邻域索引；输出 `Eigen::Matrix3d`。 | 每个候选点调用一次，结果供 Eigen EVD 使用。 | 已做 bounded production probe，但 public 边界收益不足。 |
| `searchForNeighbors` | 根据半径返回邻域索引和距离。 | 由 PCL search tree 实现。 | scatter 前置成本。 | 不在本 topic 范围内。 |
| Eigen EVD / ratio 判断 | 求 3 个特征值并更新 saliency ratio。 | 输入 scatter matrix；输出 `prg_mem` 和 `third_eigen_value_`。 | scatter 后续消费者。 | 每点一次小矩阵求解，当前保留标量。 |
| NMS / output push | 在 non-max radius 内筛选局部极大并写输出。 | 读 `third_eigen_value_` 和邻域；写 output 和 indices。 | 主流程后段。 | 与 scatter 无同构数据流，本 topic 不接管。 |

## 函数级结论

诊断边界结论是 positive：f64 RVV scatter helper 通过 QEMU correctness（正确性）、反汇编归属和 repeated board（重复板卡测试），局部收益明显。

production 边界结论是 `removed_no_production`：真实 `compute()` 边界下，历史 production probe 只把 scatter 累加改为 RVV，公开入口还包含邻域搜索、Eigen EVD、NMS 和输出构造。微调后 production public median speedup 为 1.014x，Evidence Doctor 无 Error / Warning，但 manifest decision bucket 为 `neutral`，收益接近阈值且 checksum 为 0 的 synthetic case 没有输出关键点，不能证明值得长期接入。当前 production 源码保持原标量路径。

## 标量流程与 RVV 流程对照

| 阶段 | 标量路径 | RVV 路径 | 证据边界 |
| --- | --- | --- | --- |
| 邻域搜索 | `getScatterMatrix` 调用 `searchForNeighbors` 得到 `nn_indices`。 | 不改变。 | production bench 包含该成本；诊断 bench 不包含。 |
| scatter 累加 | 对每个邻域点读取 `x/y/z`，用 double 累加 9 个矩阵槽位。 | RVV 以 `vluxei32` indexed gather（索引离散加载）读 float xyz，widen 到 double，累加 6 个对称槽位并用 vector reduction 归约。 | QEMU correctness 通过；asm gate 命中 indexed load 和 reduction。 |
| 小邻域 / 非覆盖布局 | 原标量循环。 | `neighbor_count < 16`、点型不满足 xyz AoS traits、32-bit byte offset 不可表达时 fallback（回退路径）。 | fallback 由源码 gate 和 gtest 部分覆盖；非 `PointXYZ` production direct 未覆盖。 |
| EVD / NMS / output | 标量 Eigen 和后处理。 | 不改变。 | public bench 弱正向说明 scatter 局部收益被 pipeline 稀释。 |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| diagnostic f64 scatter | `attempted_positive` | `iss_3d_phase000_scatter_f64_rerun1`：indexed 256 median 1.662x，tail 73 1.213x，contiguous 256 1.859x；Errors=0。 | 只证明 test helper / component ablation，不证明 public compute。 |
| production dispatch / fallback | `removed_no_production` | probe 期间 `run_test_compare` 和 production asm 通过，但 production public bucket 为 neutral。 | 当前提交不包含 production dispatch，公开入口继续走原标量路径。 |
| reduction shape | `historical_attempted` | probe 期间 shape gate 先 RED 失败 saw 10，微调后通过 saw 7。 | 结果正确性保持；production public 仍为 neutral，因此随 production probe 移除。 |
| generic point type gate | `partial` | 使用 `pcl::rvv::kRVVXYZAoSPointCompatible<PointInT>` 和 `rvvMaxU32ByteOffsetElements<PointInT>()`。 | 只测 `pcl::PointXYZ`；其它 xyz AoS 点型和 fallback compile matrix 未闭合。 |
| broader pipeline | `rejected_for_this_topic` | production public 弱正向，search / EVD / NMS 稀释局部收益。 | 若要继续，需要新的 profile 或专门 phase，不应把 scatter patch 直接采纳。 |

## 测试计划与结果

| 测试 / target | 层级 | 作用 | 结果 |
| --- | --- | --- | --- |
| `run_test_compare` | QEMU correctness | Std/RVV 两个 binary 跑 4 个 gtest，覆盖 diagnostic candidate、tail 和 protected production `getScatterMatrix` probe。 | 通过。 |
| `check_iss_3d_rvv_asm` | asm attribution（反汇编归属） | 证明诊断 bench 中有 indexed xyz load 和 vector reduction。 | 通过。 |
| historical production asm gate | production asm attribution | probe 期间证明 production helper 符号内有 RVV gather / reduction。 | 通过后因 public evidence neutral 移除，不作为当前 target。 |
| historical production reduction shape gate | implementation-shape gate | probe 期间防止 production 对称矩阵填充重复规约非对角槽位。 | RED 失败后通过；随 probe 移除。 |
| `board_repeated` | diagnostic board | 采集 scatter helper 局部性能。 | rerun1 positive。 |
| `board_repeated_production` | production public board | 采集真实 `compute()` 入口性能。 | micro-opt 后 neutral。 |

## 当前验证结果

| evidence role | run label | decision | 说明 |
| --- | --- | --- | --- |
| diagnostic | `iss_3d_phase000_scatter_f64_repeated` | historical unstable | indexed 256 有一个离群值，旧结果仅作历史污染风险。 |
| diagnostic | `iss_3d_phase000_scatter_f64_rerun1` | positive | 局部 helper 收益稳定，支撑进入 bounded production probe。 |
| production_public | `iss_3d_phase010_production_public_repeated` | historical neutral | median 1.005x，2/5 退化，Evidence Doctor 有 Error。 |
| production_public | `iss_3d_phase010_production_public_reduction_shape` | current neutral | median 1.014x，0/5 退化，Errors=0 / Warnings=0 / Suggestions=3；仍低于采纳线。 |

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 diagnostic / component ablation；Phase 010 是 production_public。 |
| A/B boundary | Phase 000 比较 test helper；Phase 010 比较 public `compute()`。 |
| 当前决策问题 | 当前 public RVV path 是否值得替换当前 public scalar path。 |
| diagnostic 是否可外推到 production | 不能直接外推。局部 scatter 不包含 search、EVD、NMS 和 output。 |
| comparison-boundary / baseline mismatch 风险 | 存在。局部 helper 的 1.6x 以上收益在 public compute 中被稀释到约 1.014x。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | Phase 000 rerun1 positive，所以允许 Phase 010 probe。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前没有已采纳 ISS 3D RVV family，不需要；但 public Std/RVV 必须足够正向。 |

## 生产接入判断

当前不采纳 production patch。理由是 production direct（真实生产路径证据）没有达到“值得接入”的证据强度：微调后全 5 轮正向但 median 仅 1.014x，manifest decision bucket 为 `neutral`，Evidence Doctor 提示 near-threshold 和环境 / binary identity metadata 缺口。QEMU 和 asm 证明的是正确性与路径命中，不是目标硬件收益。

当前 production 源码已回到原标量路径，正式 production 长期文档 `doc-rvv/keypoints/iss_3d-RVV.zh.md` 判定为 `not_applicable with evidence`。历史 production probe 证据保留在 topic-local phase/evaluation 文档中，用于说明为什么本 topic 不进入 production。

## Doc Suite Role Inventory

| role | status | path / evidence |
| --- | --- | --- |
| topic_navigation | standalone | `test-rvv/keypoints/iss_3d/README.zh.md` |
| testing_overview | standalone | `test-rvv/keypoints/iss_3d/doc/testing-overview.zh.md` |
| correctness_tests | standalone | `test-rvv/keypoints/iss_3d/doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `test-rvv/keypoints/iss_3d/doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `test-rvv/keypoints/iss_3d/doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone | `test-rvv/keypoints/iss_3d/doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone | `test-rvv/keypoints/iss_3d/doc/test-support-code-map.zh.md` |
| phase_index / matrix | standalone | `test-rvv/keypoints/iss_3d/doc/phases/README.zh.md` and `optimization-matrix.zh.md` |
| evaluation | standalone | `test-rvv/keypoints/iss_3d/doc/iss_3d-evaluation.zh.md` |
| production_topic_doc | not_applicable with evidence | production evidence remains neutral, so no adopted production behavior exists. |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ISSKeypoint3D::detectKeypoints` | production public entry | ISS 3D 主流程。 | `compute()` | `getScatterMatrix`、Eigen EVD、NMS | production boundary | `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` |
| `ISSKeypoint3D::getScatterMatrix` | production dispatch / fallback | 搜索邻域并计算 scatter matrix。 | `detectKeypoints` | RVV helper 或标量 loop | production probe | `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` |
| `pcl::detail::computeISSScatterMatrixRVV` | production RVV helper | traits gate 后执行 indexed gather 和 f64 reduction。 | `getScatterMatrix` | `Eigen::Matrix3d` | production direct candidate | `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` |
| `computeScatterMatrixStd` | diagnostic reference | 复刻 scatter 标量公式。 | gtest / bench | matrix checksum | correctness gate | `test-rvv/keypoints/iss_3d/include/impl/iss_3d_scatter.hpp` |
| `computeScatterMatrixCandidate` | candidate wrapper | RVV 或标量 fallback。 | gtest / bench | diagnostic helper | diagnostic evidence | `test-rvv/keypoints/iss_3d/include/impl/iss_3d_scatter.hpp` |
| `src/test_iss_3d.cpp` | correctness tests | Std/RVV 对拍和 protected production probe。 | Makefile | QEMU logs | correctness gate | `test-rvv/keypoints/iss_3d/src/test_iss_3d.cpp` |
| `src/bench_iss_3d.cpp` | bench wrapper | diagnostic 和 public bench CLI。 | Makefile / board.mk | summary script | board performance | `test-rvv/keypoints/iss_3d/src/bench_iss_3d.cpp` |
| `generate_iss_3d_evidence_manifest.py` | analysis script | 生成 summary / manifest，供 Evidence Doctor 和 registry 使用。 | Make targets | evidence outputs | evidence registry input | `test-rvv/keypoints/iss_3d/script/generate_iss_3d_evidence_manifest.py` |
| `repeated_phase010_production_public_reduction_shape/summary.md` | evidence output summary | 当前 production public 性能摘要。 | board repeated target | evaluation / phase result | production_public performance | `test-rvv/keypoints/iss_3d/log/board/repeated_phase010_production_public_reduction_shape/summary.md` |

## 剩余风险与恢复条件

- 如果用户决定重新引入弱正向 patch，需要先接受 near-threshold 风险，并最好补充环境 metadata、binary hash 和至少一个有非零 keypoint 输出的 public case。
- 当前提交路径已经移除 `iss_3d.hpp` 中的 `__RVV10__` production helper 和 dispatch，保留 test-rvv 诊断证据作为 retained candidate closeout。
- 如果继续优化，优先不要扩全 pipeline；先做 profile 或更有代表性的 public input，确认 scatter 是否仍是可影响端到端的成本。
