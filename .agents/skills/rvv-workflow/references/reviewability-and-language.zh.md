# RVV Agent 可审查性与术语解释规则

本参考适用于所有 RVV agent 工作：筛选、诊断、实现、benchmark、文档、数学函数拟合、closeout 和 reviewer 汇报。目标是让读者不用猜术语，也能看懂代码、测试和证据链。

## 1. 语言模式

默认使用中文主导：

- 回复和文档：中文解释为主，保留必要英文术语、函数名、target 名和日志原文。
- 配置解析出的测试资产、scratch prototype、诊断代码注释：中文解释为主；必要英文 API / 术语可以保留。
- production 代码注释：保持克制，只解释维护边界、fallback、语义风险和数据布局理由。

如果用户或项目要求英文主导，也不能省略术语解释。英文注释、英文文档或英文输出中的专有术语首次出现时，也要写成：

```text
English term (plain-English explanation; optional Chinese explanation if the reader is Chinese)
```

例如：

```text
lane-level helper (a helper that operates on one RVV vector register group and vl; 单个 RVV 向量寄存器级 helper)
```

可选注释风格：

- 中文注释：`核函数（kernel，只处理约化后小区间，不是完整 sin/cos）`
- 英文加中文注解：`kernel（核函数，只处理约化后小区间，不是完整 sin/cos）`
- 英文注释加英文解释：`kernel (the polynomial on the reduced interval, not the full sin/cos helper)`
- 中英双写：先写中文解释，再给英文术语和英文短释义；适合长期 test/prototype 文件。

不要写只有英文标签堆叠、没有解释的注释，例如 `lane-level batch wrapper sanity gate`。

## 1.1 开工前注释策略

worker 开始写代码或文档前，应在 S0 报告中冻结本轮注释策略。没有用户特别指定时，默认使用：

- 配置解析出的测试资产、diagnostic（诊断代码）和 prototype（原型代码）：详细中文注释。文件级说明、非平凡函数说明、复杂循环和 gate（可失败验收条件）前的块级说明都要保留。
- production（生产源码）：适中注释。只解释维护边界、fallback（回退路径）、dispatch（分流逻辑）、数值风险、数据布局和与标量路径衔接的理由，不写逐行教材。
- 文档、测试输出、Makefile 和 board.mk：中文主导，英文术语首次出现带中文解释。

如果用户要求显式选择，按下面三个问题记录结果：

1. 代码注释策略：不注释 / 简要注释 / 详细注释。
2. 注释语言策略：仅中文 / 仅英文 / 中英双写。
3. 中英双写顺序：中文在前 / 英文在前。

注释策略是本轮工作合同，不是事后润色项。若 reviewer 指出测试资产或 diagnostic 注释不足，worker 应先补可审查性，再继续扩大实现。

## 2. 自然中文工程说明

中文主导时，解释要像工程说明，不要像机器翻译或模板填空。目标是让读者快速理解“为什么这段代码/测试/证据存在”，而不是只看到术语对照。

优先使用自然标题：

- `本文件做什么`
- `阅读提示`
- `这个测试验证什么`
- `这段代码为什么存在`
- `当前不覆盖什么`

避免生硬标题：

- `中文执行地图`
- `类别`
- `生产形态证据入口说明`
- `集成原型路径命中说明`

函数或代码块说明可以包含“作用、调用者、证据角色”，但不要机械写成模板。优先写成完整句子。

不推荐：

```text
中文执行地图：
1. scalar reference path...
2. RVV path...

作用：构造输入。调用者：run_test。类别：production integration prototype gate。
```

推荐：

```text
本文件做什么：
这个测试使用一份仅测试使用的 RangeImageSpherical 实现，验证 calculate3DPoint
能否通过 RVV sincos helper 得到和标量参考链路一致的 xyz 结果。测试不会修改
production 头文件，也不覆盖 base RangeImage。

这个函数生成 RangeImageSpherical 的 image_x / image_y / range 样本，供接入原型
验收使用。它覆盖常规网格和边界点，用来证明输入角度落在有限域 helper 的合同内。
```

术语替换要服从上下文，不要逐词硬替换。常见自然写法：

