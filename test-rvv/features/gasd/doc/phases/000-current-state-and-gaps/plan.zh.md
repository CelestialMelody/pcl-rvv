# Phase 000 plan: current state and gaps

## 阶段意图和边界

本阶段建立 `gasd.hpp` 的 RVV diagnostic（诊断）闭环，先把 shape / color 两条链路都共享的 fixed-grid histogram copy（固定网格直拷贝）做成第一条 test-only candidate（测试专用候选）。本阶段不修改 production（生产源码），不证明完整 `GASDEstimation::compute()` 已经命中 RVV，也不覆盖 `Scalar=double`、其他 point type（点类型）或 production dispatch（生产分流）。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| source | `computeAlignmentTransform`、`addSampleToHistograms`、`copyShapeHistogramsToOutput`、`copyColorHistogramsToOutput` 都是当前热点候选 | `features/include/pcl/features/impl/gasd.hpp` |
| upstream test | upstream `test_gasd_estimation.cpp` 已证明 GASD 公开入口可跑通 | `test/features/test_gasd_estimation.cpp` |
| RVV topic assets | 目前新建 topic skeleton；还没有 test/bench/registry 证据 | `test-rvv/features/gasd/` |
| screening queue | 候选复筛中把 GASD 列为 `component ablation` | `doc-rvv/library-screening/features/features-retained-candidate-rescreen.zh.md` |

## 假设与候选族

| hypothesis | candidate | risk | validation |
| --- | --- | --- | --- |
| fixed-grid output 是 shape / color 两条链路共同尾段 | contiguous histogram copy | 只覆盖尾段，不代表主成本 | Std/RVV 对拍、asm、board repeated |
| 形状和颜色的固定网格写回共享同一种 copy 模式 | shared copy helper | color 分支有 boundary merge，不能直接外推到 projection | 正确性、边界值、benchmark |

## Optimization matrix

见 `../optimization-matrix.zh.md`。本阶段只允许把 `fixed-grid histogram copy` 从 `planned` 推进到 `attempted`、`adopted`、`rejected` 或 `deferred`。

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| A1 写 test-first 红灯 | `src/test_gasd.cpp` 先调用尚不存在的 candidate helper | `make run_test_rvv` 预期编译失败，证明测试能抓到缺失 helper | RED failure 由缺少 RVV helper 引起 |
| A2 写 Std/RVV diagnostic helper | `include/impl/gasd_reference.hpp`、`include/impl/gasd_copy_candidate.hpp` | Std/RVV 同输入输出；非 RVV 构建自然走 Std fallback | `run_test_compare` 通过 |
| A3 写 bench diagnostic | `src/bench_gasd.cpp`、`Makefile` | case 输出 checksum 和 us/iter | QEMU 只作为 log-shape smoke；性能结论等板卡 |
| A4 反汇编归属 | `make dump_bench_rvv` | filtered asm 命中当前 helper 的 RVV 指令 | asm boundary 写入 result |
| A5 板卡 repeated bench | board repeated target | 5-run bounded budget，decision bucket 稳定或标 unstable | Evidence Doctor 无未处理 Error |
| A6 文档回填 | `result.zh.md`、evaluation、matrix、roadmap、Handoff | 证据边界、continue/stop decision 可恢复 | result 和 Handoff 完整 |

## Evidence Doctor 和 registry 规则

board summary、checksum summary、asm attribution（反汇编归属）或 EvidenceDecision（证据决策）前，必须运行 `test-rvv/script/evidence_doctor.py` 或在 result 中人工记录 Errors / Warnings / Suggestions。当前还没有 evidence registry（证据登记表），phase 000 结束前至少要记录 registry 状态。

## 板卡复跑预算和决策桶

板卡当前由会话确认可用。本阶段预算为 5 次 repeated board run。decision bucket（决策桶）暂定：`positive` >= 1.10x，`weak-positive` 1.03x-1.10x，`neutral` 0.97x-1.03x，`negative` < 0.97x；若同一 case 跨桶摇摆则标 `unstable` 并降级结论。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic（诊断），test helper boundary（测试 helper 边界） |
| A/B boundary | test helper：Std copy vs RVV copy |
| 当前决策问题 | RVV-vs-scalar diagnostic；是否值得继续到 projection / interpolation |
| diagnostic 是否可外推到 production | unknown；它只模拟固定网格写回，不覆盖完整 descriptor 主流程 |
| comparison-boundary / baseline mismatch 风险 | yes；copy helper 和真实 production 还有投影、插值和颜色分支 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但只在 copy 路线本身有稳定正向证据时再考虑下一 phase |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；若未来进入 production adoption，必须补同一边界对拍 |

## Phase scope 与扩展队列

`validated_scope`：`std::vector<Eigen::VectorXf>` 形式的 fixed-grid copy、`float`、AoS-ish（接近 AoS 的连续输出）写回、diagnostic helper。

`unvalidated_scope`：shape sample projection、trilinear / quadrilinear interpolation、color hue projection、真实 public `compute()` dispatch、`Scalar=double`、非标准布局、小输入 fallback。

`point_type_expansion_queue`：若 phase 000 支持后续生产探针，后续 phase 需要读取 generic point type strategy（泛型点类型策略）并为 PointXYZ-like traits gate、具体实例 fallback、asm、board 和 Evidence Doctor 建独立矩阵。

## 文档更新清单

本阶段更新 `doc/gasd-evaluation.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md`、本 phase result 和 current Handoff。`doc-rvv/features/...` production 长期主题文档暂不适用。

## 继续 / 停止条件

默认继续到 A1-A6 闭环。合法停止条件只包括：测试/工具链无法构建、板卡 SSH/rsync/远端 Make target 不可用、Evidence Doctor Error 无法修复、dirty isolation 不安全，或 production 接入需要用户确认。
