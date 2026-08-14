# Phase 020 Plan: board-and-asm-evidence

## 阶段意图和边界

本阶段闭合 `two-pass centered fused 2D correlation accumulator`（两遍中心化融合 2D 相关项累加器）在
顺序点云对（ordered-cloud-pair，source/target 按相同下标一一对应）诊断层的证据形状。范围只包含
`test-rvv/registration/transformation_estimation_2D/**` 和当前 topic 的 Handoff Packet；production 源码、
公开 API、其它 row source policy（行来源策略）和长期 `doc-rvv` production 主题文档不在本阶段修改范围内。

本阶段要证明：

- QEMU smoke（小型日志形状检查）可以生成机器可读 manifest（证据清单）并由 Evidence Doctor（证据体检）检查。
- 反汇编摘要能把关键 RVV 指令归属到 `TransformationEstimation2D` 诊断候选所在的 test-support / bench binary 边界。
- board repeated benchmark（板卡重复性能测试）若可达，可以按有界预算生成 summary、manifest、doctor 和 registry 记录。

本阶段不证明：

- QEMU timing（QEMU 计时）代表目标硬件性能。
- ordered-cloud-pair 的诊断结果可以外推到 source-indexed-cloud-pair、dual-indexed-cloud-pair 或 correspondence-pair。
- production dispatch（生产分流）已经成立。

## 当前状态清单

