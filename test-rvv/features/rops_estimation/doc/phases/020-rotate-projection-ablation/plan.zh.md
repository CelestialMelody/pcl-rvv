# Phase 020 Plan: rotate-projection-ablation

## 阶段意图和边界

本阶段验证 `ROPSEstimation::rotateCloud()` 的 test-only RVV candidate（测试专用 RVV 候选）。目标是把 transformed local cloud（已变换局部点云）按 axis-angle rotation（轴角旋转）批量生成 rotated cloud（旋转点云），同时用 RVV reduction（向量规约）计算 AABB（Axis-Aligned Bounding Box，轴对齐包围盒）的 `min/max`。

本阶段不修改 production（生产源码），不覆盖 LRF、KdTree local surface、distribution matrix、central moments 或完整 `computeFeature()`。若 correctness、asm 和 component bench 成立，才把 Phase 010 distribution matrix 与 Phase 020 rotate/projection 作为 combined production-shaped probe（组合生产形态探针）的前置线索。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| Phase 000 | central moments correctness 成立，但不单独 production | `doc/phases/000-current-state-and-component-ablation/result.zh.md` |
| Phase 010 | distribution matrix diagnostic positive；5-run board median 1.440x，0/5 退化 | `doc/phases/010-distribution-matrix-ablation/result.zh.md` |
| production helper | `rotateCloud()` 逐点构造 Eigen vector、矩阵乘法、写 rotated cloud，并更新三轴 min/max | `features/include/pcl/features/impl/rops_estimation.hpp` |
| current test support | `include/impl/rops_components.hpp` 已有 component helper；`src/test_rops_estimation.cpp` 使用 production private helper oracle | `test-rvv/features/rops_estimation` |

## 假设与候选族

| candidate family | hypothesis | risk | validation |
| --- | --- | --- | --- |
| rotate cloud + AABB RVV | 每个点 3 个 strided load、9 个 multiply/add 和 6 个 min/max update；local cloud 点数较大时比 distribution matrix 更接近热路径 | AoS stride load/store、寄存器压力、FMA contraction（融合乘加）和 min/max reduction 可能造成数值微差 | production oracle 对拍、QEMU、asm；通过后再补 component bench / board repeated |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| A1 RED test | `src/test_rops_estimation.cpp` 调用尚不存在的 `rops::rotateCloudRVV()` | `make run_test_rvv` 编译失败 | failure 来自缺少 candidate symbol |
| A2 helper implementation | `include/impl/rops_components.hpp` | 非 RVV 构建走 Std；RVV 构建批量 rotate + min/max reduction | `run_test_compare` 通过 |
| A3 axis coverage | tests cover X/Y/Z axes plus non-cardinal axis | QEMU Std/RVV logs | rotated points 与 AABB 逐项近似一致 |
| A4 asm attribution | `make dump_test_rvv` / `make dump_bench_rvv` | helper 中出现 `vlse32.v`、`vfmul` / `vfmacc`、`vfredmin` / `vfredmax`、`vsse32.v` 或等价 store | result 写清归属边界 |
| A5 bench decision | bench source / result / roadmap / matrix | 若 correctness 成立，补 rotate component bench 和 board repeated；否则修正或降级 | 不因 correctness 单项结束 topic |

## Board 复跑预算与决策桶

若新增 bench，默认先跑 1 次 board smoke，再跑 5-run repeated：`points=65536`、`iterations=20`、`warmup=3`、`repeat=8`。若 median >= 1.20 且 0/5 退化，decision bucket 为 positive；median >= 1.05 且退化不超过 1/5 为 weak_positive；0.95-1.05 为 neutral；median < 0.95 为 negative；方向跨桶或长尾明显则 unstable。除非 Evidence Doctor 出现 Error、checksum 不一致或方向反转，不无限复跑。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic component |
| A/B boundary | production private helper oracle for correctness；same bench wrapper Std fallback vs RVV candidate for performance |
| 当前决策问题 | implementation-shape；rotateCloud + AABB 是否值得与 distribution matrix 组成 combined probe |
| diagnostic 是否可外推到 production | no。它不覆盖完整 `computeFeature()`、LRF、local surface、moments 或 descriptor normalization |
| comparison-boundary / baseline mismatch 风险 | yes。test helper 只处理 `PointXYZ` synthetic cloud；production 是模板入口且调用频率受 rotations / keypoints 影响 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no for rotate component alone；positive 时也只作为 combined probe 前置线索 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段没有 production patch |

## Phase scope 与未验证范围

`validated_scope`：计划覆盖 `pcl::PointXYZ` / float / AoS synthetic finite transformed local cloud，X/Y/Z 和 non-cardinal axis，rotated point output 与 AABB min/max，QEMU Std/RVV correctness，asm attribution，必要时 board component diagnostic。

`unvalidated_scope`：完整 ROPS descriptor、LRF、真实 mesh local surface 分布、其它点型、production dispatch、combined rotate + distribution + moments、不同 rotations / bins / support radius。

`point_type_expansion_queue`：本阶段不扩点型；若后续进入 production probe，再为 exact type gate 与 PointXYZ-like traits 建独立 phase。

## 继续 / 停止条件

默认完成 A1-A5。若 correctness 不成立，先修 helper；若 asm 没有目标 RVV 指令或板卡 unavailable，记录 blocker。若 rotate component positive，下一步进入 combined rotate + distribution matrix production-shaped diagnostic；若 weak / neutral / negative，也要记录 mismatch audit 后再决定是否仍值得做 combined probe。

## 文档更新清单

完成阶段后更新 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、`rops_estimation-evaluation.zh.md` 和 phase `README.zh.md`。production 长期主题文档仍不适用，除非后续真实接入 production 并通过 PI5 用户检查点。
