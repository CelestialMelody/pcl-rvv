# Phase 020 Result: circle2d getDistances 诊断消融

## 当前结论

本阶段只在 `test-rvv/sample_consensus/sac_model_circle/` 内新增
`getDistancesToModel` test-only candidate（仅测试使用候选），没有修改 production
源码，也没有改变 Phase 000 的 PI5 production candidate（生产候选检查点）状态。

当前证据不支持把这一路 `RVV sqr distance + scalar sqrt + dense double store`
形态推进到 production probe（生产探针）：5-run board repeated diagnostic（板卡重复诊断）
中，`B/A = public_getDistances_ms / candidate_getDistances_ms` 的 median 为 `0.6590x`，
min/max 为 `0.6528x / 0.6629x`，5/5 都低于 1。这个结论只拒绝当前 test-only helper
形态；它不能直接推出 `getDistancesToModel` 永远不适合 RVV，也不能替代真实 production
direct（直接生产路径）证据。

## 阶段范围

| 维度 | 已验证范围 |
| --- | --- |
| 入口 | `SampleConsensusModelCircle2D<PointXYZ>::getDistancesToModel` public row 与 test-only `getDistancesToModelCandidate` |
| 证据角色 | production-shaped diagnostic（生产形态诊断），不是 production direct |
| row source | direct indexed `indices_`；bench 使用 shuffled adjacent pairs（相邻乱序索引） |
| 点型 / layout | `PointXYZ` / float x-y AoS |
| 候选形态 | RVV 负责 x/y gather（离散加载）和平方距离；每个 lane（向量通道）仍标量 `sqrt` 并写 `double` |
| 不覆盖 | production dispatch、更多点型、`Scalar=double`、identity-index strided load、其它 row source 或更窄 sqrt / store 消融 |

## 动作回填

| action | 状态 | 命令 / 证据 | 结果 |
| --- | --- | --- | --- |
| RED 测试 | done | `make -C test-rvv/sample_consensus/sac_model_circle run_test_rvv TEST_ARGS="--gtest_filter=SampleConsensusModelCircle2D.GetDistancesCandidateMatchesPublicPath"` | helper 实现前因 `getDistancesToModelCandidate` 未声明编译失败，证明测试能抓住候选缺口。 |
| test-only helper | done | `src/test_sac_model_circle.cpp`、`src/bench_sac_model_circle.cpp` | 测试与 bench 中的派生类提供 `getDistancesToModelCandidate`；不修改 production。 |
| QEMU correctness | done | `make -C test-rvv/sample_consensus/sac_model_circle run_test_compare` | Std 5/5、RVV 5/5 通过；QEMU 只作为 correctness（正确性）和日志形状证据。 |
| 单 case correctness | done | `make -C test-rvv/sample_consensus/sac_model_circle run_circle_getdistances_candidate_test` | RVV 构建下 `GetDistancesCandidateMatchesPublicPath` 1/1 通过。 |
| asm attribution | done | `make -C test-rvv/sample_consensus/sac_model_circle dump_bench_rvv` | `getDistancesToModelCandidateRVV` 因 bench `noinline` 归属清晰，manifest 统计 13 条 RVV 指令；public `getDistancesToModel` 统计 44 条 RVV 指令，属于编译器 / 库路径可见指令，不能当手写 production RVV。 |
| board repeated diagnostic | done | `SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh make -C test-rvv/sample_consensus/sac_model_circle collect_getdistances_repeated_board_evidence` | 5 轮板卡 smoke 中 RVV gtest 每轮 1/1 通过；bench 日志位于 `log/board/repeated-20260828-phase020-getdistances-ablation/run-*`。 |
| Evidence Doctor | done | `make -C test-rvv/sample_consensus/sac_model_circle record_getdistances_board_evidence_state` | manifest / doctor / registry 已生成并登记；doctor 结果见下文。 |

## 诊断证据链

板卡数据集：`synthetic sac_model_circle direct indexed shell cloud`
（65536 points，shuffled adjacent pairs），每轮 200 iterations、5 warmup iterations，
5-run repeated board，设备记录为 `Milkv-Jupiter`。远端 make 输出仍有 clock skew warning
（时钟偏差警告）；本阶段 checksum 稳定、gtest 通过、5-run 方向一致，因此该 warning 记录为环境提示，
不解释为方向反转。

