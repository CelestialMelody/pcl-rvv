# Phase 100: stick getDistances vector writeback plan

## 阶段意图和边界

本阶段只尝试 `SampleConsensusModelStick<PointT>::getDistancesToModelRVV` 的 implementation-shape（实现形态）优化：把当前 RVV loop 内的临时 `float` staging（暂存）和逐 lane（向量通道）标量写回，替换为 RVV mask / merge（掩码 / 合并）、float-to-double widening（float 到 double 扩宽转换）和 `vse64.v` 向量写回。

本阶段不修改 public API（公开接口），不扩大 row source policy（行来源策略），不改变 point type（点类型）gate，不接入新的 `Scalar`，不改 count/select 两条入口，也不把旧 Phase 080 的 board performance（板卡性能）数字自动刷新为本实现族的采纳证据。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| production | Phase 080 已接入 `countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel` 三条 production public entry（生产公开入口）。 |
| getDistances code shape | `getDistancesToModelRVV` 已向量化 indexed gather（索引离散加载）、叉积平方距离和 `vfsqrt.v`，但最终把 `sqr/dist` 暂存到 `std::vector<float>` 后用标量 lane loop 写 `std::vector<double>`。 |
| sibling / nearby evidence | `sac_model_normal_plane.hpp` 的 `getDistancesToModel` RVV 路径已有 `vfwcvt_f_f_v_f64m4` + `vse64` 的向量 double 写回形态，但它没有 stick 的 `radius_max_` penalty 分支。 |
| correctness tests | `run_stick_getdistances_tests` 可单跑 getDistances 三个 RVV correctness case；`run_test_compare` 可跑 Std/RVV aggregate。 |
| asm gate | `check_production_asm` 目前只证明 helper 内有 RVV 指令，不区分 getDistances 的最终写回是否仍由标量 lane loop 完成。 |
| board evidence | Phase 080 production public getDistances median speedup 为 2.5883x，来自旧 staged writeback 形态。 |

## 假设与候选族

候选族：`getDistances-vector-penalty-writeback`。

假设：`sqr < radius_max_^2` 的 penalty 分支可以在 RVV 内用 mask / merge 完成。mask 为 true 时保留 `dist`，false 时选择 `2 * dist`，随后使用 `vfwcvt_f_f_v_f64m4` 和 `vse64.v` 直接写入 `distances`。这应减少临时 buffer 写读和逐 lane 标量循环开销。

主要风险：

