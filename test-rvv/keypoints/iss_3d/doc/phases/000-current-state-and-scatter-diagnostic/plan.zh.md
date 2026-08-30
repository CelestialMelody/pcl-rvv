# Phase 000 Plan: Current State And Scatter Diagnostic

## 阶段意图和边界

本阶段证明 `ISSKeypoint3D::getScatterMatrix` 的 scatter matrix（散布矩阵）邻域累加局部核是否值得继续进入 production integration loop（生产接入闭环）。阶段入口是 test-only
`computeScatterMatrixCandidate`，row source（行来源）是 indexed neighbor list（索引邻域列表），点型是 `pcl::PointXYZ`，`Scalar` 语义是生产一致的 double accumulator（双精度累加器），layout（布局）是 AoS（结构数组）中 `x/y/z` float 字段。

本阶段不证明 `searchForNeighbors`、BoundaryEstimation、Eigen EVD（特征值分解）、NMS（非极大值抑制）、完整 `detectKeypoints` 公开入口或泛型点类型已经可采纳。

## 当前状态清单

| item | 状态 |
| --- | --- |
| scaffold | `test-rvv/keypoints/iss_3d` 已有 Makefile、board.mk、聚合头、RVV helper、gtest 和 bench。 |
| inherited edit note | 中断前已把 RVV helper 从 f32 accumulation 改为 f64 accumulation；本 worker 继续前补齐本 plan，后续任何修复按本 plan 执行。 |
| previous QEMU correctness | f32 版本曾通过 `run_test_compare`，但后续 f64 改动尚未验证。 |
| previous asm | f32 版本曾通过 `check_iss_3d_rvv_asm`，但后续 f64 改动尚未验证。 |
| previous board smoke | f32 版本板卡 smoke 为正向，但 checksum 不一致，只能作为 historical evidence（历史证据）和数值风险，不作为当前决策。 |
| production state | 尚未修改 `keypoints/include/pcl/keypoints/impl/iss_3d.hpp`。 |

## 假设与候选族

| hypothesis | expected signal | risk |
| --- | --- | --- |
| f64 RVV chunk 内并行累加 6 个 covariance term（协方差项），最后 vector reduction（向量规约）回 double。 | QEMU correctness 与标量接近，asm 有 `vluxei32.v` 和 `vfred*.vs`，板卡 repeated 仍为 positive。 | f64 吞吐和 gather 成本可能抵消收益；规约顺序和 FMA contraction（融合乘加收缩）会带来小误差。 |
| 小邻域继续走标量 fallback（回退路径）。 | tail case correctness 通过，bench 中 `scatter_indexed_tail_73` 不退化。 | gate 过高或过低会影响生产收益，需要 production probe 时再确认。 |

## 优化矩阵

见 `../optimization-matrix.zh.md`。本阶段只关闭 `f64 vector reduction scatter` 这一行；production integration 和 broader pipeline 保持 deferred（暂缓）。

## 实现和测试动作

| action | artifact / command | completion |
| --- | --- | --- |
| A1: 编译并验证 f64 candidate correctness（正确性）。 | `make -C test-rvv/keypoints/iss_3d run_test_compare` | Std/RVV gtest 都通过；若失败，先修复 helper 或 test。 |
| A2: 反汇编归属。 | `make -C test-rvv/keypoints/iss_3d check_iss_3d_rvv_asm` | RVV bench 符号包含 indexed xyz load 和 vector reduction。 |
| A3: 板卡 smoke。 | `SSH_AUTH_SOCK=<agent> make -C test-rvv/keypoints/iss_3d run_board_bench_compare fetch_board_logs` | 板卡可达；Std/RVV checksum 匹配或解释误差；性能方向不为 negative。 |
| A4: repeated board 和 Evidence Doctor。 | 若现有 target 缺失，补 topic-local summary / manifest / doctor / registry target；运行 bounded 5-run repeated。 | decision bucket（决策桶）稳定；Errors 为 0；Warnings 有解释。 |
| A5: 文档和 Handoff。 | topic-local README、evaluation、testing docs、phase result、roadmap、matrix、current Handoff。 | 文档区分 diagnostic evidence 与 production evidence，列出未覆盖范围。 |
| A6: 生产接入前置判断。 | 若 A1-A4 positive，创建下一阶段 production integration plan。 | 不在 Phase 000 直接把 diagnostic 写成 adopted production。 |

