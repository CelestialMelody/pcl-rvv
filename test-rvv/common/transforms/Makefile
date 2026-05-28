# =============================================================================
#  PCL common/transforms Makefile (RISC-V)
# =============================================================================

LOG_DIR           = log
TIMESTAMP        := $(shell date +%Y-%m-%d_%H-%M-%S)
LOG_FILE          = $(LOG_VEC_MISS_DIR)/vec_missed_$(TIMESTAMP).log
LATEST_VEC_LOG    = $(LOG_DIR)/latest_vec_missed.log
FOCUS_DIR         = pcl/common/include/pcl/common/impl/transforms.hpp
FILTER_REPORT     = $(LOG_DIR)/filtered_transforms.log
VEC_LOGS_DIR      = $(LOG_DIR)/vec_logs
LOG_VEC_MISS_DIR  = $(LOG_DIR)/vec_missed_log
ANALYZE_REPORT    = $(LOG_DIR)/analyze_transforms.log

OUTPUT_DIR        = output
OUTPUT_DIR_BOARD  = $(OUTPUT_DIR)/board
OUTPUT_DIR_QEMU   = $(OUTPUT_DIR)/qemu
OUTPUT_DIR_X86    = $(OUTPUT_DIR)/x86

BUILD_DIR         = build
ASM_DIR           = $(BUILD_DIR)/asm
TEST_RVV_SHARED_SCRIPT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../script)

BENCH_STD_OUTPUT_FILE     ?= $(OUTPUT_DIR_QEMU)/run_bench_std.log
BENCH_RVV_OUTPUT_FILE     ?= $(OUTPUT_DIR_QEMU)/run_bench_rvv.log
BENCH_COMPARE_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/analyze_bench_compare.log
TEST_STD_OUTPUT_FILE      ?= $(OUTPUT_DIR_QEMU)/run_test_std.log
TEST_RVV_OUTPUT_FILE      ?= $(OUTPUT_DIR_QEMU)/run_test_rvv.log
TEST_OUTPUT_FILE          ?= $(OUTPUT_DIR_QEMU)/run_test.log
UPSTREAM_TEST_STD_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/run_upstream_test_std.log
UPSTREAM_TEST_RVV_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/run_upstream_test_rvv.log

ANALYZE_VEC_SCRIPT    = $(TEST_RVV_SHARED_SCRIPT_DIR)/analyze_vec_log.py
BENCH_COMPARE_SCRIPT  = $(TEST_RVV_SHARED_SCRIPT_DIR)/analyze_bench_compare.py
COMPARE_ASM_SCRIPT    = $(TEST_RVV_SHARED_SCRIPT_DIR)/compare_rvv_asm.py

VENV_ACTIVATE := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/.venv/bin/activate)
PYTHON ?= python3
PYTHON_RUN = bash -lc 'if [ -f "$(VENV_ACTIVATE)" ]; then . "$(VENV_ACTIVATE)"; fi; $(PYTHON) "$$@"' --

TARGET_BENCH       ?= bench_transforms
TARGET_BENCH_STD   ?= bench_transforms_std
TARGET_BENCH_RVV   ?= bench_transforms_rvv
TARGET_BENCH_X86_SIMD ?= bench_transforms_x86_simd
SRCS_BENCH          = bench_transforms.cpp
TARGET_TEST        ?= test_transforms
TARGET_TEST_STD    ?= test_transforms_std
TARGET_TEST_RVV    ?= test_transforms_rvv
SRCS_TEST           = test_transforms.cpp
TARGET_UPSTREAM_TEST ?= test_transforms_upstream
TARGET_UPSTREAM_TEST_STD ?= test_transforms_upstream_std
TARGET_UPSTREAM_TEST_RVV ?= test_transforms_upstream_rvv
SRCS_UPSTREAM_TEST  = $(PCL_SOURCE_ROOT)/test/common/test_transforms.cpp

ARCH ?= riscv
TARGET_BENCH_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH)
TARGET_TEST_BIN  = $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST)
TARGET_UPSTREAM_TEST_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_UPSTREAM_TEST)
TARGET_BENCH_ASM      = $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH).asm
TARGET_BENCH_FULL_ASM = $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH).full.asm

USE_PCL_RVV10  ?= 1

WS               = /home/zoomin/codes/RISCV/workspace
PCL_SOURCE_ROOT  = $(WS)/pcl

