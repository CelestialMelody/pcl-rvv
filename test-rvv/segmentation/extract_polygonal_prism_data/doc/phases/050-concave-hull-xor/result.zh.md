# Phase 050 Result: concave-hull-xor

## 当前结论

本阶段完成 concave hull XOR（凹包多 polygon 异或）生产扩展。`segmentRvv` 不再因为 `polygons_.size() > 1` 直接回退，而是把 hull 投影点按 `pcl::Vertices` 分组成多个 active polygon，在每个 VL chunk（RVV 可变向量长度分块）内先计算单个 polygon 的 edge parity（边奇偶判定），再把多个 polygon 的 inside mask 做 XOR 合并。

EvidenceDecision（证据决策）为 `adopted_production_behavior`。本阶段的 production public（公开入口生产证据）在 Milkv-Jupiter 板卡上为 positive bucket：nested dense median 2.18x，min 2.11x，max 2.23x；nested indexed median 2.13x，min 2.11x，max 2.17x。两组 nested Evidence Doctor（证据体检）均为 Errors=0 / Warnings=0 / Suggestions=0。

## 计划动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED tests | done | `SegmentRvvMatchesSegmentStdForNestedPolygons` 和 indexed nested 测试在旧 fallback gate 下失败 | RED 能证明生产 RVV 多 polygon 覆盖缺失 |
| production patch | done | `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` | `segmentRvv` 支持合法 multi polygon XOR；退化 polygon 和非法 polygon 顶点索引 fallback |
| bench extension | done | `src/bench_eppd.cpp`、`Makefile` | `--polygons single|nested` 可隔离 production nested case；新增 nested dense / indexed board repeated target |
| correctness | done | `make run_test_compare` | Std 2 tests、RVV 13 tests pass；nested dense / indexed production direct 输出等于 `segmentStd` |
| QEMU smoke | done | `make run_bench_std/rvv ... --path production --polygons nested` 及 indexed 变体 | dense checksum `7795032464688786530` 一致；indexed checksum `5040983731874815810` 一致；QEMU timing 不用于性能结论 |
| board repeated | done | `make run_board_eppd_repeated_production_nested`、`make run_board_eppd_repeated_production_nested_indexed` | 两组 5-run 均为 positive bucket |
| Evidence Doctor / registry | done | nested summary / manifest / doctor；`log/evidence_registry.json` | doctor 0 / 0 / 0；registry 已记录 nested evidence |

## adopted production 范围

| 维度 | 状态 | 证据 | 边界 |
| --- | --- | --- | --- |
| public entry | adopted | `segment` 在 `__RVV10__` 下短路尝试 `segmentRvv`，否则 `segmentStd` | public API 不变 |
| dense ordered nested polygons | adopted | `log/board/repeated-production-nested/summary.md`：5 runs，median 2.18x，min 2.11x，max 2.23x | `indices_->size() >= 64`；合法 polygon vertices |
| source indexed nested polygons | adopted | `log/board/repeated-production-nested-indexed/summary.md`：5 runs，median 2.13x，min 2.11x，max 2.17x | indices 合法且 32-bit byte offset 可表达 |
| single polygon regression | adopted | `make run_test_compare` pass；Phase 045 single polygon production evidence 保持 current | single polygon 行为未回退 |
| degenerate / invalid polygon | fallback | `SegmentRvvDeclinesDegeneratePolygons` 和源码 gate | 标量路径保持原语义 |

## Evidence Doctor 和 registry

| evidence | role | doctor | registry 状态 |
| --- | --- | --- | --- |
| `log/board/repeated-production-nested/summary.md` | production-public nested dense | Errors=0 / Warnings=0 / Suggestions=0 | recorded |
| `log/board/repeated-production-nested-indexed/summary.md` | production-public nested indexed | Errors=0 / Warnings=0 / Suggestions=0 | recorded |

本阶段 summary evidence（摘要证据）仍按 summary-only 策略处理：默认不提交 raw run logs（原始运行日志）。如提交 evidence summary，应使用 `git add -f` 精确选择 summary / manifest / doctor。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | 本阶段最终证据为 `production-public`；test-only candidate 只保留为实现形状和 correctness 辅助 |
| A/B boundary | public overload：Std build 和 RVV build 都通过真实 `ExtractPolygonalPrismData<PointT>::segment` |
| 当前决策问题 | 合法 multi polygon XOR 走 RVV 是否快于当前 public scalar path，且 fallback 是否保持语义 |
| diagnostic 是否可外推到 production | 不外推；最终以 nested production public board summary 为准 |
| comparison-boundary / baseline mismatch 风险 | nested dense / indexed manifest 均标记 public overload、production_public、同一 timer boundary 和 checksum policy |
| weak / negative 时是否允许 bounded production probe | 本阶段结果为 positive；若后续同边界复跑转 weak / negative，应把本阶段 summary 降为 historical 并重新决策 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；当前决策不是替换已有 multi polygon RVV family，原状态是 scalar fallback |

## doc suite closeout

| area | current shape scan | quality bar | decision | evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已加入 nested board target 和 evidence whitelist | 入口导航应反映当前 adopted 范围 | adopted | `README.zh.md` | none |
| evaluation | 已刷新函数级结论、fallback 矩阵、证据链和后续方向 | 决策审计主归属 | adopted | `doc/extract_polygonal_prism_data-evaluation.zh.md` | none |
| production topic doc | 已从 single polygon 更新到 legal single / nested polygon | 长期文档只保存当前生产行为和证据链 | adopted | `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md` | none |
| phase index / matrix / roadmap | 已加入 Phase 050 result 和 adopted matrix 行 | phase loop 需可恢复 | adopted | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` | 默认下一 phase：`060-point-type-expansion` |
| artifact tracking | topic docs 和长期文档均在当前 topic commit boundary；raw logs ignored-local | 提交前需要路径限定扫描 | adopted for closeout | `git status --short --untracked-files=all -- ...` | commit phase 再决定 staging |

## 继续 / 停止判断

本阶段完成并采用；没有需要回滚的生产改动。当前仍有一个授权范围内的可继续优化方向：point type expansion（点类型扩展）。它不改变 public API，也不需要重写算法，但会扩大 production coverage（生产覆盖范围）到代表性 PointXYZ-like AoS float 点型，必须用独立 correctness、asm、board 和 Evidence Doctor 证明，不能从 `PointXYZ` 直接外推。

`next_phase_default = 060-point-type-expansion`。size threshold tuning（规模阈值调优）暂缓到点类型扩展之后，因为阈值收益会受点类型 stride、polygon 数和 VLEN 影响。
