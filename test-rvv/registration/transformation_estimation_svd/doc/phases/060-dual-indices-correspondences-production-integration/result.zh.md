# Phase 060 结果：dual-indices / correspondences production integration

## 执行范围

本阶段把剩余两条 row source policy（行来源策略）接入真实 production（生产源码）路径：

- `dual-indices-cloud-pair`
- `correspondence-pair`

生产范围只覆盖 `Scalar=float`、`use_umeyama_ == true`、source/target dense、`n >= 16`、source/target 分别满足 `pcl::rvv::RVVXYZAoSFloatLayout` 的分流门控。dual-indices 还要求两侧 indices 等长且全部合法；correspondence 还要求 query/match 全部合法。`Scalar=double`、非 dense 输入、`use_umeyama_ == false` 和其它 out-of-scope gate 继续回退到原 `ConstCloudIterator` 标量路径。

历史 case-filter / 日志里的 `full-cloud` 仍只作为 legacy alias（历史别名）保留，正式文档继续使用 `ordered-cloud-pair`。

## 计划动作回填

| 动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| production helper | done | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` | 新增 `accumulateTransformationEstimationSVDDualIndicesCloudPairRVV`、`estimateRigidTransformationSVDDualIndicesCloudPairRVV`、`accumulateTransformationEstimationSVDCorrespondencePairRVV` 和 `estimateRigidTransformationSVDCorrespondencePairRVV`。 |
| public dispatch | done | 同上 | dual-indices 与 correspondence 公开入口在原 size / correspondence 检查后短路尝试 RVV helper，失败时自然进入 iterator 标量 fallback。 |
| production direct correctness | done | `src/test_tesvd.cpp`、`make -C test-rvv/registration/transformation_estimation_svd run_test_compare` | Std / RVV 各 22 个 gtest 通过；`ProductionDirectRVVAcceptsDualIndicesCloudPair` 与 `ProductionDirectRVVAcceptsCorrespondencePair` 覆盖 path-hit，`PointXYZI` / `PointXYZRGB` 代表性 xyz AoS 点型也通过。 |
| fallback correctness | done | 同上 | `ProductionDirectRVVRejectsDualIndicesOutOfScopeGates` 与 `ProductionDirectRVVRejectsCorrespondenceOutOfScopeGates` 覆盖小规模、非 dense、非法 index / correspondence、`use_umeyama_ == false` 和 `Scalar=double`。 |
| board correctness smoke | done | `make -C test-rvv/registration/transformation_estimation_svd run_board_test_smoke` | RVV board gtest 22/22 通过。 |
| production board repeated | done | `log/board/production_dual_indices_cloud_pair_repeated/summary.md`、`log/board/production_correspondence_pair_repeated/summary.md` | dual-indices public Std/RVV median 为 4K `6.805x`、64K `6.404x`、256K `5.964x`；correspondence public Std/RVV median 为 4K `8.649x`、64K `8.644x`、256K `7.872x`。两者 overall decision bucket 都为 `positive`。 |
| Evidence Doctor | done | `log/board/production_dual_indices_cloud_pair_repeated/evidence_doctor.md`、`log/board/production_correspondence_pair_repeated/evidence_doctor.md` | 两份报告都为 Errors=0、Warnings=1、Suggestions=0；warning 都是 256K 长尾 / 方差，需要按 size 分开解释。 |
| ASM attribution | done | `build/asm/riscv/bench_transformation_estimation_svd_rvv.full.asm` | dual-indices / correspondence 公开 overload 符号与预期 gather / FMA / reduction 指令簇一致；correspondence 路径的 `vlse32` base-pointer 修正已保留。 |
| evidence registry | done | `log/evidence_registry.json` | 两个 production direct board summary / manifest / doctor 已 record。 |

## EvidenceDecision

`dual-indices-cloud-pair` 和 `correspondence-pair` 都达到 `adopted / production-ready`。当前 production 结论已经覆盖四条 row source policy：

- ordered-cloud-pair
- source-indexed-cloud-pair
- dual-indices-cloud-pair
- correspondence-pair

## 证据解释

Correctness（正确性）：QEMU Std / RVV 各 22 个 gtest 通过，board smoke 22/22 通过。测试覆盖 public Umeyama 语义锚点、四条 row source 的 fused reference、production helper path-hit、fallback gate，以及 `PointXYZI` / `PointXYZRGB` 代表性 xyz AoS 布局。

Performance（性能）：dual-indices 与 correspondence 的 production direct repeated board 都是 positive。256K 组都出现 long-tail warning，但数值方向一致，没有翻转决策桶，因此只按 size 分开报告，不把单一规模的波动外推到其它规模或其它 row source。

ASM attribution（反汇编归因）：`dump_bench_rvv` 已刷新，dual-indices / correspondence public overload 符号内可见预期的 gather / load / FMA / reduction 指令簇。correspondence 路径仍保留 `vlse32` base-pointer 逻辑修正，不回退到旧写法。

## Optimization Matrix 更新

| candidate family | row source policy | 状态 | 证据 | next action |
| --- | --- | --- | --- | --- |
| `production_direct_dispatch_dual_indices` | dual-indices-cloud-pair | adopted / production-ready | correctness、ASM、board repeated、doctor 均闭合 | none |
| `production_direct_dispatch_correspondence` | correspondence-pair | adopted / production-ready | correctness、ASM、board repeated、doctor 均闭合 | none |
| `dual_indices_fused_accum` | dual-indices-cloud-pair | diagnostic adopted | Phase 050 正向；已完成 production 接入 | none |
| `correspondence_fused_accum` | correspondence-pair | diagnostic adopted | Phase 050 正向；已完成 production 接入 | none |

## Remaining Scope

- `Scalar=double` 继续 fallback，不进入当前 production 结论。
- non-dense 输入继续走标量路径。
- `PointXYZI` / `PointXYZRGB` 等 gate-allowed xyz AoS 点型已完成 correctness path-hit，但板卡性能仍只以代表性 `PointXYZ` 口径说明。

## 继续 / 停止决定

本阶段 production integration loop 已闭合。默认下一步是 topic-local docs、evaluation、matrix、roadmap 和长期 `doc-rvv` 的收尾刷新，不再把任何 row source 拉回当前 production 范围。
