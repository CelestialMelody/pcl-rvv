# Phase 040：decode 稳定性复核结果

## 实际范围

本阶段只运行 test-rvv（测试资产）里的 decode-only repeated board（仅解码重复板卡性能采集），没有修改 production（生产源码），也没有覆盖 `log/board/repeated_phase000` 的当前 12-case summary。本阶段的证据角色是 component ablation（组件消融）：它只判断连续 diff byte 解码 helper 是否仍有稳定局部收益线索。

执行命令：

```bash
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase040_decode \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_contiguous_*'
```

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| decode-only board repeated | done | `log/board/repeated_phase040_decode/summary.md` | 6 个 decode case 的 10-run median 均大于 1。 |
| 独立输出目录 | done | `log/board/repeated_phase040_decode/` | 未覆盖默认 12-case summary；Phase 000/020 的当前默认证据仍保留在 `repeated_phase000`。 |
| Evidence Doctor（证据体检） | done | `log/board/repeated_phase040_decode/evidence_doctor.md` | `Errors=0，Warnings=1，Suggestions=0`；唯一 warning 是 `decode_contiguous_64` 的长尾。 |
| 文档刷新 | done | 本 result、phase index、roadmap、matrix、evaluation、bench/evidence、optimization evidence | 当前 decode 结论升级为 10-run weak-positive diagnostic，但仍不作为 production evidence（生产证据）。 |

## Decode-only 10-run 结果

| case | runs | median | min | max | p10 | p90 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `decode_contiguous_16` | 10 | 1.29x | 1.14x | 1.29x | 1.14x | 1.29x | weak-positive diagnostic |
| `decode_contiguous_64` | 10 | 1.23x | 1.23x | 4.86x | 1.23x | 1.64x | weak-positive diagnostic with long-tail warning |
| `decode_contiguous_256` | 10 | 1.26x | 1.26x | 1.26x | 1.26x | 1.26x | weak-positive diagnostic |
| `decode_contiguous_1024` | 10 | 1.26x | 1.13x | 1.27x | 1.17x | 1.27x | weak-positive diagnostic |
| `decode_contiguous_4096` | 10 | 1.19x | 1.14x | 1.20x | 1.14x | 1.20x | weak-positive diagnostic |
| `decode_contiguous_16384` | 10 | 1.15x | 1.12x | 1.22x | 1.13x | 1.21x | weak-positive diagnostic |

## Evidence Doctor 处理

`log/board/repeated_phase040_decode/evidence_doctor.md` 报告 `Errors=0，Warnings=1，Suggestions=0`。

唯一 warning 是 `decode_contiguous_64` 的 `long_tail_or_variance`：median 为 1.23x，但 max 为 4.86x，max/min 为 3.96。处理方式：

- 不删除该异常值，也不把 4.86x 写成稳定收益。
- 结论以 median、min/max 和 p10/p90 为主，写成 weak-positive diagnostic。
- 可能解释包括板卡温度、频率、调度、缓存状态或 run 顺序；当前没有 per-iteration trace（逐迭代跟踪）和温度 / governor / freq metadata，因此不能进一步归因。
- 该 warning 不触发 Error，因为没有 checksum 不一致、没有退化频率，也没有改变 `median > 1` 的 decision bucket。

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | component_ablation（组件消融），属于 diagnostic（诊断证据）。 |
| A/B boundary | test helper；Std/RVV 都在 `src/bench_point_coding.cpp` 的 synthetic decode case 内。 |
| 当前决策问题 | decode contiguous helper 是否仍值得进入更接近 production context（生产上下文）的 test-only scout。 |
| diagnostic 是否可外推到 production | no。当前只覆盖连续输出片段，不覆盖真实 `pointDiffDataVectorIterator_` 生命周期、完整 decompression pipeline（解压流水线）或 octree context。 |
| comparison-boundary / baseline mismatch 风险 | helper 内 Std/RVV 边界一致；外推到 production 风险高。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前不允许直接 production probe；允许下一 phase 做 test-only production-shaped context scout（生产形态上下文侦察）。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。当前没有 production boundary，也没有 production direct（真实生产入口直连）证据。 |

## EvidenceDecision

`EvidenceDecision`：`weak-positive diagnostic / no-production`。

本阶段证明：

- decode contiguous RVV helper 在 10-run board repeated 中没有出现 median 退化，6 个规模均保持弱正向。
- Phase 010 中 decode 小规模不稳的历史疑虑被降级为“仍需记录长尾，但当前 10-run 桶稳定”。
- decode 路线可以作为下一阶段 test-only production-shaped context scout 的输入。

本阶段不能证明：

- `PointCoding::decodePoints` 的真实 production iterator 已经变快。
- 完整 `OctreePointCloudCompression` 解码路径已经变快。
- 其它点类型、其它 `Scalar`、真实 diff vector 分布或 allocator / iterator 状态已覆盖。

## 阶段反思和下一步

`continue_stop_decision`：Phase 040 的 decode-only 稳定性问题已闭合，未命中板卡或 Evidence Doctor 停止条件；production 仍未授权且证据不足。下一默认 phase 是 `050-production-shaped-context-scout`，范围必须仍在 `test-rvv/io/point_coding/**` 内，先构造更接近 `PointCoding` 对象状态的 test-only scout，再决定是否值得请求 production integration loop（生产接入闭环）授权。
