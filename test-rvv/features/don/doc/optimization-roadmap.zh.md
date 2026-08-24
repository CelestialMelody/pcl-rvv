# DON optimization roadmap

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| strided AoS normal diff RVV | `computeFeature()` 源码逐点 normal 差 | ordered normal cloud, `pcl::Normal`, float AoS | 减少三分量差、finite mask 和输出写回的逐点成本 | curvature `sqrt`、stride load/store 和前置 normal estimation 可能稀释收益 | correctness、asm、board smoke、Evidence Doctor | partial-production-candidate | completed by `000` and `010` |
| repeated board stability | Evidence Doctor `low_run_count` warning | helper-only diagnostic board bench | 把单次 weak signal 升级成可判断稳定性的诊断证据 | 不包含 production dispatch 和前置 normal estimation | 5-run repeated summary、manifest、doctor、registry | completed | `010-diagnostic-repeated-board` |
| production-shaped `DifferenceOfNormalsEstimation` diagnostic / PI1 | 筛选队列建议建立函数级 correctness + QEMU 评估；phase 010 repeated 为 weak-positive | 真实 `Feature::compute()` 调用形态；首个 production probe 收窄到 `PointNT=pcl::Normal` / `PointOutT=pcl::Normal` | 关闭 bench helper 与公开入口对象状态错配风险，并评估可维护 production patch | production-public 边界实际抵消 helper-only 收益 | PI1 plan、production direct correctness、fallback、asm、board、Evidence Doctor | rolled-back-negative | `040-rollback-closeout` |
| production detail ablation | phase 030 public negative 与 helper-only weak-positive 方向不一致 | exact normal public boundary / detail helper boundary | 定位退化来自 finite mask、sqrt、strided store 还是 wrapper/timer boundary | no-sqrt 弱正向破坏 production 语义；normal-only 接近阈值 | phase plan、QEMU correctness、asm、board repeated、Evidence Doctor | completed-no-production | `050-production-detail-ablation` |
| approximate / alternative sqrt family | phase 050 证明 `sqrt` 是成本中心 | curvature 计算 | 可能追回 full semantics 中的 `vfsqrt` 成本 | 当前 production 语义是严格 `sqrt`；近似需要误差预算和用户授权，当前 GCC/RVV include 未发现直接可用 `vfrsqrt` intrinsic | 新语义授权、math helper、correctness tolerance、board repeated、Evidence Doctor | turn_stop_deferred_requires_user_semantics_authorization | none |
| generic normal point type expansion | 泛型点类型策略文档 | `PointNT` / `PointOutT` normal-like AoS 点型 | 避免长期 exact `pcl::Normal` gate 收窄模板入口 | exact `pcl::Normal` production-public 已负向，扩大点类型前没有收益基础 | point-type expansion phase、fallback tests、board、asm、Evidence Doctor | rejected-for-now | only if a later production detail candidate becomes positive |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | helper-only diagnostic 候选成立 | QEMU correctness、board correctness、asm 和 1-run smoke 均闭合，但 doctor 暴露 `low_run_count` | repeated board summary | completed by `010` |
| 010 | PI1 production scope | 5-run repeated board 为 `weak_positive` 且 doctor 无 finding，足以进入有界 production probe 讨论 | PI1 范围、fallback、production direct tests、用户授权 | high |
| 030 | production-public negative | 真实 `Feature::compute()` 边界下 repeated board median `0.908x`，Evidence Doctor 有退化频率 Error | 用户确认回滚，或另建 production detail 消融 phase | completed by `040` for rollback |
| 040 | production detail 消融 | 已回滚当前 RVV production path，但 helper-only 与 public 证据方向不一致，仍可解释退化根因 | 新 phase plan、同边界 asm / board / Evidence Doctor | medium |
| 050 | no-sqrt 弱正向但语义不成立 | finite-only no-mask 稳定负向；no-sqrt median `1.125x` 但写零 curvature；normal-only median `1.025x` near-threshold | 只有用户允许改变 / 近似 curvature 语义或提供严格 faster sqrt helper 时恢复 | stop |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| production patch | 已完成有界 production probe，但 production-public board repeated 为 `negative`；用户已确认回滚，phase 040 已撤掉 production RVV helper / dispatch | 只有新的 production detail candidate 在同边界转为正向后才恢复 production probe |
| generic normal point type production | 首个证据只覆盖 exact `pcl::Normal` normal cloud，且 exact production-public 性能未成立 | 只有新的 production detail candidate 证明正向后，才恢复 point-type expansion |
| approximate sqrt production | phase 050 唯一较强信号来自去掉 `sqrt`，但这破坏 curvature 语义 | 用户明确授权近似 curvature 误差预算，或未来有严格 faster sqrt helper 可验证 |
