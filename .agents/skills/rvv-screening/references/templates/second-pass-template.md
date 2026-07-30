# Second-pass 模板

用于把 first-pass 粗筛候选转成模块实施队列。目标是复核 `high/mid` 初始候选基线，修正误筛和漏筛，形成可执行但仍需函数级评估的主题队列。

## 输入

- `artifact_layout.module_triage_doc_template` 解析出的 first-pass 文档。
- `artifact_layout.screening_root_template` 解析出的模块优化流程或同类文档。
- 模块源码、上游 test/benchmark 入口、同类已完成 RVV 文档。

## 输出

```text
artifact_layout.module_second_pass_doc_template
```

## 必须回答

- 每个 first-pass `high/mid` 候选的去向。
- 新增补充候选的来源、证据和边界。
- 具体函数入口或函数族，而不是只按文件名判断。
- 主成本覆盖类型：`direct-main-path`、`partial-preprocess`、`tail-compress`、`diagnostic`、`non-standalone`。
- 预期覆盖条件、fallback 条件、测试和 bench 可行性。

`high/mid` 是必须复核的下限集合，不得静默丢弃。纳入 first-pass `low` 或未列入候选的文件时，必须说明来源、源码证据、为什么属于漏判，以及为什么没有扩展成重新全模块或全库筛选。

每个候选至少下钻到：

- 公开入口或主要调用路径。
- 具体可能 RVV 化的函数、loop 或 helper。
- 输入输出、点类型、字段、indices、organized 条件和数据布局。
- 循环规模、算术密度、访存模式、分支/状态复杂度。
- 数值语义风险和输出顺序风险。
- 是否已有上游 test/benchmark 或可建立专项 test/bench。
- 推荐去向。

## 主成本覆盖类型

- `direct-main-path`：RVV 覆盖公开入口主成本或输出生成主路径，优先考虑纳入建议队列。
- `partial-preprocess`：只覆盖前置预处理，例如 min/max、leaf id、字段预扫描，需评估后续 sort/search/map/lattice/Eigen/整点复制是否稀释收益。
- `tail-compress`：只覆盖后处理压缩、threshold 或拷贝前置 mask，通常不宜单独纳入建议队列，除非入口极常用且覆盖面明确。
- `diagnostic`：有清晰局部实验价值，需要用诊断证据判断是否可进入 production，不承诺生产分流。
- `non-standalone`：公开头、薄 wrapper、显式实例化、伴随 src 或真实循环在其它主题中，不单独实施。

## 固定分类

- `建议进行 RVV 优化的文件`：优先进入函数级生产价值评估。
- `保留实施的候选文件`：保留后续复筛或诊断价值，但当前不排入建议队列。
- `暂缓或不推荐考虑 RVV 优化的文件`：当前证据不支持独立 RVV 主题。

建议队列通常要求 RVV 覆盖 `direct-main-path`，或入口极常用、实现极小且风险低。只覆盖局部子公式的主题，必须能设计 full diagnostic 或 production case 来判断局部收益是否被后续主成本稀释。

保留候选应说明后续复筛需要回答的问题，例如主成本是否被 search/sort/map/heap/Eigen/状态机稀释，是否能建立可归因 bench 诊断，或是否等待同模块已完成主题证据。

暂缓或不推荐项应说明是否合并到其它主题、是否因真实循环在别处、是否因控制流/状态机/外部 solver 主导，以及什么条件下可重新考虑。

## 文档结构

1. 输入依据。
2. 二轮筛选统计。
3. 文件级变化理由。
4. 建议进行 RVV 优化的文件。
5. 保留实施的候选文件。
6. 暂缓或不推荐考虑 RVV 优化的文件。
7. 执行清单 / 状态表。

## 二轮筛选统计字段

统计至少包含：

- first-pass 文件总数、`high/mid/low` 数量。
- 第二轮初始候选基线数量，即 `high/mid` 必查数量。
- 新增补充候选数量和来源；为 0 时也明确写出。
- 第二轮候选总数，即 `high/mid` 必查和新增补充候选去重后的数量。
- 建议进行 RVV 优化的文件数量。
- 保留实施的候选文件数量。
- 暂缓或不推荐考虑 RVV 优化的文件数量。
- 源码冲突、合并、删除或不单独实施数量。
- first-pass `high/mid` 中降级为不单独实施、暂缓或删除的数量。

## 文件级变化理由

逐项说明 first-pass `high/mid` 候选为何保持、升级、降级、合并、转入 `bench 诊断主题`、暂缓或删除。若纳入 `low` 或未列出的补充候选，说明补充来源、证据和必要性。

变化理由应区分“文件有大循环”和“RVV 覆盖公开入口主成本”。只覆盖前置预处理、尾段压缩或被不规则主成本稀释的文件，不应仅凭局部 loop 直接排入建议队列。

## 执行清单

执行清单使用 `当前结论 / 下一步条件`，不列可推导的长文档位置。常用字段：

- `顺序`
- `主题`
- `主文件`
- `推荐入口 / 第一 RVV 目标`
- `状态`
- `当前结论 / 下一步条件`

状态表应覆盖模块筛选、函数级评估、RVV 实现、专项测试、bench、QEMU、反汇编、目标硬件闭环、主题文档和工作日志。

## 输出限制

- 不修改模块源码。
- 不建立 `artifact_layout.topic_test_dir_template` 解析出的 topic 测试资产目录。
- 不运行目标硬件 bench。
- 可以做非破坏性源码阅读、搜索、静态分析和必要的轻量构建或测试入口确认。
- 发现 first-pass 与当前源码冲突时，在 second-pass 文档中记录并修正去向；不要扩展成全库重新筛选。
