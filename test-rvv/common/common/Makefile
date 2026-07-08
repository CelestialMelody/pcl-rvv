# =============================================================================
#  PCL common 模块 Makefile (RISC-V & x86)
#  用于 common.h / impl/common.hpp 的单元测试与性能分析
#
#  Usage:
#    make                    -> Build bench (ARCH=riscv，默认 USE_PCL_RVV10=1，Eigen RVV 始终开启)
#    make run_bench          -> Clean, build, run (当前变量)
#    make run_bench_rvv      -> PCL 手写 RVV 内核 (__RVV10__)，Eigen RVV 开启
#    make run_bench_std      -> 无 __RVV10__，仅 Eigen RVV + 自动向量化
#    make dump_bench         -> Clean，按默认变量 dump
#    make dump_bench_rvv     -> 反汇编 -> build/asm/$(ARCH)/bench_common_rvv.full.asm
#    make dump_bench_std     -> 反汇编 -> build/asm/$(ARCH)/bench_common_std.full.asm
#    make compare_asm        -> Python 对比上述路径下的 std vs rvv .full.asm
#    make ARCH=x86           -> Build for x86
# =============================================================================

# ================================================
# 日志与报告配置
# ================================================
LOG_DIR           = log
TIMESTAMP        := $(shell date +%Y-%m-%d_%H-%M-%S)
LOG_FILE          = $(LOG_VEC_MISS_DIR)/vec_missed_$(TIMESTAMP).log
LATEST_VEC_LOG    = $(LOG_DIR)/latest_vec_missed.log
FOCUS_DIR         = pcl/common/include/pcl/common
FILTER_REPORT     = $(LOG_DIR)/filtered_common.log
VEC_LOGS_DIR      = $(LOG_DIR)/vec_logs
LOG_VEC_MISS_DIR  = $(LOG_DIR)/vec_missed_log
ANALYZE_REPORT    = $(LOG_DIR)/analyze_common.log
OUTPUT_DIR        = output
OUTPUT_DIR_BOARD  = $(OUTPUT_DIR)/board
OUTPUT_DIR_QEMU   = $(OUTPUT_DIR)/qemu

BUILD_DIR         = build
ASM_DIR           = $(BUILD_DIR)/asm
SCRIPT_DIR        = script
TEST_RVV_SHARED_SCRIPT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../script)

BENCH_STD_OUTPUT_FILE = $(OUTPUT_DIR_QEMU)/run_bench_std.log
BENCH_RVV_OUTPUT_FILE = $(OUTPUT_DIR_QEMU)/run_bench_rvv.log
BENCH_COMPARE_OUTPUT_FILE = $(OUTPUT_DIR_QEMU)/analyze_bench_compare.log
TEST_OUTPUT_FILE      = $(OUTPUT_DIR_QEMU)/run_test.log

ANALYZE_VEC_SCRIPT    = $(TEST_RVV_SHARED_SCRIPT_DIR)/analyze_vec_log.py
BENCH_COMPARE_SCRIPT  = $(TEST_RVV_SHARED_SCRIPT_DIR)/analyze_bench_compare.py
COMPARE_ASM_SCRIPT    = $(TEST_RVV_SHARED_SCRIPT_DIR)/compare_rvv_asm.py
MAKEFILE_DIR          := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
UV_BIN               ?= uv
# 参数脚本统一走本目录 .venv；由 uv 自动创建并安装依赖（numpy/scipy）
VENV_DIR              := $(MAKEFILE_DIR)/.venv
VENV_PYTHON           := $(VENV_DIR)/bin/python
VENV_DEPS_STAMP       := $(VENV_DIR)/.deps-stamp
PYTHON_BENCH_COMPARE  := $(shell test -x '$(VENV_PYTHON)' && echo '$(VENV_PYTHON)' || echo python3)

