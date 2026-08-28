# Phase 060 Result: getDistances production probe

## 执行范围

本阶段把 Phase 050 的 full-RVV `getDistancesToModel` 候选接入 production public entry（生产公开入口）做 bounded production probe（有界生产探针）。生产补丁只覆盖 `SampleConsensusModelCircle2D<PointT>::getDistancesToModel`，入口仍使用既有公开 API，RVV gate（准入条件）限定为：

- `__RVV10__` 构建；
- `PointT` 的 `x/y` 都是单个 `float` 字段；
- `pcl::index_t` 是 signed 32-bit；
- `input_->points.size()` 不超过 u32 byte offset（32 位字节偏移）可表达范围；
- row source（行来源）仍是 direct indexed `indices_`。

本阶段不把 `PointXYZ` 板卡结果外推到所有点型、`Scalar=double`、自定义 layout 或其它 SAC 模型。

## 生产补丁

| 文件 | 当前改动 | PI5 前状态 |
| --- | --- | --- |
| `sample_consensus/include/pcl/sample_consensus/sac_model_circle.h` | 新增 `getDistancesToModelStandard` 和 `getDistancesToModelRVV` 受保护声明。 | 保留为 production probe patch，等待用户确认。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | public `getDistancesToModel` 先做模型有效性检查，再按 RVV gate 分流；fallback 调用 `getDistancesToModelStandard`。RVV helper 执行 gather x/y、平方距离、`vfsqrt`、abs、`vfwcvt` 和 `vse64`。 | 不写成 adopted；不自行回滚。 |
| `test-rvv/sample_consensus/sac_model_circle/**` | 新增 production direct gtest、bench row、manifest mode、Evidence Doctor target 和 Phase 060 证据。 | 可作为 summary-only 证据候选；raw logs 默认不提交。 |

## 动作结果

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| correctness | done | `make -C test-rvv/sample_consensus/sac_model_circle run_test_compare` | Std/RVV gtest 通过；public、Standard helper 和 direct RVV helper 在 `1e-6` 误差预算内一致。 |
| production asm | done | `make -C test-rvv/sample_consensus/sac_model_circle check_getdistances_production_asm` | `getDistancesToModelRVV` 符号内确认 `vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`。 |
| repeated board | done | `collect_getdistances_production_repeated_board_evidence` | 5-run production public Std/RVV B/A 为 `1.4697, 1.4737, 1.4962, 1.4755, 1.4687`，median `1.4737x`，min/max `1.4687x / 1.4962x`。 |
| Evidence Doctor | done | `getdistances-production-repeated-evidence-doctor.md` | Errors=0，Warnings=0，Suggestions=0。 |
| registry | done | `record_getdistances_production_board_evidence_state` | Phase 060 manifest / doctor / json 已登记。 |

## Evidence paths

- manifest：`test-rvv/sample_consensus/sac_model_circle/doc/phases/060-getdistances-production-probe/getdistances-production-repeated-evidence-manifest.json`
- doctor markdown：`test-rvv/sample_consensus/sac_model_circle/doc/phases/060-getdistances-production-probe/getdistances-production-repeated-evidence-doctor.md`
- doctor json：`test-rvv/sample_consensus/sac_model_circle/doc/phases/060-getdistances-production-probe/getdistances-production-repeated-evidence-doctor.json`
- run label：`circle-phase060-getdistances-production-repeated-board`

## Checksum 和正确性口径

Phase 060 manifest 不把 raw `getDistancesToModel` checksum（校验和）作为 bitwise correctness gate（逐 bit 正确性验收）。原因是 Std build 使用标量 `std::sqrt` / double 写回，RVV build 使用 float `vfsqrt` 后 `vfwcvt + vse64` 写 double；二者允许在 `1e-6` 距离误差预算内一致，但不要求逐 bit 相同。

manifest 仍保留 raw output checksum 供审计：

- Std public `getDistancesToModel`：`13773555566871062090`
- RVV public `getDistancesToModel`：`11299951231741505995`

正确性 gate 是 QEMU 和板卡 gtest：`SampleConsensusModelCircle2D.PublicGetDistancesMatchesStandardAndDirectRVV`。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production direct（真实生产入口直连证据）。 |
| A/B boundary | Std build public `getDistancesToModel` vs RVV build public `getDistancesToModel`。 |
| 当前决策问题 | 接入后真实公开入口是否仍快于当前标量公开入口，并且 asm / correctness / Evidence Doctor 是否闭合。 |
| diagnostic 是否可外推到 production | Phase 050 不能直接外推；Phase 060 已补 production direct 证据。 |
| comparison-boundary / baseline mismatch 风险 | public overload、timer boundary、row source、mask 和 dense output 口径一致；实现族不同是本阶段目标变量。 |
| clean adoption 是否需要用户确认 | 需要。PI5 是用户检查点；当前只能报告 evidence-supported production probe，不得自行写成 adopted。 |

## EvidenceDecision

当前决策为 `PI5_pending_user_confirmation`。Phase 060 的 production public Std/RVV 证据为 positive-stable，且 Evidence Doctor 无阻塞异常；从证据看，这个 production patch 值得采纳。但按 workflow 规则，采纳前必须暂停，保留当前 patch，等待用户明确确认。

确认前不得：

- 把 `getDistancesToModel` 写成 adopted production behavior；
- 用 Phase 060 数据刷新正式 `doc-rvv` 的最终采用表；
- 创建提交；
- 自行回滚 production patch。

## 继续 / 停止判断

本阶段命中合法停止条件：PI5 用户检查点。下一步需要用户在以下方向中明确选择：

- 采纳 / 保留当前 getDistances production patch，然后进入 S11 production closeout，并用接入后的 Phase 060 板卡数据刷新正式 `doc-rvv`。
- 调整当前 patch 或补更宽范围证据，例如更多点型 dedicated board。
- 回滚当前 getDistances production patch，并保留 Phase 050/060 为未采纳生产探针证据。
