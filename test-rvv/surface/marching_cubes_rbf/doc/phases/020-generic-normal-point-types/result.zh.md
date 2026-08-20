# 020 generic normal point types 扩展结果

## 结果摘要

本阶段已把 `MarchingCubesRBF<PointNT>::voxelizeData()` 的 production RVV gate（生产 RVV 门控）
从 exact `pcl::PointNormal` 扩展为 `pcl::rvv::RVVXYZNormalFloatLayout<PointNT>::value`。
当前 adopted production behavior（已采用生产行为）覆盖：

- `__RVV10__` 构建；
- `input_->size() >= 16`；
- 满足 PCL traits（点类型字段特征）注册的 xyz + normal 单 `float` AoS layout（结构数组布局）；
- 本阶段实测代表点型：`pcl::PointNormal`、`pcl::PointXYZINormal`、`pcl::PointXYZRGBNormal`。

`EvidenceDecision`: `adopted_production_behavior`

采用理由：三个已测代表点型的 production direct（真实生产路径）正确性通过，板卡 Std/RVV 对比均为正向，
新增点型为 `1.06x-1.08x`。收益仍是 weak-positive（弱正向），但实现只替换为公共 traits gate，
fallback 简单，public API（公开接口）不变，符合用户“板卡显示有收益即可接入”的本轮授权。

## 实际执行范围

| 维度 | 计划范围 | 实际结果 |
| --- | --- | --- |
| production entry | `MarchingCubesRBF<PointNT>::voxelizeData()` | 已完成。 |
| 点类型 | `PointXYZINormal`、`PointXYZRGBNormal`，保留 `PointNormal` 回归 | 已完成。 |
| gate | `RVVXYZNormalFloatLayout<PointNT>` + `input_->size() >= 16` | 已完成。 |
| fallback | 非 RVV、小规模、traits 不成立 | 小规模 fallback 已测；非 RVV 构建由 Std target 覆盖；traits 不成立保持编译期返回 false。 |
| 不证明范围 | 所有自定义 normal-like 点型、Eigen solve、surface emission | 保持不证明。 |

