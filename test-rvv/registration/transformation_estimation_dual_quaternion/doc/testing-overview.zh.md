# 测试体系总览

## 本文职责

本文说明 `transformation_estimation_dual_quaternion` topic 的测试类型、运行入口和证据边界。
当前生产提交候选只包含 `ordered-cloud-pair`、`source-indexed-cloud-pair`、
`dual-indexed-cloud-pair` 三类 retained RVV path；`correspondence-pair` production RVV 已移除。

## 运行入口分类

| target | 主测试类型 | 主要日志 / summary | 证据边界 |
| --- | --- | --- | --- |
| `make run_test_compare` | QEMU correctness | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | Std `28/28`、RVV `32/32`；验证 retained production path-hit、fallback 和诊断候选正确性，不证明性能。 |
| `make run_qemu_bench_smoke` | QEMU log-shape smoke | `log/qemu/run_bench_std.log`、`log/qemu/run_bench_rvv.log` | 默认 `production-public-retained-row-sources`；只证明 bench 输出、checksum 和 manifest 口径。 |
| `make run_qemu_smoke_evidence_doctor` | Evidence Doctor | `log/qemu/evidence_manifest.json`、`log/qemu/evidence_doctor.md` | 9 comparisons；Errors=0，Warnings=9，Suggestions=0；warnings 来自 no-warmup smoke。 |
| `make dump_bench_rvv` | asm smoke | `build/asm/riscv/bench_transformation_estimation_dual_quaternion_rvv.asm` | 证明 retained bench binary 有 RVV 指令；性能结论仍看 board。 |
| `make run_board_bench_production_public_repeated` | retained production board repeated | 三份 retained summary / doctor | `Milkv-Jupiter` 5-run；三类 retained public entry 均 positive，doctor clean。 |
| historical correspondence board targets | diagnostic / historical probe | `log/board/correspondence_*`、`production_public_correspondence_pair_repeated` | 只作为 correspondence 专项背景；不属于当前 production patch。 |

## 覆盖矩阵

| row source policy | 点型 / Scalar | correctness | QEMU smoke | board | production direct | 当前状态 |
| --- | --- | --- | --- | --- | --- | --- |
| ordered-cloud-pair | representative xyz AoS / `Scalar=float` | RVV path-hit pass；fallback pass | retained smoke | median `3.232x / 3.644x / 3.656x`；doctor `0/0/0` | retained | `retained_production_candidate` |
| source-indexed-cloud-pair | representative xyz AoS / `Scalar=float` / valid indices | RVV path-hit pass；fallback pass | retained smoke | median `2.540x / 2.631x / 2.586x`；doctor `0/0/0` | retained | `retained_production_candidate` |
| dual-indexed-cloud-pair | representative xyz AoS / `Scalar=float` / valid dual indices | RVV path-hit pass；fallback pass | retained smoke | median `2.015x / 1.808x / 2.021x`；doctor `0/0/0` | retained | `retained_production_candidate` |
| correspondence-pair | `PointXYZ` / `float` / correspondences | public scalar compatibility pass | historical only | Phase 015 64K / 256K negative；doctor `2/3/0` | scalar only | `removed_from_production` |
| indexed point-type layout diagnostics | `PointXYZI` / `PointXYZRGB` | diagnostic pass | diagnostic smoke | Phase 012 / 013 positive with warnings | not production direct | `diagnostic_positive_with_warnings` |

## 当前可提交证据

默认可 review 的 summary evidence（摘要证据）：

- `log/evidence_registry.json`
- `log/qemu/evidence_manifest.json`
- `log/qemu/evidence_doctor.md`
- `log/board/production_public_retained_row_sources_repeated/summary.md`
- `log/board/production_public_retained_row_sources_repeated/evidence_manifest.json`
- `log/board/production_public_retained_row_sources_repeated/evidence_doctor.md`
- `log/board/production_public_source_indexed_cloud_pair_repeated/summary.md`
- `log/board/production_public_source_indexed_cloud_pair_repeated/evidence_manifest.json`
- `log/board/production_public_source_indexed_cloud_pair_repeated/evidence_doctor.md`
- `log/board/production_public_dual_indexed_cloud_pair_repeated/summary.md`
- `log/board/production_public_dual_indexed_cloud_pair_repeated/evidence_manifest.json`
- `log/board/production_public_dual_indexed_cloud_pair_repeated/evidence_doctor.md`

`log/qemu/*.log`、`log/board/**/run-*/*.log` 和 `build/` 是 ignored-local / local-only 产物。

## 当前结论边界

QEMU correctness 证明代码路径和数值预算；QEMU smoke 不支持 speedup（加速比）排序。
board repeated 证明三类 retained production public entry 在当前合成 dense xyz AoS 输入上 positive。
这些结论不能外推到 `Scalar=double`、unsupported point layout、非 dense cloud、越界 indices、
真实 workload correspondence 分布或 correspondence production RVV。
