# Phase 000 Result: current-state-and-gaps

## 执行范围

本阶段按 `plan.zh.md` 启动 `transformation_estimation_2D` topic。实际执行范围与计划一致：只创建 topic-local 文档和本地 work log（工作日志），不修改 production（生产源码），不创建 `doc-rvv` 长期主题文档。

## 动作完成矩阵

| 动作 | 状态 | 证据路径 | 结论 |
| --- | --- | --- | --- |
| A1 S0 偏好冻结 | done | `tmp/rvv-work-logs/registration/transformation_estimation_2D/20260814-topic-start/s0.yaml`、`s0.zh.md` | defaults 已读取，local override 缺失，prompt override 只限定 topic 和 worker 角色。 |
| A2 源码 shape scan | done | `doc/transformation_estimation_2D-evaluation.zh.md` | 已记录四个公开入口、`ConstCloudIterator` 统一层、centroid / demean / correlation / transform 边界。 |
| A3 topic-local evaluation | done | `doc/transformation_estimation_2D-evaluation.zh.md` | 当前结论为 `diagnostic-plan-ready`，未批准 production。 |
| A4 optimization roadmap | done | `doc/optimization-roadmap.zh.md` | 已建立 candidate family、暂缓 / 拒绝路线和默认恢复队列。 |
| A5 optimization matrix | done | `doc/phases/optimization-matrix.zh.md` | 已列出 scalar baseline、fused correlation、row source carry-over、production dispatch 和 doc-suite parity 状态。 |
| A6 phase index | done | `doc/phases/README.zh.md` | 下一阶段恢复入口为 `010-scaffold-and-ordered-cloud-pair-correlation-diagnostic`。 |
| A7 current handoff | done | `tmp/rvv-work-logs/registration/transformation_estimation_2D/current-handoff/current-handoff.zh.md` | Handoff 写入本地 ignored 路径，默认不提交。 |

## 源码和数据流结论

`TransformationEstimation2D` 的 protected helper 先分别计算 source / target centroid（质心），再写出两份 4xN demean matrix（去中心化矩阵），随后做 correlation matrix（相关矩阵）乘法。2D angle 只依赖 `H00`、`H01`、`H10` 和 `H11`。Phase 010 的主候选是 fused 2D correlation accumulator（融合 2D 相关项累加器），默认从顺序点云对（ordered-cloud-pair，source/target 按相同下标一一对应）开始。

当前 public input semantics（公开入口输入语义）仍有未闭合项：`compute3DCentroid(ConstCloudIterator&)` 用 `pcl::isFinite` 过滤 centroid，但 `demeanPointCloud(ConstCloudIterator&, Eigen::Matrix&)` 会写出 iterator 中的每一行。Phase 010 必须先用测试刻画非有限 x/y/z、空输入、size mismatch 和非法 index / correspondence 的现有行为，不能把 pairwise finite mask（成对有限值掩码）直接写成 production 语义。

## Optimization Matrix 更新

| candidate family | decision | 说明 |
| --- | --- | --- |
| scalar baseline characterization | planned | 需要 public semantics tests 和 baseline bench。 |
| fused 2D correlation accumulator / ordered-cloud-pair | planned | Phase 010 默认实现 diagnostic helper 和 same-chain correctness。 |
| fused 2D correlation accumulator / indexed policies | deferred | 等 ordered-cloud-pair evidence 后逐 policy 审计。 |
| common RVV reuse audit | planned | common 层普通 cloud RVV 路径不能直接证明 iterator overload 已受益。 |
| production dispatch | not_applicable | 当前无 correctness、asm、board 或 Evidence Doctor 证据。 |
| topic-local doc suite parity | phase_deferred + unblocked | Phase 010 创建 scaffold 后补 testing overview、correctness tests、benchmark/evidence、optimization evidence 和 code map。 |

## Evidence Doctor 和 Evidence Registry

本阶段不生成 benchmark、board summary、checksum summary、asm attribution 或 EvidenceDecision，因此 Evidence Doctor（证据体检）脚本未运行。人工检查结论：

| 类别 | 状态 | 说明 |
| --- | --- | --- |
| Errors | 0 | 没有把任何性能或 production 结论写入文档。 |
| Warnings | 1 | 当前没有可执行证据；EvidenceDecision 只能停在 `diagnostic-plan-ready`。 |
| Suggestions | 2 | Phase 010 创建 JSON manifest（证据清单）入口；Phase 010 如生成 summary，应接入 `log/evidence_registry.json`。 |