ifeq ($(ARCH),riscv)
CC  = riscv64-unknown-linux-gnu-gcc
CXX = riscv64-unknown-linux-gnu-g++
STRIP = riscv64-unknown-linux-gnu-strip
OBJDUMP = riscv64-unknown-linux-gnu-objdump

PCL_INSTALL_ROOT = $(WS)/riscv/pcl-rvv
RISCV_DEPS       = $(WS)/riscv
RISCV_SYSROOT   := $(shell $(CC) -print-sysroot)

BOOST_ROOT  = $(RISCV_DEPS)/boost
EIGEN_ROOT  = $(RISCV_DEPS)/eigen-rvv
GTEST_ROOT  = $(RISCV_DEPS)/gtest

EIGEN_RVV_FLAGS ?= -DEIGEN_RISCV64_USE_RVV10 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl
LIB_DIRS_LIST = $(PCL_INSTALL_ROOT)/lib $(BOOST_ROOT)/lib $(GTEST_ROOT)/lib
empty :=
space := $(empty) $(empty)
LIB_PATH_VAL := $(subst $(space),:,$(LIB_DIRS_LIST))

CXXFLAGS_ARCH = -march=rv64gcv -mabi=lp64d \
	-fopt-info-vec-missed=$(LOG_FILE) \
	-DPCL_SILENCE_MALLOC_WARNING=1
CXXFLAGS_ARCH += $(EIGEN_RVV_FLAGS)
ifeq ($(USE_PCL_RVV10),1)
CXXFLAGS_ARCH += -D__RVV10__
endif

LDFLAGS = $(foreach dir,$(LIB_DIRS_LIST),-L$(dir) -Wl,-rpath-link=$(dir))
RUN_CMD = LD_LIBRARY_PATH=$(LIB_PATH_VAL):$$LD_LIBRARY_PATH qemu-riscv64 -L $(RISCV_SYSROOT) -cpu rv64,v=true,vlen=256,elen=64
VEC_TPYE_NAME = RVV (RISC-V Vector)
VEC_REGEX_STR := "[[:space:]]+v[a-z0-9]+(\.[a-z0-9]+)*[[:space:]]+"
endif

ifeq ($(ARCH),x86)
CC  = gcc
CXX = g++
STRIP = strip
OBJDUMP = objdump

WORKSPACE_X86     ?= /home/zoomin/workspace
X86_INSTALL_ROOT  ?= $(WORKSPACE_X86)/x86
PCL_SOURCE_ROOT   = $(WORKSPACE_X86)/pcl
PCL_INSTALL_ROOT  = $(X86_INSTALL_ROOT)/pcl
EIGEN_ROOT        = $(X86_INSTALL_ROOT)/eigen-3.3.9
BOOST_ROOT        = $(X86_INSTALL_ROOT)/boost-1.88.0
GTEST_ROOT        ?=

LIB_DIRS_LIST = $(PCL_INSTALL_ROOT)/lib $(BOOST_ROOT)/lib
ifneq ($(strip $(GTEST_ROOT)),)
LIB_DIRS_LIST += $(GTEST_ROOT)/lib
endif
empty :=
space := $(empty) $(empty)
LIB_PATH_VAL := $(subst $(space),:,$(LIB_DIRS_LIST))

CXXFLAGS_ARCH = -march=native -mtune=native \
	-fopt-info-vec-missed=$(LOG_FILE) \
	-DPCL_SILENCE_MALLOC_WARNING=1
LDFLAGS = $(foreach dir,$(LIB_DIRS_LIST),-L$(dir) -Wl,-rpath-link=$(dir))
RUN_CMD = LD_LIBRARY_PATH=$(LIB_PATH_VAL):$$LD_LIBRARY_PATH
VEC_TPYE_NAME = AVX/SSE (x86 SIMD)
VEC_REGEX_STR = %[xyz]mm
endif

ifneq ($(filter $(ARCH),riscv x86),$(ARCH))
$(error Unsupported ARCH=$(ARCH), expected riscv or x86)
endif

INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include

ifneq ($(strip $(GTEST_ROOT)),)
INCLUDES += -I$(GTEST_ROOT)/include
endif

