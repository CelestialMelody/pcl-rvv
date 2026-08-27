# organized_multi_plane_segmentation 函数级评估

## S2 函数级评估

`OrganizedMultiPlaneSegmentation<PointT, PointNT, PointLT>` 的公开入口包括 `segment`、`segmentAndRefine` 和返回 `PlanarRegion` 的 overload（重载）。当前目标文件是 `segmentation/include/pcl/segmentation/impl/organized_multi_plane_segmentation.hpp`。

标量路径先检查 input cloud（输入点云）、normal cloud（法线点云）数量和 organized cloud（有组织点云）条件，然后按每点计算 `plane_d[i] = point dot normal`。`plane_d` 交给 `PlaneCoefficientComparator`，再由 `OrganizedConnectedComponentSegmentation` 生成 labels 和 label_indices。之后每个候选 region（区域）执行 `computeMeanAndCovarianceMatrix`、`eigen33`、curvature（曲率）判断和 model / inlier 输出。返回 `PlanarRegion` 的入口还会查 boundary indices（边界索引）、把输入点复制成 boundary cloud，并可选调用 `projectToPlaneFromViewpoint` 把边界点沿 viewpoint（视点）射线投到平面。

可 RVV 化片段是三个局部循环：`plane_d` point-normal dot、boundary gather copy、projection loop。暂不优先处理 CCL、region covariance、`eigen33` 和 refine label growth，因为这些阶段包含 union / comparator / per-region solver / 状态更新，局部 RVV 不能直接代表公开入口收益。

初步判断：`partial-preprocess` / diagnostic。当前没有 production patch，也没有 production direct 证据；本阶段只建立 test-only component ablation。若组件板卡结果为 positive 或 weak-positive，下一阶段应进入 production-shaped diagnostic，量化这些片段在接近公开入口的总耗时中是否可见。

本轮最终判断见下方 S11 Closeout：当前证据已经把该 topic 收束为 `bench-only/no-production`。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `OrganizedMultiPlaneSegmentation::segment` | production public entry | 计算 `plane_d`、连接组件、拟合平面 | 用户公开入口 | comparator、CCL、covariance、`eigen33` | production boundary；本阶段不修改 | `segmentation/include/pcl/segmentation/impl/organized_multi_plane_segmentation.hpp` |
| `projectToPlaneFromViewpoint` | production helper | 把 boundary cloud 沿视点投到平面 | `segmentAndRefine` region 输出 | `PlanarRegion` boundary points | projection 语义来源 | 同上 |
| `computePlaneDValuesStd/RVV` | diagnostic reference / candidate | 复刻 `plane_d` 局部循环 | `src/test_omps.cpp`、`src/bench_omps.cpp` | checksum / correctness / bench | test-only correctness 与组件性能 | `include/impl/omps_components.hpp` |
| `gatherBoundaryCloudStd/RVV` | diagnostic reference / candidate | 复刻 boundary copy 局部循环 | test / bench | projected boundary 或 checksum | test-only correctness 与组件性能 | `include/impl/omps_components.hpp` |
| `projectBoundaryFromViewpointStd/RVV` | diagnostic reference / candidate | 复刻投影公式 | test / bench | projected cloud | test-only correctness 与组件性能 | `include/impl/omps_components.hpp` |
| `assembleRegionBoundariesStd/RVV` | production-shaped diagnostic | 按多个 region 复制 boundary cloud，并可选投影 | test / bench | region boundary output checksum | 生产形态诊断；不是真实 production dispatch | `include/impl/omps_components.hpp` |
| `bench_omps` | bench wrapper | 输出组件耗时和 checksum | Makefile / board.mk | analyze script / Evidence Doctor | component ablation；QEMU timing 不作性能结论 | `src/bench_omps.cpp` |
| `generate_omps_board_evidence_manifest.py` | analysis script | 生成 summary / manifest，补 case metadata | board repeated logs | Evidence Doctor | 证据清单主入口 | `script/generate_omps_board_evidence_manifest.py` |
| Phase 010 result | documentation section | 回填 production-shaped diagnostic 证据 | worker / reviewer | matrix、roadmap、Handoff | 当前 EvidenceDecision 主归属 | `doc/phases/010-production-shaped-boundary-projection/result.zh.md` |
| Phase 000 plan | documentation section | 冻结本阶段范围和证据计划 | worker / reviewer | phase result、matrix、roadmap | recovery pointer（恢复入口） | `doc/phases/000-current-state-and-component-ablation/plan.zh.md` |

