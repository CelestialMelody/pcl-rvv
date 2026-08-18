# Phase 014 Result：production reentry decision packet

## 当前结论

本阶段完成接入前判断包，没有修改 production TEDQ header。当前生产源码仍保持标量。

建议进入的不是“采纳 production 优化”，而是一个有界 PI1 production integration loop
（生产接入闭环）尝试：先冻结接入范围，做 production path-hit（真实路径命中）、
fallback（回退路径）、production asm attribution（生产符号反汇编归因）和 board repeated
direct（板端真实公开入口重复测试）。PI1/PI5 证据未通过前，不能写 production-ready。

## 证据折叠

| area | 当前证据 | 对接入判断的含义 |
| --- | --- | --- |
| ordered-cloud-pair helper / component | helper 和 C1/C2 accumulation-only 均约 2.0x positive，doctor clean | 说明前端逐点公式值得保留为 production 候选来源。 |
| production-public direct 历史尝试 | Phase 002 临时 patch 的 4K/64K/256K median 约 `1.001x/1.002x/1.001x`，overall neutral，doctor 有 Error / Warnings | 阻止直接采纳；下一次必须重做 path-hit、fallback、asm 和 board direct。 |
| source-indexed / dual-indexed staged 和 direct gather | source-indexed staged positive；dual-indexed staged weak-positive；direct gather family source/dual positive | 说明 indexed row source 有 test-rvv 诊断价值，但不能继承 ordered production 结论。 |
| correspondence direct index stream | Phase 008 positive / weak-positive；Phase 009 segment-load rejected；Phase 010 locality-aware production candidate rejected | correspondence baseline 可作为候选，但不建议先接 locality-aware 或 segment-load 路线。 |
| point-type layout | Phase 012 correspondence 六组 positive，doctor `0/5/0`；Phase 013 source/dual 12 组 positive，doctor `0/4/0` | 代表性 extra-field layout 不再是主要 blocker；warning 要求逐 case 解读。 |

## 建议的 PI1 范围

如果授权进入 PI1，建议先做窄范围、可撤回的 production reentry：

| scope | 建议 | 理由 |
| --- | --- | --- |
| 第一入口 | 先只接 `ordered-cloud-pair` public overload | 它是标量 helper 最直接的入口，也是 Phase 002/004 合同已经定义清楚的边界。 |
| 点型 / Scalar | `PointXYZ` / `PointXYZI` / `PointXYZRGB`，`Scalar=float`，xyz AoS layout | layout gate 和 point-type 诊断已覆盖代表性 extra-field layout；`Scalar=double` 先保持标量。 |
| 规模 gate | 小输入 fallback；中大规模才允许 RVV | 保留现有小规模 fallback 语义，避免 setup 成本吞掉收益。 |
| 暂不接 | `source-indexed-cloud-pair`、`dual-indexed-cloud-pair`、`correspondence-pair` | 三者已有 test-rvv positive / weak-positive，但 production overload 需要独立 dispatch、fallback 和 direct board 证据；不建议一次性扩大。 |
| 不采用 | `vlseg3e32` segment-load、locality-aware correspondence dispatch | Phase 009/010 已有负向或不支持 production 分流的证据。 |

## PI1 必须暂停或撤回的条件

| gate | 停止条件 |
| --- | --- |
| correctness | production direct gtest、fallback 或 public boundary 任一失败。 |
| path-hit | 无法证明 public ordered-cloud-pair 在 RVV 构建中真实命中 RVV path。 |
| asm | 只能看到 test-support RVV 指令，无法归因到 production hot boundary。 |
| board | production-public repeated 不是至少 `weak_positive`，或 Evidence Doctor 出现未修正 Error。 |
| evidence freshness | `make evidence_status` 出现未解释的 `unregistered_change`、`unregistered_file` 或 stale doc。 |

## 阶段决定

`continue_stop_decision`：停在用户判断点。

`next_phase_default`：等待用户明确选择是否授权进入 PI1。若授权，下一阶段为
`PI1-production-reentry-contract`，并且只覆盖上表的窄范围；若不授权，当前 topic 保持
`no-production`，test-rvv 诊断证据链收口。