## Evidence Doctor 和 Registry 规则

本阶段优先使用 `test-rvv/script/evidence_doctor.py` 和 `test-rvv/script/evidence_registry.py` 的通用入口；若 ISS 3D 尚缺 topic-local manifest 生成器，A4 先补一个只输出 summary metadata（摘要元数据）的本地脚本或 Make target。没有 registry 前，所有新 bench log 都标为 `evidence_registry_status=not_available/manual_check`，不能写成 fresh。

## 阶段完成条件

- `adopted` for diagnostic candidate：QEMU correctness 通过、asm 归属通过、板卡 repeated 为 positive，Evidence Doctor 无未解释 Error。
- `attempted`：能编译和运行，但板卡为 weak / neutral / negative，或 Warnings 限制 production 外推。
- `blocked`：工具链、板卡或 Evidence Doctor Error 无法在本轮修复。
- `deferred`：production integration、泛型点型、完整 pipeline 和 contiguous fast path 都只在本阶段结束后按 roadmap 继续。

## 板卡复跑预算和决策桶

本阶段默认 5-run repeated，最多只在命令失败或单次污染可解释时补 1 次完整 repeated。按每个 case 的中位 speedup 归桶：

| bucket | 判断 |
| --- | --- |
| positive | 所有主 case speedup > 1.10x，且 checksum / correctness 无 Error。 |
| weak-positive | 主 case 大多 > 1.03x，但有尾部或连续 case 接近 1.0x。 |
| neutral | 主 case 在 0.97x-1.03x。 |
| negative | 任一主 case 稳定 < 0.97x，且无可解释测量污染。 |
| unstable | 预算耗尽后方向摇摆或长尾主导。 |

## 继续 / 停止条件

默认继续到 A1-A5。若 diagnostic repeated 为 positive，本轮继续进入 `010-production-integration`，因为用户已明确允许 production direct 板卡结果正向时采纳。只有板卡不可达、工具链失败、Evidence Doctor Error 无法修复、dirty isolation 不安全，或 repeated bucket 为 neutral / negative / unstable 且没有更值得尝试的本 topic 候选时，才停止并输出 Handoff。

## 文档更新清单

Phase 000 完成后更新：

- `result.zh.md`
- `optimization-matrix.zh.md`
- `../optimization-roadmap.zh.md`
- `../../iss_3d-evaluation.zh.md`
- topic-local README / testing / correctness / benchmark / optimization / code-map 文档
- `tmp/rvv-work-logs/keypoints/iss_3d/current-handoff/current-handoff.zh.md`

production 长期文档 `doc-rvv/keypoints/iss_3d-RVV.zh.md` 只在 production direct 证据正向并采纳后创建。

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 diagnostic / component ablation。 |
| A/B boundary | test helper：`computeScatterMatrixStd` vs `computeScatterMatrixCandidate`。 |
| 当前决策问题 | RVV-vs-scalar 局部核是否值得进入 bounded production probe。 |
| diagnostic 是否可外推到 production | 只能部分外推。公式和邻域索引读取来自 `getScatterMatrix`，但不包含 search、EVD、NMS、线程调度和公开入口成本。 |
| comparison-boundary / baseline mismatch 风险 | 有。test helper 的计时边界比 production 更窄，positive 只能支持进入 production probe。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 checksum 正确且某个真实邻域规模 case 为 weak-positive，可允许窄 production probe；若 repeated 为 neutral / negative 且没有 profile 支持，不建议继续接 production。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前 production 没有已 adopted RVV family；若进入生产，只需 production public Std/RVV 和 production detail 归属证据证明新 RVV path 快于当前标量 path。 |

## Phase Scope 与扩展队列

| field | value |
| --- | --- |
| validated_scope | test-only indexed neighbor scatter, `pcl::PointXYZ`, float xyz loads, double scatter accumulation, AoS layout, neighbor counts 73 / 256。 |
| unvalidated_scope | 泛型 `PointInT`、非 `PointXYZ` 布局、完整 `detectKeypoints`、其它 row source、其它邻域分布、其它 `Scalar`、线程调度。 |
| point_type_expansion_queue | production 阶段若只 gate 到 `PointXYZ` 或 `has_xyz` traits，必须补非覆盖点型 fallback test 和后续 traits 扩展计划。 |
| phase_closeout_boundary | 只能关闭 f64 scatter component diagnostic；不能关闭完整 ISS 3D topic。 |
