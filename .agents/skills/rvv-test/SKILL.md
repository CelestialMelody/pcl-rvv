---
name: rvv-test
description: 设计、实现或审查 C/C++ RVV 优化的测试、诊断、benchmark、消融和证据日志。适用于 unit test、边界/对抗测试、数值一致性、production-shaped diagnostic、production direct、fallback tests、benchmark、component ablation、upstream smoke、QEMU、反汇编、板卡和 evidence policy。
---

# RVV Test 与证据工作流

本 skill（技能）统一管理 RVV topic（主题）的测试和证据规则。`rvv-test` 是 umbrella
skill（总称 skill）：覆盖 unit test（单元测试）、integration smoke（集成冒烟）、
numerical consistency（数值一致性）、diagnostic/probing（诊断 / 探针）、benchmark
（性能测试）、component ablation（组件消融）、QEMU、反汇编、板卡和 evidence logs
（证据日志）。旧 diagnostics / benchmarking 职责已经迁移到本 skill。

`rvv-workflow` 负责 S0-S12 生命周期、短 prompt（提示词）、偏好冻结、handoff（交接）和
reviewer（审查者）协议。`rvv-test` 负责回答“该写哪些 test/diagnostic/bench、这些证据能证明什么、不能证明什么”。
`rvv-project-config` 负责项目依赖、交叉编译、QEMU、board（板卡）和 Makefile harness（测试运行框架）的环境配置。

## 适用范围

- unit test（单元测试）和 regression（回归测试）。
- integration / E2E smoke（集成 / 端到端冒烟测试）。
- boundary/adversarial test（边界 / 对抗测试）。
- numerical consistency（数值一致性）和 floating-point（浮点）语义检查。
- production-shaped diagnostic（生产形态诊断）。
- production direct（真实生产路径证据）。
- fallback tests（回退路径测试）。
- benchmark / bench（性能测试）。
- component ablation（组件消融，用于拆分瓶颈线索）。
- diagnostic/probing（诊断 / 探针测试）。
- upstream/integration smoke（上游 / 集成冒烟测试）。
- 可选 sanitizer（运行时检查工具）和 profiling（性能剖析）。
- evidence logs（证据日志）策略：`summary-only`、`sanitized-logs`、`raw-logs`。

## 命名原则

- 新规则优先写入 `rvv-test`，不要新增独立的 diagnostics 或 benchmarking 规则源。
- `diagnostic` 只表示诊断 / 探针这一类测试，不再作为覆盖 unit、bench、消融和证据日志的总称。
- 新 agent asset 和新 topic 文档不要使用带 only 后缀的 diagnostic 标签。需要表达未接入生产时，
  写成“未接 production 的诊断结论”；需要表达筛选或 EvidenceDecision 的诊断阶段时，写成
  `diagnostic`。历史 topic 中的旧标签可在回头完善文档时同步改写。
- `benchmarking` 只表示性能测量子领域，不单独承载 QEMU、反汇编、日志、数值一致性或生产证据规则。
- test support（测试支撑代码）的聚合入口目录、聚合入口命名、topic token（主题短标识）与长名缩写策略、
  内部目录、内部头文件前缀、扩展名和 compatibility alias（兼容别名）策略，默认从 `.agents/config/defaults.yaml` 的
  `test_support` 读取；`.agents/local/user-preferences.yaml` 可覆盖本机偏好，当前 prompt 的明确要求优先。
- topic 内部若有混合用途 helper（reference、diagnostic、ablation、candidate wrapper、bench-facing helper），默认使用
  `test_support.internal_directory` 解析出的宽口径内部目录；只有内容确实是狭义 diagnostic/probing（诊断 / 探针）时才使用
  配置或当前 topic 既有结构指定的狭义诊断位置。

## 必读 References

按任务选择窄 reference：

