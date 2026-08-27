# GrabCut RVV 诊断入口

本 topic 评估 GrabCut organized n-link（规则图像邻接边）、color staging（颜色暂存）和 `segmentation/src/grabcut_segmentation.cpp` 中 GMM（高斯混合模型）概率密度公式的 RVV 可行性。当前生产源码已保留 / 采纳两个窄范围 RVV patch：`initGraph()` unknown trimap terminal weight（未知 trimap 端点权重）RVV helper，以及 `learnGMMs()` component assignment（分量归属选择）RVV helper。Phase 110 接入后 production-public（真实公开入口）`public_extract` 5-run median B/A 为 `1.207907x`，Evidence Doctor（证据体检）为 `Errors=0, Warnings=0, Suggestions=0`。Phase 120 接入后 component profile（组件剖析）显示没有当前 topic 内值得继续自动推进的新 RVV 方向。

## 阅读路径

| 文档 | 作用 |
| --- | --- |
| `doc/grabcut_segmentation-evaluation.zh.md` | 函数级评估、标量流程、候选边界和生产接入判断。 |
| `doc/optimization-roadmap.zh.md` | 跨阶段候选路线、默认恢复动作和暂缓条件。 |
| `doc/phases/README.zh.md` | phase（阶段）索引和当前恢复入口。 |
| `doc/phases/010-gmm-probability-density-diagnostic/result.zh.md` | Phase 010 结果，记录 GMM same-chain（同构链路）诊断、板卡数据和 Evidence Doctor（证据体检）边界。 |
| `doc/phases/020-terminal-weight-public-shaped-diagnostic/result.zh.md` | Phase 020 结果，记录 K=5 GMM mixture（混合概率）和 terminal `-log` 口径的 5-run repeated board 证据。 |
| `doc/phases/030-initgraph-no-solve-diagnostic/result.zh.md` | Phase 030 结果，记录 `initGraph` no-solve diagnostic（不含求解诊断）的 5-run repeated board 和 diagnostic-to-production mismatch audit（诊断到生产错配审计）。 |
| `doc/phases/040-production-integration-plan/plan.zh.md` | PI1 production integration plan（生产接入计划），冻结生产补丁前的范围、fallback（回退路径）、测试和暂停条件。 |
| `doc/phases/050-evidence-registry-target-alias/result.zh.md` | Phase 050 结果，记录 summary-only（只提交摘要）证据登记入口。 |
| `doc/phases/060-production-initgraph-terminal-evidence/result.zh.md` | Phase 060 结果，记录生产补丁、production direct（真实生产路径）测试、反汇编、板卡 repeated 和 PI5 EvidenceDecision。 |
| `doc/phases/070-public-extract-wall-time-adoption-check/result.zh.md` | Phase 070 结果，记录接入后的真实 `setBackgroundPointsIndices()` + `extract()` public wall-time（公开入口总耗时）板卡 repeated 证据。 |
| `doc/phases/080-public-extract-component-profile/result.zh.md` | Phase 080 结果，记录接入后 public-shaped profile（公开入口形态剖析）和 `learn_gmms` 组件占比。 |
| `doc/phases/090-learn-gmm-component-assignment-diagnostic/result.zh.md` | Phase 090 结果，记录 `learnGMMs()` assignment-only（仅分量归属）RVV 诊断。 |
| `doc/phases/100-learn-gmms-full-production-shaped-diagnostic/result.zh.md` | Phase 100 结果，记录完整 `learnGMMs()` production-shaped diagnostic（生产形态诊断）和 production integration authorization（生产接入授权）边界。 |
| `doc/phases/110-learn-gmms-production-integration-plan/result.zh.md` | Phase 110 结果，记录真实 `learnGMMs()` production patch、接入后板卡证据和用户确认采纳。 |
| `doc/phases/120-post-learn-gmms-adoption-profile/result.zh.md` | Phase 120 结果，接入 `learnGMMs()` 后重跑 public component profile（公开入口组件剖析），结论为 `profile_non_actionable`。 |
| `doc/phases/130-closeout-and-submit-audit/result.zh.md` | Phase 130 收尾审计，记录 production dispatch、文档套件、证据登记和提交边界检查。 |
| `doc/phases/optimization-matrix.zh.md` | candidate family（候选族）到 correctness、bench、board、asm 和 doctor 状态的矩阵。 |
| `../../../doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md` | 已采纳 production behavior（生产行为）的长期维护文档。 |

## 常用命令

