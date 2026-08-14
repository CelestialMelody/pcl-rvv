# Phase 040 结果：source-indexed production integration

## 执行范围

本阶段把 `source-indexed-cloud-pair`（源索引点云对）接入真实 production（生产源码）路径。生产范围只包含：

- public entry（公开入口）：`estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)`。
- row semantics（行语义）：`source[indices_src[i]]` 与 `target[i]` 配对。
- gate（门控条件）：`Scalar=float`、`use_umeyama_ == true`、source/target dense、`indices_src.size() == cloud_tgt.size()`、`n >= 16`、source/target 分别满足 `RVVXYZAoSFloatLayout`，且每个 source index 非负并落在 source cloud 内，source cloud 大小满足 32-bit byte offset gather 边界。

本阶段没有接入 dual-indices、correspondences、`Scalar=double`、非 dense 输入或 `use_umeyama_ == false`。

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| production helper | done | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` | 新增 `accumulateTransformationEstimationSVDSourceIndexedCloudPairRVV` 和 `estimateRigidTransformationSVDSourceIndexedCloudPairRVV`，source 侧用 `indexed_load3_f32m2`，target 侧用 `strided_load3_f32m2`。 |
| public dispatch | done | 同上 | source-indexed overload 在原 size check 后短路尝试 RVV helper，失败时自然进入 `ConstCloudIterator` 标量 fallback。 |
| production direct correctness | done | `run_test_compare`，Std/RVV 各 12 tests | `ProductionDirectRVVAcceptsSourceIndexedCloudPair` 覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 的 path-hit 和矩阵误差预算。 |
| fallback correctness | done | `run_test_compare` | `ProductionDirectRVVRejectsSourceIndexedOutOfScopeGates` 覆盖小规模、非 dense、非法 index、`use_umeyama_ == false` 和 `Scalar=double`。 |
| QEMU smoke | done | `ALLOW_QEMU_BENCH_COMPARE=1 make ... run_bench_compare BENCH_ARGS="--case-filter source-indexed-cloud-pair --iterations 3 --warmup-iterations 1"` | 只证明 bench binary、case label 和日志形状；不作为性能结论。 |
| ASM attribution | done | `build/asm/riscv/bench_transformation_estimation_svd_rvv.full.asm` | source-indexed public overload 符号 `0x21fb6` 内可见 `vlsseg3e32.v`、`vluxseg3ei32.v`、`vfmacc.vv` 和 `vfredosum.vs`。 |
| board correctness | done | `run_board_test_smoke` | RVV board 12/12 gtest 通过。 |
| production board repeated | done | `log/board/production_source_indexed_cloud_pair_repeated/summary.md` | public Std/RVV median 为 4K `9.634x`、64K `12.217x`、256K `11.558x`，overall decision bucket 为 `positive`。 |
| Evidence Doctor | done | `log/board/production_source_indexed_cloud_pair_repeated/evidence_doctor.md` | Errors=0、Warnings=0、Suggestions=0。 |

## EvidenceDecision

`source-indexed-cloud-pair` 在上述 gate 下升级为 `adopted / production-ready`。这个结论只覆盖 dense source-indexed public overload、`Scalar=float`、xyz AoS layout-gated 点型和当前 32-bit byte offset gather 边界。未逐项上板的 `PointXYZI` / `PointXYZRGB` 等 gate-allowed 点型继承 correctness 与代表性性能判断，不能写成逐类型性能已证明。

## 仍未接入范围

- `dual-indices-cloud-pair` 仍保持标量，因为需要双 gather、两侧 index 合法性和独立板卡证据。
- `correspondence-pair` 仍保持标量，因为需要 query/match 语义、correspondence 边界和独立板卡证据。
- `Scalar=double` 明确 fallback；当前没有 double RVV 计划。
- 非 dense 或包含 NaN / Inf 的输入保持标量路径；当前 RVV gate 只覆盖 dense 输入。

## Evidence registry 和新鲜度

生产 source-indexed summary / manifest / doctor 已由 `record_board_production_source_indexed_cloud_pair_state` 登记到 `log/evidence_registry.json`。本 result、README、benchmark 文档、optimization evidence、evaluation 和长期 `doc-rvv` 引用这些路径后，`doc_ref_missing` 应消失；QEMU 文件若因本轮 rerun 发生 digest 变化，需要重新运行 `record_qemu_correctness_state` 和 `record_qemu_smoke_evidence_state`。

## 继续 / 停止决定

本阶段 production integration loop 已闭合。默认下一 phase 是 `050-dual-indices-correspondences-audit` 或等价 row-source phase，只评估 dual-indices / correspondences；不得把 ordered-cloud-pair 或 source-indexed 的 positive summary 外推成其它 row source 的 production 结论。
