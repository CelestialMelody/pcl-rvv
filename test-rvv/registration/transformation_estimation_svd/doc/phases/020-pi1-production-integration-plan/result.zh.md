# Phase 020 结果：ordered-cloud-pair production integration

## 实际执行范围

本阶段从 PI1 production integration plan（生产接入计划）继续到 PI5 production evidence decision（生产证据决策）。执行范围没有扩大出 PI1 冻结边界：

- production entry（生产入口）：`estimateRigidTransformation(const PointCloud<PointSource>&, const PointCloud<PointTarget>&, Matrix4&)`。
- row source policy（行来源策略）：ordered-cloud-pair（顺序点云对，source/target 按相同下标一一对应）。
- RVV gate（RVV 门控）：`__RVV10__`、`Scalar=float`、`use_umeyama_ == true`、source/target 分别满足 `pcl::rvv::RVVXYZAoSFloatLayout`、两侧 dense、`n >= 16`。
- 保持标量：非 RVV 构建、小规模、非 dense、`use_umeyama_ == false`、`Scalar=double`、source-indexed、dual-indices、correspondences。

历史 bench label（性能测试标签）中的 `full-cloud` 只作为 legacy label（历史标签）保留；当前语义名称统一写 ordered-cloud-pair。

## 动作完成表

| 动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| PI2 production patch | done | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` | 新增 ordered-cloud-pair RVV helper 和 public dispatch；公开 API 不变。 |
| PI3 production direct correctness | done | `src/test_tesvd.cpp`、`make ... run_test_compare` | Std 8/8、RVV 8/8；`PointXYZ`、`PointXYZI`、`PointXYZRGB` 代表性 xyz AoS 点型通过。 |
| fallback gate tests | done | `ProductionDirectRVVRejectsOutOfScopeGates` | 小规模、非 dense、`use_umeyama_ == false` 和 `Scalar=double` 均不命中 RVV helper。 |
| board correctness smoke | done | `log/board/test_smoke/run_test.log` | 板卡 RVV gtest 8/8 passed。 |
| production asm attribution | done | `build/asm/riscv/bench_transformation_estimation_svd_rvv.full.asm` | public overload 符号 `0x1fe04` 内可见 `vlsseg3e32.v`、`vfadd.vv`、`vfmacc.vv`、`vfredosum.vs`。 |
| production repeated board | done | `log/board/production_ordered_cloud_pair_repeated/summary.md` | public Std/RVV median：4K `14.372x`、64K `24.471x`、256K `23.841x`；bucket `positive`。 |
| Evidence Doctor | done | `log/board/production_ordered_cloud_pair_repeated/evidence_doctor.md` | Errors=0、Warnings=1、Suggestions=0。 |
| evidence registry | done | `log/evidence_registry.json` | production summary / manifest / doctor 已 record；后续以 `evidence_status` 检查 freshness。 |
| 文档刷新 | done | README、evaluation、benchmark、optimization evidence、matrix、roadmap、doc-rvv | 旧 `partial-production-candidate` 结论已刷新为 ordered-cloud-pair adopted。 |

## 证据解释

Correctness（正确性）：QEMU Std/RVV 各 8 个 gtest 通过，板卡 RVV smoke 8/8 通过。测试覆盖 public Umeyama 语义锚点、test-only fused reference、production helper path-hit（路径命中）、代表性 `PointXYZI` / `PointXYZRGB` layout gate，以及 fallback gate。

Performance（性能）：生产直连 repeated board 使用 `public-umeyama` case-filter，Std build 作为 baseline，RVV build 在满足 gate 时命中 production RVV helper。5-run B/A（baseline ms / candidate ms）全部大于 1.15，overall decision bucket 为 `positive`。

Evidence Doctor（证据体检）：唯一 Warning 是 4K `group_outlier`，因为 4K median `14.372x` 明显低于 64K / 256K 的约 `24x`。处理策略是按 size 分开报告，不把大规模收益外推到 4K；该 Warning 不阻塞 ordered-cloud-pair 生产接入。

## Optimization Matrix 更新

| candidate family | row source policy | 状态 | 证据 | next action |
| --- | --- | --- | --- | --- |
| `production_direct_dispatch` | ordered-cloud-pair | adopted / production-ready | correctness、asm、board repeated、doctor 均闭合 | no action inside current production scope |
| `generic_xyz_aos_dispatch` | ordered-cloud-pair | correctness adopted / performance bounded | `PointXYZI`、`PointXYZRGB` direct correctness pass；未逐类型上板 | 只有要声明逐类型性能时才补 per-type board |
| `source_indexed_fused_accum` | source-indexed-cloud-pair | deferred | 未实现，不能继承 ordered-cloud-pair 结论 | Phase 030 独立审计 |
| `dual_indices_or_correspondences` | dual-indices / correspondences | deferred | 未实现，不能继承 ordered-cloud-pair 结论 | Phase 030 或独立 phase |

## Rerun Budget

| 项 | 结果 |
| --- | --- |
| 计划预算 | 5-run repeated board，20 iterations，5 warm-up。 |
| 实际复跑 | 5/5 completed。 |
| decision bucket | `positive`，无需追加预算。 |
| 降级条件 | 未触发；doctor Warning 已按证据边界解释。 |

## 继续 / 停止决定

`continue_stop_decision`：ordered-cloud-pair production integration loop 已闭合，当前生产补丁可进入 review。继续扩大到 source-indexed、dual-indices、correspondences 或 `Scalar=double` 会改变 row source / Scalar 范围，必须作为下一阶段独立取证。

`stop_condition_hit`：当前 phase 矩阵内没有未阻塞的 production direct 动作；剩余方向属于新 row-source phase。

`next_phase_default`：`030-row-source-audit`，先写 plan，再为 source-indexed / dual-indices / correspondences 建 candidate matrix、correctness、bench、asm、board 和 Evidence Doctor 计划。