- [references/test-taxonomy.zh.md](references/test-taxonomy.zh.md)：测试类别和证据层级。
- [references/optimization-phase-loop.zh.md](references/optimization-phase-loop.zh.md)：多阶段优化循环、阶段 plan/result 布局、optimization roadmap（优化路线图）、optimization matrix（优化矩阵）、Evidence Doctor 异常处理和 continue / stop criteria（继续 / 停止条件）。
- [references/entry-shapes-and-test-support.zh.md](references/entry-shapes-and-test-support.zh.md)：入口形态、row source policy（行来源策略）和诊断 / 生产分层。
- [references/numerical-consistency.zh.md](references/numerical-consistency.zh.md)：数值一致性、FMA（融合乘加）、reduction（规约）和反汇编归属。
- [references/performance-and-ablation.zh.md](references/performance-and-ablation.zh.md)：bench 合同、板卡性能、组件消融和负向归因。
- [references/evidence-doctor.zh.md](references/evidence-doctor.zh.md)：Evidence Doctor（证据体检 / 证据校验器），用于在 benchmark、board summary、checksum、asm attribution 和 EvidenceDecision 前发现可疑数据模式、输出 Errors / Warnings / Suggestions，并要求 worker 解释、重跑、降级证据边界或保留风险。
- [references/evidence-manifest-and-naming.zh.md](references/evidence-manifest-and-naming.zh.md)：Evidence manifest（证据清单）与命名合同，定义 topic-local wrapper、manifest 字段、Makefile target、case label 字典和 alias 迁移规则。
- [references/evidence-output-policy.zh.md](references/evidence-output-policy.zh.md)：证据日志、脚本归属、脱敏、提交边界和 summary-only 默认策略。
- [references/registration-topic-evidence.zh.md](references/registration-topic-evidence.zh.md)：registration（配准）topic 的
  transformation estimation（变换估计）、correspondence estimation（对应关系估计）和 row source 证据清单。

## 基本规则

- QEMU（仿真器）只证明 correctness（正确性）、日志形状和路径命中，不证明真实性能。
- 板卡或目标硬件 benchmark 才能支撑性能结论。
- bench compare 只能跑板卡或目标硬件。worker 默认不得在 QEMU 上执行 `run_bench_compare`、完整 bench matrix 或任何会生成 Std/RVV 数值对比表的 bench compare target；QEMU bench compare 没有性能意义且浪费时间。QEMU 侧默认只允许编译 bench binary，或在确有调试必要时运行非 compare、极小规模、少 case、少 iteration 的可运行性 smoke，并写清 `qemu_smoke_only`。凡是要给用户看的数值 bench，默认只在板卡或目标硬件上运行。
- 板卡复跑必须有 bounded rerun budget（有界复跑预算）和 decision bucket（决策桶）。数字轻微波动但决策桶不变时不要无限复跑；预算耗尽后仍摇摆时标成 `unstable`、降级结论或交给人工判断。
- 如果一次复跑改变了已经写入文档的方向、decision bucket、数值结论、Evidence Doctor 数量或证据角色，旧 summary 和 phase result 立即降级为 historical evidence；必须刷新相关 topic 文档、evaluation、Handoff Packet 和 phase 文档，不能继续把旧数值当当前 truth。
- staged candidate（分阶段候选）、production-shaped diagnostic 和 production direct 必须分层，不能互相替代。
- public-entry-shaped（公开入口形态相似）不等于 production dispatch（生产分流）。
- diagnostic evidence（诊断证据）不等于 production evidence（生产证据）。
- diagnostic 或 production-shaped diagnostic 的 repeated board 结果为 `weak`、`negative`、`neutral` 或
  `unstable` 时，worker 不能直接推出 `no-production` / `rejected`，也不能直接拒绝 bounded production
  probe（有界生产探针）。只要 diagnostic 结果参与 production 取舍，必须先完成
  `diagnostic-to-production mismatch audit`：写清 evidence role（证据角色）、A/B boundary（A/B 边界）、
  当前决策问题、diagnostic 是否可外推到 production、comparison-boundary / baseline mismatch
  （比较边界 / 基线不一致）风险、弱 / 负 / 中性 / 不稳定时 bounded production probe 的条件，
  以及 clean adoption 是否需要同一 production boundary（生产边界）内的 RVV-vs-RVV detail A/B。
- production public Std/RVV（真实公开入口标量 / RVV）positive 只证明当前 public RVV path 是否快于
  当前 public scalar path；它不能证明新 RVV family（实现族）优于已有 adopted RVV family。若决策是
  RVV-family-selection（RVV 实现族选择），必须补同一 production boundary 内的 RVV-vs-RVV detail A/B；
  否则只能写成 bounded production candidate、explicit probe（显式探针）或 experiment path（实验路径），
  不能 clean-adopt。
