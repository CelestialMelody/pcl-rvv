# recognition/surface_normal_modality RVV 主题

本目录保存 `recognition/include/pcl/recognition/surface_normal_modality.h` 的 RVV
专项测试资产。当前主题短标识为 `snm`，对应完整 topic `surface_normal_modality`。

## 当前状态

- 当前 phase：`030-spread-quantized-map-rvv`
- 当前状态：depth-to-normal / quantize、5x5 filter 和默认 spread 的 production direct（真实生产路径）已采纳，当前实现可视为 adopted production behavior。
- 当前证据角色：production_direct。
- production 状态：`recognition/include/pcl/recognition/surface_normal_modality.h` 已接入 RVV / scalar 分流。
- `doc-rvv` 状态：已创建 `doc-rvv/recognition/surface_normal_modality-RVV.zh.md`。

## 常用命令

```bash
make -C test-rvv/recognition/surface_normal_modality run_test_compare
make -C test-rvv/recognition/surface_normal_modality run_qemu_smoke
make -C test-rvv/recognition/surface_normal_modality dump_bench_rvv
make -C test-rvv/recognition/surface_normal_modality check_snm_rvv_asm
make -C test-rvv/recognition/surface_normal_modality check_snm_production_rvv_asm
SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make -C test-rvv/recognition/surface_normal_modality board_repeated record_evidence_state_repeated
```

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论必须来自
board（板卡）或目标硬件。

## 文档入口

- 函数级评估：`doc/surface_normal_modality-evaluation.zh.md`
- 阶段索引：`doc/phases/README.zh.md`
- 优化路线图：`doc/optimization-roadmap.zh.md`
- 优化矩阵：`doc/phases/optimization-matrix.zh.md`
- Phase 030 result：`doc/phases/030-spread-quantized-map-rvv/result.zh.md`
- 当前 production direct board summary：`log/board/repeated_phase030_spread_rvv/summary.md`
