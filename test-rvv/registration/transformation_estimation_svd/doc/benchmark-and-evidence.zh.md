# Benchmark 与证据说明

## 本文职责

本文记录 `src/bench_tesvd.cpp` 的输出合同、case-filter、计时边界和证据提交边界。

## Bench 输出格式

bench 输出包含：

- `Dataset:`：说明 synthetic dense ordered-cloud-pair pairs。当前 bench 输出里的 `full-cloud` 仅保留为 legacy label，正式 row-source policy 名称是 ordered-cloud-pair。
- `Indexed Dataset:`：说明 synthetic dense source-indexed-cloud-pair pairs。
- `Dual-Indexed Dataset:`：说明 synthetic dense dual-indices-cloud-pair pairs。
- `Correspondence Dataset:`：说明 synthetic dense correspondence-pair pairs。
- `Iterations:` / `Warmup Iterations:`：计时参数。
- `<case>: <ms> ms/iter`：`analyze_bench_compare.py` 可解析的 case 行。
- `Total Time:` 和 `checksum:`：Evidence Doctor（证据体检）和 freshness（证据新鲜度）检查使用。

## CLI 参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `--iterations` | `20` | 计时循环次数。 |
| `--warmup-iterations` | `3` | warm-up（预热）次数。 |
| `--case-filter` | all | `public-umeyama`、`ordered-cloud-pair`、`source-indexed-cloud-pair`、`dual-indices-cloud-pair`、`correspondence-pair` 或 `all`。旧 `fused-full-cloud` 仍作为兼容别名可用。 |

## Bench Label / case-filter 字典

| case-filter | label | 计时边界 |
| --- | --- | --- |
| `public-umeyama` | `public Umeyama ordered-cloud-pair 4K/64K/256K`、`public Umeyama source-indexed-cloud-pair 4K/64K/256K`、`public Umeyama dual-indices-cloud-pair 4K/64K/256K`、`public Umeyama correspondence-pair 4K/64K/256K` | production public ordered / indexed / dual / correspondence paths。Std build 是原标量动态矩阵路径；RVV build 满足 gate 时命中 production RVV helper。 |
| `ordered-cloud-pair` | `fused accum candidate ordered-cloud-pair 4K/64K/256K` | test-only fused ordered-cloud-pair candidate，包括 fused accumulation 和 Eigen 3x3 SVD；`fused-full-cloud` 是兼容别名。 |
| `source-indexed-cloud-pair` | `public Umeyama source-indexed-cloud-pair 4K/64K/256K`、`fused accum candidate source-indexed-cloud-pair 4K/64K/256K` | source-indexed public path 与 test-only fused source-indexed candidate；production summary 只使用 public source-indexed label。 |
| `dual-indices-cloud-pair` | `public Umeyama dual-indices-cloud-pair 4K/64K/256K`、`fused accum candidate dual-indices-cloud-pair 4K/64K/256K` | dual-indices public path 与 test-only fused dual-indices candidate；production summary 只使用 public dual-indices label。 |
| `correspondence-pair` | `public Umeyama correspondence-pair 4K/64K/256K`、`fused accum candidate correspondence-pair 4K/64K/256K` | correspondence public path 与 test-only fused correspondence candidate；production summary 只使用 public correspondence label。 |

## 推荐 Target

| target | 用途 | 说明 |
| --- | --- | --- |
| `run_test_compare` | correctness | 首选，先跑。 |
| `dump_bench_rvv` | asm attribution | 检查 candidate RVV 指令。 |
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

## Checksum 来源

checksum 来自 4x4 matrix 按 `1e6` 缩放后的 FNV-like 混合值，并额外混入 `used_rvv` 标志。它用于发现路径或结果变化，不替代矩阵误差测试。

## 当前 QEMU 证据

`run_test_compare` 当前通过 Std / RVV 各 22 tests。窄 QEMU bench smoke 仍使用 `--case-filter source-indexed-cloud-pair --iterations 3 --warmup-iterations 1`，输出见：

- `test-rvv/registration/transformation_estimation_svd/log/qemu/run_test_std.log`
- `test-rvv/registration/transformation_estimation_svd/log/qemu/run_test_rvv.log`
- `test-rvv/registration/transformation_estimation_svd/log/qemu/run_bench_std.log`
- `test-rvv/registration/transformation_estimation_svd/log/qemu/run_bench_rvv.log`
- `test-rvv/registration/transformation_estimation_svd/log/qemu/analyze_bench_compare.log`

