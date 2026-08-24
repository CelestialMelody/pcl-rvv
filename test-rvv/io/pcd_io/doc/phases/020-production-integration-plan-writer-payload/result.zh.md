# Phase 020 结果：writer payload 生产接入计划

本阶段完成 PI1 production integration plan（生产接入计划）。本阶段没有修改 production（生产源码），也没有
把 Phase 010 的 production-shaped diagnostic（生产形态诊断）升级成 production-ready（可接入生产）。

## 阶段范围回填

validated_scope：

- production entry（生产入口）：`PCDWriter::writeBinaryCompressed(std::ostream&, const PCLPointCloud2&, ...)`。
- candidate family（候选实现族）：4 字节字段 AoS（结构数组）到 field-major（字段连续）pack 使用 RVV stride load/store（跨步加载 / 连续存储），后续 LZF compression（LZF 压缩）保持现有实现。
- dispatch shape（分流形态）：public entry 保持“语义检查 -> RVV 短路 -> Std fallback（标量回退）”；原标量主体应抽成 `Std` / internal fallback helper 或等价局部 helper。
- evidence role（证据角色）：PI1 planning evidence（计划证据），输入证据仍是 Phase 000 / 010 diagnostic。

unvalidated_scope：

- production direct（真实生产路径证据）、fallback tests（回退测试）、production asm attribution（生产反汇编归属）和 production board repeated summary（生产板卡重复摘要）。
- file-name overload、mmap / file lock / `msync`、reader unpack、finite scan、templated writer、ASCII writer。
- non-4-byte fields、混合 field size、泛化到所有 PCD layout。

## 计划动作结果

| action | status | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| A1 写 PI1 plan | done | `plan.zh.md` | 候选范围、fallback 矩阵、production direct test、asm、board 和暂停条件已冻结。 |
| A2 回填 PI1 result | done | 本文件 | 当前停在生产补丁授权边界。 |
| A3 同步 topic-local docs | done | README、evaluation、roadmap、optimization matrix、phase index | 默认恢复入口从 PI1 更新到 PI2 authorization checkpoint。 |
| A4 验证文档与证据状态 | done | 见本文“验证” | 文档 whitespace、Phase 000 / 010 registry freshness 已检查。 |

## PI1 决策表

| decision area | result |
| --- | --- |
| production scope | 只允许 `PCDWriter::writeBinaryCompressed(std::ostream&, ...)` 的 4 字节字段 pack probe。 |
| helper shape | 推荐邻近 production-local helper：Std fallback helper 保留现有语义，RVV helper 只在 `__RVV10__` 下编译。 |
| public API | 不修改公开 API。 |
| field gate | 有效字段 size 必须等于 4，offset 和 `cloud.point_step` 必须 4 字节对齐。 |
| fallback | 所有不满足 gate 的路径走现有标量主体。 |
| production direct evidence | PI2 后必须新增真实 public ostream 入口测试；不能用 test-only payload helper 代替。 |
| board evidence | PI4 必须重跑 production public Std/RVV repeated summary、Evidence Doctor 和 registry。 |
| production topic doc | 仍 not_applicable；只有 PI5 通过且用户确认采纳后才创建 `doc-rvv/io/pcd_io-RVV.zh.md`。 |

## Fallback 矩阵回填

