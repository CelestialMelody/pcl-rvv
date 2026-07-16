# PCL RVV Agent 背景摘要

当前实践围绕“面向 C/C++ 高性能库的 RISC-V RVV 优化 agent”展开。PCL 是阶段性验证项目，但最终 agent 应能通过 library adapter 扩展到不同 C/C++ 库。

## 目标

- 构建可扩展、证据驱动、human-in-the-loop 的 RVV 优化工作流。
- 覆盖项目配置、候选筛选、函数分析、RVV 实现、测试验证、性能评估和文档沉淀。
- 形成可复用的规则检查器、评测用例、文档模板和优化经验。

## 当前经验

已有实践显示，单一“大融合 prompt/skill”容易让关键规则藏在长文档中。更稳妥的组织方式是：

- 常用入口模板保留短提醒。
- 关键门禁规则进入对应 skill。
- 细节放入一层 reference，按需读取。
- 筛选、诊断、实现、benchmark、文档和项目配置分工明确。

## 泛化方向

后续迁移到其它库时，应替换 library adapter，而不是改写 RVV 核心 workflow：

- 源码组织。
- 测试框架。
- benchmark 流程。
- 文档规范。
- QEMU / board 验证入口。
- 产物和日志归档约定。