- `test-only` 写成 `仅测试使用` 或 `测试专用`。
- `include shadow` 写成 `测试专用头文件覆盖`，第一次出现时说明它只影响某个 target 的 include 路径。
- `gate` 写成 `验收条件` 或 `失败会返回非 0 的检查`。
- `production integration prototype` 写成 `生产接入原型`，并说明它是实验代码还是最终生产代码。
- `caller-shaped smoke` 写成 `调用方形态冒烟测试`，并说明它模拟哪个调用方输入。
- `reference path` 写成 `参考链路`，并说明参考值来自 libm、double reference 还是原标量实现。
- `RVV path` 写成 `RVV 执行链路`，并说明是否真的执行 RVV intrinsic。

如果一句话里出现多个英文术语，先用中文说明主干，再把英文原词放到括号里。不要让括号和英文词把句子切碎。

## 3. 英文术语首次出现必须解释

中文回复、中文文档、代码注释和测试输出中，英文术语首次出现时写成：

```text
英文术语（中文解释）
```

后续可以写成“中文主名 + 英文原词”，例如“入口形态 entry shape”“可失败验收 gate”。

常见例子：

- `entry shape（入口形态，测试或诊断如何模拟真实公开入口调用）`
- `local fragment（局部片段，只覆盖算法中的一小段代码）`
- `production-shaped diagnostic（生产形态诊断，尽量复用真实入口状态和调用方式的 test-only 诊断）`
- `production direct（直接生产路径证据，真实生产入口命中 RVV 分流后的证据）`
- `same-chain（同构链路，标量和 RVV 按同一数学/逻辑操作顺序对拍）`
- `reference path（参考链路，通常是原标量实现、libm 或 double reference）`
- `RVV path（RVV 链路，实际执行 RVV intrinsic 的路径）`
- `gate（会导致测试失败的验收条件）`
- `fallback（回退路径，不满足 RVV 条件时继续使用标量或既有实现）`
- `dispatch（分流逻辑，决定走 RVV 还是 fallback）`
- `smoke（小型下游验证，用于证明某个调用形态的风险）`
- `caller-shaped smoke（调用方形态 smoke，按真实调用方输入形状构造的下游验证）`
- `benchmark / bench（性能测试）`
- `QEMU correctness（QEMU 正确性验证，不代表真实性能）`
- `board evidence（板卡证据，来自目标硬件的性能或正确性结果）`

如果同一回复中术语很多，不要连续堆英文。优先用中文概念组织句子，再保留英文原词。

推荐：

```text
调用方形态 smoke（caller-shaped smoke）已经是可失败验收 gate（失败会返回非 0）。
```

不推荐：

```text
caller-shaped smoke now has a sanity gate and batch RVV path.
```

对于不是标准库 API 或论文中固定使用的词，优先使用中文主称，再把英文放进括号。英文保留的目的应是方便读者对应源码、case 名、反汇编或 RVV intrinsic，而不是让标题和段落变成英文标签串。例如：

- `full-cloud` 写成 `全云顺序扫描（full-cloud，source/target 按相同下标一一对应）`。
- `correspondences` 写成 `对应关系索引路径（correspondences，由 index_query/index_match 指定点对）`。
- `staging` 写成 `分阶段暂存（staging，把 RVV 算出的中间量交给后续阶段）`。
- `lane` 写成 `向量通道（lane，RVV 向量寄存器中的一个元素位置）`。
- `gather` 写成 `离散加载（gather，按索引读取不连续地址）`。
- `stride load` 写成 `跨步加载（stride load，按固定字节间隔读取结构数组字段）`。

如果某个英文词已经出现在函数名、benchmark case 名或反汇编指令里，可以保留英文原词，但附近要用一句话说明它在当前 topic 中的具体含义和证据边界。

不要在新 agent asset 或新 topic closeout 中使用带 only 后缀的 diagnostic 标签。
诊断已经是 RVV 证据链的必要层级；真正需要表达的是是否已经接入 production（生产源码）
以及证据能覆盖到哪里。未接 production 时写“未接 production 的诊断结论”；作为阶段性策略时写
`diagnostic`（诊断）。历史文档中保留的旧词，应在回头完善该 topic 时按上述语义改写。

纯英文文本中也必须解释术语，推荐：

```text
caller-shaped smoke (a downstream smoke test that uses inputs shaped like a real caller)
```

如果读者是中文使用者，优先补中文解释：

```text
caller-shaped smoke (a downstream smoke test shaped like a real caller; 调用方形态 smoke)
```

## 4. 解释术语的证据边界

术语不仅要翻译，还要说明它能证明什么、不能证明什么。

