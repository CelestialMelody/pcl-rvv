# Phase 040: stick getDistances diagnostic plan

## 阶段意图和边界

本阶段只为 `SampleConsensusModelStick<PointT>::getDistancesToModel` 建立 production-shaped diagnostic（生产形态诊断）。目标是先证明当前源码的系数语义和 outlier penalty（外点惩罚）能被 test-only candidate（测试专用候选）复刻，再评估 RVV（RISC-V Vector，可变长向量扩展）是否值得后续 bounded production probe（有界生产探针）。

本阶段不修改 production（生产源码），不新增 public API（公开接口），不证明 count/select 的 production patch 已授权，也不覆盖泛型点类型或真实 production dispatch（生产分流）。

## 当前源码语义

`getDistancesToModel` 和 count/select 不一致：它把系数 0-2 当 line point（线上的点），但把系数 3-5 直接当 line direction（线方向）再归一化，而不是第二个端点。随后按 `indices_` 顺序写 dense（稠密）`distances` 输出：

```text
sqr_threshold = radius_max_ * radius_max_
sqr_distance = (line_pt - point).cross3(line_dir).squaredNorm()
distance = sqrt(sqr_distance)                  if sqr_distance < sqr_threshold
distance = 2 * sqrt(sqr_distance)              otherwise
```

本阶段必须保持：

| 语义点 | 验收要求 |
| --- | --- |
| 系数解释 | candidate 使用 `model_coefficients[3..5]` 作为方向，不能复用 count/select 的第二端点解释。 |
| 输出顺序 | `distances[i]` 对应 `(*indices_)[i]`，输出大小等于 `indices_->size()`。 |
| penalty | 设置 `radius_max_` 后，平方距离不小于 `radius_max_^2` 的点必须输出 `2 * sqrt(sqr_distance)`。 |
| evidence boundary | 诊断证据不能替代 production direct evidence。 |

## 候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `getDistances-indexed-gather-f32m2-sqrt-store` | 使用 indexed xyz load 批量计算平方距离和 sqrt，再按 dense 输出顺序写回 `std::vector<double>`。 | float sqrt 与 production 的 `std::sqrt(float)` 转 double 需容差验证；double store 和 scalar 转换可能吞噬收益。 |
| `getDistances-rvv-sqr-scalar-sqrt-store` | RVV 只负责平方距离，sqrt / penalty / double store 保持标量。 | 如果负向，只能拒绝当前 staging family，不能证明所有 getDistances RVV 都不可行。 |

初版优先尝试第一候选；若 intrinsic 支持或 correctness 阻塞，再降级到第二候选并在 result 中记录边界。

## 优化矩阵增量

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| getDistances-indexed-gather-f32m2-sqrt-store | direct indexed `indices_` | `PointXYZ`, float xyz AoS, `Eigen::VectorXf`, `radius_max_` as penalty threshold | test-only candidate vs public `getDistancesToModel` | `make run_test_compare`；case 覆盖方向系数、乱序 indices、dense 输出和 penalty | board repeated compare，case label `public getDistancesToModel` / `diagnostic candidate getDistancesToModel` | 5-run bounded budget 后才给性能结论 | `getDistancesToModelCandidateRVV` | `run_repeated_board_evidence_doctor` | planned | RED/GREEN 后跑 QEMU、asm 和板卡 |

## 实现和测试动作

| action | 产物 | 命令 / 证据 | 完成判据 |
| --- | --- | --- | --- |
| RED test | `src/test_sac_model_stick.cpp` | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | 先因缺少 `getDistancesToModelCandidate` 编译失败。 |
| GREEN candidate | `include/impl/sac_model_stick_diagnostic.hpp` | 同上 | Std/RVV 两个构建均通过 public/candidate getDistances 对拍。 |
| bench 扩展 | `src/bench_sac_model_stick.cpp` | `make -C ... dump_bench_rvv`，板卡 repeated | bench 输出 count、select、getDistances public/candidate 行。 |
| manifest / doctor | `script/generate_stick_board_evidence_manifest.py`、Phase 040 summary evidence | `make -C ... record_repeated_board_evidence_state && make -C ... repeated_evidence_status` | 新 run label、doctor 和 registry 指向 Phase 040 文档。 |

## 板卡复跑预算和决策桶

板卡可用时使用 `SSH_AUTH_SOCK=/run/user/$(id -u)/keyring/ssh` 注入命令环境。默认 repeated budget（有界复跑预算）为 5 run，decision bucket（决策桶）沿用 Phase 000/020。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`；代码只在 `test-rvv` 派生类中。 |
| A/B boundary | test helper：公开标量入口 vs 测试专用 RVV candidate。 |
| 计时边界 | 只包含入口调用和 dense `distances` 写回；不包含点云、indices 和系数构造。 |
| 当前决策问题 | getDistances RVV sqrt/store family 是否值得进入 bounded production probe。 |
| diagnostic 是否可外推到 production | unknown。它复用真实对象状态和公开入口语义，但没有 production helper、dispatch 和 fallback 边界。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 correctness、asm、board 异常解释闭合，且能提出更接近 production 的 bounded probe 时才允许；否则保留为 attempted diagnostic。 |

## Phase Scope 与扩展队列

`validated_scope`：计划只覆盖 `getDistancesToModel`、direct indexed `indices_`、`PointXYZ`、float xyz AoS、`Eigen::VectorXf` 系数、`radius_max_` penalty、dense distance output 和 synthetic stick-distance cloud。

`unvalidated_scope`：production dispatch、count/select production patch、泛型点类型、`Scalar=double`、真实 RANSAC 上游路径和非 AoS fallback。

## 继续 / 停止条件

默认继续到 correctness、bench build、asm、5-run board repeated、Evidence Doctor 和 registry。若当前 candidate 负向或 Evidence Doctor 报错，写成诊断拒绝或阻塞，不把它外推成所有 getDistances RVV 不可行。
