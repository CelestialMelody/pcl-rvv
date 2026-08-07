# RVV Compiler Auto Vectorization Diagnostics

本文说明 compiler auto-vectorization（编译器自动向量化）和 missed-vectorization report（未自动向量化报告）在 RVV topic 中的证据角色。它适用于需要判断“编译器是否已经能处理这段循环”、或需要解释“为什么仍需要手写 RVV”的早期评估与证据计划。

## 适用范围

`-fopt-info-vec-missed=<file>` 是 GCC 的优化报告开关。它把编译器放弃自动向量化的原因写入指定文件。常见原因包括循环控制流复杂、内存访问不规则、别名关系不清、reduction（规约）形态不能识别、调用无法内联、或目标指令集成本模型不接受。

这个报告适合回答三个窄问题：

- S2 函数级评估：目标循环是否已经接近编译器可自动向量化的形态。
- S4 证据计划：如果结论依赖“自动向量化不足”，需要记录报告摘要或说明未使用原因。
- S8 反汇编归因补充：当二进制里出现疑似自动生成的 RVV 指令，或 scalar tail（标量尾段）被编译器生成 reduction 指令时，用报告辅助解释来源。

报告不能证明手写 RVV 一定更快，也不能证明 production dispatch（生产分流）已经命中。性能结论仍来自目标硬件 benchmark。路径结论仍来自测试、trace 和符号级反汇编归因。

## 如何开启

普通 topic build 默认不生成 missed-vectorization report，避免每次 `run_test_compare`、`run_bench_compare` 或 board deploy 产生新的本机日志。需要报告时使用显式入口：

```bash
make -C test-rvv/<module>/<topic> generate_vec_report
```

也可以在需要的构建命令上临时开启：

```bash
make -C test-rvv/<module>/<topic> run_bench_rvv ENABLE_VEC_MISSED=1
```

公共 Makefile 使用以下变量控制：

```make
ENABLE_VEC_MISSED ?= 0
LOG_VEC_MISS_DIR  ?= log/vec_missed_log
LOG_FILE          ?= $(LOG_VEC_MISS_DIR)/vec_missed_<timestamp>.log
```

`generate_vec_report` 应自行用 `ENABLE_VEC_MISSED=1` 重建目标 bench，再从 `log/vec_missed_log/` 中选择报告并生成过滤摘要。普通构建命令不应携带 `-fopt-info-vec-missed`。

## 产物边界

默认产物位置：

| 路径 | 作用 | 提交边界 |
| --- | --- | --- |
| `log/vec_missed_log/vec_missed_*.log` | GCC 原始 missed-vectorization report。 | 不默认提交。 |
| `log/latest_vec_missed.log` | 指向本轮选定原始报告的本机入口。 | 不默认提交。 |
| `log/filtered_<topic>.log` | 按 topic focus 过滤后的短日志。 | 默认不提交；引用时优先摘录结论。 |
| `log/analyze_<topic>.log` | 分析脚本生成的分类摘要。 | 需要引用时可提交小型 summary，先检查路径和隐私信息。 |
| `log/vec_logs/` | 分析脚本的中间输出。 | 不默认提交。 |

如果 evaluation（函数级评估）或主题文档引用该诊断，应写报告摘要、生成命令和证据边界。不要复制长 raw log。

## 如何解读

missed-vectorization report 应按编译器诊断处理，不能按算法结论处理。读报告时至少检查：

- 报告对应的源码行是否仍在当前 hot path（热点路径）内；模板实例化和内联可能让行号指向 wrapper。
- 构建参数是否和实际 bench 或 production 构建一致，包括 `-O3`、`-march`、`-mabi`、Eigen RVV flags 和 `__RVV10__`。
- 报告说某个循环 missed 时，反汇编中是否仍有来自手写 intrinsic、库函数或其它循环的 RVV 指令。
- 报告没有出现 missed 时，是否存在循环被完全内联、展开、删除、常量折叠或落在其它 translation unit 的情况。

`missed` 只能说明当前编译器在当前参数下没有采用某个自动向量化方案。它不能证明“未来编译器也不会向量化”。如果报告显示某段 scalar tail 被自动向量化，文档应记录实际机器码形态，并用 objdump 符号范围确认该指令是否属于当前 helper。

## 与反汇编归因的关系

S8 的主证据是 objdump（反汇编导出）和 asm attribution（反汇编归因）。归因脚本应确认关键 RVV 指令属于当前 helper、production 符号、bench harness、Eigen/libm、编译器自动向量化区域或无关代码。

missed-vectorization report 只回答编译器为什么没有自动生成某些向量路径，或辅助解释某些自动生成路径的来源。它不能替代符号级归因，也不能替代板卡性能证据。

## 文档写法

推荐在 evaluation 或 Handoff Packet 中使用短记录：

```text
auto-vectorization diagnostic:
command: make -C test-rvv/<module>/<topic> generate_vec_report
summary: target loop missed because <reason>; scalar tail <not vectorized / auto-vectorized into vfred*>
evidence boundary: auxiliary S2/S4/S8 diagnostic; production path and performance still rely on tests, asm attribution and board summary.
```

如果没有使用该诊断，也应在结论依赖自动向量化判断时写清原因，例如“本轮结论来自 production direct asm attribution 和板卡 repeated bench，没有依赖编译器 missed-vectorization 判断”。
