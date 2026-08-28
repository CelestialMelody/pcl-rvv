# Phase 040: stick getDistances diagnostic result

## 阶段结论

`getDistances-indexed-gather-f32m2-sqrt-store` 已完成本阶段 diagnostic（诊断）闭环。它只证明 `SampleConsensusModelStick<PointT>::getDistancesToModel` 在 `PointXYZ`、direct indexed row source（直接索引行来源）、float xyz AoS（结构数组）、`Eigen::VectorXf` 系数、`radius_max_` penalty（外点惩罚）和测试专用 candidate（候选实现）边界下，RVV（RISC-V Vector，可变长向量扩展）路径可以复刻当前源码语义，并在板卡 repeated bench（重复性能测试）中表现为 positive-stable（稳定正向）。

本阶段没有修改 production（生产源码）`sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp`。结论是 `partial-production-candidate`：可以写 PI1 production integration plan（生产接入计划），但不能把测试专用 helper 写成 production direct evidence（真实生产路径证据）或 adopted production behavior（已采用生产行为）。

## 计划回填

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| RED test | done | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` 曾因缺少 `getDistancesToModelCandidate` 编译失败 | 测试先卡住缺口，证明新增 case 不是空跑。 |
| GREEN candidate | done | `include/impl/sac_model_stick_diagnostic.hpp` 新增 `getDistancesToModelCandidate`，随后 `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` 通过 Std/RVV 6/6 | candidate 保持 direction coefficient（方向系数）语义、dense 输出顺序和 penalty 分支。 |
| bench 扩展 | done | `src/bench_sac_model_stick.cpp` 输出 public/candidate 的 count、select 和 getDistances 行 | getDistances 行纳入板卡 repeated manifest。 |
| asm attribution（反汇编归属） | done | `build/asm/riscv/bench_sac_model_stick_rvv.full.asm` | `getDistancesToModelCandidateRVV` 符号内可见 indexed load、`vfmacc.vv`、`vfsqrt.v` 和 staged store；manifest 统计 candidate 行 RVV 指令数为 44。 |
| board repeated | done | `SSH_AUTH_SOCK=/run/user/$(id -u)/keyring/ssh make -C test-rvv/sample_consensus/sac_model_stick collect_repeated_board_evidence` | 5-run budget 已用完，candidate 决策桶稳定正向。 |
| manifest / doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_stick record_repeated_board_evidence_state` | 生成并登记 Phase 040 manifest、Evidence Doctor 和 JSON。 |

## 正确性与语义证据

`getDistancesToModel` 和 count/select 的系数语义不同：当前源码把 `model_coefficients[3..5]` 直接作为 line direction（线方向）归一化，而不是第二个端点。测试新增两个 case：

- `GetDistancesCandidateMatchesPublicDirectionCoefficientSemantics`：验证 candidate 与公开入口在方向系数语义下一致，避免误复用 count/select 的端点语义。
- `GetDistancesCandidatePreservesPenaltyAndDenseIndexedOrder`：验证乱序 `indices_` 下输出仍按 dense（稠密）序列写入 `distances[i]`，并保留 `sqr_distance >= radius_max_^2` 时 `2 * sqrt(sqr_distance)` 的外点惩罚。

QEMU correctness（QEMU 正确性验证，不代表真实性能）命令为：

```bash
make -C test-rvv/sample_consensus/sac_model_stick run_test_compare
```

当前 Std/RVV 两个构建均通过 6 个 gtest。

## 板卡性能

当前性能证据来自 `test-rvv/sample_consensus/sac_model_stick/doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-manifest.json`，run label（运行标签）为 `stick-phase040-repeated-board`，输入规模为 65536 点、200 次 iteration（迭代）和 5 次 warmup（预热）。QEMU 不参与性能结论。

| case | Std ms/iter | RVV ms/iter | speedup min / median / max | decision bucket | 证据角色 |
| --- | --- | --- | --- | --- | --- |
| public `getDistancesToModel` | 2.003038, 1.971896, 1.974417, 1.980677, 2.136907 | 2.140162, 1.979603, 2.153360, 2.158769, 2.140994 | 0.9169x / 0.9359x / 0.9981x | negative cross-check | 公开入口当前仍未接 RVV；该行只说明 Std/RVV build 对照没有生产收益。 |
| diagnostic candidate `getDistancesToModel` | 1.977217, 1.992251, 1.983393, 1.987454, 2.165201 | 0.777953, 0.783555, 0.773897, 0.777765, 0.777469 | 2.5416x / 2.5553x / 2.7849x | positive-stable | 测试专用 candidate 的 production-shaped diagnostic 性能证据。 |

