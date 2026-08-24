# Phase 052 计划：more generic xyz AoS board evidence

## 阶段意图和边界

本阶段延续 Phase 051 的 `more-generic-xyz-aos-point-types`：在不修改 production 源码、不扩大 dispatch gate 的前提下，为 Phase 051 新增的常见 PCL xyz AoS 点型补 board repeated（板卡重复采集）证据。

冻结范围：

- entry：ordered-cloud-pair public overload。
- row source：ordered only；不覆盖 source-indexed、dual-indexed 或 correspondence。
- point type：仅 Phase 051 已验证的 `PointXYZRGBA -> PointXYZRGBA`、`PointXYZL -> PointXYZ`、`PointNormal -> PointXYZRGB`、`PointWithRange -> PointWithRange` 和 `PointWithViewpoint -> PointXYZ`。
- `Scalar`：只覆盖 `float`。
- production：不改 production 源码；只补已采纳 traits-gated xyz AoS gate 的板卡证据边界。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| correctness | Phase 051 后 Std/RVV 各 16 tests passed。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU smoke | `more-generic-xyz-aos-point-types-public` 已有 5 comparisons，Doctor `0/0/0`。 | `log/qemu/more_generic_xyz_aos_point_types_public/evidence_doctor.md` |
| board evidence | not_run。 | 需要本阶段新增 board repeated target / summary / doctor。 |
| production source | 已采纳 ordered / row-source / correspondence sorted-copy / matrix-local helper；本阶段不修改。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `more-generic-xyz-aos-board` | Phase 051 的更多常见 PCL xyz AoS 点型命中同一 traits-gated xyz AoS public path，板卡上应与代表点型一样保持 positive。 | 点型 stride、padding、字段布局和混合 source/target 组合可能改变 memory behavior；若结果 weak/negative，只降级这些具体点型的性能边界，不回推否定 Phase 041 代表点型或 production gate。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `more-generic-xyz-aos-board` | ordered-cloud-pair | Phase 051 five combos / `float` / traits-gated xyz AoS | public ordered overload | current `run_test_compare` 16 tests | `more-generic-xyz-aos-point-types-public` | 新增 board repeated 5 runs，B/A = Std public ms / RVV public ms | production generic public overload boundary；不新增 ASM adoption | board Doctor | pending |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| A1 board target 接线 | `Makefile` | 新增 collect / manifest / doctor / registry / run target，case-filter 为 `more-generic-xyz-aos-point-types-public`。 |
| A2 summary wrapper 扩展 | `script/generate_tesvd_scale_board_repeated_summary.py` | 能识别 `more generic public scale` label，并在 manifest 中写清 more-generic evidence role。 |
| A3 文档同步 | README、testing overview、benchmark/evidence、optimization evidence、matrix、roadmap、phase result、doc-rvv 边界 | 记录 Phase 052 是 board evidence，不改变 production gate。 |
| A4 验证 | py_compile、make dry-run / QEMU registry / board repeated（若板卡可用） | 脚本编译通过；target 可解析；若板卡可用则生成 summary / manifest / doctor 并登记。 |

## Evidence Doctor 和 Registry

Board repeated 使用 5 runs、20 iterations、5 warmup。若 Doctor 出现 Error，先修复 summary / manifest 或降级证据；Warning 必须按点型 / size 分开解释。QEMU timing 不参与本阶段性能结论。

## 完成条件

- `positive_more_generic_board_complete`：5 个点型组合 board repeated 全部 positive，Doctor 无 Error，registry fresh。
- `weak_or_mixed_more_generic_board`：存在 weak / negative / unstable；只收束对应点型的性能边界，不回推否定 correctness。
- `turn_stop_deferred with board_required`：板卡不可用或配置缺失；保留 target / docs / 恢复命令。

## 继续 / 停止条件

本阶段完成后，更多自定义 xyz AoS、row-source 更广点型、非法 index / correspondence、`Scalar=double` 和新的 shuffle mitigation family 仍是独立 scope。若 Phase 052 板卡结果 positive，也只是把 Phase 051 新增点型从 correctness / smoke 扩到 board repeated，不自动关闭全部泛型点型。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-public board evidence`，真实 public ordered overload。 |
| A/B boundary | Std public ordered scale vs RVV public ordered scale；同一 case-filter，计时包含 public path 和 3x3 SVD 后段。 |
| 当前决策问题 | Phase 051 新增常见 PCL xyz AoS 点型是否也有板卡 repeated performance 支撑。 |
| diagnostic 是否可外推到 production | 当前是 production-public 边界；只能外推到这 5 个具体组合，不能外推全部自定义点型或 row source。 |
| comparison-boundary / baseline mismatch 风险 | low；Std/RVV 使用同一 public wrapper。 |
| weak / negative / unstable 时是否允许 bounded production probe | 不适用；production gate 已存在，本阶段只决定是否升级性能证据边界。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；没有新 RVV family 选择问题。 |