## 计划动作回填

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| production 头引入 `pcl/rvv_point_traits.h` 并改用 `RVVXYZNormalFloatLayout<PointNT>` | done | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` |
| 保留 `input.size() < 16` runtime fallback | done | `SmallPointNormalInputUsesStableFallback` |
| 测试支撑初始化 `PointXYZINormal` / `PointXYZRGBNormal` 额外字段 | done | `test-rvv/surface/marching_cubes_rbf/include/impl/marching_cubes_rbf_core.hpp` |
| 新增 production direct gtest | done | QEMU 和板卡均 7/7 通过 |
| 新增 production direct bench case | done | `mcrbf_prod_pointxyzinormal_*`、`mcrbf_prod_pointxyzrgbnormal_*` |
| manifest 标注新增点型 | done | `log/board/evidence_manifest.json` 中 point type 已区分 |
| QEMU correctness / smoke / asm / board / Evidence Doctor | done | 见下方证据链 |
| 文档同步 | done | 本 result、phase index、matrix、roadmap、evaluation、`doc-rvv` 均已刷新 |

## 正确性、反汇编和板卡证据

| 类别 | 命令 / 路径 | 结果 |
| --- | --- | --- |
| QEMU correctness（QEMU 正确性） | `make -C test-rvv/surface/marching_cubes_rbf run_test_compare` | Std/RVV 各 7/7 通过。 |
| QEMU bench smoke（QEMU 冒烟） | `run_bench_rvv --case-filter mcrbf_prod_pointxyzinormal_n24_r18 --iterations 1 --warmup-iterations 0` | 可运行并输出 checksum；不作为性能结论。 |
| asm attribution（反汇编归属） | `make -C test-rvv/surface/marching_cubes_rbf dump_bench_rvv` + `nm -C` | `PointNormal`、`PointXYZINormal`、`PointXYZRGBNormal` 三个 RVV helper 实例存在；可见 `vfmacc/vfredusum/vfsqrt/vle64/vse64`。 |
| board correctness（板卡正确性） | `make -C test-rvv/surface/marching_cubes_rbf run_board_test fetch_board_logs` | RVV gtest 7/7 通过。 |
| board production bench（板卡生产路径性能） | `make -C test-rvv/surface/marching_cubes_rbf run_board_production_smoke fetch_board_logs run_board_evidence_doctor` | 新增点型 production direct 均正向，checksum 一致。 |
| Evidence Doctor（证据体检） | `test-rvv/surface/marching_cubes_rbf/log/board/evidence_doctor.md` | `Errors=0, Warnings=12, Suggestions=0`；warning 均为 `low_run_count`。 |

## 板卡 production direct 结果

| case | point type | Std ms | RVV ms | speedup | 结论 |
| --- | --- | ---: | ---: | ---: | --- |
| `mcrbf_prod_pointnormal_n24_r18` | `pcl::PointNormal` | 4.7078 | 4.3612 | 1.08x | regression positive |
| `mcrbf_prod_pointnormal_n40_r20` | `pcl::PointNormal` | 11.4994 | 10.7491 | 1.07x | regression positive |
| `mcrbf_prod_pointnormal_n56_r18` | `pcl::PointNormal` | 15.1841 | 14.3598 | 1.06x | regression positive |
| `mcrbf_prod_pointxyzinormal_n24_r18` | `pcl::PointXYZINormal` | 4.7060 | 4.3499 | 1.08x | adopted |
| `mcrbf_prod_pointxyzinormal_n40_r20` | `pcl::PointXYZINormal` | 11.4824 | 10.7908 | 1.06x | adopted |
| `mcrbf_prod_pointxyzrgbnormal_n24_r18` | `pcl::PointXYZRGBNormal` | 4.7032 | 4.3558 | 1.08x | adopted |
| `mcrbf_prod_pointxyzrgbnormal_n40_r20` | `pcl::PointXYZRGBNormal` | 11.5031 | 10.7386 | 1.07x | adopted |

这些数据来自接入 traits gate 后的板卡生产 smoke。bench 内部为 `iterations=5`、`warmup=2`；
Evidence Doctor 仍只解析到每个 case 一个 summary B/A value，因此结论是 weak-positive，而不是强稳定 repeated-board 结论。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_direct`。 |
| A/B boundary | 真实 `MarchingCubesRBF<PointNT>::voxelizeData()` public overload / production dispatch。 |
| 当前决策问题 | `RVV-vs-scalar`：traits-gated normal AoS 点型是否值得命中同一 production RVV family。 |
| diagnostic 是否可外推到 production | 不外推；本阶段对每个新增代表点型都跑了 production direct correctness 和 board bench。 |
| comparison-boundary / baseline mismatch 风险 | 低：Std/RVV 使用同一 bench wrapper、同一输入构造、同一 checksum policy。 |
| weak-positive 处理 | 可以采纳，但文档必须标注 low-run warning 和不能外推到强稳定或所有自定义点型。 |
| 是否需要 RVV-vs-RVV detail A/B | 不需要。本阶段不是实现族选择，只扩大同一已采纳 RVV family 的 point type gate。 |

## Evidence Doctor 解释

`Errors=0`，因此没有必须修正后才能使用的证据错误。12 个 warning 全部是 `low_run_count`：
manifest 当前只从 summary 表解析每个 case 一个 B/A value，无法判断异常频率。处理方式：

- 当前阶段只写 weak-positive adoption；
- 不把 QEMU timing（QEMU 计时）写成性能证据；
- 不声称所有自定义 traits-compatible 点型都有相同收益；
- 若后续 reviewer 要求强稳定证据，再进入 analyzer / repeated-values summary phase。

## 优化矩阵更新

| candidate family | decision | 理由 |
| --- | --- | --- |
| traits-gated normal AoS RVV path | adopted_production_behavior | `PointXYZINormal` 和 `PointXYZRGBNormal` production direct 均正向，correctness / asm / board / doctor 闭合。 |
| exact `PointNormal` regression | adopted_production_behavior | 接入 traits gate 后仍为 `1.06x-1.08x` 正向，板卡 gtest 通过。 |
| richer repeated summary | deferred | 不是当前优化实现；仅在需要强稳定证据时继续。 |
| all custom normal-like point types | not_adopted_as_performance_claim | traits gate 允许满足布局的实例编译期进入 RVV，但性能证据只覆盖本阶段代表点型。 |

## 继续 / 停止决定

`continue_stop_decision`: stop_for_user_review

`stop_condition_hit`: 当前授权范围内最值得执行的实现优化已经闭合；剩余未做项主要是证据基础设施增强或扩大到未指定自定义点型，不属于本阶段继续自动接入的优化实现。

`next_phase_default`: 若需要更强稳定性，创建 `030-production-repeated-values-summary`，改造 bench / analyzer 生成逐次 B/A values 并重跑板卡；若 reviewer 接受 weak-positive，则当前 traits-gated normal AoS production 行为可以进入提交准备。
