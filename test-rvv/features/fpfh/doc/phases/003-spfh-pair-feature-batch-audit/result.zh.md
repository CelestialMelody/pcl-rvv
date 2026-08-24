# Phase 003 Result: spfh-pair-feature-batch audit

## 结论

本阶段完成 `computePointSPFHSignature` 的 pair-feature batch（点对特征批处理）审计，结论是
`turn_stop_deferred with stop_condition_hit`：当前不建议在同轮继续实现 test-only RVV candidate。

停止原因不是板卡不可用，也不是缺少 RVV math helper。真实停止条件是语义风险和证据边界不闭合：

- `features/src/pfh.cpp::computePairFeatures` 使用 `std::acos(std::fabs(angle))` 选择 pair 顺序，再用
  `std::atan2` 计算 f1。现有 `acos_RVV_f32m2` / `atan2_RVV_f32m2` 都是近似 helper，不是 strict libm replacement（严格 libm 替换）。
- SPFH 的 11-bin histogram（直方图）对 f1/f2/f3 的 bin 边界敏感。即使角度误差很小，也可能让靠近边界的样本进入相邻 bin。
- histogram scatter（直方图离散累加）存在同一 bin 冲突。直接 RVV scatter 会改变累加组织；保留标量 scatter 则需要 staging（暂存）并证明额外内存流量不抵消收益。
- Phase 002 repeated board 中 `component_spfh_signature` 只是 avg 1.010x 的 near-threshold diagnostic（接近阈值诊断）；没有足够证据支持在当前 production closeout 后继续扩大复杂候选。

因此本阶段不修改 production，不新增 candidate。后续若要恢复，必须先建立数学 helper 的 caller-specific bin-stability
测试和 scatter staging 设计。

## 计划回填

| action | 状态 | 结果 |
| --- | --- | --- |
| A1 source audit | done | `computePointSPFHSignature` 逐邻居调用 `computePairFeatures`，再按 f1/f2/f3 的 bin 公式做标量 histogram 累加。 |
| A2 math helper audit | done | `common/include/pcl/common/impl/rvv_math.hpp` 已有 `acos_RVV_f32m2` 和 `atan2_RVV_f32m2`，但它们是近似 helper，不能直接替代 bin-sensitive libm chain。 |
| A3 candidate decision | done | 本阶段不进入实现；先要求 bin-stability 和 scatter staging 证据。 |
| A4 文档同步 | done | 更新 phase index、optimization matrix、roadmap 和 evaluation 后续方向。 |

## 标量路径审计

`computePairFeatures` 的关键步骤：

1. `dp2p1 = p2 - p1`，`f4 = norm(dp2p1)`；距离为 0 时返回 false。
2. 计算 `angle1 = n1.dot(dp2p1) / f4` 和 `angle2 = n2.dot(dp2p1) / f4`。
3. 用 `std::acos(std::fabs(angle1)) > std::acos(std::fabs(angle2))` 判断是否交换点对方向。
4. 用 `cross3` 生成 Darboux frame（Darboux 坐标系）中的 `v` 和 `w`；`v_norm == 0` 时返回 false。
5. `f2 = v.dot(n2)`，`f1 = std::atan2(w.dot(n2), n1.dot(n2))`。
6. `computePointSPFHSignature` 将 f1 映射到 `[-pi, pi]` 的 11 bins，将 f2/f3 映射到 `[-1, 1]` 的 11 bins，并用固定 `hist_incr` 累加。

可批处理的部分是 dot/cross/norm/atan2 的 per-neighbor math。当前最难闭合的是第 3 步换序比较和第 6 步 histogram
scatter 的语义。

## Math helper 审计

| helper | 当前合同 | 与 SPFH pair-feature 的关系 | 风险 |
| --- | --- | --- | --- |
| `acos_RVV_f32m2` | 输入向量在 `[0,1]`；`acos(x) ~= sqrt(1-x) * Q(1-x)`；文档报告 max error 约 `1.311302e-06 rad`。 | 可用于 `std::acos(std::fabs(angle))` 的换序比较，输入域形式匹配。 | 换序比较是大于关系。若两个 angle 的 acute angle 很接近，近似误差可能改变是否交换点对。 |
| `atan2_RVV_f32m2` | Hastings-style polynomial approximation，返回 `[-pi, pi]`；注释称约 `0.01 deg` max error。 | 可用于 f1。 | f1 直接进入 11-bin mapping；靠近 bin 边界的样本可能换 bin。 |
| scalar bin formulas | `floor(bins * ((f1 + pi)/(2*pi)))` 和 `floor(bins * ((f + 1) * 0.5))` 后 clamp。 | 这是 SPFH descriptor 的离散输出。 | 只做浮点近似对拍不足够，必须证明 bin index 稳定或定义 fallback。 |

## EvidenceDecision

| 决策问题 | 结论 |
| --- | --- |
| 是否缺少 RVV math helper | 否。`acos` 和 `atan2` helper 已存在。 |
| 是否可以直接实现 pair-feature RVV candidate | 否。bin-stability 和 pair order stability 未闭合。 |
| 是否需要板卡复跑 | 当前不需要。没有候选实现时，板卡可用也不能替代语义门禁。 |
| 是否影响 Phase 002 adopted patch | 不影响。weighted FPFH production patch 保持 adopted with bounded scope。 |

最终 decision：`deferred_with_stop_condition_hit`。

## 恢复条件

恢复 `spfh-pair-feature-batch` 前需要先完成这些窄任务：

1. 建立 caller-specific scalar/RVV math chain test，输入来自 FPFH synthetic neighborhood，比较 pair-order decision、f1/f2/f3 和 bin index，而不仅是角度误差。
2. 统计样本距离 bin boundary（分箱边界）的 margin；对 margin 太小的 lane 定义标量 fallback 或保留标量 binning。
3. 设计 scatter 策略：要么 RVV 只 staging f1/f2/f3 后标量 tail 累加，要么按 11 bins 分组规约并证明冲突语义。
4. 在 `component_spfh_signature` 上做 candidate-vs-scalar repeated board；只有强正向且 Doctor 无 Error 才能考虑 production probe。

## 文档更新

- `doc/phases/README.zh.md`：新增 Phase 003 状态。
- `doc/phases/optimization-matrix.zh.md`：`spfh-pair-feature-batch` 改为 `deferred_with_stop_condition_hit`。
- `doc/optimization-roadmap.zh.md`：补充恢复条件。
- `doc/fpfh-evaluation.zh.md`：后续方向改为先做 bin-stability/scatter staging 审计。
