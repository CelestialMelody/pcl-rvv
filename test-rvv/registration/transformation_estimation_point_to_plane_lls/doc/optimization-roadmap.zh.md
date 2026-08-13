# Optimization Roadmap

本路线图记录 `transformation_estimation_point_to_plane_lls` 的跨阶段优化搜索空间。
阶段 `plan/result` 负责本阶段闭环；`doc/phases/optimization-matrix.zh.md` 负责证据状态；
本文件只保存后续仍可恢复的 candidate family（候选族）、暂缓原因和恢复条件。

## 当前边界

当前 adopted（已采用）边界是：

```text
production-candidate/full-cloud-f32-aos-layout-gated-source-xyz-target-xyznormal-float-rvv-fused-formula-block-dispatch-representative-pointtypes
```

它只覆盖 full-cloud（全云顺序扫描，source 和 target 按相同下标配对）公开 overload、
`Scalar=float`、source `x/y/z` f32 AoS layout gate（float 字段结构数组布局门控）和
target `x/y/z/normal_x/normal_y/normal_z` f32 AoS layout gate。板卡 repeated evidence
（重复板卡性能证据）只覆盖三类代表组合：`PointNormal -> PointNormal`、
`PointXYZ -> PointNormal` 和 `PointXYZ -> PointXYZINormal`。

仍保持边界外：