- component-only ablation（仅组件消融）只能提供瓶颈线索，不能替代端到端 profile（剖析），也不能单独决定 production。
- ordered-cloud-pair（顺序点云对，source/target 按相同下标一一对应）、source-indexed-cloud-pair（源索引点云对）、dual-indexed-cloud-pair（双索引点云对）和 correspondence-pair（对应关系点对）是不同 row source policy。production 必须逐 policy 独立批准。
- point type（点类型）和 row source policy 一样是独立批准维度。`PointXYZ`、`PointNormal`、
  `PointXYZINormal`、PointXYZ-like traits 集合、PointNormal-like traits 集合、source/target 混合组合、
  `Scalar` 和 layout 都必须在 correctness / fallback / bench / asm / board / Evidence Doctor 矩阵中分别闭合。
  具体点型或代表性点型的正向结果不能外推成泛型模板入口、其它 source/target 组合或其它 `Scalar` 已证明。
- `paths.test_root` 解析目录下的全局 `script/` 只放跨 topic 可复用脚本；与当前优化对象强绑定的脚本放在 `artifact_layout.topic_test_dir_template` 解析目录的本地 `script/` 下。通用 Evidence Doctor 脚本路径由 `artifact_layout.evidence_doctor_script_template` 解析，通用 evidence registry（证据登记表）脚本路径由 `artifact_layout.evidence_registry_script_template` 解析；若 raw log 解析依赖某个 topic 的 case label、helper 名、字段布局或反汇编符号，应在 topic-local `script/` 下生成 manifest 后再调用全局 doctor / registry。
- compiler auto-vectorization（编译器自动向量化）诊断默认不开启；需要评估编译器潜力或解释 missed-vectorization（未自动向量化）原因时，显式运行 topic Makefile 的 `generate_vec_report` 或等价目标。
- benchmark、board summary、checksum summary、asm attribution 或 EvidenceDecision 前必须按 `references/evidence-doctor.zh.md` 执行 Evidence Doctor（证据体检）检查；发现 Errors / Warnings / Suggestions 时，summary、evaluation 或 Handoff Packet 必须说明处理动作，不能无解释地把异常数据转成结论。
- 官方 Make / script target 覆盖证据文件时应更新 topic-local `log/evidence_registry.json` 或等价登记表；S0 恢复、phase loop 恢复和提交前必须检查 registry / manifest / doc refs，发现 `unregistered_change`、`unregistered_file` 或 `stale_doc_pending_refresh` 时先降级当前数值结论。
- 短 prompt 继续已有 topic、复杂 topic 回访或当前阶段仍有未阻塞优化动作时，必须按 `references/optimization-phase-loop.zh.md` 恢复或创建 phase plan（阶段计划），同时维护 topic-level optimization roadmap（主题级优化路线图）和 optimization matrix。roadmap 记录可尝试的 candidate family、历史经验、阶段反思中新生成的路线和下一阶段优先级；matrix 跟踪 candidate family、row source、点类型 / `Scalar`、test、bench、board、asm 和 doctor 状态。只完成一个 helper、一个 target、一次 bench 或一张表不能作为合法停止理由。
- evidence logs 默认 `summary-only`。raw run 目录不默认提交。
- 生成在 `artifact_layout.qemu_output_subdir` 或 `artifact_layout.board_output_subdir` 解析目录下的 correctness run log、bench analyze log 和 summary artifact，只有被 `paths.doc_root` 或 `paths.test_root` 解析目录下的文档明确引用为证据路径、run label 或摘要输入时，才进入提交候选；未被文档引用的日志和摘要留在本机工作区。
- closeout 或 production-candidate 文档必须把 test、bench、QEMU、反汇编和板卡证据汇总到“正确性与高效性证据链”。未接 production 的诊断结论使用“诊断证据链”，并写清 diagnostic evidence 不能替代 production evidence。

## 与其它 Skill 的边界

- `rvv-workflow`：生命周期、偏好冻结、handoff packet（交接数据包）、reviewer protocol（审查协议）。
- `rvv-implementation`：production 源码、dispatch/fallback、点类型 traits（字段特征）、注释上限。
- `rvv-documentation`：evaluation（函数级评估）、主题文档、closeout（收尾文档）。
- `rvv-project-config`：依赖库、交叉编译、QEMU / board 环境、Makefile harness 配置、env var（环境变量）解释。

如果发现测试策略缺口，优先补 `rvv-test`。如果发现环境变量、工具链或板卡配置缺口，补 `rvv-project-config`。
