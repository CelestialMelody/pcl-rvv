# RVV 实现模式

## 基本形态

```text
public entry
  -> #if defined(__RVV10__) && RVV gate 命中: *_RVV(...)
  -> else: *_Std(...)
```

`*_Std` / `*_Standard` 保留原标量语义，非 RVV 构建、未覆盖类型、小规模、non-dense、indexed 等路径必须自然落回 Std。该 helper 可以是成熟 topic 常见的成员函数，也可以是在不想扩大类声明 / ABI / protected API 表面时使用的邻近 internal 或 `detail` free helper；关键是 public entry 只做小型分发，fallback 边界可被 reviewer 直接引用。`*_RVV` 名字应只用于真实 RVV 路径或公开短路分流层；这类 helper 默认放在 `__RVV10__` 条件编译内。不要为了让公开入口少写一层 `#if`，在非 RVV 构建里常驻一个名字带 `RVV`、只返回 false 的 helper，除非有跨文件 ABI、模板兼容或已有 SIMD 风格要求，并在主题文档里说明原因。

生产接入阶段必须把原标量主体变成可命名、可引用、可测试或可反汇编归因的 `*_Std` / `*_Standard` 路径。不能让公开入口保留大段上游标量循环，再在函数开头插入一个 RVV try-and-return；这种形态虽然语义上可能能 fallback，但 reviewer 很难判断原标量路径边界、fallback 覆盖和后续维护风险。若旧接口、模板可见性、ABI 或重载关系使拆分代价过高，必须在生产接入计划、Handoff Packet 和主题文档中写明原因、保留的标量主体行号和额外验证。

公开入口应尽量只保留上游语义检查和短路 dispatch（分流）。如果 RVV 接入后公开入口里同时出现
非平凡 RVV gate、iterator 构造和标量主体调用，优先抽成命名清楚的 `*_Std` / `*_RVV`
helper，让 reviewer 可以一眼看出：

- 哪段是原标量路径。
- 哪段是 RVV 候选路径。
- 哪些公开 overload 只自然落回 Std。
- 哪些 fallback 没有改变原入口语义。

已有 SSE/AVX/NEON 风格的文件尤其应保持这种组织方式；新增 RVV 不应把 public entry 变成大段
实现主体。多个公开入口共享同一数学 pipeline 时，可以用 policy（策略类型）复用内部实现，但
公开入口仍应呈现为“检查 -> RVV 短路 -> Std fallback”的小型分发层。

## 命名

- 承载 RVV 指令或公开短路分流的 helper 可使用 `*_RVV`。
- traits、类型检测、阈值常量、mask helper、metadata 展开等语义小工具不应额外带 `RVV/Rvv`。
- 前置 staging 结构和 history entry 使用领域语义命名。

## 新模式记录

如果主题引入此前未出现过的新 RVV 组织模式，主题文档必须单独说明：

- 模式名称和动机。
- 与已有单公式筛选、规约、图像式邻域、gather 或压缩模式的区别。
- helper/lambda 为什么是语义小工具，不是新分流层。
- 哪些条件保证标量语义一致。
- 哪些 fallback 边界不能套用。
- 反汇编如何证明路径命中。

## 算法等价改写

如果 RVV 路径与标量源码结构明显不同，应记录：

- 原标量结构。
- RVV 改写结构。
- 等价条件。
- 适用边界。
- 仍保留的标量阶段。
- local fragment、full diagnostic 和生产入口分别对应哪些 case。

局部片段正确且加速但 full 入口收益不足时，保留为 bench 诊断主题，不接入生产分流。

## 生产失败回收

低风险且已有强模式支撑的候选可以先做生产实现再验证；但目标硬件结果不成立时，必须回收默认生产路径。可把正确但不加速的实验移动或保留到专项诊断代码中，并在评估、主题文档和工作日志中说明不接生产原因。

中风险、收益不确定、访存形态不规则、需要新组织模式、或同类模式已有退化记录的候选，优先在专项 test/bench 中做诊断原型。只有原型在目标硬件上证明收益、checksum、fallback、反汇编路径和维护边界都成立后，再改生产分流。

## Fused formula 与 ILP 取舍

fused formula（融合公式）写法能否接 production，不能只看源码有没有更少的算术步骤，要看最终机器码、hot path、寄存器压力和 helper 边界。对 registration 这类 RVV topic，`vfmsac/vfmacc` 是否进入 hot path、是否引入 out-of-line helper、是否出现 vector spill/reload、以及 `vsetvli` / load / reduction 的整体形态，都比“理论上更短”更重要。

如果某个 `ILP` 变体和非 `ILP` 变体在当前二进制里 asm 等价，就不要把它写成独立机器码候选；它最多是源码层面的 code-shape preference（代码形态偏好）或调度诊断。只有当当前二进制、同一编译配置和同一 helper 符号里，`ILP` 真的产生不同 hot path，才把它记成独立候选。

`AbcdFused` / `AbcdFusedIlp` 可以作为这条规则的典型例子：当前 asm 等价，所以不能声称 `Ilp` 有独立机器码收益；但如果 correctness、production-symbol asm attribution 和 warm-up 多轮 RVV-vs-RVV bench 都闭合，生产实现可以优先采用更显式暴露独立 multiply 和依赖链的源码写法，再由反汇编和板卡证据确认是否真的值得保留。