# ================================================
# 目标定义
# ================================================
TARGET_BENCH   ?= bench_common
SRCS_BENCH     = bench_common.cpp
TARGET_TEST    = test_common
SRCS_TEST      = test_common.cpp
TARGET_BENCH_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH)
TARGET_TEST_BIN  = $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST)
# 与 test-rvv/2d/Makefile 一致：反汇编输出到 build/asm/$(ARCH)/
TARGET_BENCH_ASM      = $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH).asm
TARGET_BENCH_FULL_ASM = $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH).full.asm

# common.hpp 数学函数相关测试
TARGET_ATAN2_TEST = atan2_test
SRCS_ATAN2_TEST = atan2_test.cpp
TARGET_ATAN2_TEST_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_ATAN2_TEST)

TARGET_ACOS_TEST = acos_test
SRCS_ACOS_TEST = acos_test.cpp
TARGET_ACOS_TEST_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_ACOS_TEST)

TARGET_EXPF_TEST = expf_test
SRCS_EXPF_TEST = expf_test.cpp
TARGET_EXPF_TEST_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_EXPF_TEST)

TARGET_EXPF_REMEZ_VS_TAYLOR = expf_remez_vs_taylor
SRCS_EXPF_REMEZ_VS_TAYLOR = expf_remez_vs_taylor.cpp
TARGET_EXPF_REMEZ_VS_TAYLOR_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_EXPF_REMEZ_VS_TAYLOR)

TARGET_LOGF_TEST = logf_test
SRCS_LOGF_TEST = logf_test.cpp
TARGET_LOGF_TEST_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_LOGF_TEST)

# 是否启用 PCL 手写 RVV 内核 -D__RVV10__（仅 riscv 时生效）；为 1 时 common 内热点走 RVV 内核
USE_PCL_RVV10  ?= 1

# 默认架构
ARCH ?= riscv

ifeq ($(ARCH), riscv)
MODULE ?= common
TOPIC ?= common
include ../../mk/rvv-env.mk
OBJDUMP_CMD = $(OBJDUMP)
VEC_TPYE_NAME = RVV (RISC-V Vector)
endif

ifeq ($(ARCH), x86)
	CC  = gcc
	CXX = g++
	X86_INSTALL_DIR = /home/zoomin/workspace/x86
	WS              = /home/zoomin/workspace
	PCL_SOURCE_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../..)
	PCL_INSTALL_ROOT = $(X86_INSTALL_DIR)/pcl
	EIGEN_ROOT      = $(X86_INSTALL_DIR)/eigen-3.3.9
	BOOST_ROOT      = $(X86_INSTALL_DIR)/boost-1.88.0
	GTEST_ROOT      = /usr

	LIB_DIRS_LIST = $(PCL_INSTALL_ROOT)/lib $(BOOST_ROOT)/lib
	empty :=
	space := $(empty) $(empty)
	LIB_PATH_VAL := $(subst $(space),:,$(LIB_DIRS_LIST))

	CXXFLAGS_ARCH = -march=native -mtune=native -fopt-info-vec-missed=$(LOG_FILE)
	LDFLAGS = $(foreach dir,$(LIB_DIRS_LIST),-L$(dir) -Wl,-rpath-link=$(dir))
	RUN_CMD = LD_LIBRARY_PATH=$(LIB_PATH_VAL):$$LD_LIBRARY_PATH

	OBJDUMP_CMD = objdump
	VEC_TPYE_NAME = AVX/SSE (x86 SIMD)
	VEC_REGEX_STR = %[xyz]mm
endif

# ================= 包含路径 =================
# 优先使用源码中的 common，便于修改后直接测
INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include

ifeq ($(ARCH), riscv)
	INCLUDES += -I$(GTEST_ROOT)/include
endif

# ================= 编译与链接 =================
CXXFLAGS = -std=c++17 -O3 -g $(CXXFLAGS_ARCH) -DPCL_NO_PRECOMPILE $(INCLUDES)

