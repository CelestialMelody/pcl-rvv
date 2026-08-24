# 000 current-state-and-diagnostic-plan 计划

## 阶段意图和边界

本阶段建立 DON（Difference of Normals，法线差分）topic 的第一条可执行证据链，目标是证明 `features/include/pcl/features/impl/don.hpp` 中 `computeFeature()` 的逐点 normal 差、非有限值置零和 curvature 写回可以被 test-only RVV candidate（仅测试使用的 RVV 候选）复刻。

本阶段不修改 production（生产源码），不声明真实公开入口已经接入 RVV，也不外推到 `PointXYZI`、`PointXYZRGB` 等输入点型的完整生产模板结论。当前验证范围是 normal cloud 顺序逐点输入，`PointNT=pcl::Normal` / `PointOutT=pcl::Normal`，`Scalar=float`，AoS（结构数组）布局。

## 当前状态清单

| 项目 | 当前状态 |
| --- | --- |
| 源码入口 | `pcl::DifferenceOfNormalsEstimation<PointInT, PointNT, PointOutT>::computeFeature(PointCloudOut&)` |
| 标量语义 | 对每个点写 `0.5 * (small.normal - large.normal)`；若输出 normal 任一分量非有限则置零；最后写 curvature 为输出 normal 的欧氏范数 |
| 公开入口检查 | `initCompute()` 要求 small / large normal cloud 存在且尺寸等于 `input_->size()` |
| 现有 upstream 测试 | 仓库 `test/features` 中未发现 DON 专项测试 |
| 当前 topic 资产 | 本阶段新建 `test-rvv/features/don/` |
| 生产状态 | 未修改 production |
| 板卡状态 | 用户已说明板卡可用；本阶段先建立 QEMU correctness（QEMU 正确性验证）和 bench 入口，随后按预算跑板卡 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| strided AoS normal diff RVV | `pcl::Normal` 的 normal_x/y/z 是连续 float 字段，可用 RVV 分块加载、计算、mask 和写回 | curvature 的 `sqrt` 成本和 stride store 可能稀释收益 |
| finite mask + scalar-compatible zeroing | RVV 先算三分量差，再用 finite mask 决定写正常值或零值 | NaN / Inf 语义必须和标量路径一致 |
| production-shaped diagnostic | 先在 test-only helper 里复刻逐点热点，再决定是否进入 production integration loop | diagnostic 证据不能替代 production direct 证据 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| strided AoS normal diff RVV | ordered normal cloud | `pcl::Normal` / float / AoS | test-only `computeDoNRVV()` | `run_test_compare` | `run_bench_rvv` then board compare | planned, user says board available | `dump_bench_rvv` should show RVV float load/store/math | planned after board summary | planned | implement candidate, run QEMU correctness, run asm, run board smoke |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED 测试 | `src/test_don.cpp` 调用尚未实现的 `computeDoNRVV()` | `make -C test-rvv/features/don run_test_rvv` 因缺少 candidate 入口失败 |
| GREEN candidate | `include/don.h`、`include/impl/don_core.hpp` | `run_test_compare` 通过，Std/RVV 两种 build 都满足同一语义 |
| bench 入口 | `src/bench_don.cpp` | QEMU 只运行 RVV log-shape smoke；性能结论等待 board |
| 反汇编 | `make -C test-rvv/features/don dump_bench_rvv` | asm 中出现可归属到 candidate 的 RVV 指令 |
| 板卡 smoke | `make -C test-rvv/features/don board_smoke` | 有界复跑预算内得到 board correctness 和 bench summary |

## Evidence Doctor 和 registry 规则

本阶段若生成 board summary（板卡摘要），必须用 `test-rvv/script/evidence_doctor.py` 或人工 Evidence Doctor（证据体检）记录 Errors / Warnings / Suggestions。当前还没有 topic-local `log/evidence_registry.json`，阶段 result 必须写 `evidence_registry_status=not_available` 或补登记。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic（诊断） |
| A/B boundary | test helper（测试 helper） |
| 当前决策问题 | RVV-vs-scalar 与 implementation-shape（实现形态） |
| diagnostic 是否可外推到 production | unknown；它只复刻逐点热点，不证明 `DifferenceOfNormalsEstimation::compute()` 公开入口 dispatch |
| comparison-boundary / baseline mismatch 风险 | yes；bench helper 不包含前置 normal estimation，也不包含 `Feature::compute()` 对 output 尺寸和对象状态的处理 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但只能在逐点热点、`PointNT/PointOutT` normal 字段布局和 fallback gate 可控时进入 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；当前没有已采用的 DON RVV family，但 production public positive 仍只能证明 public RVV path 快于 public scalar path |

## 阶段完成条件

本阶段完成需要：RED 已被验证、candidate correctness 通过、bench binary 可构建、反汇编或等效归属检查完成、板卡 smoke 或明确工具阻塞记录完成，并更新 result、optimization matrix、roadmap 和 Handoff。

## 板卡复跑预算和决策桶

首轮 board smoke 预算为 1 次 correctness + 1 次 bench compare；若方向接近阈值或 Evidence Doctor 给出长尾 / metadata warning，下一阶段再扩大到 5-run repeated board。decision bucket 初步使用 positive / weak-positive / neutral / negative / unstable，不用 QEMU timing 作为性能结论。

## 继续 / 停止条件

若本阶段 correctness、asm 和 board smoke 支持 candidate，默认下一 phase 是 `010-production-shaped-diagnostic-or-PI1-plan`，先补真实 `DifferenceOfNormalsEstimation` 调用形态和 fallback gate，再决定是否进入 production integration loop。只有生产接入需要用户最终采纳、板卡或工具不可达、Evidence Doctor Error 无法解释、dirty isolation 不安全时停止。

## 文档更新清单

- `doc/don-evaluation.zh.md`：S2 evaluation 和诊断证据链主归属。
- `doc/optimization-roadmap.zh.md`：候选搜索空间和下一阶段恢复条件。
- `doc/phases/optimization-matrix.zh.md`：跨阶段矩阵。
- `tmp/rvv-work-logs/features/don/current-handoff/`：如需保存 Handoff，按配置解析；本轮默认只在回复中输出。