| fallback condition | PI1 status | PI2 / PI3 requirement |
| --- | --- | --- |
| non-RVV build 或未定义 `__RVV10__` | planned | Std build 必须编译并走 Std helper。 |
| `cloud.fields.empty()` | planned | 保持现有 `-1` 错误。 |
| `cloud.data.empty()` | planned | 保持现有 warning 和空 payload 行为。 |
| effective fields empty | planned | 回退 Std，不走 RVV pack。 |
| field size 不是 4 | planned | 回退 Std，并用 mixed field size test 证明。 |
| field offset 不按 4 字节对齐 | planned | 回退 Std，并用 unaligned offset test 证明。 |
| `cloud.point_step` 不按 4 字节对齐 | planned | 回退 Std。 |
| `data_size == 0` | planned | 保持 compressed_size=0 / data_size=0。 |
| overflow guard | planned | 保持现有 `-2` 错误，必要时用 guard-level test 或人工不可构造边界说明。 |
| `_` padding field | planned | 与现有标量一样过滤。 |
| file-name overload | not in scope | 不修改；若用户要求，可在 PI4 做 smoke，但不作为本 probe 的 adoption evidence。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| actual evidence role | PI1 planning evidence based on production-shaped diagnostic |
| actual A/B boundary | Phase 010 是 test helper / production-shaped payload wrapper；PI1 本身没有新增 A/B。 |
| 当前决策问题 | 是否可以进入 bounded production probe（有界生产探针）。 |
| diagnostic 是否可外推到 production | 只能外推到 pack + LZF payload 可能保留收益；不能外推到 public overload、fallback、ostream flush 或 file-name overload。 |
| comparison-boundary / baseline mismatch 风险 | 存在。PI2-PI5 必须用真实 public entry 同边界 A/B 修正。 |
| clean adoption 是否需要 production boundary 内 A/B | 需要。没有 PI4 production repeated board 和 PI5 EvidenceDecision 前，不能 clean-adopt。 |

## Evidence Doctor 和 registry

本阶段没有新增性能数据，沿用 Phase 000 / 010 当前证据：

| evidence | doctor result | registry status |
| --- | --- | --- |
| `log/board/component_ablation_repeat_5` | Errors=0，Warnings=0，Suggestions=9 | fresh |
| `log/board/writer_payload_repeat_5` | Errors=0，Warnings=0，Suggestions=4 | fresh |

Suggestions 主要是环境 metadata 和 binary identity 缺失。当前结论仍保持 diagnostic / planning 边界，因此不降级。
PI4 若生成 production direct 性能证据，必须重新生成 manifest、Evidence Doctor 和 registry 记录。

## 验证

| check | command | result |
| --- | --- | --- |
| whitespace | `git diff --check -- test-rvv/io/pcd_io` | pass |
| artifact tracking | `git status --short --untracked-files=all -- test-rvv/io/pcd_io` | 仅当前 topic 产物和本地 build / log；raw logs 默认 local-only。 |
| Phase 000 registry | `python3 test-rvv/script/evidence_registry.py check --registry test-rvv/io/pcd_io/log/evidence_registry.json --scan-glob 'test-rvv/io/pcd_io/log/board/component_ablation_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}' --fail-on any` | fresh |
| Phase 010 registry | `python3 test-rvv/script/evidence_registry.py check --registry test-rvv/io/pcd_io/log/evidence_registry.json --scan-glob 'test-rvv/io/pcd_io/log/board/writer_payload_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}' --fail-on any` | fresh |

## EvidenceDecision

EvidenceDecision：`partial-production-candidate / PI1 complete`。

production_decision：

- 本阶段不修改 `io/src/pcd_io.cpp`。
- 当前可以进入 PI2 production patch（生产补丁），但需要用户明确确认。
- PI2 范围只允许 `std::ostream` overload 的 4 字节字段 pack probe。不得扩大到 reader、file-name overload、templated writer、ASCII writer、non-4-byte fields 或 public API 变更。

## 阶段反思和后续队列

| candidate / action | status | reason | next action |
| --- | --- | --- | --- |
| writer `std::ostream` production patch | turn_stop_deferred with stop_condition_hit | 继续会修改 production 源码。 | 用户确认后进入 PI2。 |
| production direct tests | blocked by PI2 | 需要真实 production helper 或可观测分流。 | PI2 后进入 PI3。 |
| production board repeated / Evidence Doctor | blocked by PI2/PI3 | 需要 production direct bench / summary。 | PI4。 |
| topic-local doc suite split | phase_deferred + unblocked | 当前 README / evaluation 可恢复，但缺独立 testing overview、benchmark/evidence、code map。 | 若暂不授权 PI2，可另开 structure-doc-suite phase。 |
| reader unpack / finite scan | deferred | 与 writer payload 是不同生产边界。 | writer PI2-PI5 后再排，或用户选择切换。 |

continue_stop_decision：

- Phase 020 完成。
- 当前没有板卡 blocker。
- 停止条件命中：继续主线会修改 `io/src/pcd_io.cpp`，需要明确 production patch 授权。
- next_phase_default：`PI2 production_patch for writer std::ostream 4-byte field payload`。
