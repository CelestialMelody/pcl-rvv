# First-pass 模板

用于全库或模块级文件粗筛，目标是形成可供 second-pass 复核的 `high/mid/low` 文件级基线。

## 输入

- `<repo>` 当前源码。
- `doc-rvv/library-screening/README.zh.md` 或同类筛选总入口。
- `doc-rvv/library-screening/module-screening.zh.md` 或同类模块索引。
- 目标模块源码、上游 test/benchmark 入口和已完成同类 RVV 主题文档。

## 输出

```text
doc-rvv/library-screening/modules/<module>-function-triage.zh.md
```

## 判断口径

检查每个源码文件是否包含值得 second-pass 继续下钻的 SIMD/RVV 片段：

- 循环规模：点数、像素、邻域、correspondence、bin 或文件大小相关循环。
- 算术密度：乘加、距离、统计、几何谓词、权重、规约、小矩阵或数学函数。
- 访存模式：连续、固定 stride、AoS 字段、organized 行列、indices gather 或临时数组。
- 分支复杂度：简单 mask 与深分支、状态机、随机采样、map/hash、search 或 solver 的区别。
- 语义风险：非结合规约、阈值边界、NaN/Inf、输出顺序、indices、字段复制、自定义点类型。
- 可验证性：std/RVV 对拍、fallback case、边界 case、QEMU 和板卡验证是否可构造。

优先级定义：

- `high`：文件内存在明显批量循环或数学密集片段，且粗看具备较强 SIMD/RVV 评估价值。
- `mid`：存在可向量化片段，但主成本、数据布局、语义风险或测试入口需要 second-pass 继续确认。
- `low`：以声明、薄 wrapper、调度、类型、构建胶水、小规模固定计算或明显不规则状态路径为主。

`high/mid` 是 second-pass 必查基线，不是最终实施全集。`low` 不是永久排除；second-pass 发现明确漏判时可以补入，并说明源码证据。

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
- 不把 first-pass high/mid 写成“建议优化”或“实施队列”；这些结论留给 second-pass。

## 限制

- 不修改源码。
- 不建立 `test-rvv/<module>/<topic>/`。
- 不承诺生产分流、最终收益或公开入口主成本覆盖。

## Closeout

完成后报告：

- first-pass 文档位置。
- high/mid/low 数量。
- second-pass 应读取的下一份文档。
- 是否发现需要补充到 workflow、diagnostics、implementation、benchmarking 或 documentation skill 的通用规则。