EXTRA_CXXFLAGS ?=
EXTRA_LDFLAGS  ?=
X86_STD_EXTRA_CXXFLAGS ?= -U__SSE__ -U__SSE2__ -U__SSE3__ -U__SSSE3__ -U__SSE4_1__ -U__SSE4_2__ -U__AVX__ -U__AVX2__ -U__AVX512F__ -fno-tree-vectorize
X86_SIMD_EXTRA_CXXFLAGS ?=
CXXFLAGS = -std=c++17 -O3 -g $(CXXFLAGS_ARCH) -DPCL_NO_PRECOMPILE $(INCLUDES) $(EXTRA_CXXFLAGS)
LIBS_BENCH = -lpcl_common -lm
LIBS_TEST  = -lpcl_common -lgtest -lgtest_main -lpthread -lm

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
$(LOG_DIR):
	mkdir -p $(LOG_DIR)
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)
$(VEC_LOGS_DIR): $(LOG_DIR)
	mkdir -p $(VEC_LOGS_DIR)
$(LOG_VEC_MISS_DIR): $(LOG_DIR)
	mkdir -p $(LOG_VEC_MISS_DIR)
$(BUILD_DIR)/$(ARCH): $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/$(ARCH)
$(ASM_DIR): $(BUILD_DIR)
	mkdir -p $(ASM_DIR)
$(ASM_DIR)/$(ARCH): $(ASM_DIR)
	mkdir -p $(ASM_DIR)/$(ARCH)
$(OUTPUT_DIR_QEMU): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_QEMU)
$(OUTPUT_DIR_BOARD): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_BOARD)
$(OUTPUT_DIR_X86): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_X86)

$(TARGET_BENCH_BIN): $(SRCS_BENCH) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Benchmark ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_BENCH) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_BENCH) -o $(TARGET_BENCH_BIN)

$(TARGET_TEST_BIN): $(SRCS_TEST) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Unit Test ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_TEST) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_TEST) -o $(TARGET_TEST_BIN)

$(TARGET_UPSTREAM_TEST_BIN): $(SRCS_UPSTREAM_TEST) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Upstream Unit Test ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_UPSTREAM_TEST) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_TEST) -o $(TARGET_UPSTREAM_TEST_BIN)

run_bench: clean_bench $(TARGET_BENCH_BIN)
	@echo "[RUN] Benchmark on $(ARCH) (USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(RUN_CMD) ./$(TARGET_BENCH_BIN)

run_test: clean_test $(TARGET_TEST_BIN) | $(BUILD_DIR)/$(ARCH) $(OUTPUT_DIR_QEMU)
	@echo "[RUN] Unit Test on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_TEST_BIN) 2>&1 | tee $(TEST_OUTPUT_FILE)

run_test_std: clean_test_std | $(BUILD_DIR)/$(ARCH) $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_test USE_PCL_RVV10=0 TARGET_TEST=$(TARGET_TEST_STD) TEST_OUTPUT_FILE=$(TEST_STD_OUTPUT_FILE)
run_test_rvv: clean_test_rvv | $(BUILD_DIR)/$(ARCH) $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_test USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) TEST_OUTPUT_FILE=$(TEST_RVV_OUTPUT_FILE)
run_test_compare: run_test_std run_test_rvv

run_upstream_test: clean_upstream_test $(TARGET_UPSTREAM_TEST_BIN) | $(BUILD_DIR)/$(ARCH) $(OUTPUT_DIR_QEMU)
	@echo "[RUN] Upstream Unit Test on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_UPSTREAM_TEST_BIN) 2>&1 | tee $(UPSTREAM_TEST_OUTPUT_FILE)
run_upstream_test_std: clean_upstream_test_std | $(BUILD_DIR)/$(ARCH) $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_upstream_test USE_PCL_RVV10=0 TARGET_UPSTREAM_TEST=$(TARGET_UPSTREAM_TEST_STD) UPSTREAM_TEST_OUTPUT_FILE=$(UPSTREAM_TEST_STD_OUTPUT_FILE)
run_upstream_test_rvv: clean_upstream_test_rvv | $(BUILD_DIR)/$(ARCH) $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_upstream_test USE_PCL_RVV10=1 TARGET_UPSTREAM_TEST=$(TARGET_UPSTREAM_TEST_RVV) UPSTREAM_TEST_OUTPUT_FILE=$(UPSTREAM_TEST_RVV_OUTPUT_FILE)
run_upstream_test_compare: run_upstream_test_std run_upstream_test_rvv
run_test_all: run_test_compare run_upstream_test_compare

