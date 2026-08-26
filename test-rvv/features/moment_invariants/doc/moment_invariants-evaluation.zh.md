# moment_invariants function evaluation

## 当前 EvidenceDecision

`features/include/pcl/features/impl/moment_invariants.hpp` 当前结论为 adopted production behavior（已采纳生产行为）。真实 `MomentInvariantsEstimation::computeFeature` 在 RVV 构建、`PointOutT=pcl::MomentInvariants`、输入点型满足 `RVVXYZAoSFloatLayout<PointT>`、`cloud.is_dense`、indexed neighbor list（索引邻域列表）规模达到 16 且 u32 byte offset 可表达时，centroid（质心）之后的六个中心矩累加进入 RVV gather（离散加载）和 vector reduction（向量规约）路径。其它路径回退到标量 helper。

生产接入后的板卡 production-public（生产公开入口）证据为 weak_positive（弱正向）：`PointXYZ` median `1.067x`、`PointXYZI` median `1.071x`、`PointXYZRGB` median `1.078x`、`PointXYZRGBA` median `1.078x`，四组均 5/5 正向且 `B/A < 1 = 0/5`。这些数据来自接入后的真实 public overload（公开重载）bench，不使用 Phase 000/010 的诊断数据作为生产性能结论。

## 范围和目标源码

| 对象 | 范围 |
| --- | --- |
| production source | `features/include/pcl/features/impl/moment_invariants.hpp`，已新增有界 RVV production path。 |
| public entry | `MomentInvariantsEstimation::computeFeature`。 |
| target helper | indexed `computePointMomentInvariants(cloud, indices, ...)`；full-cloud overload 保持标量。 |
| adopted point types | `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`，均为 `Scalar=float`、dense AoS layout。 |
| test support | `test-rvv/features/moment_invariants/include/`、`src/`、`script/` 和 `log/evidence_registry.json`。 |
| 不覆盖 | `PointXYZRGBNormal`、`PointXYZINormal`、用户自定义点型、`Scalar=double`、其它输出类型、full-cloud overload、非 dense surface、Kdtree/search 优化。 |

## 标量路径

`computeFeature` 按 `indices_` 遍历输出点。dense 输入直接调用 `searchForNeighbors`；非 dense 输入先检查查询点 `isFinite`。邻域搜索失败时，`j1/j2/j3` 写成 NaN，并把 `output.is_dense` 置为 false。邻域搜索成功后，公开入口调用 indexed `computePointMomentInvariants(*surface_, nn_indices, j1, j2, j3)`。

indexed helper 先调用 `compute3DCentroid(cloud, indices, xyz_centroid_)`，再遍历邻域 indices。每个点减去 centroid 后累加 `mu200/mu020/mu002/mu110/mu101/mu011`，最后组合：

| 输出 | 标量公式 |
| --- | --- |
| `j1` | `mu200 + mu020 + mu002` |
| `j2` | `mu200*mu020 + mu200*mu002 + mu020*mu002 - mu110^2 - mu101^2 - mu011^2` |
| `j3` | `mu200*mu020*mu002 + 2*mu110*mu101*mu011 - mu002*mu110^2 - mu020*mu101^2 - mu200*mu011^2` |

## 标量流程与 RVV 流程对照

