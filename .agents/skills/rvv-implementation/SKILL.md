---
name: rvv-implementation
description: 实现或审查 C/C++ 高性能库中的 RVV 生产路径。适用于上游 RVV helper、fallback gate、公开入口分发、RVV load/store 封装、in-place 安全、注释粒度、代码审查，以及把函数级评估结论落到可维护源码的任务。
---

# RVV 实现工作流

使用本 skill 前，应已有函数级评估或诊断证据，说明目标函数、覆盖条件、fallback 条件和生产接入价值。

回复、实现说明、配置解析出的测试资产和 prototype 注释遵循 `rvv-workflow/references/reviewability-and-language.zh.md`：英文术语首次出现时必须解释；中文主导时给中文解释，英文主导时也要给 plain-English explanation（白话解释）。中文说明要自然，避免翻译腔和模板填空。production 代码注释保持克制；test/prototype 代码可以更详细，说明 helper 作用、调用者、fallback/gate 边界和证据角色。

中优先级函数不等于默认放弃。应先尝试 RVV 可行性；只有实现困难、语义风险、覆盖条件过窄、验证成本过高、收益不可证明或破坏可维护性时才暂缓，并把原因写入评估和文档。

## 实现结构

- 公开 API 不变。
- 常驻 `*_Std` / `*_Standard` 标量 helper；可做成既有类的成员 helper，也可在不改变公开 API /
  protected 声明更稳时做成邻近 internal / `detail` free helper，但必须让 public entry 的 Std fallback
  边界一眼可见。
- `__RVV10__` 下提供 `*_RVV` helper；承载 RVV 指令或 RVV 分流语义的 helper 默认不要在非 RVV 构建中以“只返回 false”的 stub 常驻，公开入口用条件编译包住 RVV 尝试并自然落回 Std。
- 公开入口用短路分流选择 RVV 或自然落回 Std。
- 生产接入后，公开入口不能表现为“先尝试 RVV，失败后继续在同一个入口里执行大段原标量主体”。必须把原标量主体抽成命名清楚的 `*_Std` / `*_Standard` helper，或在主题文档和 Handoff Packet 中说明无法拆分的具体语言 / ABI / 模板约束。仅保留几行参数准备、已有语义检查和最终 `Std` fallback 调用。
- 不强制新增 dispatch helper；只有多个公开入口共享复杂选择逻辑时才增加。
- 主路径 helper 放在对应分发入口附近，命名空间遵循所在文件风格。
- 主路径 RVV helper 不额外包入 `detail`，除非该文件已有同类历史 SIMD 风格。
- 命名要区分实际 RVV 路径和语义小工具。承载 RVV 指令或公开短路分流的 helper 可使用 `*_RVV`；traits、类型检测、阈值常量、mask helper 等非分流实体使用语义名。

实现结构细则见 [references/implementation-patterns.md](references/implementation-patterns.md)。

## Fallback 与 gate

必须明确：

- 小规模输入。
- `double` 或不支持的 scalar。
- non-dense 或 NaN/Inf。
- indexed/gather/subset。
- 非连续存储。
- 字段类型不兼容。
- 非标准布局。
- 复杂分支、状态机或收益不确定路径。

细则见 [references/fallback-and-dispatch.md](references/fallback-and-dispatch.md)。

## 访存封装

生产 RVV 代码优先复用公共 load/store 封装，而不是在各主题复制裸 intrinsic。标准 `x/y/z` 字段优先使用 xyz wrapper；normal、intensity、label 或自定义字段优先复用 primitive。

访存封装细则见 [references/point-load-store.md](references/point-load-store.md)。

如果 production 入口是模板点类型，或诊断证据只覆盖 `PointNormal` / `PointXYZ` 等具体类型但生产补丁准备接入模板入口，必须读取 `artifact_layout.generic_point_type_strategy_doc_template` 解析出的文档。实现时二选一：

- 泛型接入：用 PCL traits（点类型字段特征）、字段 offset、POD / standard-layout 和 alignment gate 证明当前 `PointSource` / `PointTarget` 可走 RVV；不满足时 fallback。
- 窄范围接入：明确只对已证明的具体点类型或布局分流，其它模板实例 fallback；文档和 Handoff Packet 不能把它写成泛型成立。

如果生产算法只读取标准 `x/y/z` 字段，模板入口的默认目标应是 PointXYZ-like traits gate，而不是
`std::is_same_v<PointXYZ>` exact-type gate。exact-type gate 只能作为阶段性例外，必须在 phase
plan/result、optimization matrix 和 Handoff 中写清：为什么 traits / wrapper 暂时阻塞、哪些点类型未覆盖、
其它模板实例如何 fallback、下一 `point_type_expansion_queue` phase 如何补 correctness、fallback、
bench、asm、board 和 Evidence Doctor。不能把 exact-type gate 写成最终 generic template 实现。

模板点类型的标量语义应先按源码字段访问理解：例如 source 只读 `x/y/z`、target 读 `x/y/z/normal_x/normal_y/normal_z` 时，原标量路径支持的是“这些字段访问能编译且语义成立”的点型组合，不是一定 `PointSource == PointTarget`，也不是一定 exact `PointNormal`。RVV 若只覆盖 `PointNormal -> PointNormal`，这是有意收窄的 production gate；若要扩成泛型，必须分别证明 source 和 target 当前读取字段、布局、stride/gather、Scalar 和数据流证据，不能把某个 exact 点型的 bench 或测试外推成模板泛型成立。

point-to-plane、normal-based registration（基于法线的配准）还必须额外证明 normal 字段。`x/y/z` traits 成立不代表 `normal_x/normal_y/normal_z` 成立；若公共 normal field gate 不足，先收窄到已证明点类型，或补 traits gate 后再接入。

## 注释规则

新增注释只解释维护边界，不复述代码表面行为。适合注释：

- 覆盖范围和 fallback 原因。
- AoS/stride/gather/mask/VL chunk 组织理由。
- `vcompress`、staging 与后续标量状态机衔接。
- in-place 安全。
- FRM/FCSR 或浮点语义边界。

不适合注释：

- “加载字段”。
- “计算结果”。
- “保存输出”。
- 逐行复述 intrinsic 名称。

修改旧源码时保持所在文件格式，不做无关格式化。

## PCL adapter 注意点

当前 PCL 旧代码常见函数调用空格风格，例如 `foo (bar)`。抽出 `*_Std` helper 时尽量保持原标量代码文本风格，不顺手做格式化。新增 RVV 代码前先检查同模块已有 RVV 文件的命名空间风格；只有短 traits、小型类型检测、既有 SSE/AVX helper 或不应暴露到主路径语义层的小工具才放入 `detail`。

如果文件已有 x86 SSE/AVX 实现，可以参考减少重复访存、合并同一语义操作、复用寄存器的思路；RVV 仍需按 VL chunk、AoS/SoA 布局、in-place 安全和 fallback 重新设计，不能照搬 x86 单点寄存器粒度。
