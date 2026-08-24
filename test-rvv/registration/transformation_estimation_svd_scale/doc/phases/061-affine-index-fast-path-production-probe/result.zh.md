# Phase 061 结果：affine index fast-path production probe

## 结果摘要

Phase 061 已把 Phase 060 的 contiguous offset fast path 从 test-only diagnostic（测试专用诊断）推进为 bounded production probe（有界生产探针）。生产补丁只在 `__RVV10__`、`Scalar=float`、dense、traits-gated xyz AoS、`nr_points >= 16` 且 step=1 contiguous indices / correspondences 命中时走 contiguous offset RVV accumulation；其它输入继续走既有 gather、correspondence sorted-copy 或父类 fallback。

当前证据支持该 production probe：correctness Std/RVV 各 23 tests 通过；QEMU smoke 6 comparisons，Evidence Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；board repeated 6/6 positive，median B/A 范围 `10.115x` 到 `14.021x`，Board Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。

本阶段到达 PI5 用户检查点后，用户已确认当前有收益实现可以接入。Phase 062 adoption closeout 将本 production patch 收口为 adopted production behavior；本文件保留 PI5 曾经需要人工确认的边界。

## 实际变更

| area | 变更 |
| --- | --- |
| production helper | 新增 contiguous index / correspondence range 检测，以及 contiguous offset source/target RVV accumulation。 |
| production dispatch | source-indexed、dual-indexed、correspondence 三个 public overload 在连续 offset 输入下优先走 contiguous fast path；未命中时保持原 row-source RVV / sorted-copy / fallback 行为。 |
| correctness | 新增 `AffineIndexFastPathPublicProbeMatchesReference`，覆盖三类 row source 的 public overload 与 selected-cloud reference 一致。 |
| bench | 新增 `row-source-affine-index-fast-path-production-probe` case-filter，Std/RVV public path 对比会计入 contiguous 检测成本。 |
| evidence target | 新增 `record_qemu_affine_index_fast_path_production_probe_state` 和 `run_board_bench_affine_index_fast_path_production_probe_repeated`。 |

## 验证命令和结果

| command | result |
| --- | --- |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded` | Std 23 tests passed；RVV 23 tests passed。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_affine_index_fast_path_production_probe_state` | QEMU smoke 6 comparisons；Doctor `0/0/0`；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_affine_index_fast_path_production_probe_repeated` | board 5-run repeated 完成；6/6 case positive；Doctor `0/0/0`。 |
| `python3 -m py_compile .../generate_tesvd_scale_board_repeated_summary.py .../generate_tesvd_scale_qemu_evidence_manifest.py .../generate_tesvd_scale_detail_ab_summary.py` | 通过。 |

## Board repeated 摘要

证据路径：

- `log/board/row_source_affine_index_fast_path_production_probe_repeated/summary.md`
- `log/board/row_source_affine_index_fast_path_production_probe_repeated/evidence_doctor.md`

| case | median B/A | min | max | bucket |
| --- | ---: | ---: | ---: | --- |
| source-indexed 64K | `14.021x` | `13.859x` | `14.314x` | positive |
| dual-indexed 64K | `10.616x` | `10.421x` | `10.632x` | positive |
| correspondence 64K | `11.564x` | `11.409x` | `11.684x` | positive |
| source-indexed 256K | `13.809x` | `13.695x` | `13.954x` | positive |
| dual-indexed 256K | `10.115x` | `9.813x` | `10.301x` | positive |
| correspondence 256K | `10.977x` | `10.807x` | `11.132x` | positive |

## EvidenceDecision

| question | decision |
| --- | --- |
| correctness | passed，Std/RVV 各 23 tests。 |
| QEMU role | smoke only，路径、label、manifest 和 Doctor clean；不作性能结论。 |
| board role | production-public probe evidence，真实 public overload 下比较 Std/RVV，包含 contiguous 检测成本。 |
| decision bucket | `positive_adopted_after_user_confirmation`。 |
| adopted 状态 | `adopted-by-user`；用户已确认当前有收益实现可以接入。 |

## 边界和风险

- 覆盖：`PointXYZ -> PointXYZ` / `Scalar=float` / dense xyz AoS / step=1 contiguous source indices、target indices 或 correspondences / 64K 与 256K board evidence。
- 不覆盖：stride、reverse、shuffle、非法 index / correspondence、非 dense、`Scalar=double`、全部自定义 layout、全部点型全集。
- source-indexed fast path 的 target 输入是 selected target cloud，所以 target offset 固定为 0；dual-indexed 和 correspondence 使用 source / target 两端 offset。
- QEMU smoke 的 speedup 表只用于 sanity check；性能方向只采 board summary。

## PI5 用户检查点和采纳结果

PI5 时建议用户检查以下内容后决定是否采纳；当前用户已确认采纳，因此这些条目转为 adoption closeout 的证据索引：

| item | path / command |
| --- | --- |
| production diff | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| correctness | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded` |
| QEMU smoke | `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_affine_index_fast_path_production_probe_state` |
| board repeated | `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_affine_index_fast_path_production_probe_repeated` |
| board summary | `log/board/row_source_affine_index_fast_path_production_probe_repeated/summary.md` |
| board doctor | `log/board/row_source_affine_index_fast_path_production_probe_repeated/evidence_doctor.md` |

Phase 062 已据此执行 adoption closeout：把该 fast path 写成 adopted production behavior，刷新 production 长期主题文档，并更新 matrix / roadmap 的状态。

## 下一步

默认下一步是 Phase 062 adoption closeout。完成后，当前优化矩阵中仍可继续的更广方向包括 `Scalar=double` 数值预算、更广 custom layout / alignment 取样和新的 row-source mitigation family；这些都需要独立 phase 目标和同边界证据，不能从本 contiguous fast path 直接外推。