## Evidence Doctor

Evidence Doctor（证据体检）输入和输出：

- `test-rvv/sample_consensus/sac_model_stick/doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_stick/doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_stick/doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-doctor.json`

结果为 Errors=1、Warnings=0、Suggestions=0。唯一 Error 是 public `getDistancesToModel` 行的 `ba_degradation_frequency`：5/5 次 B/A 都低于 1。处理动作不是重跑或判定 candidate 失败，而是把 public 行降级为 negative cross-check（负向交叉检查）。理由是该行的 A/B boundary（A/B 边界）是公开入口 Std build 对 RVV build，但 production 源码没有 getDistances RVV dispatch；它不能支撑 candidate 取舍，只能提示当前公开入口在 RVV build 中没有生产收益。

candidate 行没有 Evidence Doctor issue。当前阶段只把 candidate 行用于 `partial-production-candidate` 判断；若后续进入 PI2-PI5，必须生成 production public 或 production detail manifest，并重新运行 Evidence Doctor。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`；代码只在 `test-rvv` 派生类中。 |
| A/B boundary | test helper：`SampleConsensusModelStickDiagnostic::getDistancesToModelCandidate` 的 Std/RVV 对照；public 行只作未接 RVV 的交叉检查。 |
| 当前决策问题 | 当前 sqrt/store candidate family 是否值得进入 bounded production probe（有界生产探针）。 |
| diagnostic 是否可外推到 production | 只能部分外推到公式、indexed gather（索引离散加载）、`vfsqrt` 和 dense output 方向；不能外推到 production dispatch、fallback、public entry 真实收益或泛型点型。 |
| comparison-boundary / baseline mismatch 风险 | public 行和 candidate 行不是同一 production boundary；public 行触发 Error 后已降级，不参与 candidate 正向结论。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | candidate 不是弱 / 负 / 中性 / 不稳定；若后续 production public 证据变弱或退化，必须在 PI5 暂停等待用户判断。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前 stick 没有已采用 getDistances RVV family，所以 PI2-PI5 后可先用 production public Std/RVV 判断是否采纳；若同时引入其它 getDistances RVV family，则需要同边界 RVV-vs-RVV detail A/B。 |

## 范围边界

已验证范围：`getDistancesToModel`、direct indexed `indices_`、`PointXYZ`、float xyz AoS、`Eigen::VectorXf` 系数、`radius_max_` penalty、dense `std::vector<double>` 输出、规模 65536 的 synthetic stick-distance bench。

未验证范围：production dispatch（生产分流）、production fallback（回退路径）、泛型点类型、`Scalar=double`、真实 RANSAC 上游路径、identity indices fast path、非 AoS 布局，以及 count/select/getDistances 三入口合并 production patch。

## EvidenceDecision

本阶段决策为 `partial-production-candidate`。`getDistancesToModel` candidate 的 correctness、asm 和板卡 repeated 证据支持继续写 `050-stick-getdistances-production-integration-plan`，但 production patch 需要用户明确授权。没有授权前，production 源码保持不变，`doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md` 仍为 not_applicable。

## registry 状态

`log/evidence_registry.json` 已登记 Phase 040 的 manifest、Evidence Doctor Markdown 和 JSON，run label 为 `stick-phase040-repeated-board`，case filter 为 `stick-getdistances-phase040-repeated`。当前 result 明确引用这三份摘要证据；raw board logs 和 build 产物仍按 summary-only 策略留在本机。

## 继续 / 停止判断

Phase 040 内没有未完成动作。当前 topic 仍有未阻塞但需要授权的 production integration loop（生产接入闭环）方向：count、select 和 getDistances 都已有 PI1 或即将新增 PI1 计划；进入 PI2 production patch 必须等待用户明确授权。

`next_phase_default`：创建并维护 `050-stick-getdistances-production-integration-plan`，然后停在 `pending_user_authorization_for_PI2`。如果用户不授权 production，当前 topic 只能保持三个入口的 diagnostic / partial-production-candidate 状态，不能写成 adopted production。
