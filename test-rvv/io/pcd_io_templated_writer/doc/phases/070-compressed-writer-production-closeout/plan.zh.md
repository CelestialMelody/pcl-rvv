# Phase 070: compressed writer production closeout 计划

## 阶段意图和边界

本阶段在用户确认“板卡 production-public 结果有收益即可采纳”后，把 Phase 060 的
`writeBinaryCompressed<PointT>` production patch 从 PI5-positive 提升为 adopted production behavior
（已采用生产行为），并创建正式长期文档
`doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。长期文档必须使用接入后的板卡数据，即
`production_compressed_repeat_5` 的 production-public summary，而不是 Phase 000 / 010 的诊断数据。

本阶段只关闭 compressed writer 的 production closeout。不修改 `writeBinary<PointT>` 的 production 路径；
binary writer 仍按 Phase 050 作为下一条独立 production boundary（生产边界）处理。

## 当前状态清单

| item | current evidence |
| --- | --- |
| production source | `io/include/pcl/io/impl/pcd_io.hpp` 已含 compressed writer RVV pack helper 和 fallback gate。 |
| Phase 060 result | `test-rvv/io/pcd_io_templated_writer/doc/phases/060-compressed-writer-PI2-production-patch/result.zh.md`。 |
| production-public summary | `test-rvv/io/pcd_io_templated_writer/log/board/production_compressed_repeat_5/summary.md`。 |
| Evidence Doctor | `test-rvv/io/pcd_io_templated_writer/log/board/production_compressed_repeat_5/evidence_doctor.md`，Errors=0，Warnings=4，Suggestions=0。 |
| correctness | `make -C test-rvv/io/pcd_io_templated_writer run_test_compare`，Std/RVV 各 8 个 gtest 通过。 |
| asm | `dump_bench_rvv` 后 public writer 实例化符号附近可见 `vlse32.v` / `vse32.v`。 |
| stale docs | README、evaluation、optimization evidence、roadmap、matrix、Handoff 仍有 PI5 pending 或 PI2 authorization 旧话术。 |

## 优化矩阵

| candidate family | scope | required closeout evidence | decision target |
| --- | --- | --- | --- |
| compressed writer production public patch | `writeBinaryCompressed<PointT>`；有效字段均为 4 字节且 source / destination / offset / stride 对齐 | production-direct correctness、fallback、asm、board repeated、Doctor、registry、正式 `doc-rvv` | adopted |
| binary writer packed output | `writeBinary<PointT>`；Phase 040/050 only | 不在本阶段采纳；只保留下一 phase 恢复入口 | pending separate PI2 |

## 实现和文档动作

| action | artifact / command | done criteria |
| --- | --- | --- |
| A1 create formal doc-rvv | `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md` | 含当前状态、实现设计、fallback、Traceability Map、bench 数据、证据链和遗留风险。 |
| A2 sync topic-local docs | README、evaluation、optimization evidence、benchmark/evidence、roadmap、matrix、phase README | 不再把 compressed writer 写成 pending；binary writer 仍明确是 separate production boundary。 |
| A3 sync screening and handoff | `doc-rvv/library-screening/io/io-retained-candidate-rescreen.zh.md`、`tmp/rvv-work-logs/.../current-handoff.*` | current decision 写为 compressed writer adopted；下一默认动作指向 binary writer PI2 或停止条件。 |
| A4 freshness / registry check | evidence registry、summary、Doctor、`git diff --check`、tail whitespace scan | 证据路径 fresh，文档数值与 summary 一致。 |

## Evidence Doctor 和 registry 规则

- 使用 `log/board/production_compressed_repeat_5/evidence_manifest.json`、
  `summary.md` 和 `evidence_doctor.md` 作为本阶段 production closeout 的 performance evidence。
- Doctor Errors 必须为 0；Warnings 必须在正式文档中解释。大规模 long-tail / variance 保留
  min / median / max；small case 降级为 smoke-only。
- `log/evidence_registry.json` 必须对本 run 保持 fresh。

## 阶段完成条件

- `compressed writer production public patch` 在 matrix / roadmap / evaluation / Handoff 中为 adopted。
- 正式 `doc-rvv` 主题文档已创建，并且不复制诊断流水；它只保留长期 production 事实。
- stale PI5 pending 文档话术已更新。
- verification 命令通过，且没有把 binary writer 的 component evidence 写成 production adopted。

## 板卡复跑预算和决策桶

本阶段不新增板卡复跑。Phase 060 已用 5-run bounded budget 完成 production-public repeated evidence。
decision bucket：大规模 compact 和 padding 为 positive；small 512 为 smoke-only。

## 继续 / 停止条件

完成 closeout 后仍存在一个可继续方向：binary writer PI2。它会修改同一 production file 的另一条 public
入口，属于 separate production boundary。若当前 dirty isolation 和文档状态允许，下一阶段应按
`doc/phases/050-binary-writer-PI1-production-integration-plan/plan.zh.md` 进入 Phase 080 binary writer PI2；
若发现 closeout 后没有可验证收益方向或需要新增用户选择，则在 Handoff 中暂停并说明。

## 文档更新清单

- 新增：`doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`
- 新增：本 phase result。
- 更新：README、evaluation、testing / correctness / benchmark / optimization evidence、roadmap、matrix、
  phase index、screening、Handoff。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public closeout；Phase 000/010 只作为历史诊断背景。 |
| A/B boundary | `PCDWriter::writeBinaryCompressed<PointT>` public overload。 |
| 当前决策问题 | 用户确认后，是否把当前 production patch 作为 adopted production behavior 记录。 |
| diagnostic 是否可外推到 production | 不需要外推；采用 Phase 060 真实 production-public 数据。 |
| comparison-boundary / baseline mismatch 风险 | 真实 public bench 已包含 header、pack、LZF、mmap/write 和 checksum。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 大规模 positive；small 不稳定只保留 smoke-only。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；当前没有既有 RVV family 与之竞争。 |