- source-indexed（source 单侧索引路径）、dual-indices（双侧索引路径）和 correspondences（对应关系路径）。
- weighted LLS。
- `Scalar=double`。
- 不满足 f32 AoS layout gate 的点型组合。
- 满足 gate 但未逐类型上板的特殊点型性能结论。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| fused-formula block-reduction production dispatch | 当前源码、Phase 000/010 证据和 production-dispatch 5-run summary | full-cloud, f32 AoS, `Scalar=float` | 当前默认 hot path；代表点型 5-run median 为 `2.80x` 到 `3.15x`。 | reduction tree 和逐点公式树不同；未逐类型覆盖所有 gate-allowed 点型。 | 已有 production direct tests、asm attribution、board summary。 | adopted | 只有 hot path、bench case 或 dispatch gate 改变时才重跑。 |
| production helper shape review | Phase 000/010 result 的 reviewer 风险 | production RVV helper 内部 A/B/C/N block groups | 可能降低 helper size 和重复 load/formula 的维护成本。 | 可能改变机器码、寄存器压力或数值树；收益不保证。 | 先做源码 shape patch；重跑 `run_test_std`/`run_test_rvv`、`dump_bench_rvv`；若机器码或计时边界变化，再按风险决定板卡。 | deferred | `030-helper-shape-review`，只在 reviewer 要求或合入前可维护性门槛触发时做。 |
| evidence registry adoption | Phase loop / Evidence Doctor 规则 | topic-local generated logs、board summary 和 freshness check | 让 run 覆盖、manual rerun 和 stale docs 可机器检查。 | 当前只登记 summary/QEMU correctness digest；没有生成 topic-local manifest 或 doctor report。 | `log/evidence_registry.json`、registry check 和文档白名单。 | adopted | none; keep using registry on future recoveries. |
| legacy pointer / compatibility alias cleanup | 当前 agent 默认策略和 Phase 030 恢复检查 | root evaluation pointer、旧 test support alias | 删除无依赖旧入口，减少 reviewer 路径歧义。 | 历史 phase 文档仍记录当时保留决策；当前源码/脚本不能依赖旧路径。 | `rg` 依赖检查、std/RVV correctness。 | adopted | none; keep legacy-free by default. |
| topic-local doc suite parity | Phase 040 maturity audit and weighted sibling structure quality bar | README、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map | 让测试语义、bench label、证据白名单和代码地图有稳定读者路径，减少 evaluation 大文档负担。 | 文档拆分可能与旧 evaluation/topic doc 重复；需要保持主归属清楚。 | doc suite、registry freshness check、`git diff --check`、std/RVV QEMU correctness temp logs。 | adopted | none; keep docs synchronized on future evidence changes. |
| test source split | Phase 010 layout audit and Phase 050 structure layout | `src/test_teptpl_*.cpp`、thin `src/bench_teptpl.cpp`、bench impl headers | 提高 reviewer 定位效率，减少 1500+ 行测试源和 1000+ 行 bench 源的认知负担。 | 纯搬迁 diff 已验证；case 名、bench label 和 target 名保持不变。 | std/RVV correctness 40/40；std bench compile smoke；RVV bench compile / asm smoke。 | adopted | none; 新增测试按现有四个 gtest 分组落位。 |
| internal helper layout | Phase 010 sibling structure audit and Phase 050 structure layout | `include/impl/teptpl_*.hpp` internal headers | 与当前配置解析出的 `include/impl` 布局一致，删除旧 `test_support/` include 入口。 | 不改变 production API、hot path、bench label 或 evidence role。 | include graph 迁移、文档 code map、std/RVV correctness 和 bench compile smoke。 | adopted | none; 不新增 compatibility alias。 |
| more f32 AoS representative pointtypes | registration generic gate 审计 | gate-allowed source xyz / target xyz+normal 点型组合 | 降低“代表点型不等于逐类型性能”的风险。 | 板卡预算增加；特殊 stride/padding 可能暴露新布局风险。 | 逐类型 production direct tests、asm attribution 和 repeated board sampling。 | deferred | 需要 release 风险或 reviewer 指定点型时做。 |
| `Scalar=double` RVV exploration | registration LLS 数值扩展 | full-cloud double output scalar | 理论上可覆盖 double transform matrix 输出。 | 当前 RVV helper 和板卡证据只证明 float；double reduction / solver 预算独立。 | 独立数值预算、QEMU correctness、asm 和 board bench。 | deferred | 另开 phase；不从 float 结论继承。 |
| source-indexed / dual-indices / correspondences carry-over audit | registration row-source evidence 规则 | indexed / correspondences row source policies | 检查 adopted full-cloud formula / reduction family 是否有迁移价值。 | 历史诊断负向且分布敏感；退化不能单因归因为 gather。 | policy-specific candidate、same-boundary bench、component ablation 或 profile、board repeated summary。 | deferred | 独立 follow-up；不能默认进入 production integration。 |
| trusted-dense path | 历史 diagnostic | full-cloud dense input | 可能减少 finite mask 成本。 | 会改变公开 finite 语义；`is_dense` 合同不足以替代字段级 finite check。 | 明确 `is_dense` production 合同、invalid-lane 负向测试和 board A/B。 | rejected for current boundary | 只有公开语义重新审计后恢复。 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000-current-state-and-gaps | evidence registry adoption | 本 topic 已有 summary-only 板卡证据，但没有 registry；人工 freshness 检查可用但不够自动化。 | registry target、summary 白名单和 stale check。 | medium |
| 010-test-harness-layout-audit | test source split | 源码已迁到 `src/`，但 test / bench 文件仍很长。 | 拆分后 correctness 和 bench compile smoke。 | low / reviewer-triggered |
| 020-roadmap-and-evaluation-recovery | roadmap and evaluation recovery | phase loop 规则要求 roadmap；evaluation 主路径应与 `artifact_layout.evaluation_doc_template` 一致。 | `git diff --check`、引用检查和 phase result。 | high / current phase |
| 030-legacy-cleanup-and-evidence-registry | legacy cleanup and registry adoption | 当前规则要求无明确依赖的旧路径 / 兼容入口默认删除；registry 已有通用脚本可记录当前 summary-only 证据。 | `rg` 依赖检查、registry check、std/RVV correctness。 | high / completed current phase |
| 040-doc-suite-parity | topic-local doc suite parity | Phase 040 ready-for-review validity check 发现 README、testing overview、correctness tests、benchmark/evidence、optimization evidence 和 code map 仍缺失；这会影响短 prompt 恢复和 reviewer 定位。 | 新增 doc suite、registry freshness check、`git diff --check`、std/RVV QEMU correctness temp logs。 | high / completed current phase |
| 050-structure-layout | test source split + internal helper layout | 最新 phase loop 规则把 `test-source-split` 和 `internal-helper-layout` 都判为 `phase_deferred + unblocked`，且二者共享 include graph。 | 多源 gtest、thin bench entry、`include/impl` 迁移、std/RVV correctness、bench compile / asm smoke。 | high / completed current phase |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| production helper shape review | 当前没有证据矛盾，且修改 hot path 可能需要重新取证。 | reviewer 要求压缩 helper、合入前维护门槛触发，或后续 hot path 本来要修改。 |
| legacy pointer / compatibility alias | Phase 030 恢复检查未发现当前依赖，已删除。 | 若外部脚本或 reviewer workflow 明确要求旧入口，再以新 phase 恢复 alias，并记录依赖证据和删除条件。 |
| test source split | Phase 050 已采用；旧单文件状态只保留在历史 phase 文档中。 | 若未来新增新的测试职责，按 public semantics / candidates / production direct / row sources 分组增量维护。 |
| internal helper layout | Phase 050 已采用；旧 `test_support/` 不再作为当前常规内部 helper 路径。 | 若外部脚本明确依赖旧目录，再另开兼容 phase 并记录删除条件。 |
| source-indexed / dual-indices / correspondences production | full-cloud production 正向不能抵消 indexed / correspondences 历史负向；需要独立 profile 和同边界证据。 | 有新的 row-source phase plan、policy-specific candidate、board repeated evidence 和 Evidence Doctor 输入。 |
| `Scalar=double` | 当前 float path 的数值和性能证据不能外推到 double。 | 有 double-specific numerical budget、asm 和目标硬件结果。 |
| trusted-dense | 当前 public semantics 仍逐字段 finite check；跳过 finite mask 会改变风险面。 | `is_dense` 合同与 invalid-lane 语义审计完成，并有专项负向/正向测试。 |

## 默认恢复动作

当前 `roadmap_default_recovery_queue` 已闭合 Phase 050 的两个未阻塞结构项：

1. `050-structure-layout/test-source-split`：adopted。
2. `050-structure-layout/internal-helper-layout`：adopted。

`ready_for_review_validity_checked` 只是检查标签，不再作为队列项。完成 Phase 050 后，当前没有仍在
topic-local 测试资产 / 文档边界内的高优先级 `phase_deferred + unblocked` 结构动作。若继续当前 topic，
默认只剩需要扩大范围或新增证据的独立 phase：

1. `060-helper-shape-review`：审查 / 压缩 production RVV helper；会触碰 production hot path，必须重跑 correctness，若机器码或性能边界变化再决定是否补板卡。
2. `060-row-source-family-carryover`：indexed / correspondences production exploration；不能继承 full-cloud 结论，需要 policy-specific candidate、bench、asm 和板卡证据。
3. `060-pointtype-or-scalar-expansion`：更多 f32 AoS 点型或 `Scalar=double`；需要独立数值预算和目标硬件结果。
