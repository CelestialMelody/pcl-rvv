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
- test support（测试支撑代码）的聚合入口目录、聚合入口命名、topic abbreviation（主题缩写）要求、
  内部目录、内部头文件前缀、扩展名和 compatibility alias（兼容别名）策略，默认从 `.agents/config/defaults.yaml` 的
  `test_support` 读取；`.agents/local/user-preferences.yaml` 可覆盖本机偏好，当前 prompt 的明确要求优先。
- topic 内部若有混合用途 helper（reference、diagnostic、ablation、candidate wrapper、bench-facing helper），默认使用
  `test_support.internal_directory` 解析出的宽口径内部目录；只有内容确实是狭义 diagnostic/probing（诊断 / 探针）时才使用
  配置或当前 topic 既有结构指定的狭义诊断位置。

## 必读 References

按任务选择窄 reference：

- [references/test-taxonomy.zh.md](references/test-taxonomy.zh.md)：测试类别和证据层级。
- [references/entry-shapes-and-test-support.zh.md](references/entry-shapes-and-test-support.zh.md)：入口形态、row source policy（行来源策略）和诊断 / 生产分层。
- [references/numerical-consistency.zh.md](references/numerical-consistency.zh.md)：数值一致性、FMA（融合乘加）、reduction（规约）和反汇编归属。
- [references/performance-and-ablation.zh.md](references/performance-and-ablation.zh.md)：bench 合同、板卡性能、组件消融和负向归因。
- [references/evidence-output-policy.zh.md](references/evidence-output-policy.zh.md)：证据日志、脚本归属、脱敏、提交边界和 summary-only 默认策略。
- [references/registration-topic-evidence.zh.md](references/registration-topic-evidence.zh.md)：registration（配准）topic 的
  transformation estimation（变换估计）、correspondence estimation（对应关系估计）和 row source 证据清单。

## 基本规则

- QEMU（仿真器）只证明 correctness（正确性）、日志形状和路径命中，不证明真实性能。
- 板卡或目标硬件 benchmark 才能支撑性能结论。
- staged candidate（分阶段候选）、production-shaped diagnostic 和 production direct 必须分层，不能互相替代。
- public-entry-shaped（公开入口形态相似）不等于 production dispatch（生产分流）。
- diagnostic evidence（诊断证据）不等于 production evidence（生产证据）。
- component-only ablation（仅组件消融）只能提供瓶颈线索，不能替代端到端 profile（剖析），也不能单独决定 production。
- full-cloud（全云顺序扫描）、source-indexed（源索引路径）、dual-indices（双索引路径）和 correspondences（对应关系路径）是不同 row source policy。production 必须逐 policy 独立批准。
- `test-rvv/script/` 只放跨 topic 可复用脚本；与当前优化对象强绑定的脚本放在对应 topic 测试目录的本地 `script/` 下。
- compiler auto-vectorization（编译器自动向量化）诊断默认不开启；需要评估编译器潜力或解释 missed-vectorization（未自动向量化）原因时，显式运行 topic Makefile 的 `generate_vec_report` 或等价目标。
- evidence logs 默认 `summary-only`。raw run 目录不默认提交。
- closeout 或 production-candidate 文档必须把 test、bench、QEMU、反汇编和板卡证据汇总到“正确性与高效性证据链”。未接 production 的诊断结论使用“诊断证据链”，并写清 diagnostic evidence 不能替代 production evidence。

## 与其它 Skill 的边界

- `rvv-workflow`：生命周期、偏好冻结、handoff packet（交接数据包）、reviewer protocol（审查协议）。
- `rvv-implementation`：production 源码、dispatch/fallback、点类型 traits（字段特征）、注释上限。
- `rvv-documentation`：evaluation（函数级评估）、主题文档、closeout（收尾文档）。
- `rvv-project-config`：依赖库、交叉编译、QEMU / board 环境、Makefile harness 配置、env var（环境变量）解释。

如果发现测试策略缺口，优先补 `rvv-test`。如果发现环境变量、工具链或板卡配置缺口，补 `rvv-project-config`。
