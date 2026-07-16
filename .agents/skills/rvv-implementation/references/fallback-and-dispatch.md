# Fallback 与分发

## Gate 原则

gate 应尽量靠近公开入口已有语义判断之后，避免在公开 API 中堆叠大段 RVV 主体逻辑。`*_RVV` 可内部处理小规模 fallback，减少公开入口复杂度。

常见 gate：

- 编译期：`__RVV10__`、点类型 traits、scalar 类型、字段类型、standard-layout。
- 运行期：输入规模、dense、indices、stride、field offset、对象配置、阈值参数。
- 语义期：输出顺序、状态机、NaN/Inf、FRM/FCSR、in-place。

输入规模 gate 应写成当前 RVV stage 的 work item count，而不是固定变量名。不同入口可能是 `indices.size()`、cloud size、candidate count、projected count、neighbor pair count 或其它上界。分阶段 RVV 中，后续 stage 因 candidate count 过小、布局或 VLEN gate 失败时，应优先从已有 staging 进入对应 scalar tail，而不是丢弃前面已完成的 RVV 工作。

使用 32-bit indexed byte offset 时，gate 要证明所有有效访问满足 `index * sizeof(PointT) <= UINT32_MAX`。若入口已经依赖 PCLBase / PointCloud / indices 的有效性检查，可以用 `cloud.size() <= UINT32_MAX / sizeof(PointT)` 证明有效 indices 的 byte offset 范围；若输入是 raw index 数组或入口没有证明 index 落在 cloud 内，则需要额外验证每个 index 范围，或回退标量路径。

## Weak Speedup 处理

`1.05x ~ 1.2x` 的 full 或 production 收益可以接生产，但只适合入口常用、实现小、fallback 简单、语义风险低且证据完整的路径。

如果需要复杂 staging、泛型状态展开、不规则 gather、多路压缩输出，或 RVV 只覆盖前置片段，应先保留诊断证据，并说明不满足弱收益接入条件。

## in-place 安全

输入输出可能重叠时，每个 VL chunk 内必须先 load 完所有将被读取的输入字段，再 store 输出字段。不能先写某字段导致同一 chunk 后续读取被污染。

## FRM/FCSR

如果 helper 使用显式舍入模式 intrinsic 或直接修改 FCSR/FRM，必须保存并恢复调用者原浮点环境。专项测试或 bench 至少覆盖“同一进程先命中 RVV 主路径，再运行 fallback/标量 case”的顺序。

## 局部优化限制

如果 RVV helper 为了保持标量语义或让误差归因单一而限制编译器自动优化，例如局部使用 `no-tree-vectorize` 防止尾段标量累加被改成向量 reduction，必须同步说明：

- 限制作用范围。
- 为什么需要该限制。
- 是否影响手写 RVV intrinsic。
- 非 GCC/Clang 下如何处理。
- 反汇编如何确认预期路径。
- 保护的是累加顺序、tie 选择、状态机更新、浮点环境还是 checksum 归因。

不能只在代码里留下 pragma。