| 片段 | production 标量流程 | production RVV 流程 | 当前判断 |
| --- | --- | --- | --- |
| search | `computeFeature` 每个输出点调用 `searchForNeighbors`。 | 保持标量 / 既有 PCL search。 | 本 topic 不优化 KdTree。 |
| centroid | `compute3DCentroid`。 | 保持标量 centroid。 | 只替换 centroid 之后的累加。 |
| moment accumulation | 标量循环按 indices 取点并累加六个中心矩。 | `indexed_load3_f32m2` 按当前点型 offset gather xyz，并用 `vfredusum` 做六项规约。 | Phase 030/040 production-public 板卡结果支持保留。 |
| output / fallback | `computeFeature` 写 NaN、`output.is_dense` 和 `j1/j2/j3`。 | 输出写回仍在 public entry 的原流程里。 | public API 不变；fallback 回到标量 helper。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `MomentInvariantsEstimation::computeFeature` | production public entry | 邻域搜索、NaN fallback 和输出写回。 | PCL features public estimator。 | indexed `computePointMomentInvariants`。 | production boundary（生产边界）。 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| `momentInvariantsIndexedMomentsStd` | production Std helper | 复刻原 indexed 中心矩标量循环。 | indexed `computePointMomentInvariants` fallback。 | `momentInvariantsFinalize`。 | reference path（参考链路）。 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| `momentInvariantsIndexedMomentsRVV` | production RVV helper | traits-gated indexed gather 和六项 vector reduction。 | indexed `computePointMomentInvariants`。 | `momentInvariantsFinalize`。 | production direct RVV path（真实生产 RVV 路径）。 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| `momentInvariantsFullMomentsStd` | production Std helper | full-cloud overload 的标量循环。 | full-cloud `computePointMomentInvariants`。 | `momentInvariantsFinalize`。 | scalar-only boundary（仅标量边界）。 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| `computeMomentSummaryStd/RVV` | diagnostic reference / candidate | 测试专用 helper-only 诊断实现。 | gtest / historical bench。 | summary / checksum。 | diagnostic evidence（诊断证据）。 | `include/impl/moment_invariants_reductions.hpp` |
| `bench_moment_invariants.cpp` | bench wrapper | helper-only、production-shaped 和 production-public case-filter。 | Makefile bench targets。 | summary generator。 | board performance input（板卡性能输入）。 | `src/bench_moment_invariants.cpp` |
| `check_mi_production_asm.py` | asm script | 检查 production 符号内是否出现 RVV load/reduction。 | Makefile asm targets。 | reviewer / phase result。 | asm attribution（反汇编归属）。 | `script/check_mi_production_asm.py` |
| `generate_mi_repeated_summary.py` | analysis script | 生成 summary 和 manifest。 | Makefile record targets。 | Evidence Doctor。 | Evidence Doctor 输入。 | `script/generate_mi_repeated_summary.py` |
| Phase 030/040 summaries | evidence output summary | 接入后 production-public repeated board 结果。 | board repeated targets。 | 本文、`doc-rvv`、phase result。 | production performance evidence（生产性能证据）。 | `log/board/repeated_phase030_*`、`log/board/repeated_phase040_*` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | adopted | `computePointMomentInvariants(cloud, indices, ...)` 在 `__RVV10__` 下短路尝试 RVV，失败后调用 Std helper。 | full-cloud overload 保持标量。 |
| layout / traits gate | adopted | `RVVXYZAoSFloatLayout<PointT>` + `PointOutT=pcl::MomentInvariants`。 | normal 复合点型和自定义点型未做 board 扩展。 |
| dense / finite boundary | adopted fallback | RVV helper 要求 `cloud.is_dense`；非 dense public case 通过测试保持标量/NaN 行为。 | RVV path 当前不做 finite mask。 |
| reduction | adopted | gtest 容差覆盖 RVV vector reduction 改变累加树；asm 归属到 `vfred*sum`。 | raw checksum 不作为 strict equality gate（严格相等验收）。 |
| production scope | adopted narrow scope | Phase 030/040 production-public summaries。 | 不覆盖其它输出类型、`Scalar=double` 或 search 优化。 |

## 生产接入后的最终证据更新

| 证据 | 路径 | 结果 | 限制 |
| --- | --- | --- | --- |
| QEMU correctness | `make run_test_compare` | Std/RVV 各 8/8 pass。 | QEMU 不证明性能。 |
| board correctness | `make run_board_test` | RVV gtest 8/8 pass。 | 小型板卡 correctness，不是 repeated performance。 |
| `PointXYZ` production board | `log/board/repeated_phase030_production_compute_feature/summary.md` | median `1.067x`，min `1.023x`，max `1.085x`，0/5 below 1，bucket `weak_positive`。 | 只覆盖 `PointXYZ`。 |
| `PointXYZI` production board | `log/board/repeated_phase040_production_pointxyzi_compute_feature/summary.md` | median `1.071x`，min `1.065x`，max `1.075x`，0/5 below 1，bucket `weak_positive`。 | 只覆盖 `PointXYZI`。 |
| `PointXYZRGB` production board | `log/board/repeated_phase040_production_pointxyzrgb_compute_feature/summary.md` | median `1.078x`，min `1.066x`，max `1.091x`，0/5 below 1，bucket `weak_positive`。 | 只覆盖 `PointXYZRGB`。 |
| `PointXYZRGBA` production board | `log/board/repeated_phase040_production_pointxyzrgba_compute_feature/summary.md` | median `1.078x`，min `1.059x`，max `1.087x`，0/5 below 1，bucket `weak_positive`。 | 只覆盖 `PointXYZRGBA`。 |
| Evidence Doctor | 对应 `evidence_doctor.md` | Phase 030/040 production summaries 均无 Error / Warning；每组 2 个 metadata suggestion。 | 建议后续补 taskset/governor/freq/temperature 和 binary hash。 |
| Evidence registry | `log/evidence_registry.json` | Phase 030/040 summary、manifest、doctor 均登记为 fresh。 | raw logs 不默认提交。 |