- `vmerge` 的 true / false 操作数顺序必须和 RVV intrinsic（内建函数）合同一致。
- 写回 `double` 后的数值必须和 Standard helper 保持当前测试误差预算内一致。
- 旧 Phase 080 板卡数据不能证明新 RVV family（实现族）优于旧 RVV family；本阶段若只跑本地 correctness 和 asm，只能写成 implementation-shape adopted pending board A/B。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| getDistances-vector-penalty-writeback | direct indexed `indices_` | traits-gated xyz AoS；current tests cover `PointXYZ` plus Phase 090 representative types；`Eigen::VectorXf` | production `getDistancesToModelRVV` implementation detail | planned: `run_stick_getdistances_tests` and `run_test_compare` | not planned on QEMU; board A/B needed for performance adoption | deferred unless board is run with new repeated budget | planned: `getDistancesToModelRVV` must contain `vfsqrt.v`, `vfwcvt.f.f.v` and `vse64.v` | production evidence freshness check; no new board manifest unless board is run | planned |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| tighten asm gate first | `script/check_stick_production_asm.py` | 旧实现下 `check_production_asm` 应因为缺少 getDistances 向量 double 写回而失败。 |
| implement vector writeback | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` | 移除 `staged_sqr` / `staged_dist` 和 lane loop；用 RVV mask / merge / widening store 写 `distances`。 |
| focused correctness | `make -C test-rvv/sample_consensus/sac_model_stick run_stick_getdistances_tests` | RVV build 的 getDistances 三个 case 通过。 |
| aggregate correctness | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | Std/RVV aggregate 均通过。 |
| production asm | `make -C test-rvv/sample_consensus/sac_model_stick clean_bench_rvv && make -C test-rvv/sample_consensus/sac_model_stick check_production_asm` | 三个 helper 命中 RVV，且 getDistances 命中 `vfwcvt.f.f.v` / `vse64.v`。 |
| evidence freshness | `production_evidence_status`、`repeated_evidence_status`、`vector_writeback_evidence_status` | 历史 summary evidence 和 Phase 100 summary evidence 均登记为 fresh；若因文档刷新导致 stale，先修正 registry 或降级说明。 |
| diff hygiene | `git diff --check -- <topic paths>` | 无空白错误。 |

## Evidence Doctor 和 registry 规则

本阶段若本地 correctness 和 asm 通过且板卡可用，则生成 Phase 100 独立 board manifest 和 Evidence Doctor（证据体检），不覆盖 Phase 080 文件。历史 Phase 080 production manifest / doctor / registry 只证明旧 staged writeback 生产边界的 Std/RVV 正向收益；Phase 100 manifest / doctor / registry 证明当前 vector writeback 形态的 public getDistances Std/RVV 正向收益。若后续要量化新旧 RVV family 的精确差值，需要新增同一 production boundary 内的 RVV-vs-RVV detail A/B。

## 阶段完成条件

- correctness 和 asm 均通过：本实现形态可以作为 `implementation-shape adopted` 保留，但性能收益状态为 `board A/B pending`。
- correctness 失败：回退本阶段 production helper 改动，并把候选写成 `rejected` 或 `blocked`。
- asm gate 失败：若编译器未产生预期 `vfwcvt` / `vse64`，本阶段不能声称已移除标量写回形态。
- board 不运行：不刷新 Phase 080 速度数字，不把本实现族写成性能 clean adoption。

## 板卡复跑预算和决策桶

本阶段先跑本地 correctness 和 asm gate。若需要本轮继续到板卡 A/B，预算为 5-run production repeated，沿用 Phase 080 的 `PRODUCTION_REPEATED_BOARD_DIR` 机制但必须使用新的 phase label 或在 result 中明确覆盖边界；decision bucket 使用 positive / weak-positive / neutral / negative / unstable。若无法区分旧 RVV family 与新 RVV family 的同边界比较，只能写 public Std/RVV result，不能关闭 RVV-family-selection。

## 继续 / 停止条件

若本地 correctness / asm 失败，立即修正或回退本阶段改动。若本地证据通过但未跑板卡，本阶段可以停在 `board A/B pending`，因为性能 clean adoption 需要新的板卡预算和可能的同边界旧/新 RVV 对比。若板卡可用且用户要求继续证明收益，应创建或扩展 production board evidence phase，而不是复用旧 Phase 080 数字。

## 文档更新清单

更新本 phase `result.zh.md`、phase index、optimization matrix、optimization roadmap、topic-local optimization evidence、benchmark/evidence 和长期 `doc-rvv` 中描述 getDistances 当前实现形态的段落。长期 `doc-rvv` 只能写源码事实和本地 correctness / asm 事实；性能数字继续标注为 Phase 080 旧基线，除非本阶段实际重跑板卡。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-detail implementation-shape；若后续跑 public bench，再另记 production-public。 |
| A/B boundary | production detail helper `getDistancesToModelRVV`。 |
| 当前决策问题 | RVV-family-selection：新向量写回形态是否比旧 staged scalar lane 写回更值得保留。 |
| diagnostic 是否可外推到 production | 不需要 diagnostic 外推；代码直接修改 production helper。 |
| comparison-boundary / baseline mismatch 风险 | 有。Std/RVV public speedup 只能说明新 RVV 相对 Std；不能证明新 RVV family 优于旧 RVV family。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；本阶段已经是有界 production detail probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要，若要声称性能优于旧实现族。只做 correctness / asm 时不能写性能 clean adoption。 |