# bench 仅需 pcl_common（及 Eigen/STL）
LIBS_BENCH = -lpcl_common -lm
# test 需要 gtest
LIBS_TEST = -lpcl_common -lgtest -lgtest_main -lpthread -lm

# ================================================
# 目录管理
# ================================================
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LOG_DIR):
	mkdir -p $(LOG_DIR)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(VEC_LOGS_DIR): $(LOG_DIR)
	mkdir -p $(VEC_LOGS_DIR)

# -fopt-info-vec-missed=$(LOG_FILE) 写入子目录，GCC 不会自动 mkdir
$(LOG_VEC_MISS_DIR): $(LOG_DIR)
	mkdir -p $(LOG_VEC_MISS_DIR)

$(BUILD_DIR)/$(ARCH): $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/$(ARCH)

$(ASM_DIR): $(BUILD_DIR)/$(ARCH)
	mkdir -p $(ASM_DIR)

$(ASM_DIR)/$(ARCH): $(ASM_DIR)
	mkdir -p $(ASM_DIR)/$(ARCH)

$(OUTPUT_DIR_QEMU): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_QEMU)

$(OUTPUT_DIR_BOARD): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_BOARD)

# ================================================
# Python venv（uv）
# ================================================
$(VENV_PYTHON):
	@command -v $(UV_BIN) >/dev/null 2>&1 || (echo "缺少 uv，请先安装 uv" >&2; exit 1)
	@echo "[VENV] create/update $(VENV_DIR)"
	@$(UV_BIN) venv $(VENV_DIR)

$(VENV_DEPS_STAMP): $(VENV_PYTHON)
	@echo "[VENV] install numpy/scipy for parms scripts"
	@$(UV_BIN) pip install --python "$(VENV_PYTHON)" numpy scipy
	@touch "$(VENV_DEPS_STAMP)"

# ================================================
# 构建规则
# ================================================
$(TARGET_BENCH_BIN): $(SRCS_BENCH) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Benchmark ($(ARCH))..."
	$(CXX) $(CXXFLAGS) $(SRCS_BENCH) $(LDFLAGS) $(LIBS_BENCH) -o $(TARGET_BENCH_BIN)

$(TARGET_TEST_BIN): $(SRCS_TEST) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Unit Test ($(ARCH))..."
	$(CXX) $(CXXFLAGS) $(SRCS_TEST) $(LDFLAGS) $(LIBS_TEST) -o $(TARGET_TEST_BIN)

$(TARGET_ATAN2_TEST_BIN): $(SRCS_ATAN2_TEST) $(TEST_RVV_SHARED_SCRIPT_DIR)/term_table.hpp | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling atan2_test ($(ARCH))..."
	$(CXX) -std=c++17 -O3 -g $(INCLUDES) $(if $(filter riscv,$(ARCH)),-march=rv64gcv -mabi=lp64d -D__RVV10__ $(EIGEN_RVV_FLAGS),) $(SRCS_ATAN2_TEST) -o $(TARGET_ATAN2_TEST_BIN) -lm

$(TARGET_ACOS_TEST_BIN): $(SRCS_ACOS_TEST) $(TEST_RVV_SHARED_SCRIPT_DIR)/term_table.hpp | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling acos_test ($(ARCH))..."
	$(CXX) -std=c++17 -O3 -g $(INCLUDES) $(if $(filter riscv,$(ARCH)),-march=rv64gcv -mabi=lp64d -D__RVV10__ $(EIGEN_RVV_FLAGS),) $(SRCS_ACOS_TEST) -o $(TARGET_ACOS_TEST_BIN) -lm

$(TARGET_EXPF_TEST_BIN): $(SRCS_EXPF_TEST) | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling expf_test ($(ARCH))..."
	$(CXX) -std=c++17 -O3 -g $(INCLUDES) $(if $(filter riscv,$(ARCH)),-march=rv64gcv -mabi=lp64d -D__RVV10__ $(EIGEN_RVV_FLAGS),) $(if $(filter x86,$(ARCH)),,) $(SRCS_EXPF_TEST) -o $(TARGET_EXPF_TEST_BIN) -lm

