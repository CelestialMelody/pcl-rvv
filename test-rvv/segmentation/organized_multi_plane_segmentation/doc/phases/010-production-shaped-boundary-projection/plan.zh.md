# Phase 010: 生产形态 boundary/projection 诊断计划

## 阶段意图和边界

Phase 000 已证明 viewpoint projection（视点投影）在测试专用组件消融中为 `positive`，boundary gather（边界点云离散加载）为 `neutral`，`plane_d_dot` 为 `negative`。本阶段只把正向或近阈值的 boundary 末段放进 production-shaped diagnostic（生产形态诊断，尽量模拟真实生产入口末段但不修改 production 源码），验证 `segment` / `segmentAndRefine` 返回 `PlanarRegion` 前的循环边界：按 region 拿 boundary indices、复制 boundary cloud、在 `project_points=true` 时投影、再消费输出。

本阶段不修改 `segmentation/include/pcl/segmentation/impl/organized_multi_plane_segmentation.hpp`，不接入 production dispatch（生产分流），不复刻 CCL、refine、region fitting、`computeMeanAndCovarianceMatrix` 或 `eigen33`。阶段范围仍冻结为 `PointXYZ + Normal + Label`、`Scalar=float`、有组织连续输入、有效 boundary indices。

## 当前状态清单

| 项目 | 当前状态 | 证据 |
| --- | --- | --- |
| Phase 000 correctness | Std/RVV 三个组件对拍通过 | `make -C test-rvv/segmentation/organized_multi_plane_segmentation run_test_compare` |
| Phase 000 asm | 三个 RVV helper 均有归属符号和 RVV 指令 | `build/asm/riscv/bench_omps_rvv.asm` |
| Phase 000 board | `projection` median 1.34x positive；`boundary_gather` median 1.03x neutral；`plane_d_dot` median 0.81x negative | `log/board/repeated/summary.md` |
| Evidence Doctor | 1 Error 指向 `plane_d_dot` 5/5 退化；3 Warning / 1 Suggestion 约束结论边界 | `log/board/repeated/evidence_doctor.md` |

## 候选族和假设

| candidate family | 入口 / row source | 假设 | 风险 |
| --- | --- | --- | --- |
| `region_boundary_projection_rvv` | 多 region 的 boundary copy + optional projection | projection 的局部正向在更接近 `PlanarRegion` 输出末段的边界仍可见 | gather、每 region 临时 cloud、输出消费可能稀释收益 |
| `region_boundary_gather_only_rvv` | 多 region 的 boundary copy，不投影 | 若 `project_points=false`，gather 仍可能有弱收益 | Phase 000 已是 neutral，可能不值得生产接入 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `region_boundary_projection_rvv` | per-region valid boundary index rows | `PointXYZ / float / AoS` | production-shaped test helper | planned RED/GREEN `run_test_compare` | planned `bench_omps --case-filter region_projected` | planned 5 runs | `assembleRegionBoundariesRVV` plus callee projection/gather symbols | planned | planned |
| `region_boundary_gather_only_rvv` | per-region valid boundary index rows | `PointXYZ / float / AoS` | production-shaped test helper | planned `run_test_compare` | planned `bench_omps --case-filter region_gather_only` | planned 5 runs | `assembleRegionBoundariesRVV` plus gather symbol | planned | planned |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 RED test | `src/test_omps.cpp` | 新增生产形态 boundary/projection 对拍，首次运行因 RVV helper 未实现而失败 |
| A2 GREEN helper | `include/impl/omps_components.hpp` | Std/RVV helper 按相同 region 输入输出 checksum，一致性在误差预算内 |
| A3 bench 扩展 | `src/bench_omps.cpp` | 新增 `region_projected` 与 `region_gather_only` case；输出可被现有 compare parser 读取 |
| A4 QEMU / asm / board | `run_test_compare`、`dump_bench_rvv`、`run_board_omps_repeated` | correctness、反汇编、5-run board summary 和 Evidence Doctor 刷新 |
| A5 文档刷新 | phase result、matrix、roadmap、evaluation、Handoff | 记录证据边界、继续 / 停止条件和 production 接入判断 |

## Evidence Doctor、registry 和复跑规则

继续使用 `script/generate_omps_board_evidence_manifest.py` 生成 summary / manifest，再调用 `test-rvv/script/evidence_doctor.py`。Phase 010 会扩展 manifest case 字典，使 `region_projected` 与 `region_gather_only` 带上 production-shaped diagnostic 的 timer boundary（计时边界）和 row source（行来源）metadata。

板卡复跑预算为 5 runs；若新增 case 的 bucket 接近阈值、长尾明显或 Evidence Doctor 新增未解释 Error，最多追加一次同边界 5-run。桶规则沿用 Phase 000：`>=1.15` 且方向稳定为 `positive`，`1.05..1.15` 为 `weak_positive`，`0.95..1.05` 为 `neutral`，`<0.95` 为 `negative`，跨桶摇摆为 `unstable`。

## Diagnostic 到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic，不是 production direct |
| A/B boundary | test helper，模拟 production 末段 loop；不包含真实 `segment` 公开入口 |
| 当前决策问题 | projection / boundary 末段是否值得进入 PI1 生产接入计划 |
| diagnostic 是否可外推到 production | 只能部分外推；它覆盖 boundary 输出末段，不覆盖 CCL、refine、model fitting 和对象成员状态 |
| comparison-boundary / baseline mismatch 风险 | 有；region 输入是 synthetic（合成输入），boundary discovery 不在计时内 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 `region_projected` 稳定 positive，且 fallback / exact point type gate 可控时才允许 PI1 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要；本阶段不能 clean-adopt |

## Phase scope 与扩展队列

`validated_scope`：`PointXYZ`、`Scalar=float`、有效 boundary index rows、多 region production-shaped helper、`project_points=true/false`。

`unvalidated_scope`：真实 `segment` / `segmentAndRefine` public entry、`PointNormal`、`PointXYZINormal`、泛型 normal traits、`Scalar=double`、invalid indices、非连续 layout、真实 `PlanarRegion` allocator 细节和 production dispatch。

`point_type_expansion_queue`：仅当 Phase 010 的生产形态诊断稳定 positive 后，后续 phase 才考虑 `PointXYZ` exact production probe；泛型点型扩展必须另建 phase。

## 继续 / 停止条件

若 `region_projected` 在 repeated board 上稳定 positive，下一默认阶段是 `020-pi1-production-integration-plan`，只写 PI1 计划并冻结 exact `PointXYZ` / `project_points=true` 候选范围。若 `region_projected` 为 neutral / negative / unstable，进入未接 production 的 diagnostic closeout。若 Evidence Doctor Error 无法修复或板卡不可达，输出 blocked Handoff。

## 文档更新清单

本阶段更新 Phase 010 result、optimization matrix、optimization roadmap、evaluation 和 current Handoff。没有 adopted production behavior 前仍不创建 `doc-rvv/segmentation/organized_multi_plane_segmentation-RVV.zh.md`。
