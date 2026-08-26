# Phase 045 Result: production-closeout-after-adoption

## 当前结论

本阶段完成 S11 production closeout（生产收口）。用户已确认“板卡上的测试结果如果显示有收益即可采纳”；Phase 040 的 production public（公开入口生产证据）dense / indexed repeated board（重复板卡性能测试）均为 positive bucket，因此当前 production patch（生产补丁）记录为 adopted production behavior（已采用生产行为）。

正式长期主题文档已创建：`doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md`。文档以真实 `ExtractPolygonalPrismData<PointT>::segment(PointIndices&)` production dispatch（生产分流）、fallback（回退路径）、production direct tests（真实生产入口直连测试）、反汇编和接入后的板卡证据为中心。

## 计划动作回填

| action | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| 创建正式 production 长期主题文档 | done | `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md` | 已覆盖当前状态、函数语义、当前采用方式、fallback 矩阵、范围决策、Traceability Map、数值算例和证据链 |
| 刷新 topic-local 文档 | done | `README.zh.md`、`doc/extract_polygonal_prism_data-evaluation.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` | 状态从 PI5 待确认刷新为 adopted production behavior |
| 刷新筛选队列 | done | `doc-rvv/library-screening/segmentation/segmentation-function-evaluation-queue.zh.md` | 第二主题状态改为已采纳 |
| 刷新 Handoff Packet | done | `tmp/rvv-work-logs/segmentation/extract_polygonal_prism_data/current-handoff/` | 下一恢复动作改为后续优化方向判断 |
| evidence freshness check | done | `python3 ../../script/evidence_registry.py check ... --require-doc-ref --fail-on any` | registry fresh；新增 `doc-rvv` 已引用 production evidence |
| correctness / asm / diff check | done | `make run_test_compare`、`make dump_bench_rvv`、`git diff --check -- ...` | correctness、反汇编归属和 diff whitespace 通过 |

## adopted production 范围

| 维度 | 状态 | 证据 | 边界 |
| --- | --- | --- | --- |
| public entry | adopted | `segment` 在 `__RVV10__` 下短路尝试 `segmentRvv`，否则 `segmentStd` | public API 不变 |
| dense ordered indices | adopted | `log/board/repeated-production/summary.md`：5 runs，median 1.75x，min 1.74x，max 1.75x | `indices_->size() >= 64` |
| source indexed points | adopted | `log/board/repeated-production-indexed/summary.md`：5 runs，median 1.75x，min 1.72x，max 1.75x | indices 合法且 32-bit byte offset 可表达 |
| point type / layout | adopted within gate | `RVVXYZAoSFloatLayout<PointT>`；`PointXYZ` / `PointXYZI` correctness | 更多点型性能仍需扩展 phase |
| polygon | adopted for single polygon | single polygon production public bench | multi polygon concave hull XOR 仍 scalar fallback |

## Evidence Doctor 和 registry

| evidence | role | doctor | registry 状态 |
| --- | --- | --- | --- |
| `log/board/repeated-production/summary.md` | production-public dense | Errors=0 / Warnings=0 / Suggestions=0 | recorded and fresh |
| `log/board/repeated-production-indexed/summary.md` | production-public indexed | Errors=0 / Warnings=0 / Suggestions=0 | recorded and fresh |

本阶段没有生成新的 raw logs（原始日志）。summary evidence（摘要证据）仍按 summary-only 策略处理：默认不提交 raw run 目录，若提交 evidence summary，应使用 `git add -f` 精确选择 summary / manifest / doctor。

## doc suite closeout

| area | current shape scan | quality bar | decision | evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已列当前结论、阅读路径、命令和证据白名单 | 入口导航清楚 | adopted | `README.zh.md` | none |
| evaluation | 已记录函数语义、Traceability Map、fallback 和证据链 | 决策审计主归属 | adopted | `doc/extract_polygonal_prism_data-evaluation.zh.md` | none |
| benchmark and evidence | README / evaluation / formal doc 引用 summary / manifest / doctor | 性能只来自板卡 repeated summary | adopted | production summary 和 doctor | none |
| optimization evidence | matrix + roadmap + formal doc 当前采用方式 | 候选状态可恢复 | adopted | `doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` | 后续 phase 继续维护 |
| test-support code map | evaluation 和 formal doc Traceability Map 覆盖 production、test、bench、script | 复杂 topic 需要可追踪性 | adopted | 两处 Traceability Map | none |
| production topic doc | 正式 `doc-rvv` 已存在 | 只保存 adopted production 行为与证据链 | adopted | `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md` | 后续扩大 production 后刷新 |
| artifact tracking | topic docs / formal doc 均在当前 topic commit boundary；raw logs ignored-local | 未跟踪文件需在提交前显式选择 | adopted for closeout | `git status --short --untracked-files=all -- ...` | commit phase 再决定 staging |

## 继续 / 停止判断

本阶段 closeout 完成，不再阻塞生产采纳。当前仍有授权范围内可继续的优化方向，但它们都扩大当前 adopted scope，需要新 phase 和同边界证据：

| candidate | 状态 | 理由 | 默认下一步 |
| --- | --- | --- | --- |
| concave hull 多 polygon XOR | phase_deferred + unblocked | 当前 production 仍 fallback；`test_concave_prism` 类语义真实存在，RVV 可沿用 single polygon edge-parity mask 结构扩展 | 开 `050-concave-hull-xor`，先做 production direct correctness，再做 board bench |
| point type expansion（点类型扩展） | phase_deferred + unblocked | 当前 traits gate 理论覆盖更多 xyz AoS float 点型，但性能证据不能从 `PointXYZ` 外推 | 开 `060-point-type-expansion`，补代表性点型 correctness / asm / board |
| size threshold tuning（规模阈值调优） | deferred | 阈值 64 保守可用；收益可能依赖 VLEN 和 polygon 边数 | 在前两项后按需要做 board sweep |

默认下一 phase 选择 `050-concave-hull-xor`，因为它能扩大当前函数真实 concave hull 输入的 RVV 覆盖；若 board 显示 neutral 或 negative，则保持多 polygon 标量 fallback。