Evidence registry（证据登记表）当前 `not_available`。没有 summary / manifest / doctor / analyze log 需要 freshness check（新鲜度检查）。

## Validation

| 检查 | 命令 | 结果 |
| --- | --- | --- |
| artifact tracking scan | `git status --short --untracked-files=all -- test-rvv/registration/transformation_estimation_2D tmp/rvv-work-logs/registration/transformation_estimation_2D` | topic docs 为 untracked / to-be-staged；tmp work log 被 ignore。 |
| topic untracked scan | `git ls-files --others --exclude-standard -- test-rvv/registration/transformation_estimation_2D tmp/rvv-work-logs/registration/transformation_estimation_2D` | 列出 6 个 topic 文档。 |
| ignored work log scan | `git check-ignore -v tmp/rvv-work-logs/registration/transformation_estimation_2D/...` | `.git/info/exclude:16:tmp/*` 命中，work log local-only。 |
| whitespace scan | `rg -n "[ \\t]+$" test-rvv/registration/transformation_estimation_2D tmp/rvv-work-logs/registration/transformation_estimation_2D` | 无命中。 |
| writing style trigger scan | `rg -n "<trigger words>" test-rvv/registration/transformation_estimation_2D tmp/rvv-work-logs/registration/transformation_estimation_2D` | 无命中。 |

未运行 QEMU、bench、反汇编或板卡命令。原因是本阶段没有 test / bench scaffold，也没有性能或生产接入结论。

## Doc Suite Parity 审计

| area | current shape scan | quality bar / optional calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | `README.zh.md` 已创建。 | 需要当前结论、先读路径、目录分工、命令和提交边界。 | adopted | 当前 README 覆盖这些入口。 | 随 Phase 010 scaffold 更新命令。 |
| testing-overview | not_present | 复杂 topic 默认需要测试体系总览。 | phase_deferred + unblocked | 当前还没有 test / bench target。 | Phase 010 创建。 |
| correctness-tests | not_present | 需要逐测试说明输入、断言和证明范围。 | phase_deferred + unblocked | 当前还没有 gtest。 | Phase 010 创建。 |
| benchmark-and-evidence | not_present | 需要 bench label、QEMU/board、Evidence Doctor、registry 和提交边界。 | phase_deferred + unblocked | 当前还没有 bench。 | Phase 010 创建。 |
| optimization-evidence | partial via roadmap/matrix | 需要候选到代码、target 和证据映射。 | phase_deferred + unblocked | 当前还没有 candidate code。 | Phase 010 或 Phase 020 创建。 |
| test-support-code-map | partial via evaluation Traceability Map | 需要聚合头、internal helper、src、script 和 output code map。 | phase_deferred + unblocked | 当前还没有 test support scaffold。 | Phase 010 创建。 |
| evaluation | created | 需要 S2 评估、Traceability Map 和 production boundary。 | adopted | `doc/transformation_estimation_2D-evaluation.zh.md`。 | 随证据更新。 |
| long-term doc-rvv | not_present | 只有 adopted production 行为后适用。 | not_applicable with evidence | 当前无 production patch 或 PI5 证据。 | production 采用后创建。 |
| phase index / result | created | 需要 phase 恢复入口、计划和结果。 | adopted | `doc/phases/README.zh.md`、本文件。 | Phase 010 新建 plan/result。 |
| artifact tracking | untracked topic docs; ignored work log | README / evaluation / roadmap 引用的 topic docs 必须进入 tracked / to-be-staged 或明确 local-only。 | adopted for Phase 000 | `git status --short --untracked-files=all -- test-rvv/registration/transformation_estimation_2D` 列出 topic docs。 | review 时把 topic docs 纳入 topic commit；tmp work log 排除。 |

## Continue / Stop Decision

`continue_stop_decision`：本轮可以停在 S4 边界。用户目标是开启新 topic；当前已经建立 S0、evaluation、roadmap、matrix、phase index、phase result 和 Handoff，且没有 test / bench scaffold 可以运行。

`stop_condition_hit`：用户限定本轮目标为 topic opening（开启 topic），未授权 production 修改。继续到 Phase 010 仍在当前 topic 授权范围内，但它是下一阶段工程实现，需要新增 test / bench scaffold。

`next_phase_default`：

```text
010-scaffold-and-ordered-cloud-pair-correlation-diagnostic
```

下一阶段应创建 `src/`、`include/`、`include/impl/`、Makefile 和完整 topic-local doc suite，先实现 ordered-cloud-pair fused correlation diagnostic，再运行 QEMU correctness 和 bench build / log-shape smoke。性能结论必须等待板卡或目标硬件。
