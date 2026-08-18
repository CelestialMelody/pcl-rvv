# Phase 008 Result：correspondence direct index-stream

## 当前结论

本阶段针对 `correspondence-pair` 的 query/match 展开成本增加了
direct index-stream candidate：它直接从 `pcl::Correspondence` AoS（数组结构）
记录按固定 stride 读取 `index_query/index_match`，再复用已有双侧
`vluxei32` gather、C1/C2 公式、f32 到 f64 规约和 Eigen 4x4 solve。

当前已完成实现、Std/RVV correctness、QEMU log-shape smoke 和板卡 repeated。
production TEDQ header、public API 和 production dispatch 均未修改。

## 计划动作回填

| action | 状态 | 证据和结论 |
| --- | --- | --- |
| A1 direct correspondence ingress | done | `include/impl/tedq_candidates.hpp` 新增 `estimateDualQuaternionCorrespondenceDirectIndexStreamCandidate`；仅在标准布局、32-bit `index_t`、32-bit byte offset 和最小规模 gate 成立时命中 RVV，否则回退既有 direct/scalar 语义。 |
| A2 correctness | done | 当前回归 `make run_test_compare`；Std/RVV 各 `24/24 tests passed`。`CorrespondenceDirectIndexStreamCandidateMatchesScalar` 通过；RVV stats 命中 `used_rvv/used_gather/used_correspondence_index_stream`，Std 保持 fallback。 |
| A3 bench case | done | `correspondence-direct-index-stream-comparison` 输出 staged、Phase 007 direct gather、新 direct index stream 三侧；QEMU 三侧 checksum 分别在 4K/64K/256K 全一致。 |
| A4 board comparison | done | `log/board/correspondence_direct_index_stream_repeated/`；5 runs、每次 20 iterations、warm-up 5，registry 已登记。 |
| A5 decision | done | staged/direct gather/direct index stream 三侧 checksum 全一致；staged-vs-index-stream、direct-gather-vs-index-stream 的 overall bucket 均为 `positive`。 |

## QEMU 证据

QEMU 只用于 correctness、路径和日志形状，不用于性能排序。Phase 008 smoke
使用 `--case-filter correspondence-direct-index-stream-comparison`，Std/RVV
三侧的 4K、64K、256K checksum 均一致；manifest 和 doctor 路径为：

- `log/qemu/evidence_manifest.json`
- `log/qemu/evidence_doctor.md`

QEMU 的 `Errors / Warnings / Suggestions` 数量和当前 smoke 输入应以该 doctor
报告为准；若出现 Error，不能把本阶段写成严格性能通过。

## RVV 入口与边界

新 candidate 只覆盖：

- `PointXYZ` / `float`；
- 标准布局的 `PointT` 和 `pcl::Correspondence`；
- `index_t` 为 32-bit；
- source/target byte offset 可由 32-bit gather offset 表达；
- correspondence 数量达到既有 RVV 最小规模阈值。

不覆盖其它点类型、`Scalar=double`、任意非标准布局、生产公开入口或任意真实
correspondence 分布。已有 Phase 007 direct gather 保留为 baseline；本阶段不会把
新 candidate 自动升级为默认路径。

## Evidence Doctor、registry 和证据边界

板卡 summary、manifest 和 Evidence Doctor 由 topic-local script 生成并由
`log/evidence_registry.json` 登记。Warning 必须按 correspondence 的 query/match
展开、AoS locality（局部性）、index locality、gather、寄存器压力和 solver 稀释
等候选解释；没有 profile 或消融时不能单因归因于某一个因素。

无论板卡结果如何，本阶段形成的是 test-rvv implementation-family diagnostic
evidence（诊断证据），不能替代 production direct evidence，也不能改变当前
`no_production_after_component_positive_public_neutral` 总决策。

## 板卡结果

Milkv-Jupiter 5-run repeated 的 median B/A 如下：

| 比较 | 4K | 64K | 256K | overall |
| --- | ---: | ---: | ---: | --- |
| staged ordered reuse / direct indexed gather | `1.565x` | `1.832x` | `1.663x` | `weak_positive` |
| staged ordered reuse / direct index stream | `2.505x` | `3.076x` | `2.527x` | `positive` |
| direct indexed gather / direct index stream | `1.575x` | `1.679x` | `1.442x` | `weak_positive` |

Evidence Doctor 为 `Errors=0, Warnings=6, Suggestions=0`。warning 来自 64K / 256K
long-tail 和 64K group outlier；它们需要保留 temperature / frequency / cache /
scheduling 或数据相关分支等假设，不能单因归因于 AoS、gather 或 index stream。

## 继续 / 停止决定

- 当前阶段：`done / diagnostic_positive`。
- `continue_stop_decision`：继续进入下一窄 phase；当前仍有未阻塞的 test-only RVV 候选。
- `next_phase_default`：`009-correspondence-segment-load`，比较 `vlseg3e32` 读取
  `query/match/distance` 三列与 Phase 008 的两次 `vlse32`，保留 Phase 008
  direct index stream 作为 baseline。
- production integration loop：未授权，不启动。