$(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN): $(SRCS_EXPF_REMEZ_VS_TAYLOR) | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling expf_remez_vs_taylor ($(ARCH))..."
	$(CXX) -std=c++17 -O3 -g $(INCLUDES) $(if $(filter riscv,$(ARCH)),-march=rv64gcv -mabi=lp64d -D__RVV10__,) $(if $(filter x86,$(ARCH)),,) $(SRCS_EXPF_REMEZ_VS_TAYLOR) -o $(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) -lm

$(TARGET_LOGF_TEST_BIN): $(SRCS_LOGF_TEST) | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling logf_test ($(ARCH))..."
	$(CXX) -std=c++17 -O3 -g $(INCLUDES) $(if $(filter riscv,$(ARCH)),-march=rv64gcv -mabi=lp64d -D__RVV10__ $(EIGEN_RVV_FLAGS),) $(SRCS_LOGF_TEST) -o $(TARGET_LOGF_TEST_BIN) -lm

# ================================================
# 数学函数相关参数计算
# ================================================
parms:
	$(VENV_PYTHON) $(SCRIPT_DIR)/parms.py

parms_expf: $(VENV_DEPS_STAMP)
	$(VENV_PYTHON) $(SCRIPT_DIR)/parms_expf.py

parms_atan2: $(VENV_DEPS_STAMP)
	$(VENV_PYTHON) $(SCRIPT_DIR)/parms_atan2.py

parms_log1p: $(VENV_DEPS_STAMP)
	$(VENV_PYTHON) $(SCRIPT_DIR)/parms_log1p.py

parms_acos: $(VENV_DEPS_STAMP)
	$(VENV_PYTHON) $(SCRIPT_DIR)/parms_acos.py

# ================================================
# 运行
# ================================================
run_bench: clean_bench $(TARGET_BENCH_BIN)
	@echo "[RUN] Benchmark on $(ARCH) (USE_PCL_RVV10=$(USE_PCL_RVV10)..."
	$(RUN_CMD) ./$(TARGET_BENCH_BIN)

run_test: clean_test $(TARGET_TEST_BIN) | $(BUILD_DIR)/$(ARCH) $(OUTPUT_DIR_QEMU)
	@echo "[RUN] Unit Test on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_TEST_BIN) 2>&1 | tee $(TEST_OUTPUT_FILE)

run_atan2_test: $(TARGET_ATAN2_TEST_BIN) | $(BUILD_DIR)/$(ARCH)
	@echo "[RUN] atan2 approximation vs std::atan2..."
	$(RUN_CMD) ./$(TARGET_ATAN2_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR)/run_atan2_test.log

run_acos_test: $(TARGET_ACOS_TEST_BIN) | $(BUILD_DIR)/$(ARCH)
	@echo "[RUN] acos approximation vs std::acos..."
	$(RUN_CMD) ./$(TARGET_ACOS_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR)/run_acos_test.log

run_expf_test: $(TARGET_EXPF_TEST_BIN) | $(BUILD_DIR)/$(ARCH)
	@echo "[RUN] expf approximation vs std::expf..."
	$(RUN_CMD) ./$(TARGET_EXPF_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR)/run_expf_test.log

run_expf_remez_vs_taylor: $(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) | $(BUILD_DIR)/$(ARCH)
	@echo "[RUN] expf Remez/Taylor vs std::expf..."
	$(RUN_CMD) ./$(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) 2>&1 | tee $(OUTPUT_DIR)/run_expf_remez_vs_taylor.log

run_logf_test: $(TARGET_LOGF_TEST_BIN) | $(BUILD_DIR)/$(ARCH)
	@echo "[RUN] logf Remez+RVV vs std::logf..."
	$(RUN_CMD) ./$(TARGET_LOGF_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR)/run_logf_test.log

