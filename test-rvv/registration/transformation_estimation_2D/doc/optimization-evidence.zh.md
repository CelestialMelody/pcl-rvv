# 优化证据索引

## 本文职责

本文把 `transformation_estimation_2D` 的候选优化方式映射到测试支撑代码、target、证据路径和当前决策。它不替代 phase result，也不把 diagnostic evidence（诊断证据）升级成 production evidence（生产证据）。

## 当前结论摘要

Phase 010/020 建立了顺序点云对（ordered-cloud-pair，source/target 按相同下标一一对应）的 test-only fused 2D correlation candidate。候选从最初的 raw sums（原始和）公式调整为两遍中心化结构：先求 x/y 质心，再直接累加中心化后的 2x2 `H`。这样避免两份 4xN dynamic demean matrix（动态去中心化矩阵）和 Eigen correlation multiply，同时降低 near-cancellation（近抵消）风险。

Phase 050 已完成 production-public probe（生产公开入口探针）。QEMU smoke 和 asm attribution 证明路径可命中；最新 `Milkv-Jupiter` board production-public repeated 为 4K `4.222x`、64K `5.310x`、256K `4.947x`，Doctor `0/0/0`，当前 narrow production candidate 保留。Phase 030 随后在 test-rvv 范围完成三类 row-source materialize-to-ordered candidate 的 correctness、QEMU、asm 和板卡 repeated，但 Doctor 为 `1/2/6`，仍不进入 production。

## 优化方式总表

| 优化方式 | 代码路径 | target | 当前证据 | 决策 |
| --- | --- | --- | --- | --- |
| scalar public baseline | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` | `run_test_compare` | public ordered-cloud-pair、source-indexed、dual-indexed、correspondence valid scalar-boundary 通过；Std/RVV 16/16。 | current scalar truth |
| two-pass centered fused 2D correlation | `include/impl/te2d_candidates.hpp` | `run_test_candidates`、`run_board_bench_ordered_cloud_pair_repeated` | diagnostic correctness 通过；near-cancellation 样本通过；diagnostic board 5-run 为 `weak_positive`。 | retained diagnostic |
| production-public fused probe | retained narrow production patch | `run_qemu_production_public_evidence_doctor`、`run_board_bench_ordered_cloud_pair_public_repeated` | QEMU Doctor 0/0/0；asm public boundary present；最新 board public repeated 为 4.222x / 5.310x / 4.947x，Doctor 0/0/0。 | production-candidate-supported / user-review-pending |
| raw sums fused formula | 无保留实现 | 首次 RVV run 暴露 | near-cancellation 样本曾出现大误差，已改为中心化结构。 | rejected for Phase 010 |
| dense finite gate | `estimateFused2DCandidate` | `run_test_compare` | z 非有限输入触发 fallback，x/y 非有限 public 行为被记录。 | adopted in test-only diagnostic |
| materialize-to-ordered fused candidate | source-indexed-cloud-pair | `estimateFused2DSourceIndexedCandidate`、`run_board_bench_row_source_repeated` | Std/RVV 16/16；QEMU 3 cases；board median 1.073x / 1.023x / 1.038x；Doctor 1/2/6。 | attempted / diagnostic-only |
| materialize-to-ordered fused candidate | dual-indexed-cloud-pair | `estimateFused2DDualIndexedCandidate`、`run_board_bench_row_source_repeated` | Std/RVV 16/16；QEMU 3 cases；board median 1.081x / 1.014x / 1.038x，64K 出现 0.956x 长尾；Doctor 1/2/6。 | attempted / diagnostic-only |
| materialize-to-ordered fused candidate | correspondence-pair | `estimateFused2DCorrespondenceCandidate`、`run_board_bench_row_source_repeated` | Std/RVV 16/16；QEMU 3 cases；board median 1.067x / 1.013x / 1.035x，64K 为 2/5 退化；Doctor 1/2/6。 | attempted / diagnostic-only |
| production dispatch | retained narrow production patch | Phase 050 result | PI5 证据支持保留当前窄范围 production patch，等待用户审阅。 | production-candidate-supported / user-review-pending |

## 标量路径与 RVV 路径差异

| 阶段 | 当前 production | test-only candidate / probe |
| --- | --- | --- |
| centroid | `compute3DCentroid(ConstCloudIterator&)`，逐点 `pcl::isFinite`。 | dense finite gate 通过后，顺序扫描 x/y 两遍求质心。 |
| demean | 写出 source / target 两份 4xN dynamic matrix。 | 不写出矩阵，第二遍直接累加中心化后的 `H00/H01/H10/H11`。 |
| correlation | Eigen matrix multiply。 | RVV 构建用 `vlsseg3e32` load、`vfsub` 中心化、`vfmacc` 累加、`vfredosum` 规约。 |
| solve | `atan2`、`cos/sin` 和 translation 标量计算。 | 保持标量计算。 |
| fallback | 不满足 production gate 时继续当前公开入口标量路径。 | test-only candidate 和 production patch 均保留 fallback。 |

## 代码级证据索引

| 代码 | 证据角色 | 说明 |
| --- | --- | --- |
| `estimatePublic2D` | public semantic anchor（公开语义锚点） | 调用真实 `TransformationEstimation2D`。 |
| `estimateFused2DStd` | same-chain scalar reference（同构标量参考链路） | 两遍中心化累加，不使用 RVV。 |
| `estimateFused2DCandidate` | test-only RVV candidate | RVV 构建且 dense finite、规模不少于 16 时尝试 RVV。 |
| `accumulateFused2DRVV` | RVV hotspot candidate | 两遍 RVV：第一遍求质心，第二遍求中心化 2x2 correlation。 |
| `matrixChecksum` | bench smoke checksum | 检查输出路径和矩阵明显变化。 |

## 细粒度 target 字典

| target | 证明点 | 输出 |
| --- | --- | --- |
| `run_test_compare` | Std/RVV correctness 全量。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`。 |
| `run_test_public_semantics` | 公开入口语义子集。 | 覆盖 `TransformationEstimation2D.Public*`。 |
| `run_test_candidates` | fused candidate 子集。 | 覆盖 `TransformationEstimation2D.Fused*`。 |
| `run_bench_ordered_cloud_pair_smoke` | RVV diagnostic bench 可运行和日志形状。 | `log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log`。 |
| `run_bench_ordered_cloud_pair_public_smoke` | RVV public probe bench 可运行和日志形状。 | `log/qemu/production_public/run_bench_ordered_cloud_pair_public_rvv.log`。 |
| `generate_asm_attribution_summary` | 反汇编归因摘要，区分 candidate、production scalar、Eigen / stdlib 和 bench 边界。 | `log/qemu/asm_attribution.md`、`log/qemu/asm_attribution.json`。 |
| `run_qemu_production_public_evidence_doctor` | production-public QEMU manifest 和 Evidence Doctor。 | `log/qemu/production_public/evidence_manifest.json`、`log/qemu/production_public/evidence_doctor.md`。 |
| `run_board_bench_ordered_cloud_pair_repeated` | 5-run diagnostic board repeated summary 和 Evidence Doctor。 | `log/board/ordered_cloud_pair_repeated/summary.md`、manifest、doctor。 |
| `run_board_bench_ordered_cloud_pair_public_repeated` | 5-run production-public board repeated summary 和 Evidence Doctor。 | `log/board/ordered_cloud_pair_public_repeated/summary.md`、manifest、doctor。 |
| `run_bench_row_source_smoke` / `record_qemu_row_source_state` | 3 类 row source × 3 个规模的 QEMU smoke、asm、manifest 和 Doctor。 | `log/qemu/row_source/` 下的 summary-only 证据。 |
| `run_board_bench_row_source_repeated` / `record_board_row_source_state` | 5-run row-source board repeated summary 和 Evidence Doctor。 | 已生成 summary、manifest、Doctor；用于判断是否需要新的 gather/staging candidate。 |
| `evidence_status` | registry freshness。 | 检查本地生成的 registry 和文档引用。 |

