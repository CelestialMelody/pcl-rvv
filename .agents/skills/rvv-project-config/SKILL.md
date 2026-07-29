---
name: rvv-project-config
description: 标准化 C/C++ 高性能库的 RVV 项目环境和测试运行环境。适用于依赖库、交叉编译工具链、PCL 交叉编译、QEMU、board、Makefile harness、sanitize logs、run/fetch/analyze 入口、配置文件读取和 env var 边界；不承载测试策略本身。
---

# RVV 项目配置工作流

使用这个 skill（技能）时，目标不是直接写 RVV 优化代码，也不是决定应该写哪些测试。目标是把项目依赖、交叉编译、QEMU（仿真器）、board（板卡）、Makefile harness（测试运行框架）和配置读取整理成可复用、可审查、可迁移的环境层。

测试策略属于 `rvv-test`。例如 unit test（单元测试）、production-shaped diagnostic（生产形态诊断）、benchmark（性能测试）、component ablation（组件消融）和 evidence logs（证据日志）策略，应写入 `rvv-test`。本 skill 只解释这些测试如何通过工具链、Makefile、QEMU、board 和环境变量运行。

核心原则：

- 本机私有配置不提交。
- 公共构建逻辑不写死个人路径、私有 IP、用户名或单台设备默认值。
- 可提交配置只放默认偏好、目录约定和 env var（环境变量）名称。
- 本机覆盖项放入 `.agents/local/user-preferences.yaml`、`config.mk` 或 shell 环境变量。
- 测试行为保持可复现，环境缺失时错误信息要指向配置入口。

Makefile 分层模式和检查清单见 [references/makefile-env.md](references/makefile-env.md)。跨库适配、测试入口和验证能力声明见 [references/library-adapter.md](references/library-adapter.md)。

## 职责范围

### 1. 项目环境搭建

- PCL 源码根、测试根和文档根。
- PCL 依赖库安装前缀。
- RISC-V 交叉编译工具链和 sysroot（系统根目录）。
- PCL 交叉编译产物和链接路径。
- 本机路径、依赖路径和工具链路径如何通过 local override（本机私有覆盖）或 env var 注入。

### 2. 测试环境搭建

- QEMU runner（仿真运行器）。
- board 部署、运行、抓回日志和分析入口。
- Makefile harness 的公共片段、topic Makefile 和 board.mk 分层。
- sanitize logs（日志脱敏）、run/fetch/analyze 目标如何接线。
- 哪些命令会生成 `build/`、`output/`、`log/`，以及这些目录默认不提交。

### 3. 配置读取

- 解释 `.agents/config/defaults.yaml` 和 `.agents/local/user-preferences.yaml` 如何影响环境变量名、work log 路径、依赖路径和板卡配置。
- 说明可提交默认配置只保存 env var 名和占位符。
- 私有信息只能放 local override、`config.mk` 或 shell 环境变量。

## 非职责范围

- 不决定测试矩阵。测试矩阵属于 `rvv-test`。
- 不决定 production（生产源码）是否接入。生产接入属于 `rvv-workflow` 生命周期和 `rvv-implementation`。
- 不写 topic 文档结构。文档结构属于 `rvv-documentation`。

## 工作流

1. **先巡检，不急着改**
   - 搜索硬编码的源码根目录、依赖安装目录、工具链前缀、sysroot、板卡 IP、SSH 用户名、设备标签。
   - 区分已经使用公共模板的测试和早期自包含 Makefile。
   - 默认排除 `output/`、`build/`、`log/` 和生成的 `.log`；只有需要追溯历史命令时才读取这些产物。

2. **划清配置边界**
   - 仓库提交内容不能包含个人绝对路径、私有 IP 或单一设备默认值。
   - 公共默认值放进共享环境片段，例如 `mk/rvv-env.mk`。
   - 本机覆盖项放进被忽略的 `config.mk`。
   - `config.mk.example` 放在测试根目录，作为用户入口，而不是放进 `mk/` 这类实现目录。
   - library adapter 只声明项目结构和验证能力，不保存个人路径、私有地址或单台设备默认值。
   - agent 偏好默认值放入 `.agents/config/defaults.yaml`；本机私有覆盖放入 `.agents/local/user-preferences.yaml`。

3. **优先推导，减少复制**
   - 从共享片段或测试根目录的位置推导源码根目录。
   - 在依赖目录布局有约定时，从一个安装前缀推导子依赖目录。
   - 所有推导值都必须能被命令行变量或本机 `config.mk` 覆盖。
   - 已有 Makefile 使用的旧变量名可以保留为别名，降低迁移成本。

4. **检查配置路径，而不是检查偶然文件**
   - 检查配置目录是否存在，例如源码根目录、依赖安装前缀、已安装库目录。
   - 不要在公共配置里检查某个 topic 的具体头文件或源文件。
   - 错误信息要指出缺失路径，并提示用户查看 `config.mk.example` 或设置对应变量。

5. **渐进式重构**
   - 先改已经使用公共模板的测试。
   - 再把环境配置从构建规则里拆出来。
   - 然后逐批迁移早期自包含 Makefile。
   - 不要把 RVV 功能代码改动和构建环境整理混在一个提交里。

6. **用证据验证**
   - 至少运行一个曾经失败的目录，或用 dry-run 查看编译命令。
   - 确认编译/链接路径使用新的源码根、依赖根和工具链。
   - 故意传入一个错误路径，验证报错信息是否能指导用户配置。
   - 单独报告生成的未跟踪日志、build 目录和本机配置文件，不要混入提交。

7. **写清 adapter 能力**
   - 记录源码根、测试根、主题目录命名、文档位置、QEMU 入口、板卡入口和日志归档约定。
   - 明确哪些能力可用、哪些需要用户本机配置后才可用。
   - 示例使用 `<repo>`、`<module>`、`<topic>`、`<board-host>`、`<ssh-user>` 等占位符。

## Makefile / Board 边界

- “如何运行某个测试类别”属于 `rvv-test`，例如 production direct test、fallback tests 或 component ablation。
- “如何配置工具链、QEMU、board、部署目录和日志抓回”属于本 skill。
- Makefile / board.mk 的公共变量应使用占位符和 env var 名，例如 `PCL_RVV_BOARD_HOST`，不能写真实 IP 或用户名。
- 只在 board 目标中检查 board 配置。普通 QEMU 或本机构建不应因为板卡配置缺失而失败。

## 提交边界

按 review 边界拆提交：

- 共享环境配置集中化；
- 环境层与 topic 构建规则拆分；
- 后续按迁移批次提交，例如 `filters`、`common`、`2d`、`sample_consensus`、`app`。

不要 stage 本机 `config.mk`、生成的 build 目录或运行日志。只有当 example 配置、README 说明、公共 mk 片段和 topic Makefile 清理共同构成一个完整配置改造时，才放在同一个提交里。