# 三种 bench 运行目标（互不覆盖，对应下方三种 dump）
# run_bench_rvv10: 与 PCL 安装的 Eigen 配置一致，避免 aligned malloc warning；热点仍由 __RVV10__ 手写 RVV 控制
run_bench_rvv: clean_bench_rvv | $(BUILD_DIR)/$(ARCH) $(OUTPUT_DIR_QEMU)
	@$(MAKE) ARCH=$(ARCH) run_bench USE_PCL_RVV10=1 TARGET_BENCH=bench_common_rvv 2>&1 | tee $(BENCH_RVV_OUTPUT_FILE)

run_bench_std: clean_bench_std | $(BUILD_DIR)/$(ARCH) $(OUTPUT_DIR_QEMU)
	@$(MAKE) ARCH=$(ARCH) run_bench USE_PCL_RVV10=0 TARGET_BENCH=bench_common_std 2>&1 | tee $(BENCH_STD_OUTPUT_FILE)

# 读取 $(BENCH_STD_OUTPUT_FILE) 与 $(BENCH_RVV_OUTPUT_FILE)，打印 Std vs RVV 对比表（需先跑 run_bench_std / run_bench_rvv）
analyze_bench_compare: $(BENCH_STD_OUTPUT_FILE) $(BENCH_RVV_OUTPUT_FILE) | $(OUTPUT_DIR_QEMU)
	@$(PYTHON_BENCH_COMPARE) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(BENCH_STD_OUTPUT_FILE) --rvv-log $(BENCH_RVV_OUTPUT_FILE) 2>&1 | tee $(BENCH_COMPARE_OUTPUT_FILE)

# ================= 分析与反汇编 =================
# 参考 2d/Makefile 的 generate_vec_report 逻辑：
# 1) 在日志目录里寻找最新 vec_missed_*.log 并建立符号链接
# 2) 依据 FOCUS_DIR 过滤；若过滤为空，回退提取 missed: 的前 N 行
# 3) 调用 analyze_vec_log.py 生成分析报告
generate_vec_report: $(TARGET_BENCH_BIN) | $(LOG_DIR) $(LOG_VEC_MISS_DIR)
	@latest_file=""; \
	if ls "$(LOG_VEC_MISS_DIR)"/vec_missed_*.log >/dev/null 2>&1; then \
		latest_file=$$(realpath "$$(ls -1t "$(LOG_VEC_MISS_DIR)"/vec_missed_*.log | head -n 1)"); \
	else \
		echo "[WARN] No vec-missed log found. Using empty log for report generation."; \
		: > "$(LATEST_VEC_LOG)"; \
	fi; \
	if [ -n "$$latest_file" ]; then \
		ln -sf "$$latest_file" "$(LATEST_VEC_LOG)"; \
	fi; \
	echo "[INFO]  Latest vectorization log linked: $(LATEST_VEC_LOG)"; \
	echo "[INFO]  Filtering report for keywords: '$(FOCUS_DIR)'"; \
	grep '$(FOCUS_DIR)' '$(LATEST_VEC_LOG)' -A 1 > '$(FILTER_REPORT)' || true; \
	if [ ! -s "$(FILTER_REPORT)" ]; then \
		echo "[WARN]  No entries matched FOCUS_DIR. Fallback: extract top missed diagnostics."; \
		grep 'missed:' '$(LATEST_VEC_LOG)' | head -n 80 > '$(FILTER_REPORT)' || true; \
	fi; \
	echo "[INFO]  Clean report generated: $(FILTER_REPORT)"; \
	echo "[ANALYZE] Generating Vectorization Analysis Report..."; \
	python3 "$(ANALYZE_VEC_SCRIPT)" "$(FILTER_REPORT)" -o "$(ANALYZE_REPORT)" -s "$(VEC_LOGS_DIR)"; \
	echo "[DONE] Analysis report saved to: $(ANALYZE_REPORT)"; \
	echo "-------------------------------------------------------"; \
	echo "Check optimization hints:"; \
	echo "  cat $(FILTER_REPORT)"; \
	echo "-------------------------------------------------------"