## 当前可提交证据

可提交候选：`Makefile`、`board.mk`、`include/**`、`src/**`、`script/**`、`doc/**`、`README.zh.md`。

默认不提交：`build/**`、`log/**`。本阶段文档引用这些路径作为本地 evidence pointers（证据指针），不是提交白名单。

## 结论边界

当前只有窄范围 production candidate 的正向证据，尚未批准泛型或其它 row source：

- `log/qemu/production_public/asm_attribution.md` 显示 public overload 或 `runPublicCase` 内联边界有 `vlsseg3e32.v`、`vfmacc` 和 `vfredosum`。
- `log/board/ordered_cloud_pair_public_repeated/summary.md` 显示 public 4K / 64K / 256K median 分别为 `4.222x`、`5.310x`、`4.947x`，均为 `positive`。
- `log/board/ordered_cloud_pair_public_repeated/evidence_doctor.md` 为 Errors=0、Warnings=0、Suggestions=0。

因此 ordered-cloud-pair production patch 当前保留，等待用户审阅后决定是否进入提交/采用。source-indexed-cloud-pair、dual-indexed-cloud-pair、correspondence-pair、generic point type 和 `Scalar=double` 仍保持标量 production。
Phase 030 的三个 row-source candidate 仍是 test-only 诊断：materialize / gather 展开成本计时已纳入，板卡只显示弱收益且 64K 不稳定，没有手写 RVV gather kernel。下一步默认是 `060-production-candidate-review-and-row-source-boundaries`。
