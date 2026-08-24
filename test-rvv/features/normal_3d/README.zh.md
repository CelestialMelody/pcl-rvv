# normal_3d RVV 主题入口

本主题评估 `features/include/pcl/features/impl/normal_3d.hpp` 中
`NormalEstimation::computeFeature` 到 `computePointNormal` 的 RVV 优化价值。

当前默认读取路径：

- 函数级评估：`doc/normal_3d-evaluation.zh.md`
- 阶段索引：`doc/phases/README.zh.md`
- Phase 000 计划 / 结果：`doc/phases/000-current-state-and-common-covariance-audit/plan.zh.md`、`doc/phases/000-current-state-and-common-covariance-audit/result.zh.md`
- 优化路线图：`doc/optimization-roadmap.zh.md`
- 优化矩阵：`doc/phases/optimization-matrix.zh.md`

当前结论：Phase 000 已完成。common covariance RVV 在 component boundary 上稳定正向，但 public
`NormalEstimation` 只有 near-threshold 弱收益；本 topic 不修改
`features/include/pcl/features/impl/normal_3d.hpp` production 路径。

常用命令：

```bash
make -C test-rvv/features/normal_3d run_test_compare
make -C test-rvv/features/normal_3d dump_bench_rvv
make -C test-rvv/features/normal_3d board_smoke
make -C test-rvv/features/normal_3d board_repeated
make -C test-rvv/features/normal_3d evidence_doctor_repeated
```

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状验证。真实性能结论必须来自板卡或目标硬件 repeated benchmark（重复性能测试）与 Evidence Doctor（证据体检）。