# 内部目标：仅构建并反汇编，不 clean（便于保留多种配置的 asm 供对比）
dump_bench_inner: $(TARGET_BENCH_BIN) | $(ASM_DIR)/$(ARCH)
	@echo "[DUMP] Generating disassembly for $(ARCH) (USE_PCL_RVV10=$(USE_PCL_RVV10), USE_EIGEN_RVV=$(USE_EIGEN_RVV), TARGET=$(TARGET_BENCH))..."
	@$(OBJDUMP_CMD) -d ./$(TARGET_BENCH_BIN) > $(TARGET_BENCH_ASM)
	@echo "[DONE] Disassembly saved to $(TARGET_BENCH_ASM)"
	@$(OBJDUMP_CMD) -d -C -S -l --no-show-raw-insn --wide ./$(TARGET_BENCH_BIN) > $(TARGET_BENCH_FULL_ASM)
	@echo "[DONE] Full disassembly with source saved to $(TARGET_BENCH_FULL_ASM)"
	@echo "-------------------------------------------------------"
	@echo "[CHECK] Vector instructions/registers ($(VEC_TPYE_NAME)) in $(TARGET_BENCH_ASM):"
	@echo "  Total matches: $$(grep -E $(VEC_REGEX_STR) $(TARGET_BENCH_ASM) | wc -l)"
	@echo "  Sample lines:"
	@grep -E $(VEC_REGEX_STR) $(TARGET_BENCH_ASM) | head -n 5 | sed 's/^/    /' || echo "    (None detected)"
	@echo "-------------------------------------------------------"

# 默认：先 clean，再按当前变量生成 bench 及 asm（默认 USE_PCL_RVV10=1）
dump_bench: clean dump_bench_inner

# 三种反汇编目标（输出到不同可执行名/ asm，便于对比）
# PCL 手写 RVV 内核（__RVV10__），common 热点走 RVV，Eigen 路径被替换
dump_bench_rvv:
	@$(MAKE) ARCH=$(ARCH) clean
	@$(MAKE) ARCH=$(ARCH) dump_bench_inner USE_PCL_RVV10=1 TARGET_BENCH=bench_common_rvv

# 仅 Eigen RVV（无 __RVV10__），观察 Eigen Packet 生成的 RVV
dump_bench_std:
	@$(MAKE) ARCH=$(ARCH) dump_bench_inner USE_PCL_RVV10=0 TARGET_BENCH=bench_common_std

# ================= RVV disassembly compare (compare_rvv_asm.py) =================
# Paths to objdump -S -l listings under build/asm/$(ARCH)/; override if needed.
ASM_FULL_STD ?= $(ASM_DIR)/$(ARCH)/bench_common_std.full.asm
ASM_FULL_RVV ?= $(ASM_DIR)/$(ARCH)/bench_common_rvv.full.asm

compare_asm: dump_bench_rvv dump_bench_std $(COMPARE_ASM_SCRIPT) | $(ASM_DIR)/$(ARCH)
	@echo "[COMPARE] RVV in $(ASM_FULL_STD) vs $(ASM_FULL_RVV)..."
	@python3 $(COMPARE_ASM_SCRIPT) "$(ASM_FULL_STD)" "$(ASM_FULL_RVV)" \
		--with-eigen --mnemonic-top 15 \
		$(EXTRA_ARGS)

