# Makefile 环境层模式

## 推荐目录结构

当一个 C/C++ RVV 测试树中有多个手写 Makefile 时，优先按 `paths.test_root`、
`artifact_layout.shared_make_subdir`、`artifact_layout.makefile_name` 和
`artifact_layout.board_makefile_name` 解析目录和文件名，整理成下面的逻辑结构：

```text
<test-root>/
├── config.mk.example      # 提交：用户可复制的本机配置模板
├── config.mk              # 不提交：本机覆盖配置
└── <shared-make-subdir>/
    ├── rvv-env.mk         # 路径、工具链、依赖根、板卡配置
    ├── rvv-topic.mk       # test/bench 构建与运行规则
    └── rvv-board-run.mk   # 板卡侧运行规则
```

`rvv-env.mk` 应该可以被早期 legacy Makefile 单独 include。这样旧测试可以先统一路径和工具链配置，不必一次性迁移到 `rvv-topic.mk`。

## 变量分层

提交到仓库的默认值用 `?=`，允许命令行和本机 `config.mk` 覆盖。

源码与依赖布局：

```make
ifndef TEST_RVV_ROOT
TEST_RVV_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
endif

-include $(TEST_RVV_ROOT)/config.mk

PCL_SRC_DIR      ?= $(abspath $(TEST_RVV_ROOT)/..)
PCL_SOURCE_ROOT  ?= $(PCL_SRC_DIR)
RV_INSTALL_DIR   ?= $(abspath $(PCL_SRC_DIR)/../riscv)
PCL_INSTALL_ROOT ?= $(RV_INSTALL_DIR)/pcl-rvv
RISCV_DEPS       ?= $(RV_INSTALL_DIR)
```

从 `MAKEFILE_LIST` 推导路径时用 `:=` 立即展开。不要用递归展开的 `=`，否则变量后续求值时 `MAKEFILE_LIST` 可能已经改变，推导出错误根目录。

工具链：

```make
RISCV_TARGET  ?= riscv64-unknown-linux-gnu
CROSS_COMPILE ?= $(RISCV_TARGET)-

ifeq ($(origin CXX),default)
CXX = $(CROSS_COMPILE)g++
endif
```

板卡设置：

```make
REMOTE_USER ?=
REMOTE_IP   ?=
BOARD_LABEL ?= RVV board
SSH_OPTS    ?= -F $(HOME)/.ssh/config
SSH_CMD     ?= ssh $(SSH_OPTS)
RSYNC_SSH   ?= ssh $(SSH_OPTS)
```

只在板卡部署/运行目标里检查 `REMOTE_USER` 和 `REMOTE_IP`，不要让普通 QEMU 或本机构建因为没配板卡而失败。

## PCL Board / SSH Adapter 规则

项目当前专项目录应能从主题 `artifact_layout.makefile_name` 解析出的文件看出板卡闭环是否接上。需要目标硬件性能结论的主题，主 Makefile 或等价构建入口应提供或通过公共 include 提供：

```text
deploy_files
deploy_board
run_board_test
run_board_bench_compare
fetch_board_logs
```

板卡侧 `artifact_layout.board_makefile_name` 解析出的文件或公共片段至少支持：

```text
run_test
run_bench_std
run_bench_rvv
run_bench_compare
analyze_bench_compare
```

职责边界：

- `deploy_*` 只同步脚本、板卡侧 Makefile 和二进制。
- `run_board_*` 只触发板卡侧测试或 bench。
- `fetch_board_logs` 只把板卡输出拉回 `artifact_layout.board_output_subdir` 解析出的本地输出目录。
- 主题 Makefile / board Makefile 只声明 `TOPIC`、`MODULE`、目标名、源码、参数、特殊库和少量覆盖变量；具体文件名由 `artifact_layout.makefile_name` 和 `artifact_layout.board_makefile_name` 决定。
- 公共规则放在 `paths.test_root` 与 `artifact_layout.shared_make_subdir` 解析出的公共 include 目录；无法使用公共 include 时，在函数级评估或问题记录中说明原因。

SSH / rsync 变量：

```make
SSH_OPTS  ?= -F $(HOME)/.ssh/config
SSH_CMD   ?= ssh $(SSH_OPTS)
RSYNC_SSH ?= ssh $(SSH_OPTS)
```

`rsync` 必须配套使用同一 SSH 配置：

```make
rsync -e "$(RSYNC_SSH)" <local-path> <ssh-user>@<board-host>:<remote-dir>/
```

不要在公共配置里写死个人路径、私有板卡地址、用户名、密钥路径或单台设备默认值。示例值使用 `<ssh-user>`、`<board-host>`、`<remote-dir>`、`<board-label>` 这类占位符。

推荐提供 `check_board_ssh` 或同等诊断目标，并让部署目标先依赖它：

```make
check_board_ssh:
	@if [ -z "$(REMOTE_USER)" ] || [ -z "$(REMOTE_IP)" ]; then \
	  echo "Set REMOTE_USER=<ssh-user> and REMOTE_IP=<board-host> for board targets."; \
	  exit 1; \
	fi
	@$(SSH_CMD) -o BatchMode=yes -o ConnectTimeout=5 $(REMOTE_USER)@$(REMOTE_IP) true
```

连接诊断口径：

- 系统 SSH config 权限异常时，优先通过 `SSH_OPTS` 指向用户可读配置，不把它写成板卡不可用。
- 受限执行环境禁止 network socket 时，记录为执行环境限制或待用户在普通 shell/授权环境复跑，不写成目标硬件性能失败。
- 不为省事放开裸 `ssh`、裸 `rsync`、裸 `make` 或所有 `make -C`；规则应尽量窄。
- 技术主题文档不写 SSH 运维细节，只记录目标硬件类型、日志相对路径、数据集、iterations、std/RVV 耗时和 speedup。SSH/rsync 排障写入工作日志、配置说明或问题讨论。

## 配置检查

检查配置目录，不检查某个 topic 的具体文件：

```make
CONFIG_REQUIRED_DIRS := \
  $(PCL_SOURCE_ROOT) \
  $(RV_INSTALL_DIR) \
  $(PCL_INSTALL_ROOT)/lib

CONFIG_MISSING_DIRS := $(foreach dir,$(CONFIG_REQUIRED_DIRS),$(if $(wildcard $(dir)/.),,$(dir)))
ifneq ($(strip $(CONFIG_MISSING_DIRS)),)
$(error Required RVV test directories do not exist: $(CONFIG_MISSING_DIRS). Set PCL_SRC_DIR/RV_INSTALL_DIR/PCL_INSTALL_ROOT in $(TEST_RVV_ROOT)/config.mk or on the make command line)
endif
```

不要在公共配置中检查类似 `common/include/.../some_new_rvv_header.h` 的具体头文件。这类问题应该由 topic 编译错误或 topic 专属检查暴露。

## 迁移检查清单

- 先只搜索源码和配置文件；默认忽略 `output/`、`build/`、`log/` 和生成的 `.log`。
- 分别列出已使用公共模板的测试和自包含 legacy Makefile。
- 确认共享变量能提供同名路径后，再删除 topic Makefile 里的硬编码路径。
- 用户示例保持通用，只使用占位符、env var 名或文档保留地址，不写个人路径、真实私有地址、用户名或单台设备名。
- 本机配置用本地 ignore 或项目约定排除，不要提交。
- 每迁移一批至少跑一个真实 build/test。
- 同时验证一次错误路径诊断。
- 不要提交本机 `config.mk`、build 目录或生成日志。
