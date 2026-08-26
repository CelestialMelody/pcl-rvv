# CPPF Optimization Roadmap

## 当前边界

当前 topic scope（主题范围）只覆盖 `PointXYZRGBNormal -> CPPFSignature`、`float`、dense finite synthetic cloud（稠密有限合成点云）、prefix indices、all-pairs output 和 test-only component ablation。production 源码未修改。

默认恢复队列：无当前授权范围内必须继续的未阻塞 production 动作。若用户后续要求继续 CPPF，应先开新的 bounded profiling / scalar-output phase，而不是直接接入 RVV production patch。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `cppf-pair-hsv-batch-rvv` | Phase 000 component ablation | `f1..f10` pair math + HSV | 原本期望用 lane-local 算术覆盖彩色 pair feature。 | SoA staging、HSV mask、output scatter 写回成本过高。 | QEMU correctness、asm、board repeated、Doctor。 | rejected with evidence | none |
| `cppf-alpha-m-batch-rvv` | PPF `alpha_m` closed-form 思路 | CPPF `alpha_m` 后段 | 原本期望替代 Eigen 旋转对象构造。 | `atan2_RVV` 和 staging 成本未被公开入口放大抵消。 | closed-form 对拍、QEMU correctness、asm、board repeated、Doctor。 | rejected with evidence | none |
| `output-resize-vs-push-back-scalar` | 当前 production 使用 `push_back` | public-like all-pairs output staging | 可能降低纯标量容器写回成本。 | 不是 RVV 算术收益，且会触碰 production 语义/容量行为。 | 独立 scalar production-shaped bench、public correctness、board repeated。 | turn_stop_deferred with stop_condition_hit | 需要用户授权新的 production-shaped scalar-output topic。 |
| `direct-AoS-no-staging-rvv` | PFH/PFHRGB direct-AoS 经验 | exact RGB+normal point type | 可能减少 SoA staging 内存流量。 | CPPF all-pairs 输出仍要顺序写回；HSV 分支和 `alpha_m` 仍重。 | 新 phase plan、RVV helper、same-boundary board repeated。 | rejected with evidence for current priority | 只有 profile 证明 staging 是唯一瓶颈且用户继续本 topic 时恢复。 |
| generic point type / row-source expansion | workflow generic expansion policy | 泛型 RGB normal traits、乱序 indices、`Scalar=double` | 只有 production candidate 正向后才有价值。 | 当前无 adopted candidate，扩大范围会增加测试矩阵但不改变负向主结论。 | production candidate 正向后补 fallback/correctness/board matrix。 | not_applicable with evidence | none |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 000 | 先做 scalar output staging 或 profile，而不是继续 RVV HSV/alpha 变体。 | 两条 RVV 候选都在 board 上负向，说明当前成本更可能在 staging、math library call 或输出写回。 | production-shaped scalar-output bench 或 perf/profile；需要新授权。 | low until requested |
| Phase 000 | 若未来重开 RVV，应优先设计 direct-AoS no-staging 版本。 | SoA staging 让 pair/HSV RVV 候选退到 `0.49x`。 | 同边界 RVV-vs-scalar board repeated 和 Doctor。 | low until profile supports |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `cppf-pair-hsv-batch-rvv` | board repeated 5/5 退化，median `0.49x`；Doctor 报 `ba_degradation_frequency`。 | 新 profile 证明 staging 可消除且 direct-AoS 版本有明确收益假设。 |
| `cppf-alpha-m-batch-rvv` | board repeated 5/5 退化，median `0.82x`；Doctor 报 `ba_degradation_frequency`。 | `atan2_RVV` 成本或 Eigen baseline 占比被更细 profile 证明可改善。 |
| production integration loop | component evidence 未达到 weak-positive 门槛；没有 production patch。 | 用户明确授权 production-shaped scalar-output 或 direct-AoS RVV probe，并接受新的 PI1 gate。 |
