# recognition/color_gradient_dot_modality RVV 主题

本目录保存 `recognition/include/pcl/recognition/color_gradient_dot_modality.h` 的 RVV 专项测试资产。
当前主题短标识为 `cgdm`，对应完整 topic `color_gradient_dot_modality`。

## 当前状态

- 当前 phase：`020-closeout-doc-suite-and-commit-readiness`
- 当前状态：adopted production behavior（已采用生产行为）。
- 当前证据角色：Phase 010 production direct（真实生产路径）证据，外加 Phase 020 closeout doc suite（收尾文档套件）。
- production 状态：`processInputData()` 在 RVV 构建下调用 `computeMaxColorGradientsRVV()`；
  `computeDominantQuantizedGradients()` 保持标量。

## 先读哪份文档

1. `doc/color_gradient_dot_modality-evaluation.zh.md`
2. `doc/phases/README.zh.md`
3. `doc/testing-overview.zh.md`
4. `doc/correctness-tests.zh.md`
5. `doc/benchmark-and-evidence.zh.md`
6. `doc/optimization-evidence.zh.md`
7. `doc/test-support-code-map.zh.md`
8. `doc/optimization-roadmap.zh.md`
9. `doc/phases/010-production-integration/result.zh.md`
10. `doc/phases/020-closeout-doc-suite-and-commit-readiness/plan.zh.md`
11. `doc/phases/020-closeout-doc-suite-and-commit-readiness/result.zh.md`

## 常用命令

```bash
make -C test-rvv/recognition/color_gradient_dot_modality run_test_compare
make -C test-rvv/recognition/color_gradient_dot_modality run_qemu_smoke
make -C test-rvv/recognition/color_gradient_dot_modality check_cgdm_rvv_asm
make -C test-rvv/recognition/color_gradient_dot_modality board_repeated
make -C test-rvv/recognition/color_gradient_dot_modality record_evidence_state_repeated
make -C test-rvv/recognition/color_gradient_dot_modality check_evidence_freshness
```

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论必须来自 board（板卡）或目标硬件。

## 文档入口

- 函数级评估：`doc/color_gradient_dot_modality-evaluation.zh.md`
- 测试总览：`doc/testing-overview.zh.md`
- 正确性测试：`doc/correctness-tests.zh.md`
- benchmark 与证据：`doc/benchmark-and-evidence.zh.md`
- 优化证据：`doc/optimization-evidence.zh.md`
- test support 代码地图：`doc/test-support-code-map.zh.md`
- 阶段索引：`doc/phases/README.zh.md`
- 优化路线图：`doc/optimization-roadmap.zh.md`
- 优化矩阵：`doc/phases/optimization-matrix.zh.md`
- Phase 000 plan：`doc/phases/000-current-state-and-cgdm-scaffold/plan.zh.md`
- Phase 000 result：`doc/phases/000-current-state-and-cgdm-scaffold/result.zh.md`
- Phase 010 result：`doc/phases/010-production-integration/result.zh.md`
- Phase 020 plan：`doc/phases/020-closeout-doc-suite-and-commit-readiness/plan.zh.md`
- Phase 020 result：`doc/phases/020-closeout-doc-suite-and-commit-readiness/result.zh.md`
- Production doc：`../../../doc-rvv/recognition/color_gradient_dot_modality-RVV.zh.md`
