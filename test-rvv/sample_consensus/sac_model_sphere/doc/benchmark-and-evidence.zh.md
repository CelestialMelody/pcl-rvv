# sac_model_sphere benchmark 与证据

## 本文职责

本文说明 bench（性能测试）、summary、manifest（证据清单）、Evidence Doctor（证据体检）、asm attribution（反汇编归属）和提交边界。性能结论只来自 board 或目标硬件。

## Bench 输出格式

`src/bench_sac_model_sphere.cpp` 是 CLI 入口；Phase 055 后 bench harness 位于
`include/bench_sac_model_sphere.h`。输出 Dataset、Iterations、Warmup Iterations、Build、Checksum
和 bench item。默认点型是 `PointXYZ`；第三个参数可以选择 `PointXYZI`、`PointXYZRGB` 或
`PointXYZRGBA`，用于点类型扩展。

- `public selectWithinDistance`
- `public countWithinDistance`
- `public getDistancesToModel`
- `diagnostic candidate selectWithinDistance`
- `diagnostic candidate getDistancesToModel`

Checksum 当前为 aggregate entry output size checksum（聚合输出规模校验），用于确认同一输入下 Std/RVV 输出规模一致；它不是完整数值误差证明，correctness 仍由 gtest 承担。

## 计时边界

点云、indices、模型系数和 threshold 构造不计入单个 bench item。计时边界只包含目标 public entry 或测试专用 candidate 函数调用。select candidate 中 RVV 计算平方距离，仍用标量 `sqrt` 和 `push_back` 写出有序 inliers / error distances；getDistances candidate 中 RVV 写 scratch squared distances 后仍逐点标量 `sqrt` 写 double 输出。

## 当前 board 证据

当前工作区的 production `vcompress` patch 使用 Phase 045 的 production repeated manifest
（生产重复证据清单）：
`doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-manifest.json`。
`PointXYZI` 点类型扩展使用 Phase 050 manifest：
`doc/phases/050-point-type-expansion/point-type-repeated-evidence-manifest.json`。
RGB/RGBA 点类型扩展使用 Phase 060 manifest：
`doc/phases/060-point-type-rgb-rgba-expansion/rgb-point-type-repeated-evidence-manifest.json` 和
`doc/phases/060-point-type-rgb-rgba-expansion/rgba-point-type-repeated-evidence-manifest.json`。
Phase 020 manifest 是上一版 adopted baseline，可作为回滚或历史对照。
历史 Phase 000 诊断 manifest 只作为接入前证据，不再作为 `selectWithinDistance` 采纳主依据。

| case | median | min | max | Evidence Doctor | 解释 |
| --- | ---: | ---: | ---: | --- | --- |
| public `selectWithinDistance` `PointXYZ` | `2.0989x` | `2.0867x` | `2.1404x` | clean for this row | Phase 045/046 后真实 public entry 命中已采纳的 `vcompress` production patch。 |
| public `selectWithinDistance` `PointXYZI` | `1.5901x` | `1.4672x` | `1.6317x` | clean for this row | Phase 050 证明 `PointXYZI` 在当前 production gate 下仍为正向。 |
| public `selectWithinDistance` `PointXYZRGB` | `1.6225x` | `1.6181x` | `1.6331x` | clean for this row | Phase 060 本次重跑 5/5 正向。 |
| public `selectWithinDistance` `PointXYZRGBA` | `1.5528x` | `0.9184x` | `1.6354x` | Warning | Phase 060 median 正向，但 1/5 run 退化且存在长尾 warning。 |
| public `countWithinDistance` | `3.5154x` | `3.5121x` | `3.5201x` | clean for this row | 已有 production RVV path 在 `PointXYZ` 回归仍稳定正向。 |
| public `getDistancesToModel` | `1.0004x` | `0.9756x` | `1.0038x` | Error | 当前 public entry 仍为标量；Std/RVV 构建对比不稳定，不作为 RVV 采纳证据。 |
| diagnostic select candidate | `1.4780x` | `1.4746x` | `1.5250x` | no candidate Error | 历史诊断证据，已被 production direct select 证据取代。 |
| diagnostic getDistances candidate | `0.7723x` | `0.7578x` | `0.7845x` | Error | 当前候选实现族 rejected。 |

## Evidence Doctor / Manifest 边界

当前 Phase 045 production evidence（生产证据）输入是
`doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-manifest.json`，报告是
`doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-doctor.md` 和
`production-vcompress-repeated-evidence-doctor.json`。Phase 020 production baseline 仍保留在
`doc/phases/020-select-production-integration-plan/production-repeated-evidence-manifest.json`。
历史诊断输入仍保留在
`doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-manifest.json`。早期
`doc/phases/000-sphere-select-distance-diagnostic/evidence-doctor.md` 没有 comparisons，只能作为历史辅助检查。