# ================= 清理 =================
clean:
	rm -f $(LATEST_VEC_LOG)
	rm -f $(TARGET_BENCH_BIN) $(TARGET_TEST_BIN)
	rm -f $(TARGET_ATAN2_TEST_BIN) $(TARGET_ACOS_TEST_BIN) $(TARGET_EXPF_TEST_BIN) $(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) $(TARGET_LOGF_TEST_BIN)
	rm -rf $(ASM_DIR)/$(ARCH)
	rm -rf $(FILTER_REPORT) $(ANALYZE_REPORT) $(VEC_LOGS_DIR) $(LOG_VEC_MISS_DIR)

clean_bench_std:
	rm -rf $(BUILD_DIR)/$(ARCH)/bench_common_std

clean_bench_rvv:
	rm -rf $(BUILD_DIR)/$(ARCH)/bench_common_rvv

clean_bench:
	rm -rf $(BUILD_DIR)/$(ARCH)/bench_common

clean_test:
	rm -rf $(BUILD_DIR)/$(ARCH)/test_common

info:
	@echo "ARCH             : $(ARCH)"
	@echo "CXX              : $(CXX)"
	@echo "PCL_SOURCE_ROOT  : $(PCL_SOURCE_ROOT)"
	@echo "PCL_INSTALL_ROOT : $(PCL_INSTALL_ROOT)"
	@echo "RUN_CMD          : $(RUN_CMD)"

# ================= 远程部署配置 =================
REMOTE_USER ?=
REMOTE_IP   ?=
REMOTE_DIR  = ~/pcl-test/common/common
REMOTE_SCRIPT_DIR = $(REMOTE_DIR)/script

SSH_OPTS ?= -F $(HOME)/.ssh/config
SSH_CMD ?= ssh $(SSH_OPTS)
RSYNC_SSH ?= ssh $(SSH_OPTS)

check_board_ssh:
	@test -n "$(REMOTE_USER)" || (echo "[config] REMOTE_USER is not set. Set it in $(TEST_RVV_ROOT)/config.mk or pass REMOTE_USER=<user>." >&2; exit 1)
	@test -n "$(REMOTE_IP)" || (echo "[config] REMOTE_IP is not set. Set it in $(TEST_RVV_ROOT)/config.mk or pass REMOTE_IP=<ip>." >&2; exit 1)
	@$(SSH_CMD) -o BatchMode=yes -o ConnectTimeout=5 $(REMOTE_USER)@$(REMOTE_IP) true

deploy_files: check_board_ssh
	@echo "[DEPLOY] Syncing analyze_bench_compare.py to $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/script/..."
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_SCRIPT_DIR)"
	@rsync -e "$(RSYNC_SSH)" -avzP $(BENCH_COMPARE_SCRIPT) $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_SCRIPT_DIR)/
	@echo "[DONE] analyze_bench_compare.py deployed."
	@echo "-------------------------------------------------------"

# 构建对应 bench 后 strip 并同步到远程（需先 deploy_lib）
deploy_bench_rvv: deploy_files clean_bench_rvv
	@$(MAKE) ARCH=$(ARCH) USE_PCL_RVV10=1 TARGET_BENCH=bench_common_rvv $(BUILD_DIR)/$(ARCH)/bench_common_rvv
	@echo "[DEPLOY] Stripping and Syncing bench_common_rvv..."
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/bench_common_rvv -o ./bench_common_rvv_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./bench_common_rvv_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/bench_common_rvv
	@rm -f ./bench_common_rvv_stripped
	@echo "[DONE] bench_common_rvv deployed."

deploy_bench_std: deploy_files clean_bench_std
	@$(MAKE) ARCH=$(ARCH) USE_PCL_RVV10=0 TARGET_BENCH=bench_common_std $(BUILD_DIR)/$(ARCH)/bench_common_std
	@echo "[DEPLOY] Stripping and Syncing bench_common_std..."
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/bench_common_std -o ./bench_common_std_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./bench_common_std_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/bench_common_std
	@rm -f ./bench_common_std_stripped
	@echo "[DONE] bench_common_std deployed."