| 命令 | 证据角色 |
| --- | --- |
| `make run_test_compare` | QEMU correctness（QEMU 正确性）对拍；只证明测试专用 reference 和 candidate 语义一致。 |
| `make run_bench_rvv` | QEMU bench log-shape smoke（日志形状小型验证）；不作为性能结论。 |
| `make dump_bench_rvv` | 反汇编线索，检查编译器或候选路径是否出现 RVV 指令。 |
| `make board_smoke` | 板卡 smoke（目标硬件小型验证）；只作为可运行和日志形状入口，repeated 性能需后续专门 target。 |
| `make run_gmm_evidence_doctor` | 从当前 board log 生成 GMM 专用 manifest（证据清单）并运行 Evidence Doctor。 |
| `make run_board_evidence_doctor` | 从当前 board log 生成 topic 级 manifest 并运行 Evidence Doctor。 |
| `make run_initgraph_evidence_doctor` | 从当前 `log/board` 单次 board smoke（板卡小型验证）生成 `initgraph_no_solve` manifest；单次结果通常只作初筛。 |
| `make run_initgraph_repeated_evidence_doctor` | 从 Phase 030 repeated board run directory（重复板卡目录）生成 manifest 并运行 Evidence Doctor；当前默认目录为 `doc/phases/030-initgraph-no-solve-diagnostic/repeated-board-20260827-144235`。 |
| `make run_production_repeated_evidence_doctor` | 从 Phase 060 production-detail repeated board 目录生成 manifest 并运行 Evidence Doctor；当前默认目录由 `doc/phases/060-production-initgraph-terminal-evidence/repeated-board-current-dir.txt` 指向 `doc/phases/060-production-initgraph-terminal-evidence/repeated-board-20260827-152529`。 |
| `make run_public_extract_repeated_evidence_doctor` | 从 Phase 070 production-public repeated board 目录生成 manifest 并运行 Evidence Doctor；当前默认目录由 `doc/phases/070-public-extract-wall-time-adoption-check/repeated-board-current-dir.txt` 指向 `doc/phases/070-public-extract-wall-time-adoption-check/repeated-board-20260827-clean-96x72`。 |
| `make run_public_extract_profile_repeated_evidence_doctor` | 从 Phase 080 public profile repeated board 目录生成 manifest 并运行 Evidence Doctor。 |
| `make run_learn_gmm_assignment_repeated_evidence_doctor` | 从 Phase 090 assignment-only repeated board 目录生成 manifest 并运行 Evidence Doctor。 |
| `make run_learn_gmms_full_repeated_evidence_doctor` | 从 Phase 100 full `learnGMMs()` repeated board 目录生成 manifest 并运行 Evidence Doctor。 |
| `make run_production_learn_gmms_repeated_evidence_doctor` | 从 Phase 110 `production_learn_gmms` repeated board 目录生成 manifest 并运行 Evidence Doctor。 |
| `make run_production_learn_gmms_public_repeated_evidence_doctor` | 从 Phase 110 接入后 `public_extract` repeated board 目录生成 manifest 并运行 Evidence Doctor。 |
| `make run_post_learn_gmms_profile_repeated_evidence_doctor` | 从 Phase 120 接入后 `public_extract_profile` repeated board 目录生成 manifest 并运行 Evidence Doctor。 |
| `make evidence_status` | 检查 Phase 030、Phase 060、Phase 070、Phase 080、Phase 090、Phase 100、Phase 110 和 Phase 120 summary artifact 的 registry freshness（登记新鲜度）和文档引用。 |

生成在 `log/` 和 `build/` 下的文件默认不提交。只有被 evaluation、phase result 或 Handoff 明确引用的 summary（摘要）或脱敏日志才进入提交候选。

## 当前状态

Phase 060 已把 GMM / terminal 路线推进到真实 `GrabCut<PointXYZRGB>::initGraph()` production-detail helper 边界。Milkv-Jupiter 5-run repeated board B/A 为 `3.1292, 3.1280, 3.0998, 3.0944, 3.1982`，Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。用户已确认保留 / 采纳 production patch 后，Phase 070 补跑真实公开入口 `public_extract`：Milkv-Jupiter clean 96x72 5-run B/A 为 `1.124687, 1.110452, 1.112645, 1.113482, 1.112866`，median `1.112866x`，Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。

Phase 080 接入后 profile 显示 `learn_gmms` 在 Std 构建中约占 `11.46%`。Phase 090 assignment-only 诊断在
Milkv-Jupiter clean 96x72 5-run 中得到 median B/A `3.528303x`。Phase 100 full `learnGMMs()` 生产形态诊断继续为 positive，5-run median B/A `3.048585x`，checksum 一致，Evidence Doctor 为
`Errors=0, Warnings=0, Suggestions=0`。

Phase 110 已接入真实 `learnGMMs()` production path：production-detail `production_learn_gmms` 5-run median
B/A 为 `2.755010x`，接入后 `public_extract` 5-run median B/A 为 `1.207907x`，两层 Evidence Doctor 均为
`Errors=0, Warnings=0, Suggestions=0`。用户确认按接入后板卡 positive 采纳该 patch。

Phase 120 已完成接入后 `public_extract_profile`：Milkv-Jupiter clean 96x72 5-run median B/A 为
`1.290653x`，checksum 一致，Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。RVV profile
中 `learn_gmms` 中位占比 `4.592618%`，低于继续优化阈值；主要剩余为 `initgraph_refine` 和
`graph_solve`。因此当前默认恢复动作是暂停本 topic 的自动性能探索。organized n-link 历史诊断约
`0.98x`，不建议直接生产扩展；max-flow solver 是状态机，当前拒绝 RVV 重写；non-organized KNN、
color staging、GaussianFitter accumulation 和 LMUL / ILP variants 都需要新的 profile 或用户新 scope
证明后再重开。

Phase 130 closeout audit（收尾审计）确认生产分发、长期文档、topic-local 文档套件、summary-only 证据登记
和提交边界均可支撑结束当前 topic。默认提交不包含 raw board logs、QEMU logs、build 输出或本地 Handoff。
