# Phase 010 计划：production-shaped compressed writer diagnostic

## 阶段意图和边界

本阶段要验证 Phase 000 的 pack-only RVV 收益在加入 LZF compression（LZF 压缩）后是否仍能保留。
阶段仍是 test-only production-shaped diagnostic（生产形态诊断）：复刻 templated compressed writer 的
`pack fields -> lzfCompress` 顺序，不做 header、mmap、file write，也不修改 production（生产源码）。

validated_scope（本阶段验证范围）：

- entry shape（入口形态）：`PCDWriter::writeBinaryCompressed<PointT>` 的压缩前置主要链路。
- point type / layout：synthetic PointXYZRGB-like 4 字节字段，含 no-padding 与 tail-padding 两种布局。
- evidence role：production-shaped diagnostic，不是 production direct。

unvalidated_scope（本阶段不验证范围）：

- 真实 `PCDWriter` public entry（公开入口）分流、fallback 和生产符号归属。
- file lock、header 生成、raw_fallocate、mmap、write 和 error handling。
- 泛型 `PointT` traits、非 4 字节字段、indices overload、ASCII writer。

## 实现和测试动作

| action | artifact / command | completion criteria |
| --- | --- | --- |
| B1 写 failing shaped correctness test | `src/test_pcdtw.cpp`，`make run_test_compare` | 新的 pack+compress candidate 在实现前不能通过或无法链接。 |
| B2 实现 test-only pack+compress helper | `include/impl/pcdtw_support.hpp` | Std/RVV build 的 compressed payload 一致；RVV build 命中 pack RVV。 |
| B3 扩展 bench case | `src/bench_pcdtw.cpp` | 增加 `compressed_*` case，计时包含 pack + LZF，不包含 checksum。 |
| B4 QEMU smoke | `make run_qemu_smoke` | correctness 和 RVV bench log-shape 通过，不写 QEMU 性能结论。 |
| B5 板卡 repeated shaped bench | `make run_board_pcdtw_shaped_repeated` | 5 次 repeated summary，decision bucket 稳定或标记 unstable。 |
| B6 Evidence Doctor 和 registry | `log/board/production_shaped_repeat_5/*` | Doctor Error 必须解除或降级；registry fresh。 |
| B7 文档回填 | result、evaluation、roadmap、matrix、Handoff | 写清是否进入 bounded production probe 或 no-production。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断） |
| A/B boundary | test helper / bench wrapper，包含 pack + LZF，不含 public file write |
| 当前决策问题 | RVV-vs-scalar 是否值得进入 bounded production probe |
| diagnostic 是否可外推到 production | unknown；若仍 positive，可支持 PI1 计划，但不能替代 production direct。 |
| comparison-boundary / baseline mismatch 风险 | yes；没有真实 file mapping、header 和 error path。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 弱正向可考虑窄 PI1；neutral/negative 默认不建议生产探针。 |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段不能 clean adopt。 |

## 板卡复跑预算和决策桶

- run budget：5 次 repeated board run，`--iterations 20 --warmup-iterations 3`。
- positive：median speedup >= `1.08x` 且无 checksum mismatch。
- weak-positive：median speedup 在 `1.03x` 到 `1.08x`，且 Doctor 无 Error。
- neutral：`0.98x` 到 `1.03x`。
- negative：median speedup < `0.98x`。
- unstable：5 次内 decision bucket 摇摆，或 Doctor Error / checksum 问题无法解除。

## Continue / Stop Conditions

若 shaped diagnostic positive / weak-positive 且 Doctor clean，下一 phase 默认是 PI1 production integration plan
（生产接入计划），但不能直接修改 production。若 neutral / negative，则写 no-production diagnostic closeout，
并把 binary writer path 作为独立后续恢复条件。