run_bench_rvv: clean_bench_rvv | $(BUILD_DIR)/$(ARCH) $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_BENCH_RVV) 2>&1 | tee $(BENCH_RVV_OUTPUT_FILE)
run_bench_std: clean_bench_std | $(BUILD_DIR)/$(ARCH) $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_BENCH_STD) 2>&1 | tee $(BENCH_STD_OUTPUT_FILE)

analyze_bench_compare: $(BENCH_STD_OUTPUT_FILE) $(BENCH_RVV_OUTPUT_FILE) | $(OUTPUT_DIR_QEMU)
	@$(PYTHON_RUN) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(BENCH_STD_OUTPUT_FILE) --rvv-log $(BENCH_RVV_OUTPUT_FILE) 2>&1 | tee $(BENCH_COMPARE_OUTPUT_FILE)
run_bench_compare: run_bench_std run_bench_rvv analyze_bench_compare

run_bench_x86_std: | $(OUTPUT_DIR_X86)
	@mkdir -p $(OUTPUT_DIR_X86)
	@$(MAKE) -C $(CURDIR) ARCH=x86 clean_bench_std
	@$(MAKE) -C $(CURDIR) ARCH=x86 run_bench USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_BENCH_STD) EXTRA_CXXFLAGS='$(X86_STD_EXTRA_CXXFLAGS)' 2>&1 | tee $(OUTPUT_DIR_X86)/run_bench_std.log
run_bench_x86_simd: | $(OUTPUT_DIR_X86)
	@mkdir -p $(OUTPUT_DIR_X86)
	@$(MAKE) -C $(CURDIR) ARCH=x86 clean_bench_simd
	@$(MAKE) -C $(CURDIR) ARCH=x86 run_bench USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_BENCH_X86_SIMD) EXTRA_CXXFLAGS='$(X86_SIMD_EXTRA_CXXFLAGS)' 2>&1 | tee $(OUTPUT_DIR_X86)/run_bench_simd.log
analyze_bench_x86_compare: $(OUTPUT_DIR_X86)/run_bench_std.log $(OUTPUT_DIR_X86)/run_bench_simd.log | $(OUTPUT_DIR_X86)
	@$(PYTHON_RUN) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(OUTPUT_DIR_X86)/run_bench_std.log --rvv-log $(OUTPUT_DIR_X86)/run_bench_simd.log \
		--heading "PCL transforms.hpp Benchmark (x86 Std vs SIMD)" --variant-label SIMD --variant-log-label "SIMD log" \
		2>&1 | tee $(OUTPUT_DIR_X86)/analyze_bench_compare.log
run_bench_x86_compare: run_bench_x86_std run_bench_x86_simd analyze_bench_x86_compare

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
	$(PYTHON_RUN) "$(ANALYZE_VEC_SCRIPT)" "$(FILTER_REPORT)" -o "$(ANALYZE_REPORT)" -s "$(VEC_LOGS_DIR)"; \
	echo "[DONE] Analysis report saved to: $(ANALYZE_REPORT)"

dump_bench_inner: $(TARGET_BENCH_BIN) | $(ASM_DIR)/$(ARCH)
	@echo "[DUMP] Generating disassembly for riscv (USE_PCL_RVV10=$(USE_PCL_RVV10), TARGET=$(TARGET_BENCH))..."
	@$(OBJDUMP) -d ./$(TARGET_BENCH_BIN) > $(TARGET_BENCH_ASM)
	@$(OBJDUMP) -d -C -S -l --no-show-raw-insn --wide ./$(TARGET_BENCH_BIN) > $(TARGET_BENCH_FULL_ASM)
	@echo "[DONE] Full disassembly saved to $(TARGET_BENCH_FULL_ASM)"
	@echo "[CHECK] Vector instructions/registers ($(VEC_TPYE_NAME)) in $(TARGET_BENCH_ASM):"
	@echo "  Total matches: $$(grep -E $(VEC_REGEX_STR) $(TARGET_BENCH_ASM) | wc -l)"

dump_bench: clean dump_bench_inner
dump_bench_rvv:
	@$(MAKE) -C $(CURDIR) clean
	@$(MAKE) -C $(CURDIR) dump_bench_inner USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_BENCH_RVV)
dump_bench_std:
	@$(MAKE) -C $(CURDIR) dump_bench_inner USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_BENCH_STD)