deploy_test: deploy_files clean_test
	@$(MAKE) ARCH=$(ARCH) clean_test $(TARGET_TEST_BIN)
	@echo "[DEPLOY] Stripping and Syncing $(TARGET_TEST)..."
	@$(STRIP) -s ./$(TARGET_TEST_BIN) -o ./$(TARGET_TEST)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_TEST)
	@rm -f ./$(TARGET_TEST)_stripped
	@echo "[DONE] $(TARGET_TEST) deployed."

deploy_atan2_test: $(TARGET_ATAN2_TEST_BIN) deploy_files
	@echo "[DEPLOY] Stripping and Syncing $(TARGET_ATAN2_TEST)..."
	@$(STRIP) -s ./$(TARGET_ATAN2_TEST_BIN) -o ./$(TARGET_ATAN2_TEST)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_ATAN2_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_ATAN2_TEST)
	@rm -f ./$(TARGET_ATAN2_TEST)_stripped
	@echo "[DONE] $(TARGET_ATAN2_TEST) deployed."

deploy_acos_test: $(TARGET_ACOS_TEST_BIN) deploy_files
	@echo "[DEPLOY] Stripping and Syncing $(TARGET_ACOS_TEST)..."
	@$(STRIP) -s ./$(TARGET_ACOS_TEST_BIN) -o ./$(TARGET_ACOS_TEST)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_ACOS_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_ACOS_TEST)
	@rm -f ./$(TARGET_ACOS_TEST)_stripped
	@echo "[DONE] $(TARGET_ACOS_TEST) deployed."

deploy_expf_test: $(TARGET_EXPF_TEST_BIN) deploy_files
	@echo "[DEPLOY] Stripping and Syncing $(TARGET_EXPF_TEST)..."
	@$(STRIP) -s ./$(TARGET_EXPF_TEST_BIN) -o ./$(TARGET_EXPF_TEST)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_EXPF_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_EXPF_TEST)
	@rm -f ./$(TARGET_EXPF_TEST)_stripped
	@echo "[DONE] $(TARGET_EXPF_TEST) deployed."

deploy_expf_remez_vs_taylor: $(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN)
	@echo "[DEPLOY] Stripping and Syncing $(TARGET_EXPF_REMEZ_VS_TAYLOR)..."
	@$(STRIP) -s ./$(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) -o ./$(TARGET_EXPF_REMEZ_VS_TAYLOR)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_EXPF_REMEZ_VS_TAYLOR)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_EXPF_REMEZ_VS_TAYLOR)
	@rm -f ./$(TARGET_EXPF_REMEZ_VS_TAYLOR)_stripped
	@echo "[DONE] $(TARGET_EXPF_REMEZ_VS_TAYLOR) deployed."

deploy_logf_test: $(TARGET_LOGF_TEST_BIN) deploy_files
	@echo "[DEPLOY] Stripping and Syncing $(TARGET_LOGF_TEST)..."
	@$(STRIP) -s ./$(TARGET_LOGF_TEST_BIN) -o ./$(TARGET_LOGF_TEST)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_LOGF_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_LOGF_TEST)
	@rm -f ./$(TARGET_LOGF_TEST)_stripped
	@echo "[DONE] $(TARGET_LOGF_TEST) deployed."

deploy_math: deploy_files deploy_atan2_test deploy_acos_test deploy_expf_test deploy_expf_remez_vs_taylor deploy_logf_test

.PHONY: deploy_libs deploy_bench_rvv deploy_bench_std deploy_test deploy_math

.PHONY: all bench test_build run_bench run_bench_rvv run_bench_std run_test run_atan2_test run_acos_test run_expf_test \
		run_expf_remez_vs_taylor run_logf_test parms parms_expf parms_atan2 parms_remez_atan parms_log1p parms_acos parms_remez_acos clean link_latest_log \
		dump_bench dump_bench_inner dump_bench_rvv dump_bench_std compare_asm info