## Fallback 矩阵

| 条件 | 行为 | 证据 |
| --- | --- | --- |
| 非 RVV 构建 | 只编译和调用 Std helper。 | `make run_test_compare` 的 Std 构建。 |
| `PointOutT != pcl::MomentInvariants` | RVV dispatch 不启用，走 Std helper。 | `if constexpr` gate 源码审计。 |
| layout 不满足 `RVVXYZAoSFloatLayout<PointT>` | `momentInvariantsIndexedMomentsRVV` 返回 false，走 Std helper。 | 源码 gate；未覆盖点型不声明性能。 |
| `cloud.is_dense == false` | RVV helper 返回 false。 | non dense public gtest 覆盖查询 NaN 和有效查询输出。 |
| 邻域规模小于 16 | RVV helper 返回 false。 | 源码 gate；小规模不作为性能主证据。 |
| `cloud.size()` 超出 u32 byte offset 可表达范围 | RVV helper返回 false。 | 源码 gate。 |
| full-cloud overload | 始终调用 `momentInvariantsFullMomentsStd`。 | 源码审计；full-cloud production 未采纳。 |

## 历史诊断证据链

Phase 000 helper-only diagnostic（只计测试 helper 的诊断）显示 `mi_accumulation_indexed,points=262144` 5-run median `1.141x`。Phase 010 production-shaped diagnostic（生产形态诊断）计入 KdTree search 外层后，`mi_public_search_shape,points=4096` median `1.031x`，并触发 near-threshold suggestion。这些结果解释了为什么必须补 Phase 030/040 production-public 证据；它们不再作为最终生产性能结论的主依据。

## doc-suite parity audit

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 独立 README 已刷新到 adopted production 状态。 | 入口、命令、证据白名单和 production doc 适用性。 | adopted | `README.zh.md`。 | none |
| testing overview | 独立 role 文档已覆盖 production targets。 | target 分类和覆盖矩阵。 | adopted | `doc/testing-overview.zh.md`。 | none |
| correctness tests | 独立 role 文档已覆盖 8 个 gtest。 | TEST 字典和断言边界。 | adopted | `doc/correctness-tests.zh.md`。 | none |
| benchmark and evidence | 独立 role 文档已覆盖 Phase 030/040 summary。 | case-filter、summary、doctor、registry。 | adopted | `doc/benchmark-and-evidence.zh.md`。 | none |
| optimization evidence | 独立 role 文档已覆盖 adopted/deferred candidate。 | candidate 状态和证据索引。 | adopted | `doc/optimization-evidence.zh.md`。 | none |
| test-support code map | 独立 role 文档已覆盖 production helper、typed asm 和 scripts。 | helper、bench、script 和 output 定位。 | adopted | `doc/test-support-code-map.zh.md`。 | none |
| phase suite | Phase 000-040 plan/result + matrix。 | 可恢复阶段循环。 | adopted | `doc/phases/`。 | none |
| production topic doc | 已创建长期文档。 | 只保存 adopted production behavior 和证据链。 | adopted | `doc-rvv/features/moment_invariants-RVV.zh.md`。 | none |

## 后续路径

默认建议暂停在 review / commit decision。Phase 040 后没有需要同轮继续推进的高优先级未阻塞优化动作：剩余方向会扩大到新点型族、full-cloud overload、其它输出类型、`Scalar=double` 或 search/KdTree，不应由当前 production patch 自动外推。

可选后续方向有三类。第一，evidence hardening（证据加固）：若准备长期对比或遇到长尾，可补 taskset、governor、freq、temperature 和 binary hash 后重跑四个 production case。第二，点型扩展：`PointXYZRGBNormal`、`PointXYZINormal` 或用户自定义点型需要新 phase 单独冻结范围并补 correctness、asm、board 和 Evidence Doctor。第三，full-cloud overload 或 KdTree/search 优化需要真实 caller/profile 证明热点后另开 topic。