| diagnostic row | public RVV ms values | candidate RVV ms values | B/A values | median | min/max | `B/A < 1` | decision bucket |
| --- | --- | --- | --- | ---: | ---: | ---: | --- |
| `getDistancesToModel` public vs test-only candidate | 2.227566, 2.215559, 2.209252, 2.246668, 2.264949 | 3.376711, 3.393894, 3.372058, 3.389310, 3.437005 | 0.6597x, 0.6528x, 0.6552x, 0.6629x, 0.6590x | 0.6590x | 0.6528x / 0.6629x | 5/5 | negative |

Evidence Doctor（证据体检）：

- manifest：`test-rvv/sample_consensus/sac_model_circle/doc/phases/020-circle-getdistances-ablation/getdistances-repeated-evidence-manifest.json`
- report：`test-rvv/sample_consensus/sac_model_circle/doc/phases/020-circle-getdistances-ablation/getdistances-repeated-evidence-doctor.md`
- run label：`circle-phase020-getdistances-repeated-board`
- result：Errors=1，Warnings=1，Suggestions=0

Doctor 的 Error 是 `ba_degradation_frequency`：5/5 B/A 都低于 1。这里不需要继续追加同边界复跑，
因为全部样本稳定落在 negative bucket（负向桶），没有跨桶摇摆。Doctor 的 Warning 是
`fewer_instructions_but_slower`：candidate RVV 指令数 13 少于 public row 的 44，但运行更慢。
合理解释是当前候选只向量化了平方距离前半段，却引入临时 float buffer、逐 lane scalar `sqrt`、
double store 和额外函数边界，瓶颈不由 RVV 指令数决定。若未来恢复该入口，应先做更窄的 sqrt/store
成本拆分或寻找真正能批量化 `sqrt` / double 输出的实现族，而不是把当前 helper 直接接入 production。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；用于判断当前 helper 形态是否值得后续 probe。 |
| A/B boundary | RVV binary 内 public overload companion vs test-only helper；manifest 已把 `boundary`、`wrapper`、`timer_boundary`、`reduction` 差异列入 allowed mismatch。 |
| 当前决策问题 | 当前 `RVV sqr + scalar sqrt/store` 形态是否值得申请 bounded production probe。 |
| diagnostic 是否可外推到 production | 不可直接外推。candidate helper 接近 production 内核，但没有真实 public dispatch，也没有 production fallback matrix。 |
| comparison-boundary / baseline mismatch 风险 | 存在。public row 与 test-only helper 的 wrapper 和计时边界不同，因此结果只能说明当前 helper 形态在相同输入上不占优。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为稳定 negative，不建议为当前 helper 形态申请 production probe；只有出现新实现族，例如 RVV sqrt helper、避免 dense double store 或消除临时 buffer，才恢复诊断。 |
| clean adoption 是否需要 production evidence | 需要。任何后续 production 接入都必须另写 PI plan、production direct correctness、fallback、asm 和 repeated board evidence；本阶段不能 clean-adopt。 |

## EvidenceDecision

| 问题 | 决策 |
| --- | --- |
| 当前 test-only getDistances candidate | `rejected with diagnostic evidence`：当前形态稳定慢于 public row，不进入 production probe。 |
| `getDistancesToModel` production 状态 | `scalar retained`：production 保持标量，不新增 `getDistancesToModelRVV`。 |
| select/count production patch | Phase 020 结束时仍为 `PI5 checkpoint still pending`；后续 Phase 040 已按用户确认采纳为 adopted production behavior。 |
| topic closeout | 后续 Phase 040 已完成 S11 production closeout，并把 identity-index strided load 关闭为 rejected with strict A/B evidence。 |

## Continue / Stop

当前阶段闭合，Phase 020 内没有未阻塞动作。本文件保留当时的 PI5 pending 语境；当前 topic
状态已经由 Phase 040 更新为 select/count production adopted、getDistances 当前 helper rejected、
identity 当前 helper rejected。恢复时应以后续 Phase 040 result 和 current handoff 为准。
