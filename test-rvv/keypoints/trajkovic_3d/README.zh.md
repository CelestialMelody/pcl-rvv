# trajkovic_3d RVV 主题入口

本主题评估 `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` 中
`TrajkovicKeypoint3D::detectKeypoints()` 的 normal response map（法线响应图）RVV 优化价值。

当前默认读取路径：

- 函数级评估：`doc/trajkovic_3d-evaluation.zh.md`
- 测试总览：`doc/testing-overview.zh.md`
- 正确性测试：`doc/correctness-tests.zh.md`
- benchmark 与证据：`doc/benchmark-and-evidence.zh.md`
- 优化证据：`doc/optimization-evidence.zh.md`
- 测试支撑代码地图：`doc/test-support-code-map.zh.md`
- 阶段索引：`doc/phases/README.zh.md`
- Phase 000 计划：`doc/phases/000-current-state-and-four-corners-diagnostic/plan.zh.md`
- Phase 000 结果：`doc/phases/000-current-state-and-four-corners-diagnostic/result.zh.md`
- Phase 010 计划：`doc/phases/010-production-public-dispatch/plan.zh.md`
- Phase 010 结果：`doc/phases/010-production-public-dispatch/result.zh.md`
- Phase 020 计划：`doc/phases/020-eight-corners-production-public-dispatch/plan.zh.md`
- Phase 020 结果：`doc/phases/020-eight-corners-production-public-dispatch/result.zh.md`
- 优化路线图：`doc/optimization-roadmap.zh.md`
- 优化矩阵：`doc/phases/optimization-matrix.zh.md`
- production 长期文档：`../../../doc-rvv/keypoints/trajkovic_3d-RVV.zh.md`

当前结论：`FOUR_CORNERS` / `EIGHT_CORNERS`、`window_size_=3`、precomputed normals（预计算法线）和 organized dense full cloud（有组织稠密完整点云）路径已采纳窄范围 production RVV（生产 RVV）。真实公开入口 `Detector::compute(output)` 的 repeated board（重复板卡测试）在 `public_four_corners_320x240` 上 median speedup 为 1.790x、在 `public_eight_corners_320x240` 上 median speedup 为 1.677x，Evidence Doctor（证据体检）为 Errors=0、Warnings=0。

当前不覆盖：非 3x3 window、non-dense 输入或法线、normal estimation（法线估计）、NMS（非极大值抑制）RVV、更宽点型和其它未验证 layout。它们继续走标量或进入后续 phase。

常用命令：

```bash
make -C test-rvv/keypoints/trajkovic_3d run_test_compare
make -C test-rvv/keypoints/trajkovic_3d check_trajkovic_3d_rvv_asm
make -C test-rvv/keypoints/trajkovic_3d board_repeated_production record_evidence_state_production
make -C test-rvv/keypoints/trajkovic_3d check_evidence_freshness
```

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状验证。真实性能结论必须来自板卡或目标硬件 repeated benchmark（重复性能测试）与 Evidence Doctor（证据体检）。

证据白名单：

- Phase 000 diagnostic summary：`log/board/repeated_phase000_four_corners_response/summary.md`
- Phase 010 production summary：`log/board/repeated_phase010_production_public/summary.md`
- Phase 020 production summary：`log/board/repeated_phase020_eight_corners_production_public/summary.md`
- Phase 010 Evidence Doctor：`log/board/repeated_phase010_production_public/evidence_doctor.md`
- Phase 020 Evidence Doctor：`log/board/repeated_phase020_eight_corners_production_public/evidence_doctor.md`
- evidence registry：`log/evidence_registry.json`

`log/**` 默认不提交；raw run logs 保持 local-only，除非后续明确要求提交脱敏日志。