ASM_FULL_STD ?= $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).full.asm
ASM_FULL_RVV ?= $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
compare_asm: dump_bench_rvv dump_bench_std $(COMPARE_ASM_SCRIPT) | $(ASM_DIR)
	@$(PYTHON_RUN) $(COMPARE_ASM_SCRIPT) "$(ASM_FULL_STD)" "$(ASM_FULL_RVV)" $(EXTRA_ARGS)

clean:
	rm -f $(LATEST_VEC_LOG)
	rm -f $(TARGET_BENCH_BIN) $(TARGET_TEST_BIN) $(TARGET_UPSTREAM_TEST_BIN)
	rm -rf $(ASM_DIR)/$(ARCH)
	rm -rf $(FILTER_REPORT) $(ANALYZE_REPORT) $(VEC_LOGS_DIR) $(LOG_VEC_MISS_DIR)
clean_bench_std:
	rm -rf $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD)
clean_bench_rvv:
	rm -rf $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV)
clean_bench_simd:
	rm -rf $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_X86_SIMD)
clean_bench:
	rm -rf $(BUILD_DIR)/$(ARCH)/bench_transforms
clean_test:
	rm -rf $(BUILD_DIR)/$(ARCH)/test_transforms
clean_test_std:
	rm -rf $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_STD)
clean_test_rvv:
	rm -rf $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV)
clean_upstream_test:
	rm -rf $(BUILD_DIR)/$(ARCH)/test_transforms_upstream
clean_upstream_test_std:
	rm -rf $(BUILD_DIR)/$(ARCH)/$(TARGET_UPSTREAM_TEST_STD)
clean_upstream_test_rvv:
	rm -rf $(BUILD_DIR)/$(ARCH)/$(TARGET_UPSTREAM_TEST_RVV)

info:
	@echo "ARCH             : $(ARCH)"
	@echo "CXX              : $(CXX)"
	@echo "PCL_SOURCE_ROOT  : $(PCL_SOURCE_ROOT)"
	@echo "PCL_INSTALL_ROOT : $(PCL_INSTALL_ROOT)"
	@echo "EIGEN_ROOT       : $(EIGEN_ROOT)"
	@echo "BOOST_ROOT       : $(BOOST_ROOT)"
	@echo "RUN_CMD          : $(RUN_CMD)"

REMOTE_USER = root
REMOTE_IP   = 172.16.89.126
REMOTE_DIR  = ~/pcl-test/common/transforms
REMOTE_SCRIPT_DIR = $(REMOTE_DIR)/script

deploy_files:
	@ssh $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_SCRIPT_DIR)"
	@rsync -avzP $(BENCH_COMPARE_SCRIPT) $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_SCRIPT_DIR)/

deploy_bench_rvv: deploy_files clean_bench_rvv
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_BENCH_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) -o ./$(TARGET_BENCH_RVV)_stripped
	@rsync -avzP ./$(TARGET_BENCH_RVV)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_BENCH_RVV)
	@rm -f ./$(TARGET_BENCH_RVV)_stripped

deploy_bench_std: deploy_files clean_bench_std
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_BENCH_STD) $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) -o ./$(TARGET_BENCH_STD)_stripped
	@rsync -avzP ./$(TARGET_BENCH_STD)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_BENCH_STD)
	@rm -f ./$(TARGET_BENCH_STD)_stripped

deploy_test: deploy_files clean_test
	@$(MAKE) -C $(CURDIR) clean_test $(TARGET_TEST_BIN)
	@$(STRIP) -s ./$(TARGET_TEST_BIN) -o ./$(TARGET_TEST)_stripped
	@rsync -avzP ./$(TARGET_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_TEST)
	@rm -f ./$(TARGET_TEST)_stripped

.PHONY: run_bench run_bench_std run_bench_rvv run_bench_compare run_bench_x86_std run_bench_x86_simd run_bench_x86_compare analyze_bench_compare analyze_bench_x86_compare run_test run_test_std run_test_rvv run_test_compare run_upstream_test run_upstream_test_std run_upstream_test_rvv run_upstream_test_compare run_test_all \
	generate_vec_report dump_bench dump_bench_inner dump_bench_rvv dump_bench_std compare_asm clean clean_bench_std clean_bench_rvv clean_bench_simd clean_bench clean_test clean_test_std clean_test_rvv clean_upstream_test clean_upstream_test_std clean_upstream_test_rvv info \
	deploy_files deploy_bench_rvv deploy_bench_std deploy_test