| area | 当前状态 | 证据 / 路径 |
| --- | --- | --- |
| Phase 010 | complete | `doc/phases/010-scaffold-and-ordered-cloud-pair-correlation-diagnostic/result.zh.md` |
| correctness | pass | `make -C test-rvv/registration/transformation_estimation_2D run_test_compare`，Std / RVV 各 10 个 gtest 通过。 |
| QEMU smoke | pass / qemu_smoke_only | `log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log`，只证明 RVV bench 可运行和 checksum 非空。 |
| registry freshness | fresh | `make -C test-rvv/registration/transformation_estimation_2D evidence_status`。 |
| asm input | partial | `build/asm/riscv/bench_transformation_estimation_2D_rvv.asm` 已生成，但 Phase 010 只做了指令存在检查。 |
| board availability | configured / unchecked | `test-rvv/config.mk` 有 `REMOTE_USER` / `REMOTE_IP` / `BOARD_LABEL`，本阶段先运行 `check_board_ssh`。 |
| production | unchanged | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` 未改。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| two-pass centered fused 2D correlation accumulator | 直接累加中心化 `H00/H01/H10/H11` 可以减少动态矩阵写出和 Eigen correlation multiply。 | RVV reduction tree（规约树）改变加法顺序；需要数值预算、asm 归属和板卡数据。 |
| topic-local Evidence Doctor wrapper | QEMU smoke 和 asm 摘要可以先形成 manifest，避免继续使用人工 partial doctor。 | QEMU manifest 只能支持日志形状和证据合同，不能支持性能排序。 |
| board repeated summary | 目标硬件 repeated run 可以给出 ordered-cloud-pair 诊断的 decision bucket。 | 板卡不可达、环境波动或 Evidence Doctor Warning 可能把性能证据降级。 |
| common RVV reuse audit | 对 bench binary 的 asm / 可选 vectorization report 做源头归属，判断候选是否只是 Eigen 或 common helper 贡献。 | helper 可能被 inline，符号边界不一定稳定，需要把结论写成 attribution boundary。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| two-pass centered fused 2D correlation accumulator | ordered-cloud-pair | dense finite `PointXYZ -> PointXYZ`, `Scalar=float` | existing `run_test_compare` / `run_test_candidates` | QEMU smoke only: `run_bench_ordered_cloud_pair_smoke` | planned: repeated board if reachable | planned: topic-local asm summary | planned: QEMU manifest + board manifest if board runs | attempted / continue |
| common RVV reuse audit | ordered-cloud-pair | ordinary cloud API vs iterator path | not applicable to correctness | optional `generate_vec_report` or asm source attribution | not required in this phase | planned | manual doctor note | planned |
| production dispatch | ordered-cloud-pair | layout-gated f32 AoS | not covered | not covered | not covered | not covered | not covered | not_applicable until PI1 |

## 实现和测试动作

| id | 动作 | 产物 | 命令 / target | 完成判据 |
| --- | --- | --- | --- | --- |
| A1 | 新增 topic-local QEMU manifest wrapper | `script/generate_te2d_qemu_evidence_manifest.py` | `make generate_qemu_smoke_evidence_manifest` | 读取 RVV smoke log 和 asm 摘要，生成 `log/qemu/evidence_manifest.json`。 |
| A2 | 新增 asm attribution summary | `script/generate_te2d_asm_summary.py`、`log/qemu/asm_attribution.md` | `make generate_asm_attribution_summary` | 摘要列出 RVV 指令总数、关键 mnemonic、候选相关符号 / bench 边界和归属限制。 |
| A3 | 接入 Evidence Doctor | `log/qemu/evidence_doctor.md` | `make run_qemu_smoke_evidence_doctor` | Doctor 没有未处理 Error；Warning 写入 result / evaluation / Handoff。 |
| A4 | 记录 QEMU evidence state | `log/evidence_registry.json` | `make record_qemu_smoke_evidence_state`、`make evidence_status` | registry fresh；新增 manifest / doctor / asm summary 被登记并被文档引用。 |
| A5 | 板卡可用性检查 | Handoff / result | `make check_board_ssh` | 可达则继续 A6；不可达则写 `turn_stop_deferred with stop_condition_hit` 和解除命令。 |
| A6 | 板卡 repeated benchmark | `log/board/ordered_cloud_pair_repeated/**` | `make run_board_bench_ordered_cloud_pair_repeated` | 可达时按 5 次 run、20 次 measurement、5 次 warm-up 生成 summary / manifest / doctor / registry。 |
| A7 | 文档刷新 | phase result、matrix、roadmap、evaluation、benchmark/evidence、Handoff | no standalone command | 所有新证据路径、doctor 结果、board 状态和下一动作同步。 |

动作依赖：A1 依赖 A2 的 asm summary 路径；A3 依赖 A1；A4 依赖 A3；A6 依赖 A5 可达。

## Evidence Doctor 和 registry 规则

QEMU manifest 只使用 `evidence_role=diagnostic` / `case_kind=qemu_smoke_only`，不输出 repeated `ba_values`，避免把 QEMU timing 写成性能结论。Evidence Doctor 的预期结果：

- Errors：0；若出现 checksum / manifest shape Error，先修脚本或降级证据。
- Warnings：允许出现 `metadata_incomplete` 或 `qemu_smoke_only` 类边界提示；必须写清不能证明性能。
- Suggestions：允许建议补 binary hash、板卡环境字段或 repeated runs；写入下一阶段或 board action。

registry 记录：

- QEMU correctness logs 继续使用 Phase 010 记录。
- QEMU smoke log、manifest、doctor 和 asm summary 使用 `qemu-te2d-ordered-cloud-pair-smoke-phase020` 或等价 run label 记录。
- Board summary / manifest / doctor 只有板卡 target 成功后记录；raw `run-*` logs 默认 ignored-local。

## 板卡复跑预算和决策桶

默认 repeated board 预算：

- runs：5。
- warm-up iterations：5。
- measurement iterations：20。
- case-filter：`ordered-cloud-pair-fused`。
- 若 Evidence Doctor 显示方向接近阈值、长尾严重、`B/A < 1` 频率异常或与旧结论冲突，最多再做 1 次同边界确认复跑。

decision bucket：

- `positive`：非 4K 主 case 全部稳定大于 1.15。
- `weak_positive`：主 case median >= 1.03 且 min >= 0.97。
- `neutral`：主 case 在 0.97 到 1.03 附近。
- `negative`：主 case median < 0.97 或 min < 0.97。
- `unstable`：不同 run 或不同规模跨 bucket 摇摆。

若板卡不可达或工具失败，本阶段仍可完成 QEMU manifest / doctor / asm attribution，但 board performance 写成 blocked，不给性能结论。

## 继续 / 停止条件

继续到 PI1 的条件：

- ordered-cloud-pair board repeated summary 至少达到 `weak_positive`，并且 Evidence Doctor 无未处理 Error。
- asm attribution 能把关键 RVV 指令归属到 test-support candidate / bench binary 热点边界，或明确说明 inline 边界但不影响诊断证据。
- production 范围仍能收窄到 ordered-cloud-pair、dense finite `PointXYZ -> PointXYZ`、`Scalar=float`，并有可隔离 fallback 计划。

合法停止条件：

- 板卡不可达、ssh / rsync / board make target 失败，且本地 QEMU / asm / doctor 已闭合。
- Evidence Doctor Error 未能修复，或 evidence registry 出现无法归属的未登记覆盖。
- 板卡预算用完后 decision bucket 为 `unstable` 或证据互相矛盾。
- 继续需要修改 production 或扩大到其它 row source policy，需要用户确认。

默认下一阶段：

- 若 Phase 020 形成 partial-production-candidate：`040-production-integration-plan`。
- 若 Phase 020 为 `negative / neutral / blocked`：更新 roadmap，默认恢复到 `030-row-source-family-carryover` 或 no-production closeout 由用户 / reviewer 判断。

## 文档更新清单

本阶段结束前更新：

- `doc/phases/020-board-and-asm-evidence/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`（若新增 script / output map）
- `doc/transformation_estimation_2D-evaluation.zh.md`
- `tmp/rvv-work-logs/registration/transformation_estimation_2D/current-handoff/current-handoff.zh.md`
- `tmp/rvv-work-logs/registration/transformation_estimation_2D/current-handoff/current-handoff.yaml`

## Sibling 经验迁移审计

| 经验维度 | 相邻 topic 里的机制 | 当前 topic 是否适用 | 状态 | 证据 / 理由 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| manifest / doctor target | `transformation_estimation_svd` 和 `transformation_estimation_dual_quaternion` 有 topic-local QEMU manifest wrapper 和 `run_qemu_smoke_evidence_doctor`。 | 适用到证据结构，不迁移算法。 | adopted | 当前 topic 也需要把 QEMU smoke 和 asm input 从人工 partial doctor 升级为 manifest / doctor。 | 新增 `generate_te2d_qemu_evidence_manifest.py`。 |
| board repeated target | 相邻 topic 用 `collect_board_*_repeated` + summary / manifest / doctor / registry。 | 适用到 target 结构。 | attempted | 当前 topic 有 ordered-cloud-pair smoke case，可按相同证据层级采集。 | 若 board 可达，新增并运行 repeated target。 |
| row source conclusion | 相邻 topic 的 `full-cloud` / ordered full cloud 正负向结论。 | 不适用到算法结论。 | rejected | `transformation_estimation_2D` 的 row source 是 ordered-cloud-pair / indexed / correspondences，不能继承其它 topic 的收益。 | 只继承证据合同。 |
| production boundary | 相邻 topic 的 PI 状态。 | 暂不适用。 | deferred | 当前 production 源码未改，Phase 020 仍是 pre-production diagnostic。 | 证据支持后另写 PI1。 |

## Roadmap 同步动作

本阶段结束后必须回填：

- `two-pass centered fused 2D correlation accumulator` 的 asm / doctor / board 状态。
- 若 asm 显示主要 RVV 指令来自 Eigen 或 common helper，新增 common RVV reuse / auto-vectorization 诊断路线。
- 若 board negative 或 unstable，新增 component ablation 或 no-production closeout 路线。
- 若 board positive，新增 PI1 production integration plan 路线和暂停条件。