- `local fragment（局部片段）` / `local fragment (a small isolated code section)` 只能证明局部代码正确或有潜在收益，不能替代入口层证据。
- `caller smoke（调用方 smoke）` / `caller smoke (a small downstream test for one caller shape)` 只能证明某个下游形态风险，不能替代专项 correctness、same-chain 对拍或板卡 bench。
- `QEMU correctness（QEMU 正确性）` / `QEMU correctness (functional evidence, not performance evidence)` 证明构建、路径和功能，不证明目标硬件性能。
- `board bench（板卡性能测试）` / `board bench (performance measurement on target hardware)` 证明目标硬件上的性能信号，但仍要看覆盖面、fallback 和维护成本。
- `production-shaped diagnostic（生产形态诊断）` / `production-shaped diagnostic (a test-only diagnostic shaped like the real production entry)` 比局部片段更接近真实入口，但仍不是 production direct。

## 5. 测试资产 / Prototype 代码注释标准

production 代码注释应克制，只解释维护边界、fallback、语义风险和数据布局理由。配置解析出的测试资产、scratch prototype、诊断代码和参数脚本的读者主要是 reviewer，注释可以更详细。

长 C++ 测试或诊断文件应包含：

- 文件级阅读提示：本文件做什么，`main()` 或 Makefile target 会按什么顺序运行。可以写“本文件做什么”，不要固定写成“中文执行地图”。
- 术语说明：reference、scalar same-chain、RVV path、smoke、bench 分别是什么意思。即使整段注释是英文，也要用括号解释这些词。
- 函数级说明：每个非平凡函数至少说明作用、调用者、证据角色。可以写成自然句，不要机械套用“作用/调用者/类别”模板。
- 测试级说明：如果 `TEST`、`TEST_F`、`TYPED_TEST` 或同等 benchmark case 的作用不能只从名称看出，在前面用 1-2 句中文说明“这个测试验证什么、为什么需要、失败时说明哪条证据断了”。如果测试名已经非常清楚，也至少在附近的表格或文件级说明中逐项解释。
- 块级说明：复杂循环、mask、staging、特殊值、误差统计、checksum、gate 判断前应有短注释。
- 边界说明：明确不覆盖哪些 production 行为，例如真实 dispatch、fallback、world transform、完整对象状态、其它 caller 形态。

不要把 production 代码写成逐行教材；但 test/prototype 中可以写接近逐段解释的注释，帮助 reviewer 快速审查。

### 5.1 诊断 helper 注释下限

配置解析出的测试资产中的 production-shaped diagnostic（生产形态诊断）或 test support（测试支撑代码）通常是 reviewer 最难读的部分，不能只靠文件头说明。下列非平凡 helper 需要在函数前或相邻块中有中文说明：

- 标量参考 helper：说明它复刻哪段 production 语义，哪些输入检查、公式、状态更新或 solver 边界必须保持一致。
- RVV lane / mask helper：说明 mask（掩码）代表什么，和 production 的有限值检查、predicate（谓词）或分支语义如何对应。
- staging / buffer / `vcompress` helper：说明为什么暂存、为什么压缩、保序性如何影响后续标量 tail，以及这段证据能证明什么、不能证明什么。
- candidate 入口 helper：说明它对应哪个公开入口形态，何时命中 RVV，何时 fallback，额外 index / weight / offset 展开是否属于 bench 计时边界。
- solve、矩阵构造或数学函数 helper：说明它是否在逐点热点循环内。如果每次 estimate 只执行一次，通常保留标量；若要向量化，必须先有调用频率和收益证据。

这些说明不需要逐行解释 intrinsic（内建函数），但要让 reviewer 能在不回看对话的情况下回答：“这段 helper 为什么存在，和 production 哪段语义对齐，失败会破坏哪条证据？”

需要长期保留性能、checksum 或 asm 证据的 topic-local 脚本，应优先生成 Evidence Doctor（证据体检）可读取的 JSON manifest，而不是只打印无法复核的 Markdown 表格。生成 manifest 的脚本如果依赖当前 topic 的 case label、helper 名、字段布局或反汇编符号，应放在该 topic 的 `script/` 下；通用检查逻辑复用 `artifact_layout.evidence_doctor_script_template` 解析出的脚本。

如果 diagnostic 或 test support 头文件已经长到难以审查，聚合入口、内部目录、兼容别名和狭义诊断位置都按 `.agents/config/defaults.yaml` 的 `test_support` 配置、可选本机覆盖和当前 topic 既有结构决定。拆出的每个内部头文件仍要有文件级中文说明，说明职责、RVV 测试证据边界，以及不能证明 production dispatch。