该 smoke 只证明 bench binary 可运行、case label 和 checksum 格式可解析；QEMU timing 不作为真实性能结论，只作为上板前的日志形状和路径风险信号。

## 当前 Board 证据

Phase 010 已完成 Milkv-Jupiter 板卡 5-run repeated ordered-cloud-pair diagnostic：

- `test-rvv/registration/transformation_estimation_svd/log/board/test_smoke/run_test.log`：board RVV correctness smoke，8 个 gtest 通过。
- `test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/summary.md`：board repeated summary。
- `test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/evidence_manifest.json`：board manifest。
- `test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/evidence_doctor.md`：board Evidence Doctor。

same-boundary fused Std/RVV（同边界 fused 标量 / RVV）median 为 `3.081x` / `3.179x` / `3.157x`，decision bucket 为 `positive`。mixed-boundary public baseline vs fused RVV（混合边界公开基线与 fused RVV）median 为 `16.174x` / `26.631x` / `26.362x`，只用于判断是否值得进入 PI1，不是 production direct。

Phase 020 已完成 ordered-cloud-pair production direct repeated board：

- `test-rvv/registration/transformation_estimation_svd/log/board/production_ordered_cloud_pair_repeated/summary.md`：production direct board summary。
- `test-rvv/registration/transformation_estimation_svd/log/board/production_ordered_cloud_pair_repeated/evidence_manifest.json`：production direct manifest。
- `test-rvv/registration/transformation_estimation_svd/log/board/production_ordered_cloud_pair_repeated/evidence_doctor.md`：production direct Evidence Doctor。

production direct public Std/RVV median 为 4K `14.372x`、64K `24.471x`、256K `23.841x`，decision bucket 为 `positive`。该性能结论只覆盖 `PointXYZ` 代表性点型；`PointXYZI` / `PointXYZRGB` 等 gate-allowed xyz AoS 点型已有 correctness 证据，但未逐类型上板。

Phase 030 已完成 source-indexed board diagnostic：

- `test-rvv/registration/transformation_estimation_svd/log/board/source_indexed_cloud_pair_repeated/summary.md`：source-indexed board repeated summary。
- `test-rvv/registration/transformation_estimation_svd/log/board/source_indexed_cloud_pair_repeated/evidence_manifest.json`：source-indexed board manifest。
- `test-rvv/registration/transformation_estimation_svd/log/board/source_indexed_cloud_pair_repeated/evidence_doctor.md`：source-indexed board Evidence Doctor。

source-indexed same-boundary fused Std/RVV median 为 4K `1.917x`、64K `1.843x`、256K `1.785x`，decision bucket 为 `positive`。mixed-boundary public baseline vs fused RVV median 为 4K `7.012x`、64K `9.031x`、256K `8.827x`，只用于判断是否值得进入 Phase 040，不是 production direct。

Phase 040 已完成 source-indexed production direct repeated board：

- `test-rvv/registration/transformation_estimation_svd/log/board/production_source_indexed_cloud_pair_repeated/summary.md`：source-indexed production direct board summary。
- `test-rvv/registration/transformation_estimation_svd/log/board/production_source_indexed_cloud_pair_repeated/evidence_manifest.json`：source-indexed production direct manifest。
- `test-rvv/registration/transformation_estimation_svd/log/board/production_source_indexed_cloud_pair_repeated/evidence_doctor.md`：source-indexed production direct Evidence Doctor。

source-indexed production direct public Std/RVV median 为 4K `9.634x`、64K `12.217x`、256K `11.558x`，decision bucket 为 `positive`。该性能结论只覆盖 `PointXYZ` 代表性点型；`PointXYZI` / `PointXYZRGB` 等 gate-allowed xyz AoS 点型已有 correctness 证据，但未逐类型上板。

Phase 050 已完成 dual-indices / correspondence diagnostic：

- `test-rvv/registration/transformation_estimation_svd/log/board/dual_indices_cloud_pair_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_svd/log/board/correspondence_pair_repeated/summary.md`

