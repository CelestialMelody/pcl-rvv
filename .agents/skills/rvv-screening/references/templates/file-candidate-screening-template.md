# 文件候选筛选模板

用于全库或模块级文件候选筛选，目标是形成可供函数评估队列复核的 `high/mid/low` 文件级基线。

## 输入

- `<repo>` 当前源码。
- `artifact_layout.screening_root_template` 解析出的筛选总入口。
- `artifact_layout.screening_root_template` 解析出的模块索引或同类文档。
- 目标模块源码、上游 test/benchmark 入口和已完成同类 RVV 主题文档。

## 输出

```text
artifact_layout.module_file_candidate_screening_doc_template
```

## 判断口径

按 [../screening-criteria.md](../screening-criteria.md) 的 `筛选对象与判定单位`、`RVV 候选判定矩阵` 和 [../stage-and-queue-policy.md](../stage-and-queue-policy.md) 的 `文件候选筛选优先级` 执行。本模板只规定文件候选筛选文档如何承载结论，不重新定义筛选标准。

检查每个源码文件是否包含值得函数评估队列继续下钻的 SIMD/RVV 片段，并记录足以复核的源码事实：

- 具体函数、loop、helper 或公开入口路径。
- trip count 来源。
- 运算、访存、分支/状态、依赖和输出语义风险。
- 是否存在测试、benchmark、profile 或已完成主题证据。

`high/mid` 是函数评估队列必查基线，不是最终实施全集。`low` 不是永久排除；函数评估队列发现明确漏判时可以补入，并说明源码证据。

## 文档结构

1. 输入依据与范围。
2. 第一轮筛选口径。
3. 第一轮筛选统计。
4. high 候选。
5. mid 候选。
6. 全量文件覆盖表。
7. 二轮交接说明。

## 格式要求

- 全量覆盖表必须一文件一行，不静默遗漏目标源码范围内的文件。
- 候选表和覆盖表中的文件路径优先使用省略模块公共前缀后的短路径，例如 `impl/foo.hpp`、`foo.h`、`src/bar.cpp`。
- 在“输入依据与范围”说明省略的公共前缀；短路径有歧义时使用 repo-relative 路径。
- 判断依据写源码事实，例如循环规模、数学项、声明/薄 wrapper、调度、search/map/solver 主导等。
- 不把文件候选筛选 high/mid 写成“建议优化”或“实施队列”；这些结论留给函数评估队列。

## 限制

- 不修改源码。
- 不建立 `artifact_layout.topic_test_dir_template` 解析出的 topic 测试资产目录。
- 不承诺生产分流、最终收益或公开入口主成本覆盖。

## Closeout

完成后报告：

- 文件候选筛选文档位置。
- high/mid/low 数量。
- 函数评估队列应读取的下一份文档。
- 是否发现需要补充到 workflow、rvv-test、implementation 或 documentation skill 的通用规则。
