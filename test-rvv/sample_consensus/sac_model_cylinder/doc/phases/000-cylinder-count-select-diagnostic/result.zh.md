# Phase 000: cylinder count/select production-shaped diagnostic 结果

## 执行摘要

本阶段完成了 `SampleConsensusModelCylinder<PointT, PointNT>::countWithinDistance` 和
`selectWithinDistance` 的 production-shaped diagnostic（生产形态诊断，测试专用代码模拟真实公开入口的数据形态）。
实际范围与 `plan.zh.md` 一致：只覆盖 direct indexed `indices_`、`PointXYZ + pcl::Normal`、
float xyz/normal AoS（结构数组）布局、synthetic cylinder cloud（合成圆柱点云）和
`count/select` 两个入口；没有修改 production（生产源码），没有新增 public API（公开接口），也没有证明真实
production dispatch（生产分流）已经存在。

EvidenceDecision（证据决策）为 `partial-production-candidate`（局部生产候选）：测试专用 RVV candidate
在板卡 repeated bench（重复性能测试）中稳定正向，correctness（正确性）、QEMU（仿真器）日志形状、
反汇编归属和 Evidence Doctor（证据体检）均未出现 Error。这个结论只能支持进入 PI1
production integration plan（生产接入计划），不能写成 adopted production behavior（已采纳生产行为）。

## 计划动作回填

| 计划动作 | 状态 | 实际产物 / 命令 | 证据与结论 |
| --- | --- | --- | --- |
| RED test | done | bench 源码加入前，`make dump_bench_rvv` 因缺少 bench `main` 链接失败。 | RED 证明 Makefile 需要真实 bench source；该失败已由新增 bench source 修复。 |
| GREEN helper | done | `include/impl/sac_model_cylinder_diagnostic.hpp`、`include/sac_model_cylinder.h`；`make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare`。 | Std/RVV 各 3 个 gtest 通过，candidate 与公开标量入口在 count、select 顺序和 stale state 清理上保持一致。 |
| bench scaffold | done | `src/bench_sac_model_cylinder.cpp`、Makefile bench / board / doctor / registry target。 | bench 输出 Dataset、Iterations、Warmup、Build、Checksum 和 public / diagnostic 四行 timing；public 行只作 companion（伴随对照），diagnostic candidate 行参与本阶段取舍。 |
| QEMU / asm | done | `make dump_bench_rvv`；`make check_diagnostic_asm`。 | `countWithinDistanceCandidateRVV` 有 74 条 RVV 指令并包含 `vcpop.m`、`vfsqrt.v`；`selectWithinDistanceCandidateRVV` 有 82 条 RVV 指令并包含 `vcompress.vm`、`vse64.v`。QEMU 窄 bench smoke 只用于日志形状，不作为性能结论。 |
| board / doctor | done | `make check_board_ssh`；`make collect_repeated_board_evidence`；`make run_repeated_board_evidence_doctor`；`make record_repeated_board_evidence_state`。 | 5-run board repeated 完成；3 个 gtest 每轮通过。Evidence Doctor 为 Errors=0、Warnings=1、Suggestions=0。 |

## 当前证据路径

| 证据 | 路径 / run label | 角色 |
| --- | --- | --- |
| phase plan | `test-rvv/sample_consensus/sac_model_cylinder/doc/phases/000-cylinder-count-select-diagnostic/plan.zh.md` | 阶段范围、矩阵和停止条件。 |
| repeated manifest | `test-rvv/sample_consensus/sac_model_cylinder/doc/phases/000-cylinder-count-select-diagnostic/repeated-evidence-manifest.json` | 机器可读 5-run board repeated summary（重复板卡摘要）。 |
| Evidence Doctor Markdown | `test-rvv/sample_consensus/sac_model_cylinder/doc/phases/000-cylinder-count-select-diagnostic/repeated-evidence-doctor.md` | 人读摘要，列出 Error / Warning / Suggestion。 |
| Evidence Doctor JSON | `test-rvv/sample_consensus/sac_model_cylinder/doc/phases/000-cylinder-count-select-diagnostic/repeated-evidence-doctor.json` | 机器可读 doctor 结果。 |
| evidence registry | `test-rvv/sample_consensus/sac_model_cylinder/log/evidence_registry.json` | run label `cylinder-phase000-repeated-board` 的登记状态。 |
| raw board logs | `test-rvv/sample_consensus/sac_model_cylinder/log/board/repeated-phase000-cylinder/` | 本机 raw log，默认不提交。 |

## 分层结果

| 证据层 | 结果 | 不能证明什么 |
| --- | --- | --- |
| correctness | `run_test_compare` 通过 Std/RVV 各 3 个 gtest。 | 不证明 production dispatch，也不覆盖其它点型、`Scalar=double` 或 `getDistancesToModel`。 |
| QEMU | narrow bench smoke 可运行并输出正确日志形状。 | QEMU timing（仿真计时）不进入性能判断。 |
| disassembly（反汇编） | diagnostic RVV helper 有目标 RVV 指令归属。 | 反汇编只归属 test-only helper，不证明 production symbol 命中 RVV。 |
| board performance（板卡性能） | diagnostic candidate count median/min/max 为 `7.22x / 6.75x / 7.83x`；select 为 `6.32x / 5.98x / 6.38x`。 | public companion 行约 `1.00x`，因为 production 源码没有 cylinder RVV dispatch；不能写成 public production speedup。 |
| Evidence Doctor | Errors=0、Warnings=1、Suggestions=0。 | Warning 需要解释；当前不是 clean production adoption（干净采纳）。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role（证据角色） | `production_shaped_diagnostic`。 |
| A/B boundary（对比边界） | baseline 是 Std build 下的 `SampleConsensusModelCylinderDiagnostic::*Candidate` scalar helper；candidate 是 RVV build 下同名 test helper。public 行是 companion，不参与 production speedup 结论。 |
| 当前决策问题 | RVV-vs-scalar 候选是否值得进入 PI1 production integration plan。 |
| diagnostic 是否可外推到 production | 只能部分外推。它证明 count/select 的 indexed xyz/normal gather、axis projection、radial norm、normal angle、mask popcount 和 `vcompress` 保序写回在测试专用 candidate 中可行；它不证明真实 public overload、fallback、非 RVV 构建或 production helper 符号。 |
| comparison-boundary / baseline mismatch（比较边界 / 基线不一致）风险 | 有。公开入口仍走 production scalar；candidate 是测试专用 helper。因此下一阶段必须先冻结 production patch 范围和 fallback 矩阵。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe（有界生产探针） | 本阶段为 positive；若后续 production direct 弱、负或不稳定，不能自动回滚，需要按 PI5 等待用户确认。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已采纳 cylinder RVV family；仍需要 production public Std/RVV、fallback、asm 和 board repeated 证据闭合后，再由用户确认是否采纳。 |

