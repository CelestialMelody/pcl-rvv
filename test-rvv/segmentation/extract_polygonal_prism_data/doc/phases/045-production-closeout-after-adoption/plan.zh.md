# Phase 045 Plan: production-closeout-after-adoption

## 阶段意图和边界

本阶段处理 PI5 后的 S11 production closeout（生产收口）：用户已确认“板卡上的测试结果如果显示有收益即可采纳”，而 Phase 040 production public（公开入口生产证据）dense / indexed repeated board（重复板卡性能测试）均为 positive bucket，因此本阶段把当前 production patch（生产补丁）记录为 adopted production behavior（已采用生产行为）。

本阶段不扩大 production 覆盖范围，不修改 `segmentRvv` 的算法边界，不新增 concave hull 多 polygon XOR 或更多点类型性能接入。后续优化方向只写入 roadmap 和 matrix，若继续推进必须另开 phase。

## 当前状态清单

| area | 当前状态 | 证据 |
| --- | --- | --- |
| production patch | public `segment` 已分流到 `segmentRvv` / `segmentStd` | `segmentation/include/pcl/segmentation/extract_polygonal_prism_data.h`、`segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` |
| correctness | Std 2 tests、RVV 11 tests；板卡 11 tests | `make run_test_compare`、`make run_board_test` |
| production board evidence | dense median 1.75x；indexed median 1.75x | `log/board/repeated-production/summary.md`、`log/board/repeated-production-indexed/summary.md` |
| Evidence Doctor（证据体检） | production dense / indexed 均为 Errors=0 / Warnings=0 / Suggestions=0 | `log/board/repeated-production/evidence_doctor.md`、`log/board/repeated-production-indexed/evidence_doctor.md` |
| asm attribution（反汇编归属） | production `segmentRvv` 符号内出现 RVV 指令 | `build/asm/riscv/bench_eppd_rvv.full.asm` |

## 实现和文档动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 创建正式 production 长期主题文档 | `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md` | 覆盖函数语义、当前采用方式、范围决策、fallback、Traceability Map、数值算例、证据链和后续条件 |
| 刷新 topic-local 文档 | README、evaluation、phase README、matrix、roadmap、Phase 040 result | 不再把当前补丁写成待确认；保留 PI5 曾经停下的审计事实 |
| 刷新筛选队列 | `doc-rvv/library-screening/segmentation/segmentation-function-evaluation-queue.zh.md` | topic 状态从 PI5 待确认改为已采纳 |
| 刷新 Handoff Packet | `tmp/rvv-work-logs/.../current-handoff/` | 下一恢复入口指向后续优化判断或下一 phase |

## Evidence Doctor 和 registry 规则

本阶段不生成新的性能日志，使用 Phase 040 已登记的 production public summary / manifest / doctor。收口后运行 `evidence_registry.py check --require-doc-ref`，要求新增 `doc-rvv` 与 topic-local 文档能引用当前生产证据；若出现 `unregistered_change` 或 `doc_ref_missing`，先修复登记或文档引用。

## 完成条件

- `doc-rvv` 正式主题文档存在，并以 production direct（真实生产路径证据）为中心。
- README、evaluation、phase index、optimization matrix、roadmap 和筛选队列表一致写成 adopted production behavior。
- freshness check、correctness、asm 和 `git diff --check` 通过或有明确降级理由。
- 后续优化方向按 `phase_deferred + unblocked` 或 `turn_stop_deferred with stop_condition_hit` 写清。若仍有当前授权内可执行方向，下一 phase 继续推进。

## 继续 / 停止条件

完成本阶段后，默认继续判断 roadmap 中的未阻塞方向。当前候选包括：

- concave hull 多 polygon XOR：可能扩大真实 `test_concave_prism` 类输入覆盖，但需要生产补丁、fallback、bench、asm、board 和 Evidence Doctor。
- point type expansion（点类型扩展）：当前 traits gate 理论覆盖更多 xyz AoS float 点型，但 production direct 性能只跑 `PointXYZ`；需要补代表性点型测试和板卡证据。
- size threshold tuning（规模阈值调优）：当前阈值为 64；需要板卡 sweep 证明是否值得改。

如果这些方向会扩大到其它 topic、缺板卡、证据矛盾或 dirty isolation 不安全，才允许停止。
