# 主题 RVV 文档结构

主题文档面向长期维护，强调算法、实现设计、staging 边界、数值语义和生产接入理由。

推荐结构：

1. 函数入口作用：公开 API 调用路径、输入输出、算法管线职责。
2. 标量路径与诊断边界：真实上游源码、当前是否修改生产入口。
3. 生产接入前置观察：对象状态、staging、隐藏标量 tail、成本归因或语义风险。
4. 覆盖范围与 fallback：点类型、dense/indexed、小规模、NaN/Inf、FRM/FCSR、非 RVV 行为。
5. 详细设计：helper、traits、staging、mask helper、数值 helper、对象状态展开。
6. 关键实现片段：展示完整阶段边界，不能只贴公式。
7. 数值算例与 VL chunk 图示。
8. Bench case 说明。
9. 测试、QEMU、反汇编和板卡证据。
10. 生产接入评估。
11. 结论与后续方向。

## 必写要点

- 被优化函数对象或函数入口在库中的作用。
- 公开入口、wrapper、dispatch、真实实现层之间的调用链。
- RVV 覆盖原标量代码的哪一段。
- 哪些阶段仍是标量，原因是什么。
- 哪些 gate 触发 fallback，fallback 后语义如何保持。
- 当前主题属于 production direct、production-shaped diagnostic、bench 诊断主题，还是生产回退说明。
- 每个 bench case 的入口、规模、参数、是否命中 RVV、speedup 计算方式和证明点。
- 板卡收益是否足以覆盖 staging、buffer 和维护成本。

## Staging 与特殊实体

如果出现 staging、metadata、history entry、mask helper、数值 helper或对象状态展开 helper，必须逐项说明：

- 实体类型。
- 输入输出。
- 对应的原标量语义。
- 不能直接复用原结构或公共 helper 的原因。
- 覆盖 / fallback 边界。
- 是否改变公开 API 或对象可见状态。

多个 staging 名称应给出对照表：

```text
| staging 名称 | 结构体 | 生成 helper / 代码位置 | 字段来源 | 下一段消费者 / tail | gate |
```

## 关键片段要求

关键 RVV 片段不能只摘几行数学公式。若实现包含类型 / 布局检查、数值 helper、finite / predicate mask、AoS stride、gather load、`vcompress`、scatter、标量写回、staging 结构或后续标量状态机，片段必须覆盖完整阶段并回答：

- 哪些条件进入 RVV，哪些条件 fallback。
- VL chunk 如何取数。
- mask 如何构造。
- 有效 lane 如何保序压缩或写回。
- staging 字段如何被后续标量 tail 消费。
- 为什么整体仍等价于标量语义。

代码片段中的注释只解释边界、fallback、访存、mask、压缩、写回、FRM/FCSR 或 staging 衔接；不要逐行复述 intrinsic。

## RVV Helper + Scalar Tail 双片段模板

当 RVV 只接管前置阶段、staging 生成、mask/压缩或局部纯函数，后续仍由标量 tail 消费 staging 时，主题文档必须同时展示两个片段。

RVV helper 片段应覆盖：

```cpp
// RVV helper: gate -> VL chunk load/gather -> formula/mask -> compress/write staging
```

标量 tail 片段应覆盖：

```cpp
// Scalar tail: read staging -> projection/search/state update/output append
```

两个片段之间必须说明：

- RVV helper 接管原标量代码的哪一段。
- staging 字段分别来自哪些原局部变量、对象成员或输入 lane。
- scalar tail 从哪一个原标量状态继续执行。
- 为什么 tail 当前保留标量，例如语义安全、间接读取、状态机、输出顺序、测试证据不足或目标硬件收益不足。
- full diagnostic 或 production case 是否证明 staging 的额外内存流量没有抵消收益。
- 若 tail 是后续增量诊断方向，列出下一步需要证明的语义和性能证据。

不要只写“前置 RVV staging + 标量尾段”。读者应能从两个片段直接定位职责分割和保留边界。

## 特殊模式要求

如果出现新 RVV 组织模式，必须单独说明：

- 模式名称和动机。
- 与既有单公式筛选、规约、图像式邻域、gather、压缩或 staging 模式的差异。
- 为什么适合当前函数。
- 哪些条件保证标量语义一致。
- 哪些 fallback 边界不能套用。
- 反汇编如何证明路径命中。

典型模式包括多谓词几何 mask、分阶段 mask 收敛、多路压缩输出、显式 FRM 控制、共享输入向量的多个线性谓词 helper/lambda、前置纯函数 RVV staging + 后续标量状态机。

## 性能章节要求

性能章节不能只列 case 名和 speedup。每个 case 至少写清：

- 对应函数入口。
- 数据规模、点类型、字段、kernel、indices 或参数组合。
- 是否命中 RVV 主路径、fallback 路径或间接受益路径。
- speedup 计算方式。
- 该 case 证明的语义或性能点。

fallback case 用于证明未覆盖路径保持语义和成本接近，不作为 RVV 主路径性能结论。