## 文档归属矩阵

| 信息类型 | 主归属 | 当前动作 |
| --- | --- | --- |
| 函数级评估、候选取舍和生产接入判断 | 本 evaluation 文档 | 当前文档维护 |
| 阶段计划、结果、Evidence Doctor 处理和继续 / 停止判断 | `doc/phases/000-current-state-and-component-ablation/`、`doc/phases/010-production-shaped-boundary-projection/` | 已回填 |
| 测试、bench 和证据字典 | `doc/testing-overview.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/correctness-tests.zh.md` | 已拆出 |
| 优化候选取舍 | `doc/optimization-evidence.zh.md`、`doc/phases/optimization-matrix.zh.md` | 已更新 |
| 跨阶段候选搜索空间 | `doc/optimization-roadmap.zh.md` | 已更新 no-production 恢复条件 |
| production 长期行为 | `doc-rvv/segmentation/organized_multi_plane_segmentation-RVV.zh.md` | not_applicable，没有 adopted production behavior |

## 需要证据才能改变的判断

若 `plane_d`、boundary gather 或 projection 在 repeated board（重复板卡测试）中稳定 positive，且 asm attribution（反汇编归属）能定位到对应 helper，当前判断可升级为 production-shaped diagnostic。若组件在板卡上 neutral / negative，或 Evidence Doctor 有未解释 error，当前 topic 应保留为未接 production 的诊断结论。任何 production 接入都需要后续 PI1-PI5，不能由本 evaluation 直接推出。

## S11 Closeout

当前 EvidenceDecision（证据决策）：`bench-only/no-production`。本 topic 不建议修改 `segmentation/include/pcl/segmentation/impl/organized_multi_plane_segmentation.hpp`。

Phase 000 的 component ablation（组件消融）显示：

| case | board summary | decision |
| --- | --- | --- |
| `plane_d_dot` | `log/board/repeated/summary.md`: median 0.81x | rejected |
| `boundary_gather` | 同上: median 1.03x | attempted, neutral |
| `projection` | 同上: median 1.34x | component positive only |

Phase 010 将局部 positive 的 projection 放入 production-shaped diagnostic（生产形态诊断）边界：

| case | board summary | decision |
| --- | --- | --- |
| `region_projected` | `log/board/phase010-region_projected/summary.md`: median 0.89x | rejected |
| `region_gather_only` | `log/board/phase010-region_gather_only/summary.md`: median 0.80x | rejected |

## 诊断证据链

Correctness（正确性）：`make -C test-rvv/segmentation/organized_multi_plane_segmentation run_test_compare` 中 Std/RVV 4 个测试通过。新增 RED/GREEN 测试证明 `assembleRegionBoundariesStd/RVV` 的 production-shaped helper 结果一致。

Asm attribution（反汇编归属）：`make -C test-rvv/segmentation/organized_multi_plane_segmentation dump_bench_rvv` 生成的 `build/asm/riscv/bench_omps_rvv.full.asm` 中可见 `assembleRegionBoundariesRVV` 调用 `gatherBoundaryCloudRVV` 和 `projectBoundaryFromViewpointRVV`；callee 内存在 `vluxei32.v`、`vfdiv.vv`、`vfmacc`、`vsse32.v`。

Board performance（板卡性能）：性能结论只来自 Milkv-Jupiter repeated board summary。Phase 010 两个生产形态 case 均 5/5 退化，Evidence Doctor 报 `ba_degradation_frequency` Error。处理方式是拒绝当前 helper family 的 production 接入，不把它写成 production evidence。

Boundary（证据边界）：当前证据不覆盖真实 `OrganizedMultiPlaneSegmentation` public entry、fallback、泛型点型、`Scalar=double`、invalid indices、非连续 layout、CCL、refine、region covariance 或 `eigen33`。但是它足以说明：当前 test-only RVV helper family 在更接近 boundary output 的诊断边界中不值得进入 PI1。

## 生产接入判断

`doc-rvv/segmentation/organized_multi_plane_segmentation-RVV.zh.md` 不适用，原因是没有 production patch、没有 PI5 production evidence，也没有用户确认采纳的 production 行为。若未来要重开，先需要真实 profile 证明 boundary/projection 末段占比显著，或提供能避免 per-region 临时 `PointCloud`、gather 写回和输出消费成本的新实现族。