## Evidence Doctor 解释

`repeated-evidence-doctor.md` 报告一个 Warning：`diagnostic candidate countWithinDistance` 的 max/min 为 `1.16`，
超过长尾阈值。该 Warning 不改变 decision bucket：5 次运行均为明显正向，min 仍为 `6.75x`，
高于计划中的 positive 阈值 `1.20x`。当前处理方式是保留 min/median/max，不剔除异常值；下一阶段若进入
production direct，应重新用同一 production boundary 采集 repeated board，并记录 taskset / governor / freq /
temperature / binary hash 等环境字段。板卡 remote make 出现 clock skew warning（时钟偏移提示），本阶段记录为环境警告，
未影响 correctness pass、summary 生成或 decision bucket。

## Phase scope 与未验证范围

`validated_scope`：`countWithinDistance` / `selectWithinDistance` 的 test-only candidate；direct indexed
`indices_`；`PointXYZ + pcl::Normal`；float xyz/normal AoS；`Eigen::VectorXf`；65536 点 shuffled adjacent pairs
bench；3 个 gtest correctness。

`unvalidated_scope`：真实 production dispatch、非 RVV fallback、`Scalar=double`、`getDistancesToModel`、
`optimizeModelCoefficients`、`projectPoints`、`doSamplesVerifyModel`、空 indices、identity indices、更多点型、
normal-like traits（法线字段特征）泛型覆盖、custom layout、真实 RANSAC 上游性能。

`point_type_expansion_queue`：后续若 production 接入只允许先冻结窄范围。泛型点型或 normal-like traits 扩展必须另起 phase，
补 traits / layout gate、fallback tests、dedicated bench、QEMU / asm、repeated board 和 Evidence Doctor。

## 文档套件审计

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | `README.zh.md` 已存在，但此前未引用 result、bench 和 evidence 白名单。 | README 应给读者入口、常用命令和 production_topic_doc 适用性。 | adopted | 本阶段同步刷新 README。 | 保持与 evaluation / phase index 一致。 |
| testing_overview | 此前缺独立 role 文档。 | 有 test、bench、board、doctor、registry，复杂 topic 默认拆出。 | adopted | 新增 `doc/testing-overview.zh.md`。 | PI1 后补 production direct target 状态。 |
| correctness_tests | 此前 correctness 说明只在测试注释和 evaluation。 | gtest 语义应有稳定字典。 | adopted | 新增 `doc/correctness-tests.zh.md`。 | production direct 后补 fallback / direct gtest。 |
| benchmark_and_evidence | 此前 bench / board / doctor 说明分散。 | board summary 和 Evidence Doctor 需要独立主归属。 | adopted | 新增 `doc/benchmark-and-evidence.zh.md`。 | production direct 后新增 production repeated summary。 |
| optimization_evidence | 此前候选状态只在 roadmap / matrix。 | 诊断候选收益和生产前置条件需索引。 | adopted | 新增 `doc/optimization-evidence.zh.md`。 | PI1 继续更新 production prerequisites。 |
| optimization_roadmap | `doc/optimization-roadmap.zh.md` 已存在但仍写 planned。 | roadmap 保存候选前沿和默认恢复队列。 | adopted | 本阶段同步刷新。 | 默认下一 phase：PI1 production integration plan。 |
| test_support_code_map | 此前缺独立 role 文档。 | helper、bench wrapper、script、output 分布多处，应可追踪。 | adopted | 新增 `doc/test-support-code-map.zh.md`。 | production patch 后加入 production helper 行。 |
| phase_index / matrix | 已存在但仍写 in_progress / planned。 | phase index 和 matrix 应可恢复当前 truth。 | adopted | 本阶段同步刷新。 | 进入 PI1 前先读 implementation 规则。 |
| evaluation_diagnostic | 已存在但证据未刷新。 | diagnostic decision 主归属在 evaluation。 | adopted | 本阶段同步刷新。 | PI1 完成后补 production 接入计划状态。 |
| production_topic_doc | 当前无 production patch / adopted behavior。 | `artifact_layout.topic_doc_template` 只适用于已采纳生产行为或 PI5 用户确认。 | not_applicable with evidence | `doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md` 不应在本阶段创建。 | 若 PI5 后用户确认采纳，再创建或刷新。 |

## Continue / stop decision

本阶段完成，`stop_condition_hit=false`。当前仍有授权范围内的下一动作：PI1 production integration plan。
PI1 只写生产接入计划，冻结候选范围、fallback、dispatch、点型 / `Scalar` 边界和证据计划；在用户没有明确授权修改
production 源码前，不进入 PI2 production patch。

`next_phase_default`：`010-cylinder-production-integration-plan`。
