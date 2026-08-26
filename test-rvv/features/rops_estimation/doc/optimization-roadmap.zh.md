# ROPS optimization roadmap

## 当前边界

本 topic 来自 features retained candidate rescreen 的 ROPS descriptor projection 行。当前已完成 test-rvv diagnostic（诊断）资产、topic-local 文档、combined production-shaped diagnostic（组合生产形态诊断）、production integration loop（生产接入闭环）和代表性点型扩展证据。完整 RoPS 路径包含 mesh local surface、LRF（Local Reference Frame，局部参考坐标系）、旋转、projection（投影）、distribution matrix（分布矩阵）、central moments（中心矩）和 descriptor normalization（描述子归一化）。当前 production patch 覆盖真实 production private helper dispatch（生产私有 helper 分流）里的 `rotateCloud()` 和 `getDistributionMatrix()`，Phase 040/050 的 repeated board 证据均为 positive。用户已确认板卡正向即可采纳，当前 patch 已进入 adopted production behavior（已采纳生产行为），长期文档位于 `doc-rvv/features/rops_estimation-RVV.zh.md`。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| central moments RVV | `computeCentralMoments()` 小矩阵循环、筛选表建议分块 | distribution matrix 后的 5 个 moments | 去掉 `std::pow` 重复开销，形成最小可测 component | 默认 5x5 太小，entropy `std::log` 仍可能支配；当前 RVV helper 需要 row/col/mass staging | correctness、asm；必要时 board component bench | attempted-correctness-only | closed by `000-current-state-and-component-ablation` |
| distribution matrix binning | 筛选表明确列出 distribution matrix | rotated cloud -> bins | 若 local point 数较大，bin index 计算可批量化 | scatter 更新不规则，冲突 bin 仍保留标量累加；test helper staging 成本与 production buffer 生命周期不完全同边界 | same-chain correctness、asm、board ablation | attempted-positive-diagnostic | closed by `010-distribution-matrix-ablation` |
| rotate/projection + AABB | MOI fused reduction 的结构经验；ROPS `rotateCloud()` 每轴每角度重复 | transformed local cloud | xyz AoS load、matrix multiply、min/max 规约可能更有收益；可与 distribution matrix 前置输入融合 | 需要处理 point type gate、small local surface 和 Eigen matrix；完整 production 路径仍含 LRF / KdTree | correctness、asm、board repeated | attempted-positive-diagnostic | closed by `020-rotate-projection-ablation` |
| combined rotate + distribution matrix | Phase 010 和 Phase 020 均为 positive diagnostic | transformed local cloud -> rotated cloud -> XY/XZ/YZ bins | 把两个正向 component 放入同一个 timing boundary，验证收益是否能在更接近 production 的组合链路中保留 | 仍不覆盖 LRF、mesh local surface、central moments、normalization；需要明确 rotated cloud / row-col staging buffer 生命周期 | same-chain correctness、asm、board repeated、Evidence Doctor | attempted-positive-production-shaped-diagnostic | closed by `030-combined-rotate-distribution-probe` |
| production private helper dispatch | Phase 030 combined probe 为 positive | `features/include/pcl/features/impl/rops_estimation.hpp` 内可维护的 bounded RVV path | 已验证 test-only 组合候选可迁移到真实 production helper dispatch，并建立 fallback 与 production-detail 证据 | 仍不是完整 public `computeFeature()` | production patch、direct correctness、asm、board repeated、Evidence Doctor、PI5 confirmation | adopted | closed by `040-production-integration-plan` and S11 closeout |
| PointXYZ-like traits expansion | Phase 040 后的 point_type_expansion_queue | `RVVXYZAoSFloatLayout<PointInT>` / float AoS 点型 | `PointXYZI` 和 `PointNormal` 代表性点型均有正向 production-detail board 证据 | 不证明所有自定义点型逐个性能，不证明额外字段输出语义 | typed correctness、asm、board repeated、Evidence Doctor、PI5 confirmation | adopted | closed by `050-point-type-expansion` and S11 closeout |
| descriptor normalization | `computeFeature()` 最后按 feature L1 norm 归一化 | feature vector length 135 | contiguous float reduction + scale 可能简单 | 长度固定但很小，收益可能被完整路径稀释 | correctness、asm、micro bench | not_now | 只有 public workload/profile 指向 normalization 成本时再开独立 phase |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | central moments 可作为 correctness scaffold，但不是优先 production 候选 | QEMU Std/RVV 3/3 通过，反汇编可归属到 helper；不过默认 5x5 矩阵太小且 staging / scalar entropy 成本明显 | 若后续 distribution matrix 或 projection 候选需要 moments fusion，再做同边界 component bench | low |
| 010 | distribution matrix 的 row/col index 批量计算有稳定板卡正向，但 scalar scatter 和 test-only staging 使它仍是 production probe 线索，不是最终生产行为 | QEMU Std/RVV 4/4；bench repeated 5-run median 1.440x，0/5 退化；Evidence Doctor 0 Error / 0 Warning / 2 Suggestion | 下一阶段优先验证 rotate/projection + AABB；若 Phase 020 也正向，再考虑 combined production-shaped probe | high |
| 020 | rotateCloud + AABB 的 AoS stride load/store、3x3 rotation 和 min/max reduction 在板卡上也有稳定正向 | QEMU Std/RVV 6/6；bench repeated 5-run median 1.220x，0/5 退化；Evidence Doctor 0 Error / 0 Warning / 2 Suggestion | 下一阶段把 Phase 010 + Phase 020 组合到同一 bench boundary，观察收益是否被 buffer 写回、scatter 和连续调用成本稀释 | high |
| 030 | combined rotate + distribution 的生产形态诊断仍保持稳定正向 | QEMU Std/RVV 7/7；board repeated 5-run median 1.630x，0/5 退化；Evidence Doctor 0 Error / 0 Warning / 2 Suggestion | 下一阶段进入 PI1 production integration plan，先限定 production patch 范围、fallback/gate 和 production direct 证据；PI5 采纳 / 回滚等待用户确认 | high |
| 040 | production private helper dispatch 接入后仍保持正向 | QEMU Std/RVV 通过；Phase 040 board repeated median 1.700x，0/5 退化；Evidence Doctor 0 Error / 0 Warning / 2 Suggestion | 用户确认后采纳；随后已补 Phase 050 点型扩展证据 | high |
| 050 | `RVVXYZAoSFloatLayout<PointInT>` traits gate 的代表性点型扩展也保持正向 | QEMU Std/RVV 16/16；post-review 补 public non-dense、helper invalid-input 和非 f32 layout fallback；PointXYZI median 1.520x；PointNormal median 1.590x；两组 Doctor 均 0 Error / 0 Warning / 2 Suggestion | 已采纳并进入 S11 production closeout；metadata hardening 作为严格归档范围另开 | high |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| full public `computeFeature()` workload/profile | 当前采纳证据覆盖 production private helper detail path；完整 public 路径还含 KdTree、LRF、moments 和 normalization | 有真实 mesh workload、profile 证明 RVV 覆盖段仍是 public 主成本时，另开 public workload/profile phase |
| full descriptor RVV rewrite | 范围过大且会混合多个未消融成本源 | projection / distribution / moments 分块证据证明值得合并 |
| central moments standalone production | Phase 000 只证明 correctness；默认矩阵 5x5 太小，RVV staging 和 scalar entropy 不支持单独 production probe | distribution / projection phase 证明 moments 是完整路径瓶颈，或出现无需 staging 的 fused candidate |
| distribution matrix standalone production | Phase 010 有 positive diagnostic，但 candidate 仍包含 row/col staging 和标量 scatter；单独接 production 可能被 rotateCloud、LRF、KdTree、descriptor normalization 稀释 | Phase 020 或 combined production-shaped diagnostic 证明 rotate + binning 的同边界收益，且明确 production buffer 生命周期 |
| descriptor normalization standalone RVV | 135 维连续数组规模固定，单独收益预计较小；当前没有 public profile 指向该尾段 | public workload/profile 显示 normalization 是主要成本，或未来有多个 descriptor 共用的 normalization RVV helper 需求 |
| distribution scatter vectorization | bin scatter 存在冲突和顺序语义风险；当前已采纳 row/col staging + scalar scatter 的正向组合 | 有冲突处理策略、同边界 correctness、asm 和 repeated board 证据后另开 phase |
| evidence metadata hardening | 补 taskset、governor、freq、temperature 和 binary hash 能增强归档质量，但不改变当前算法收益判断 | 用户要求严格证据归档，或后续出现方向反转、长尾或 reviewer 要求补齐 metadata |
