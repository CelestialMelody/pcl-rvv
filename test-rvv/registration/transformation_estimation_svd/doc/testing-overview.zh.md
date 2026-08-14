# 测试体系总览

## 本文职责

本文说明 `transformation_estimation_svd` topic 当前 test-rvv 资产如何分层，以及每类证据能证明什么、不能证明什么。

## 测试类型定义

| 类型 | 当前状态 | 证明范围 | 不能证明 |
| --- | --- | --- | --- |
| correctness（正确性） | `run_test_compare` 已通过，Std / RVV 各 22 tests | 四条 row source 的 fused scalar reference、production direct helper 和 public Umeyama 语义接近；覆盖 `PointXYZI` / `PointXYZRGB` 代表布局和 fallback gate。 | 逐点型板卡性能。 |
| QEMU path / log shape（QEMU 路径 / 日志形状） | 窄 bench smoke 已运行 | 构建、运行、case label 和 checksum 输出格式可用。 | 真实性能。 |
| asm attribution（反汇编归因） | bench RVV dump 已刷新 | test-support helper 和 production public overload 符号内均可见 strided load、indexed gather、FMA（融合乘加）和 reduction（规约）相关指令。 | 不提供周期级 profile。 |
| board performance（板卡性能） | Phase 010 / 030 / 050 diagnostic positive；Phase 020 / 040 / 060 production direct positive | 目标硬件上四条 row source 的 test-only candidate 和真实 public dispatch 均为正向。 | 不能外推到逐点型性能或 `Scalar=double`。 |
| production direct（真实生产路径证据） | Phase 020 / 040 / 060 done / positive | 真实 production public overload 在 RVV build 下命中四条 row source 对应的 RVV helper，并有 fallback / representative layout tests。 | 未逐类型上板的 gate-allowed 点型不能写成逐类型性能已证明。 |

## 运行入口分类

| target | 后端 | 证据角色 | 输出 |
| --- | --- | --- | --- |
| `run_test_compare` | QEMU / local configured runner | Std / RVV correctness | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| `dump_bench_rvv` | 编译 + objdump | asm attribution 输入 | `build/asm/riscv/bench_transformation_estimation_svd_rvv.asm` |
| `ALLOW_QEMU_BENCH_COMPARE=1 make ... run_bench_compare BENCH_ARGS="--case-filter ordered-cloud-pair --iterations 3 --warmup-iterations 1"` | QEMU smoke | 只看构建和日志形状，不写性能结论。 |
| `ALLOW_QEMU_BENCH_COMPARE=1 make ... run_bench_compare BENCH_ARGS="--case-filter source-indexed-cloud-pair --iterations 3 --warmup-iterations 1"` | QEMU smoke | 只看 source-indexed 构建和日志形状，不写性能结论。 |
| `run_board_bench_ordered_cloud_pair_repeated` | board performance diagnostic | 5-run repeated summary / manifest / doctor；不是 production direct。`run_board_bench_fused_full_cloud_repeated` 仍可作为兼容入口。 |
| `run_board_bench_production_ordered_cloud_pair_repeated` | production direct board performance | 5-run repeated summary / manifest / doctor；只覆盖 public ordered-cloud-pair、`Scalar=float`、`PointXYZ` representative performance。 |
| `run_board_bench_source_indexed_cloud_pair_repeated` | board performance diagnostic | 5-run source-indexed diagnostic summary / manifest / doctor；不是 production direct。 |
| `run_board_bench_production_source_indexed_cloud_pair_repeated` | production direct board performance | 5-run source-indexed production summary / manifest / doctor；只覆盖 public source-indexed、`Scalar=float`、`PointXYZ` representative performance。 |
| `run_board_bench_dual_indices_cloud_pair_repeated` | board performance diagnostic | 5-run dual-indices diagnostic summary / manifest / doctor；不是 production direct。 |
| `run_board_bench_production_dual_indices_cloud_pair_repeated` | production direct board performance | 5-run dual-indices production summary / manifest / doctor；只覆盖 public dual-indices、`Scalar=float`、`PointXYZ` representative performance。 |
| `run_board_bench_correspondence_pair_repeated` | board performance diagnostic | 5-run correspondence diagnostic summary / manifest / doctor；不是 production direct。 |
| `run_board_bench_production_correspondence_pair_repeated` | production direct board performance | 5-run correspondence production summary / manifest / doctor；只覆盖 public correspondence、`Scalar=float`、`PointXYZ` representative performance。 |

## 覆盖矩阵

| scope | point type / Scalar | row source policy | correctness | bench | board | decision |
| --- | --- | --- | --- | --- | --- | --- |
| fused ordered-cloud-pair candidate | `PointXYZ` / `float` | ordered-cloud-pair | pass | QEMU smoke + board repeated | positive：same-boundary median `3.081x` / `3.179x` / `3.157x` | partial-production-candidate / PI1 candidate |
| production direct dispatch | `PointXYZ` / `float` / xyz AoS | ordered-cloud-pair | pass | production direct repeated | positive：public Std/RVV median 4K `14.372x`、64K `24.471x`、256K `23.841x` | adopted / production-ready |
| production direct dispatch | `PointXYZI`、`PointXYZRGB` / `float` / xyz AoS | ordered-cloud-pair | pass | not split | representative performance inherited from `PointXYZ` only | correctness adopted；逐类型性能未单独证明 |
| source-indexed fused candidate | `PointXYZ` / `float` | source-indexed-cloud-pair | pass | QEMU smoke + board repeated | positive：same-boundary median `1.917x` / `1.843x` / `1.785x` | adopted diagnostic / Phase 040 input |
| source-indexed production direct | `PointXYZ` / `float` / xyz AoS | source-indexed-cloud-pair | pass | production direct repeated | positive：public Std/RVV median 4K `9.634x`、64K `12.217x`、256K `11.558x` | adopted / production-ready |
| dual indices fused candidate | `PointXYZ` / `float` | dual-indices-cloud-pair | pass | QEMU smoke + board repeated | positive：same-boundary median `1.801x` / `1.683x` / `1.425x` | adopted diagnostic / Phase 060 input |
| dual-indices production direct | `PointXYZ` / `float` / xyz AoS | dual-indices-cloud-pair | pass | production direct repeated | positive：public Std/RVV median 4K `6.805x`、64K `6.404x`、256K `5.964x` | adopted / production-ready |
| correspondence fused candidate | `PointXYZ` / `float` | correspondence-pair | pass | QEMU smoke + board repeated | positive：same-boundary median `2.174x` / `1.905x` / `1.772x` | adopted diagnostic / Phase 060 input |
| correspondence production direct | `PointXYZ` / `float` / xyz AoS | correspondence-pair | pass | production direct repeated | positive：public Std/RVV median 4K `8.649x`、64K `8.644x`、256K `7.872x` | adopted / production-ready |

## 当前结论边界

Phase 020 / 040 / 060 的 positive result（正向结果）批准四条 row source 的 `Scalar=float`、xyz AoS layout-gated（布局门控）生产路径。`PointXYZI` / `PointXYZRGB` 等 gate-allowed 点型只继承 correctness 和代表性性能判断，不能写成逐类型性能已证明。QEMU timing 仍不能写成性能结论。
