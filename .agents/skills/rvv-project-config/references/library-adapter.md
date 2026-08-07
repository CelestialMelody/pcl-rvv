# Library Adapter 配置层

RVV 优化 agent 不应把某个项目的目录、测试命令或板卡部署方式写死在全局流程里。每个 C/C++ 库应提供一个轻量 adapter，用于描述“如何在这个库中工作”。

## Adapter 应声明的内容

- 源码根、构建根、RVV 专项测试根和文档根。
- 模块、主题和函数级评估文档的命名规则。
- 上游原始测试、专项 test、bench、QEMU、反汇编和板卡验证入口。
- 公共 Makefile include、脚本目录和日志输出目录。
- 本机配置文件、示例配置文件和不应提交的生成目录。
- 是否存在共享库或预编译库路径，需要双库或双构建对拍。
- 是否存在平台内 x86 SIMD 对照入口。

## Adapter 不应包含的内容

- 个人绝对路径。
- 私有板卡地址、用户名、密钥路径或单一设备默认值。
- 某次实验的临时 build 目录、日志目录或命令历史。
- 当前主题专属的源码事实；这些应进入主题评估和文档。

## 建议字段

```text
repo_root: <repo>
paths.test_root: <repo>/<configured-test-root>
paths.doc_root: <repo>/<configured-doc-root>
artifact_layout.screening_root_template: <configured-doc-root>/<configured-screening-root>
artifact_layout.topic_test_dir_template: <configured-test-root>/<module>/<topic>
artifact_layout.topic_doc_template: <configured-doc-root>/<module>/<topic-doc-name>
artifact_layout.evaluation_doc_subdir: <configured-topic-local-doc-subdir>
artifact_layout.evaluation_doc_template: <configured-test-root>/<module>/<topic>/<configured-topic-local-doc-subdir>/<evaluation-doc-name>
artifact_layout.source_subdir: <configured-topic-local-source-subdir>
artifact_layout.test_source_template: <configured-topic-local-test-source-template>
artifact_layout.bench_source_template: <configured-topic-local-bench-source-template>
artifact_layout.qemu_output_subdir: <configured-qemu-output-subdir>
artifact_layout.board_output_subdir: <configured-board-output-subdir>
```

字段值应允许项目覆盖。公共规则只依赖配置键和 adapter（适配层）声明，不依赖本机配置，也不把某个库的
目录名、文档后缀或输出子目录写成通用规则。

## 验证能力声明

adapter 应把验证能力分成三类：

- `available`：当前仓库公共规则已经提供入口，例如 `run_test_compare`、`run_bench_compare`、`dump_bench_rvv`。
- `requires-local-config`：需要用户提供本机配置，例如交叉工具链、依赖安装前缀、`<board-host>` 或 `<ssh-user>`。
- `unsupported-for-topic`：当前主题尚未支持，例如没有对应上游测试或板卡部署未接入。

文档 closeout 时，只能把 `available` 且已实际运行的证据写成完成状态。`requires-local-config` 应写成待补配置或待补跑命令，不能写成验证失败。

## PCL 当前 adapter 经验

PCL 当前验证项目的默认目录、文档命名、筛选目录、公共 Makefile include、QEMU 输出目录和目标硬件日志目录
来自 `.agents/config/defaults.yaml` 的 `paths` 与 `artifact_layout`。迁移到其它库时，应由该库 adapter 或
local override（本机私有覆盖）替换这些默认值。

模块工作日志和问题讨论文档也应通过 adapter 声明。它们只用于恢复上下文和记录流程，不写入提交型技术文档。

快速定位已有 RVV 生产实现时，在 PCL 源码中优先搜索：

```text
#ifdef __RVV10__
#if defined(__RVV10__)
```

快速定位专项证据时，先用 `artifact_layout.topic_test_dir_template` 解析当前 topic 的测试资产目录，再按
`artifact_layout.makefile_name`、`artifact_layout.board_makefile_name`、`artifact_layout.test_source_template`、
`artifact_layout.bench_source_template`、`artifact_layout.evaluation_doc_template`、`artifact_layout.qemu_output_subdir` 和
`artifact_layout.board_output_subdir` 查找测试、bench、脚本和日志入口。

上述路径、文件名和搜索入口都是配置解析结果，不是 generic RVV agent 的硬编码要求。迁移到其它 C/C++ 库时，由该库 adapter 提供等价的源码、测试、文档、日志和筛选入口。

PCL 的某些路径在共享库中实现。此类主题需要明确“可执行文件宏是否影响库内路径”。如果不影响，应使用双库或双构建对拍，并在文档中写清构建、部署和日志来源。

## PCL 工作日志与问题讨论分工

PCL adapter 将主题推进事实和讨论型知识分开记录：

- 模块工作日志记录主题推进事实、证据链、状态同步、closeout 结果和下一次恢复需要的当前状态。
- 模块“问题与讨论”文档记录讨论型问题、复盘问答、实现策略取舍、workflow 规则来源和后续可能沉淀到 `.skills` 或项目知识文档的规则。
- 主题实现事实进入函数级评估和主题 RVV 文档；不要只停留在对话或工作日志中。
- 技术文档不把结论归因于对话参与者；问题讨论也应使用中性技术表述。

如果一次工作同时产生推进事实和规则复盘，先把 closeout 状态写入模块工作日志，再把策略取舍或规则来源写入问题讨论文档。
