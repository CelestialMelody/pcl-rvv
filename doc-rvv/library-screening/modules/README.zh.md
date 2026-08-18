# 模块第一轮筛选文档目录说明

本目录存放各模块 RVV 第一轮文件级筛选报告。第一轮的职责是形成可供第二轮筛选复核的文件级 high/mid/low 基线，不直接决定最终 RVV 实施队列。

## 命名规则

- 每个模块一个文件：`<module>-file-candidate-screening.zh.md`
- 通用模板：`_module-file-candidate-screening-template.zh.md`

## 固定结构

每个模块文档应使用以下结构：

1. 输入依据与范围
2. 第一轮筛选口径
3. 第一轮筛选统计
4. high 候选
5. mid 候选
6. 全量文件覆盖表
7. 二轮交接说明

## high/mid/low 含义

- `high`：文件内存在明显批量循环或数学密集片段，且粗看具备较强 SIMD/RVV 评估价值。
- `mid`：文件内存在可向量化片段，但主成本、数据布局、语义风险或测试入口需要第二轮继续确认。
- `low`：以声明、薄 wrapper、调度、类型、构建胶水、小规模固定计算或明显不规则状态路径为主。

`high/mid` 是第二轮必须复核并交代去向的初始候选基线，不是最终实施全集。`low` 不是永久排除；第二轮如果发现明显漏判，可以补入并说明证据。

## 路径格式

- 候选表和覆盖表优先使用省略模块公共前缀后的短路径。
- 文档必须在“输入依据与范围”中说明省略的公共前缀。
- 当短路径存在歧义时，应使用完整 repo-relative 路径。

## 与第二轮筛选的关系

第二轮筛选应读取本目录中的 `<module>-file-candidate-screening.zh.md`，把 `high/mid` 作为初始候选基线逐项交代去向，并输出：

```text
doc-rvv/library-screening/<module>/<module>-function-evaluation-queue.zh.md
```

第二轮按三类组织结论：

- 建议进行 RVV 优化的文件
- 保留实施的候选文件
- 暂缓或不推荐考虑 RVV 优化的文件
