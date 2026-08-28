# recognition/color_gradient_modality RVV 主题

本目录保存 `recognition/include/pcl/recognition/color_gradient_modality.h` 的
RVV 专项测试资产。当前主题短标识为 `cgm`，对应完整 topic
`color_gradient_modality`。

## 当前状态

- 当前 phase：`050-rgb-stencil-production-integration`
- 当前状态：topic closeout complete；当前优化工作已结束。若未来重开，先做
  `extractFeatures()` profile / component ablation（组件消融），不要直接写生产 RVV。
- 当前证据角色：Phase 000-020 为 production-shaped diagnostic（生产形态诊断）；
  Phase 030 与 Phase 050 为 production direct（真实生产路径）证据；Phase 040 为
  production-shaped diagnostic A/B。
- production 状态：`ColorGradientModality::processInputData()` 已有候选 RVV dispatch
  工作区补丁，并已按用户确认和 Phase 050 板卡收益视为 adopted production behavior（已采纳生产行为）。
- `doc-rvv` 状态：已创建
  `doc-rvv/recognition/color_gradient_modality-RVV.zh.md`，数据采用 Phase 050
  production direct 板卡 repeated summary。

## 常用命令

```bash
make -C test-rvv/recognition/color_gradient_modality run_test_compare
make -C test-rvv/recognition/color_gradient_modality run_qemu_smoke
make -C test-rvv/recognition/color_gradient_modality dump_bench_rvv
make -C test-rvv/recognition/color_gradient_modality check_cgm_rvv_asm
make -C test-rvv/recognition/color_gradient_modality check_cgm_filter_rvv_asm
make -C test-rvv/recognition/color_gradient_modality check_cgm_full_chain_rvv_asm
make -C test-rvv/recognition/color_gradient_modality check_cgm_production_rvv_asm
make -C test-rvv/recognition/color_gradient_modality check_cgm_stencil_rvv_asm
make -C test-rvv/recognition/color_gradient_modality board_repeated
make -C test-rvv/recognition/color_gradient_modality record_evidence_state_repeated
make -C test-rvv/recognition/color_gradient_modality check_evidence_freshness
```

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论必须来自
board（板卡）或目标硬件。

## 文档入口

- 函数级评估：`doc/color_gradient_modality-evaluation.zh.md`
- 阶段索引：`doc/phases/README.zh.md`
- 优化路线图：`doc/optimization-roadmap.zh.md`
- 优化矩阵：`doc/phases/optimization-matrix.zh.md`
- Phase 000 result：`doc/phases/000-current-state-and-cgm-scaffold/result.zh.md`
- Phase 010 result：`doc/phases/010-dominant-filter-rvv/result.zh.md`
- Phase 020 result：`doc/phases/020-full-chain-production-shaped-diagnostic/result.zh.md`
- Phase 030 plan：`doc/phases/030-production-integration-plan/plan.zh.md`
- Phase 030 result：`doc/phases/030-production-integration-plan/result.zh.md`
- Phase 040 plan：`doc/phases/040-rgb-stencil-rvv-ablation/plan.zh.md`
- Phase 040 result：`doc/phases/040-rgb-stencil-rvv-ablation/result.zh.md`
- Phase 050 plan：`doc/phases/050-rgb-stencil-production-integration/plan.zh.md`
- Phase 050 result：`doc/phases/050-rgb-stencil-production-integration/result.zh.md`
- 正式长期主题文档：`../../../doc-rvv/recognition/color_gradient_modality-RVV.zh.md`
