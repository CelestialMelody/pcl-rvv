# Phase 070: compressed writer production closeout 结果

## 结果摘要

本阶段在用户确认“接入后板卡测试有收益即可采纳”后，完成 compressed writer production closeout。
`writeBinaryCompressed<PointT>(file_name, cloud)` 的 4 字节字段 RVV pack patch 现在记录为
`adopted production behavior`（已采纳生产行为）。正式长期文档已创建：

- `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`

该文档使用 Phase 060 接入后的 production-public（真实公开入口）板卡数据，而不是早期 diagnostic
（诊断）或 production-shaped diagnostic（生产形态诊断）数据。

## 计划动作回填

| action | status | artifact / evidence |
| --- | --- | --- |
| A1 create formal doc-rvv | done | `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md` 已创建，包含实现设计、fallback、Traceability Map、bench 数据、证据链和后续边界。 |
| A2 sync topic-local docs | done | README、evaluation、optimization evidence、roadmap、matrix、phase index 已同步为 compressed writer adopted。 |
| A3 sync screening and handoff | done | screening 和 current Handoff 已同步当前状态；binary writer 保留为下一条独立 PI2。 |
| A4 freshness / registry check | pending final verification | 本 result 写成后统一执行 diff check、tail whitespace、registry fresh 和必要 correctness。 |

## Production closeout 证据

| evidence area | current evidence | closeout decision |
| --- | --- | --- |
| correctness | `make -C test-rvv/io/pcd_io_templated_writer run_test_compare`：Std/RVV 各 8 个 gtest 通过。 | production-direct public writer payload 和 fallback 语义成立。 |
| QEMU smoke | `run_bench_rvv BENCH_ARGS="--case-filter production_compressed_* --iterations 2 --warmup-iterations 1"` 通过。 | 只证明日志形状，不作为性能结论。 |
| asm | `dump_bench_rvv` 后 public writer 实例化符号附近可见 `vlse32.v` / `vse32.v`。 | RVV 指令归属到 production compressed writer pack path。 |
| board | `log/board/production_compressed_repeat_5/summary.md`。 | compact / padding 大规模 mean `1.3122x / 1.2495x`，positive；small 512 降级 smoke-only。 |
| Evidence Doctor | `log/board/production_compressed_repeat_5/evidence_doctor.md`：Errors=0，Warnings=4，Suggestions=0。 | warning 已解释；不阻塞大规模 positive adoption。 |
| registry | `log/evidence_registry.json`。 | final verification 中检查 fresh。 |

## Optimization matrix 更新

| candidate family | decision | next action |
| --- | --- | --- |
| compressed writer 4-byte RVV production pack | adopted production behavior | closed；长期文档维护 production facts。 |
| binary writer packed output | PI1-plan-complete / phase_deferred + unblocked | 创建 Phase 080 binary writer PI2，不能外推 compressed writer 证据。 |
| ASCII writer | rejected with evidence | 无默认恢复，除非 profile 指向非 stream formatting 主成本。 |

## Evidence Doctor 处理

Doctor Warnings=4：

- compact 262k 和 padding 262k 有 long-tail / variance warning。本阶段不剔除异常，长期文档保留
  mean / median / min / max；由于 min 仍为 `1.2192x` / `1.1937x`，decision bucket 保持 positive。
- compact small 512 有 1/5 退化和 long-tail warning。本阶段将它降级为 smoke-only，不作为 production
  收益主证据。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | production-public closeout。 |
| A/B boundary | `PCDWriter::writeBinaryCompressed<PointT>` public filename overload。 |
| 当前决策问题 | 用户确认后，是否把 Phase 060 production patch 写成 adopted production behavior。 |
| diagnostic 是否可外推到 production | 不需要外推；本阶段使用 Phase 060 真实 production-public 数据。 |
| comparison-boundary / baseline mismatch 风险 | public bench 已包含 header、pack、LZF、mmap/write 和 checksum。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 大规模 positive；small 不稳定已降级。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；当前没有既有 RVV family 与之竞争。 |

## Continue / Stop Decision

`continue_stop_decision`：compressed writer 方向已 closed；topic 仍有 `phase_deferred + unblocked`
方向，即 binary writer PI2。

`next_phase_default`：创建 `080-binary-writer-PI2-production-patch`，按
`doc/phases/050-binary-writer-PI1-production-integration-plan/plan.zh.md` 冻结的边界推进
`writeBinary<PointT>` production patch、production-direct correctness / fallback、asm、board repeated
summary、Evidence Doctor 和 registry。

本阶段不提交；commit 仍需用户明确授权。
