# Moment of inertia estimation RVV topic

本 topic 评估 `MomentOfInertiaEstimation<PointT>` 中逐点规约循环的 RVV（RISC-V Vector，可变长度向量）可行性。

阅读路径：

- 函数级评估：`doc/moment_of_inertia_estimation-evaluation.zh.md`
- 阶段索引：`doc/phases/README.zh.md`
- 当前默认恢复入口：`doc/phases/050-point-type-expansion/result.zh.md`
- optimization roadmap（优化路线图）：`doc/optimization-roadmap.zh.md`
- 正式 production 长期文档：`../../../doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md`

topic token（主题短标识）为 `moi`，用于缩短测试和 bench 文件名。phase 000 / 010 是 diagnostic（诊断）测试资产；phase 030 的 mean/AABB-only production probe（只接入质心和轴对齐包围盒的生产探针）已因 public compute `neutral` 且 Evidence Doctor Error 回滚。phase 040 已接入 projected covariance fusion（投影协方差融合）production path，并按接入后的板卡 repeated 结果采纳。phase 050 进一步验证了常见 `PointXYZ`-like typed scope（点型范围）：`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZRGBNormal` 也保持正向收益。

当前 production evidence（生产证据）：

- `log/board/repeated_phase040_projected_covariance_production/summary.md`
- `log/board/repeated_phase040_projected_covariance_production/evidence_doctor.md`
- phase040 public `moi_public_compute,points=65536`：5-run median `1.984x`，min `1.877x`，max `2.076x`，`0/5` 低于 1，decision bucket `positive`。
- phase040 Evidence Doctor：Errors=0，Warnings=0，Suggestions=1。
- `log/board/repeated_phase050_pointxyzi_production/summary.md`
- `log/board/repeated_phase050_pointxyzrgb_production/summary.md`
- `log/board/repeated_phase050_pointxyzrgba_production/summary.md`
- `log/board/repeated_phase050_pointxyzrgbnormal_production/summary.md`
- phase050 四组 typed public compute 全部为 `positive`，所有 Evidence Doctor 均为 `0 Error / 0 Warning / 1 Suggestion`。

常用命令：

```bash
make run_test_rvv
make run_test_compare
make dump_bench_rvv
make run_board_moi_phase040_projected_covariance_repeated
make run_board_moi_phase050_pointxyzi_repeated
make run_board_moi_phase050_pointxyzrgb_repeated
make run_board_moi_phase050_pointxyzrgba_repeated
make run_board_moi_phase050_pointxyzrgbnormal_repeated
make evidence_status_phase040
make evidence_status_phase050
```

性能结论必须来自 board / target hardware（板卡或目标硬件），QEMU 只用于 correctness（正确性）和日志形状。
