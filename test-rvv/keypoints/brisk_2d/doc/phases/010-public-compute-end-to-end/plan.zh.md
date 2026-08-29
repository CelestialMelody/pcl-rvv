# Phase 010 Plan: public compute end-to-end evidence

## 阶段意图和边界

本阶段验证 `BriskKeypoint2D::compute()` 公开入口是否能在 RISC-V / RVV 构建中通过已经采纳的
downsample helper 链，并评估该 helper 链对完整 keypoint pipeline（关键点检测流水线）的端到端影响。

本阶段证明：

- `BriskKeypoint2D::compute()` 在 synthetic organized `PointXYZRGBA` input cloud（合成有组织点云输入）上不再因为
  `Layer::halfsample()` / `twothirdsample()` 非 SSSE3 空实现而失败。
- Std/RVV 两侧公开入口输出 keypoint count（关键点数量）和 sampled keypoint checksum（抽样校验和）一致。
- 板卡 repeated bench（重复性能测试）能判断 downsample helper 收益是否能穿透完整公开入口。

本阶段不证明：

- AGAST/OAST detector（角点检测器）或 scale refinement（尺度细化）已经被 RVV 加速。
- features 模块 BRISK descriptor（描述子）已经加速。
- 公开入口若出现中性或负向结果，downsample helper 采纳必须回滚；Phase 000 已用 production-detail 证据关闭
  downsample helper，本阶段只补 public-entry evidence（公开入口证据）。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| production state | `keypoints/src/brisk_2d.cpp` 已采用 RVV downsample helper 和 portable scalar fallback。 | `keypoints/src/brisk_2d.cpp` |
| phase 000 evidence | downsample helper chain 板卡 repeated 为 weak-positive。 | `doc/phases/000-current-state-and-downsample-diagnostic/result.zh.md` |
| roadmap | `BriskKeypoint2D::compute()` end-to-end bench 为 deferred / medium。 | `doc/optimization-roadmap.zh.md` |
| test support | 当前只覆盖 `brisk::Layer` helper 和 `ScaleSpace::constructPyramid`。 | `src/test_brisk_2d.cpp`、`src/bench_brisk_2d.cpp` |

## 阶段范围

- `validated_scope`：organized `PointXYZRGBA` synthetic cloud，`BriskKeypoint2D<PointXYZRGBA>::compute()`，
  `octaves=4`，`threshold=60`，`remove_invalid_3D_keypoints=false`。
- `unvalidated_scope`：真实 PCD 输入、descriptor、AGAST/OAST RVV、`remove_invalid_3D_keypoints=true`、
  其它 point type / intensity accessor、阈值调参和完整 keypoint quality。
- `point_type_expansion_queue`：不扩展；本阶段只是公开入口 smoke / bench，点类型固定为上游示例常用
  `PointXYZRGBA`。
- `phase_closeout_boundary`：只能关闭 public-entry smoke 和 end-to-end board evidence，不能扩大 Phase 000 的
  adopted helper 边界。

## TDD gate

新增 gtest 前先声明会被捕捉的生产变化：

| test | 会捕捉的错误生产变化 |
| --- | --- |
| `PublicComputeRunsOnSyntheticOrganizedCloud` | 非 SSSE3 / RISC-V 路径重新落回空实现、`constructPyramid` 派生层失败、公开入口不能完成 compute。 |

期望值不复用 production 逻辑推导；测试只断言输出对象状态、Std/RVV 同链路一致性和有限 keypoint 坐标边界。

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| public fixture | `include/impl/brisk_2d_downsample_reference.hpp` | 增加 synthetic organized cloud 和公开入口 checksum helper。 |
| public gtest | `src/test_brisk_2d.cpp` | QEMU Std/RVV `run_test_compare` 通过，新增测试能执行真实 `BriskKeypoint2D::compute()`。 |
| public bench case | `src/bench_brisk_2d.cpp` | 新增 `brisk_public_compute_320x240` case，Std/RVV checksum 一致。 |
| board repeated | `collect_repeated_board_evidence` 或 phase010 专用 repeated target | 板卡 5 run，`run_test` 通过，bench summary 包含 public case。 |
| doctor / registry | `run_repeated_board_evidence_doctor`、`record_repeated_board_evidence_state` | Evidence Doctor 无 Error；Warning/Suggestion 要解释并降级边界。 |
| docs | result、matrix、roadmap、evaluation、doc-rvv、Handoff | 以 public-entry 板卡结果刷新“完整公开入口是否收益”结论。 |

## Evidence Doctor 和 registry

本阶段复用 topic-local manifest wrapper。wrapper 必须把 `brisk_public_compute_320x240` 标成
`production_public` evidence role（证据角色），A/B boundary（比较边界）为 public overload / public compute wrapper。
若 public case 为 neutral 或 negative，本阶段只说明完整 pipeline 中 downsample helper 收益被 AGAST/OAST 等标量成本稀释，
不自动回滚 Phase 000 adopted helper。

## 板卡复跑预算和决策桶

- 默认 5 run，每 run 使用 `--iterations 30 --warmup-iterations 5`，避免完整 keypoint pipeline 运行时间过长。
- `positive`：public compute median speedup 大于 1.20x，且 0/5 反向。
- `weak-positive`：median speedup 为 1.05x 到 1.20x，且不超过 1 次反向。
- `neutral`：0.95x 到 1.05x。
- `negative`：小于 0.95x。
- 5 run 内 bucket 摇摆则标为 `unstable`，不无界复跑。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public`，通过真实 `BriskKeypoint2D::compute()` 入口触达 production helper。 |
| A/B boundary | public overload / public compute wrapper。 |
| 当前决策问题 | 已采纳 helper 的 public-entry impact（公开入口影响），不是新的 RVV-family-selection。 |
| diagnostic 是否可外推到 production | 是，同一公开入口；但 synthetic 输入不代表真实图像分布。 |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 使用同一 wrapper、输入生成、threshold、octaves 和 checksum policy；仍存在 synthetic input 代表性风险。 |
| weak / neutral / negative 时是否允许继续 | weak-positive 支持记录公开入口也有收益；neutral/negative 只降级 public-entry impact，不回滚 helper。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要，本阶段不选择新 RVV family。 |

## 继续 / 停止条件

若 public case 为 positive / weak-positive，刷新 production `doc-rvv`，把完整公开入口写成“已观测到端到端收益但不声明
AGAST/OAST 被加速”。若 public case 为 neutral / negative，刷新 roadmap 和 docs，说明 helper 仍已采纳但完整入口收益
受标量 detector 稀释，并把 AGAST/OAST profile 或 detector phase 标为下一候选。若板卡不可达或 Evidence Doctor 出现
未解决 Error，停在 blocked handoff。
