# approximate_progressive_morphological_filter RVV topic 导航

## 当前状态

当前 topic 针对 `pcl::ApproximateProgressiveMorphologicalFilter<PointT>::extract(Indices&)`。生产补丁已由用户确认采纳：`__RVV10__` 构建在满足 xyz AoS layout gate（布局准入条件）和规模 gate 时走 RVV，否则回到 `extractStd` 标量路径。提交 commit 仍需单独授权。

接入后的 production public board（真实公开入口板卡）证据已覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA`。`PointXYZ` 在 `log/board/production-public-repeated-v2/summary.md` 中 dense median 1.60x、non-dense median 1.35x；040 扩展在 `log/board/point-type-production-repeated-v1/summary.md` 中 8 个 dense / non-dense label 全部 positive，Evidence Doctor 均为 Errors=0 / Warnings=0 / Suggestions=0。

050 阶段尝试把 tail 阶段剩余的阈值比较和输出压缩也改成 RVV，但相对 040 已采纳 RVV 基线整体为 neutral（中性），已回退。当前没有值得自动推进的下一生产优化方向；若继续，建议另开明确范围的 profiling（性能剖析）、更多点型覆盖或多线程验证。

## 阅读路径

| 想复核什么 | 入口 |
| --- | --- |
| 函数语义、候选取舍、Traceability Map（可追踪性地图） | `doc/approximate_progressive_morphological_filter-evaluation.zh.md` |
| 当前已采用生产实现的长期维护说明 | `../../../doc-rvv/segmentation/approximate_progressive_morphological_filter-RVV.zh.md` |
| 阶段计划、结果、优化矩阵和恢复入口 | `doc/phases/README.zh.md` |
| 跨阶段候选搜索空间 | `doc/optimization-roadmap.zh.md` |
| 生产公开入口板卡 summary / manifest / doctor | `log/board/production-public-repeated-v2/summary.md`、`log/board/point-type-production-repeated-v1/summary.md`、`log/board/tail-vector-filter-probe-v1/summary.md` |

## 常用验证命令

```bash
make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare
make -C test-rvv/segmentation/approximate_progressive_morphological_filter check_production_rvv_asm
make -C test-rvv/segmentation/approximate_progressive_morphological_filter \
  OUTPUT_DIR_BOARD=log/board/production-public-repeated-v2 \
  APMF_REPEATED_DIR=log/board/production-public-repeated-v2 \
  APMF_REPEATED_SUMMARY=log/board/production-public-repeated-v2/summary.md \
  APMF_EVIDENCE_MANIFEST=log/board/production-public-repeated-v2/evidence_manifest.json \
  APMF_EVIDENCE_DOCTOR_MD=log/board/production-public-repeated-v2/evidence_doctor.md \
  APMF_EVIDENCE_INCLUDE_REGEX='production public' \
  run_board_evidence_doctor
```

QEMU benchmark timing（仿真器计时）只用于日志形状和路径检查，不作为性能结论。性能结论只来自板卡或目标硬件。

## 证据白名单

当前可引用的 summary-only evidence（摘要证据）是：

- `log/board/repeated/summary.md`、`log/board/repeated/evidence_doctor.md`：component diagnostic（组件诊断）证据。
- `log/board/full-pipeline-repeated/summary.md`、`log/board/full-pipeline-repeated/evidence_doctor.md`：production-shaped diagnostic（生产形态诊断）证据。
- `log/board/production-public-repeated-v2/summary.md`、`log/board/production-public-repeated-v2/evidence_doctor.md`：production public（真实公开入口）证据。
- `log/board/point-type-production-repeated-v1/summary.md`、`log/board/point-type-production-repeated-v1/evidence_doctor.md`：接入后的点型扩展 production public 证据。
- `log/board/tail-vector-filter-probe-v1/summary.md`、`log/board/tail-vector-filter-probe-v1/evidence_doctor.md`：050 生产探针证据；用于解释 rejected / reverted，不作为当前采用实现的收益数据。

raw logs（原始日志）默认留在本地，不进入提交候选，除非用户明确要求提交并完成脱敏检查。

## 后续恢复

默认恢复到 `doc/phases/050-tail-vector-filter-probe/result.zh.md` 查看当前暂停原因。040 已完成并采纳；050 已尝试并回退。当前 roadmap 没有未阻塞且值得自动推进的高优先级生产优化候选，下一步应由 reviewer / 用户选择是否进入提交、审查、更多点型覆盖、OpenMP 多线程验证或真实数据 profile。
