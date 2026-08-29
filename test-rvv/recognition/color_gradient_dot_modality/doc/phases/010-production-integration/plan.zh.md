# Phase 010: production integration plan

## 目标

本阶段把 Phase 000 已验证的 RGB 差分和梯度数学 RVV 路径接入
`recognition/include/pcl/recognition/color_gradient_dot_modality.h`。production public entry（真实公开入口）仍是
`ColorGradientDOTModality<PointInT>::processInputData()`；公开 API 不变。

## 接入方案

| area | plan |
| --- | --- |
| production helper | 新增 `computeMaxColorGradientsRVV()`，只在 `__RVV10__ && __riscv_vector` 下编译。 |
| dispatch | `processInputData()` 在 RVV 构建下调用 RVV helper，非 RVV 构建保持 `computeMaxColorGradients()`。 |
| unchanged scalar state | `computeDominantQuantizedGradients()` 保持原标量实现，继续负责 strict `>` dominant tie-break、阈值和空 bin bit。 |
| point type / layout | 本阶段 production evidence 只覆盖 `PointXYZRGB` organized input；其它可编译点型没有独立板卡证据。 |
| fallback | 非 RVV 构建天然回标量；`width < 3 || height < 3` 保持只 resize / zero state，不执行 RVV 主循环。 |

## 验证计划

| gate | command |
| --- | --- |
| correctness | `make -C test-rvv/recognition/color_gradient_dot_modality run_test_compare` |
| QEMU smoke | `make -C test-rvv/recognition/color_gradient_dot_modality run_bench_rvv BENCH_ARGS="--case-filter process_input_320x240 --iterations 1 --warmup-iterations 1"` |
| asm | `make -C test-rvv/recognition/color_gradient_dot_modality check_cgdm_rvv_asm` |
| board production direct | `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make -C test-rvv/recognition/color_gradient_dot_modality board_repeated REPEATED_BOARD_TAG=phase010_production_direct REPEATED_BOARD_RUN_LABEL=cgdm_phase010_production_direct_repeated CGDM_REPEATED_BENCH_ARGS="--case-filter process_input_320x240,process_input_641x481_tail --iterations 20 --warmup-iterations 3"` |
| Evidence Doctor | `record_evidence_state_repeated` with `EVIDENCE_ROLE_REPEATED=production_direct` |

## 采纳条件

用户本轮明确授权：接入后板卡测试如果显示有收益即可采纳。若 production direct 的两个 `process_input_*`
case checksum 一致、Evidence Doctor 无 Errors / Warnings，且 median speedup 为 positive，则本阶段可写为
adopted production behavior（已采用生产行为）并创建正式 `doc-rvv` 文档。