fallback（回退路径）测试要能隔离触发原因。如果一个候选同时有规模阈值、identity gate（顺序一一对应验收条件）、类型 gate 或布局 gate，测试矩阵至少要有一个 case 单独覆盖每个重要 gate，避免一个小规模 case 同时绕开所有分支却被误写成完整 fallback 证据。

## 6. Python、Makefile 和输出

Python 脚本应说明：

- 输入样本来源。
- 候选模型或实验变量。
- 输出指标属于局部、入口、下游还是性能证据。
- 生成 `evidence_manifest.json` 或等价 JSON manifest 时，说明字段来源、哪些字段可由 raw log 解析、哪些字段需要 topic-local case 字典补齐。脚本输出不能只让 reviewer 看到数值表，还要能被 Evidence Doctor 复核边界和异常信号。
- 可选依赖缺失时是跳过、降级还是失败。

Makefile / board.mk 应说明：

- target 用途：correctness、smoke、bench、deploy、fetch logs。
- 默认 target 是否会运行测试。
- QEMU target 是否只代表正确性。
- 板卡 target 是否包含 bench。

Makefile / board.mk 的关键 target 用途和板卡边界应中文主导；简短文件头、变量名、固定英文短语和远端路径不必为了翻译而改写。语言规则服务于可审查性，不要求制造无意义 churn（无效改动）。

测试输出应明确：

- 哪些行是 gate。
- 哪些行只是诊断值。
- 哪些结果来自 QEMU。
- 哪些结果来自板卡或目标硬件。

## 7. 文档与 reviewer 汇报

文档和 reviewer 回复应满足：

- 面向中文读者时，先给中文结论，再给英文术语。
- 面向英文读者时，英文术语首次出现也要给 plain-English explanation（白话解释）；面向中文读者时再补中文解释。
- 不把结论归因于对话参与者；写源码、命令、测试和证据。
- 不使用只有英文缩写的标题。
- 表格列名优先中文，例如“证据”“状态”“含义”“限制”。
- 如果必须使用英文 case 名、函数名或 target 名，旁边解释其作用。
- 避免翻译腔和名词堆叠。把“当前状态、为什么这样做、还不能证明什么”写成自然段落。

## 8. 未闭合项写法

文档、reviewer 汇报和 closeout 中写“未闭合项”“剩余风险”“后续生产门禁”时，不能只列术语。每一项必须用陈述句说明：

- 这项是什么。
- 当前为什么没有闭合。
- 如果后续完成它，能证明什么或降低什么风险。
- 当前阶段是否必须完成；如果不是，说明它只属于 production（生产接入）前置工作、caller（调用方）扩展工作、文档整理工作或性能复核工作。

推荐写法：

```text
base RangeImage 输入域尚未闭合。base RangeImage 的 angle_x 会除以 cos(angle_y)，在 angle_y 接近 +-pi/2 时可能超出 [-pi, pi]，因此当前有限域 sincos helper 不能直接覆盖该调用方。闭合这项需要证明输入域仍满足 helper 合同，或为域外输入设计 fallback（回退路径）。当前阶段只验证 RangeImageSpherical 形态，因此这不是 scratch prototype 的阻塞项，但它是接入 base RangeImage production 前的门禁。
```

不推荐写法：

```text
仍未闭合：base RangeImage 输入域、production fallback、caller 白名单。
```

不要用连续问句组织未闭合项，例如“是否需要 fallback？是否覆盖 base RangeImage？”；改用陈述句写清当前状态和作用。

## 9. 自查清单

交给 reviewer 前检查：

- 是否有连续英文术语没有解释。
- 中文文本中的英文术语是否首次出现带中文解释。
- 英文文本中的专有术语是否首次出现带 plain-English explanation；如果读者是中文使用者，是否补中文解释。
- 长测试/诊断文件是否有自然的文件级阅读提示，而不是“中文执行地图”这类生硬标题。
- 非平凡函数是否说明作用、调用者、production 语义映射和证据角色。
- 注释是否像自然工程说明，而不是模板填空或英语直译。
- gate 是否明确会失败并返回非 0。
- 是否区分局部片段、入口形态、生产路径、QEMU、板卡。
- 未闭合项是否逐条说明“是什么、为什么没闭合、做了有什么用、当前是否必须做”。
- 文档是否让读者不看对话也能恢复证据链。
- 最终回复、reviewer 报告、Handoff Packet 和 `agent_asset_feedback` 是否按 `rvv-documentation/references/writing-style.md` 执行触发词检查；如需保留命中词，是否写清技术原因。
