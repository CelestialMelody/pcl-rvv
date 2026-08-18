# RVV 阶段与队列规则

本文定义 `rvv-screening` 的阶段职责、队列命名、分类口径和保留候选复筛规则。候选准入标准见 [screening-criteria.md](screening-criteria.md)；证据和执行边界见 [evidence-boundaries.md](evidence-boundaries.md)。

## 阶段职责

- 文件候选筛选：只回答“文件中是否存在值得继续下钻的可 SIMD/RVV 片段”，形成 `high/mid/low` 文件级粗筛基线。
- 函数评估队列：把文件候选筛选 `high/mid` 候选下钻到公开入口、函数族和主成本覆盖类型，形成三类模块队列：`建议进行 RVV 优化的文件`、`保留实施的候选文件`、`暂缓或不推荐考虑 RVV 优化的文件`。
- 保留候选复筛：在建议队列完成、没有明确下一主题，或已完成主题证据改变排序口径时，复筛函数评估队列 `保留实施的候选文件`。

## 主成本覆盖类型

- `direct-main-path`：RVV 覆盖公开入口主成本或输出生成主路径，优先考虑纳入建议队列。
- `partial-preprocess`：只覆盖前置预处理，例如 min/max、leaf id、字段预扫描，需评估后续 sort/search/map/lattice/Eigen/整点复制是否稀释收益。
- `tail-compress`：只覆盖后处理压缩、threshold 或拷贝前置 mask，通常不宜单独纳入建议队列，除非入口极常用且覆盖面明确。
- `diagnostic`：有清晰局部实验价值，需要用诊断证据判断是否可进入 production，不承诺生产分流。
- `non-standalone`：公开头、薄 wrapper、显式实例化、伴随 src 或真实循环在其它主题中，不单独实施。

## 文件候选筛选优先级

- `high`：文件内至少有一个强批量 loop 或函数族，满足生产价值、并行合法性、RVV 访存匹配中的两个以上强信号，且没有未解释的语义或测试硬伤。
- `mid`：存在可 SIMD/RVV 片段，但主成本、数据布局、indices/gather 成本、浮点语义、入口覆盖或测试可行性仍需函数评估队列判定。
- `low`：以声明、薄 wrapper、调度、类型胶水、小规模固定计算、不规则容器/search/solver/状态机或非热点路径为主；除非有 profile、源码变化或已完成主题证据，否则不进入函数评估队列初始候选。

`high/mid` 是函数评估队列必查基线，不是最终实施全集。`low` 不是永久排除；函数评估队列发现明确漏判时可以补入，并说明源码证据。

## 函数评估队列固定分类

- `建议进行 RVV 优化的文件`：优先进入函数级生产价值评估。
- `保留实施的候选文件`：保留后续复筛或函数级评估路径选择价值，但当前不排入建议队列。
- `暂缓或不推荐考虑 RVV 优化的文件`：当前证据不支持独立 RVV 主题。

建议队列通常要求候选覆盖 `direct-main-path`，或入口极常用、实现局部、风险低且收益可用专项 bench / production-shaped diagnostic 验证。

保留候选用于有明确 RVV 片段但仍缺少主成本证据、profile、生产形态验证或已完成模式支撑的候选。

暂缓或不推荐项用于主成本被 search/sort/map/heap/Eigen/外部 solver/分配释放稀释、只能覆盖尾段压缩或字段搬运、语义风险过高、测试不可构造或不具备独立实施边界的候选。

## 保留候选复筛输入边界

保留候选复筛默认只读取函数评估队列 `保留实施的候选文件`。它不是静态函数评估队列重跑，也不重新扩大到全模块。

只有出现以下证据时，才补入保留候选队列之外的候选，并在文档中说明补入来源、源码证据和为什么不扩展成全模块重筛：

- 当前源码变化导致函数评估队列结论失效。
- profile、上游使用场景或目标硬件证据明确指向漏筛。
- 已完成主题暴露出新的可复用模式，且能映射到原保留候选之外的具体文件或函数入口。

## 保留候选复筛输出分组

保留候选复筛只使用两类主分组：

- `建议启动函数级评估`：值得进入单 topic S1-S2 评估和后续证据计划。
- `暂缓 / 不单独实施`：当前不建议作为独立 topic 启动，或只作为其它主题上下文、伴随文件、使用场景触发项保留。

`diagnostic`、`bench-only`、`production-shaped diagnostic`、`component ablation` 和 `production direct` 是后续 topic 内的证据路径或 EvidenceDecision（证据决策）结果，不是保留候选复筛的固定候选队列名。模块复筛只负责选择是否启动函数级评估，以及首阶段要回答什么证据问题。

`建议启动函数级评估` 表必须写清 `默认评估路径 / 首阶段证据问题`，常见取值包括：

- `production-value evaluation`：优先回答是否覆盖公开入口主成本和生产价值。
- `production-shaped diagnostic`：先建立接近生产调用形态的 test-only 诊断。
- `component ablation`：先隔离 gather、staging、reduction、solver 前后处理或其它可疑瓶颈。
- `profile prerequisite`：需要真实使用场景、profile 或数据集证明目标片段接近入口主成本。
- `no-production confirmation`：预期不接 production，但值得用证据关闭风险或历史候选。

## 已完成主题反哺排序

- 优先启动能复用已完成强收益模式、能覆盖公开入口主成本、且首阶段证据问题可在一个 topic 内关闭的候选。
- 已完成主题显示 gather、staging、tail-compress、diagnostic-only 或 production 接入收益不成立时，把同类候选降级，除非新候选能说明不同的数据规模、访存形态、入口覆盖或硬件证据。
- 没有真实入口/profile 的候选默认走 `profile prerequisite` 或 `production-shaped diagnostic`，不要直接写成 production 优化。