## Evidence Registry 引用清单

本清单让 `production_vcompress_evidence_status` 能确认 summary evidence（摘要证据）仍被文档引用。
Phase 000/020/040 是历史证据，Phase 045 是当前工作区 production patch 证据。

| phase | role | registered files |
| --- | --- | --- |
| Phase 000 | production-shaped diagnostic | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-manifest.json`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-doctor.md`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-doctor.json` |
| Phase 020 | production baseline | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/020-select-production-integration-plan/production-repeated-evidence-manifest.json`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/020-select-production-integration-plan/production-repeated-evidence-doctor.md`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/020-select-production-integration-plan/production-repeated-evidence-doctor.json` |
| Phase 040 | production-detail A/B diagnostic | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/040-select-vcompress-ablation/vcompress-repeated-evidence-manifest.json`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/040-select-vcompress-ablation/vcompress-repeated-evidence-doctor.md`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/040-select-vcompress-ablation/vcompress-repeated-evidence-doctor.json` |
| Phase 045 | production direct adopted by Phase 046 | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-manifest.json`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-doctor.md`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-doctor.json` |
| Phase 050 | production direct PointXYZI | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/050-point-type-expansion/point-type-repeated-evidence-manifest.json`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/050-point-type-expansion/point-type-repeated-evidence-doctor.md`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/050-point-type-expansion/point-type-repeated-evidence-doctor.json` |
| Phase 060 | production direct RGB/RGBA | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/060-point-type-rgb-rgba-expansion/rgb-point-type-repeated-evidence-manifest.json`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/060-point-type-rgb-rgba-expansion/rgb-point-type-repeated-evidence-doctor.md`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/060-point-type-rgb-rgba-expansion/rgb-point-type-repeated-evidence-doctor.json`；`test-rvv/sample_consensus/sac_model_sphere/doc/phases/060-point-type-rgb-rgba-expansion/rgba-point-type-repeated-evidence-manifest.json`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/060-point-type-rgb-rgba-expansion/rgba-point-type-repeated-evidence-doctor.md`、`test-rvv/sample_consensus/sac_model_sphere/doc/phases/060-point-type-rgb-rgba-expansion/rgba-point-type-repeated-evidence-doctor.json` |

Manifest 记录 `taskset`、`governor`、`freq`、`temperature` 和 `binary_hash` 为 `not_recorded`。这些缺失不推翻当前 direction（方向）稳定的 count/select candidate 结论，但限制对长尾和跨批次复现的解释。

## ASM Attribution 口径

`countWithinDistanceRVV` 和 `selectWithinDistanceRVV` 在 `build/asm/riscv/bench_sac_model_sphere_rvv.full.asm`
中有符号级 RVV 指令归属，当前计数分别为 `19` 和 `27`；`selectWithinDistanceRVV` 符号中可见
`vcompress.vm`。测试专用 select/getDistances candidate
可能被内联，manifest 对候选符号计数为 `0`；文档只能写“内联区域可见 RVV gather/FMA/store 指令”，不能写成候选 hot symbol attribution 已闭合。

## 复现命令和提交边界

```bash
make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare
make -C test-rvv/sample_consensus/sac_model_sphere dump_bench_rvv
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere board_smoke
make -C test-rvv/sample_consensus/sac_model_sphere generate_board_evidence_manifest
python3 test-rvv/script/evidence_doctor.py --manifest test-rvv/sample_consensus/sac_model_sphere/doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-manifest.json --output test-rvv/sample_consensus/sac_model_sphere/doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-doctor.md --fail-on never
make -C test-rvv/sample_consensus/sac_model_sphere record_repeated_board_evidence_state repeated_evidence_status
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_production_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_sphere generate_production_board_evidence_manifest
make -C test-rvv/sample_consensus/sac_model_sphere run_production_board_evidence_doctor
make -C test-rvv/sample_consensus/sac_model_sphere record_production_board_evidence_state production_evidence_status
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_production_vcompress_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_sphere record_production_vcompress_board_evidence_state production_vcompress_evidence_status
make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZI'
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_point_type_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_sphere record_point_type_board_evidence_state point_type_evidence_status
make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGB'
make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGBA'
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_rgb_point_type_repeated_board_evidence
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_rgba_point_type_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_sphere record_rgb_point_type_board_evidence_state rgb_point_type_evidence_status
make -C test-rvv/sample_consensus/sac_model_sphere record_rgba_point_type_board_evidence_state rgba_point_type_evidence_status
```

提交候选只包含 summary evidence、`log/evidence_registry.json`、topic-local 文档和适用的
`doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md`；raw board logs、`build/` 和私有远端信息默认排除。