dual-indices same-boundary fused Std/RVV median 为 4K `1.801x`、64K `1.683x`、256K `1.425x`，decision bucket 为 `positive`。correspondence same-boundary fused Std/RVV median 为 4K `2.174x`、64K `1.905x`、256K `1.772x`，decision bucket 为 `positive`。这两条只是 Phase 060 的输入，不是 production direct。

Phase 060 已完成 dual-indices / correspondence production direct repeated board：

- `test-rvv/registration/transformation_estimation_svd/log/board/production_dual_indices_cloud_pair_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_svd/log/board/production_dual_indices_cloud_pair_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_svd/log/board/production_dual_indices_cloud_pair_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_svd/log/board/production_correspondence_pair_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_svd/log/board/production_correspondence_pair_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_svd/log/board/production_correspondence_pair_repeated/evidence_doctor.md`

dual-indices production direct public Std/RVV median 为 4K `6.805x`、64K `6.404x`、256K `5.964x`，decision bucket 为 `positive`。correspondence production direct public Std/RVV median 为 4K `8.649x`、64K `8.644x`、256K `7.872x`，decision bucket 为 `positive`。

## Evidence Doctor / Manifest 边界

当前已有 topic-bound QEMU smoke manifest 和 Evidence Doctor 报告：

- `test-rvv/registration/transformation_estimation_svd/log/qemu/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_svd/log/qemu/evidence_doctor.md`

QEMU smoke doctor 结果为 Errors=0、Warnings=0、Suggestions=0。Manifest 中有意不提供 `ba_values`（可被当成性能分布的 B/A 值），因为 QEMU timing 只用于日志形状。

Phase 010 board doctor 结果为 Errors=0、Warnings=4、Suggestions=0。3 个 `production_name_without_role` warning 来自 public build sanity 行名称中包含 production，但 manifest 已把它们标成 diagnostic sanity；1 个 `group_outlier` warning 来自 4K mixed-boundary median 低于 64K / 256K，结论中按 size 分开报告，不把大规模收益外推到小规模。

Phase 020 production direct doctor 结果为 Errors=0、Warnings=1、Suggestions=0。唯一 `group_outlier` warning 来自 4K median `14.372x` 低于 64K / 256K 组内 median；处理策略是按规模分别报告，不把大规模收益外推到 4K，也不扩大到其它 row source。

Phase 030 source-indexed diagnostic doctor 结果为 Errors=0、Warnings=4、Suggestions=0。3 个名称 warning 来自 diagnostic summary 的 public build sanity 行，已明确为 diagnostic sanity；1 个 4K mixed-boundary outlier 按 size 分开报告。

Phase 040 source-indexed production direct doctor 结果为 Errors=0、Warnings=0、Suggestions=0。

Phase 050 diagnostic doctor 结果分别为 Errors=0、Warnings=3、Suggestions=0 和 Errors=0、Warnings=7、Suggestions=0。warning 主要是 public build sanity 命名角色和 long-tail 方差。

Phase 060 production direct doctor 结果分别为 Errors=0、Warnings=1、Suggestions=0 和 Errors=0、Warnings=1、Suggestions=0。两个 warning 都是 256K 长尾，需要按 size 分开报告，不把单一规模的波动外推成其它 row source 结论。

## ASM Attribution 口径

`dump_bench_rvv` 生成 bench binary 反汇编。Phase 020 production symbol attribution（生产符号归因）已闭合到 ordered-cloud-pair public overload，符号内可见 `vlsseg3e32.v`、`vfadd.vv`、`vfmacc.vv` 和 `vfredosum.vs`。Phase 040 source-indexed public overload 符号 `0x21fb6` 内可见 `vlsseg3e32.v`、`vluxseg3ei32.v`、`vfmacc.vv` 和 `vfredosum.vs`。Phase 060 dual-indices / correspondence public overload 符号内仍可见预期的 gather / FMA / reduction 指令簇，correspondence 路径保留 `vlse32` base-pointer 修正。

## 提交边界

summary-only 默认策略：被 README、evaluation、phase result 或 Handoff 明确引用的 summary / doctor / registry 可进入审查候选。当前 `.gitignore` 只自然暴露 QEMU `evidence_doctor.md` 与 Phase 010 board `summary.md` / `evidence_doctor.md`；registry、manifest 和 raw logs 是 ignored-local，提交阶段若要保留需单独 allowlist / force-add、脱敏审查并说明边界。raw logs、build output、本机 config 和私有板卡路径默认不提交。
