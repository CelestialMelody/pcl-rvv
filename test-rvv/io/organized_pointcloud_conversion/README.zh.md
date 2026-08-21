# organized_pointcloud_conversion RVV topic

## 当前结论

本 topic 的 production patch 已按用户确认进入 adopted production behavior。当前生产接入覆盖两段路径：`OrganizedConversion<PointT>::convert(cloud, ...)` 的 cloud -> disparity / RGB / mono 转换，以及 `OrganizedPointCloudCompression<PointT>::analyzeOrganizedCloud` 的 production-detail（生产 detail）helper。Disparity/depth -> cloud decode overload 仍保持标量。完整 encode-shaped 压缩链路已做 production-shaped diagnostic（生产形态诊断），显示接入 analyze detail 后仍约 1.12x-1.17x 正向，但这不是真实 `OrganizedPointCloudCompression::encodePointCloud` public class entry（公开类入口）证据。

生产直连板卡证据来自 `log/board/production_direct_repeated/summary.md`：`PointXYZ` / `PointXYZI` uncolored 约 2.01x-2.11x，`PointXYZRGB` / `PointXYZRGBA` RGB 约 1.35x-1.43x，`PointXYZRGB` mono 约 1.64x-1.77x。Evidence Doctor 为 `Errors=0, Warnings=1`，唯一 warning 是 mono case 组内离群，因此 mono 与 RGB 分开报告。

端到端形态历史证据来自 `log/board/full_encode_repeated/summary.md`：三个 `production_full_*` case 的 median 为 1.147x、1.148x、1.157x，Evidence Doctor 为 `Errors=0, Warnings=0`。Analyze detail 接入后的当前证据来自 `log/board/analyze_production_detail_repeated/summary.md` 和 `log/board/full_encode_after_analyze_detail_repeated/summary.md`：production-detail analyze median 为 3.908x、3.815x、1.820x；after-patch full shaped median 为 1.119x、1.147x、1.174x。

## 阅读路径

| 读者问题 | 文档 |
| --- | --- |
| 当前生产实现如何工作，哪些路径回退 | `doc-rvv/io/organized_pointcloud_conversion-RVV.zh.md` |
| 本 topic 的取舍、证据边界和未覆盖范围 | `doc/organized_pointcloud_conversion-evaluation.zh.md` |
| 测试 / bench / board target 怎么分层 | `doc/testing-overview.zh.md` |
| 每个 gtest 覆盖什么 | `doc/correctness-tests.zh.md` |
| bench case、Evidence Doctor 和日志提交边界 | `doc/benchmark-and-evidence.zh.md` |
| adopted / rejected / deferred 优化方式 | `doc/optimization-evidence.zh.md` |
| 测试支撑代码和 production helper 如何定位 | `doc/test-support-code-map.zh.md` |
| 跨 phase 搜索空间和下一步 | `doc/optimization-roadmap.zh.md` |
| 阶段计划、结果和矩阵 | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` |

## 常用命令

```bash
cd test-rvv/io/organized_pointcloud_conversion
make run_test_compare
make check_production_rvv_asm
make run_production_repeated_evidence_doctor
make run_production_repeated_evidence_doctor PRODUCTION_REPEATED_DIR=log/board/full_encode_repeated
make run_production_repeated_evidence_doctor PRODUCTION_REPEATED_DIR=log/board/analyze_production_detail_repeated
make run_production_repeated_evidence_doctor PRODUCTION_REPEATED_DIR=log/board/full_encode_after_analyze_detail_repeated
make run_board_evidence_doctor
```

`run_test_compare` 证明 Std / RVV correctness（正确性）对拍；`check_production_rvv_asm` 证明 production probe 中存在 RVV 指令；`run_production_repeated_evidence_doctor` 复核 production direct repeated board manifest。性能结论只引用板卡 repeated summary，QEMU 不作为性能证据。

## 提交边界

默认提交候选是 production source、topic 测试资产、topic-local 文档、正式 `doc-rvv` 和筛选队列表。`log/board/**` 与 `log/qemu/**` raw logs 按 summary-only 策略留在本机；只有用户明确要求提交 evidence logs 时，才单独处理脱敏和 `git add -f`。

## 默认恢复入口

当前默认恢复入口是 `doc/phases/090-analyze-adoption-closeout/result.zh.md`。当前无必须继续推进的高优先级性能候选：真实 `OrganizedPointCloudCompression::encodePointCloud` public class direct 证据受 no-OpenNI cross build 限制；color byte pack 需要 profile 或同边界 A/B 证明仍是瓶颈；更多 traits-compatible 点型属于范围扩展，需要单独 phase。
