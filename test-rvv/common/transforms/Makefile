# =============================================================================
#  PCL common/transforms RVV diagnostic Makefile
#
# RISC-V build, QEMU and board deploy rules are provided by
# test-rvv/mk/rvv-topic.mk. The x86 Std vs SIMD bench remains a
# transforms-specific compatibility path.
# =============================================================================

ARCH ?= riscv

ifeq ($(ARCH),x86)

TOPIC := transforms
MODULE := common
LOG_DIR ?= log
TIMESTAMP := $(shell date +%Y-%m-%d_%H-%M-%S)
LOG_VEC_MISS_DIR ?= $(LOG_DIR)/vec_missed_log
LOG_FILE ?= $(LOG_VEC_MISS_DIR)/vec_missed_$(TIMESTAMP).log
OUTPUT_DIR ?= output
OUTPUT_DIR_X86 ?= $(OUTPUT_DIR)/x86
BUILD_DIR ?= build
TEST_RVV_SHARED_SCRIPT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../script)
BENCH_COMPARE_SCRIPT := $(TEST_RVV_SHARED_SCRIPT_DIR)/analyze_bench_compare.py
VENV_ACTIVATE ?= $(abspath $(CURDIR)/.venv/bin/activate)
PYTHON ?= python3
PYTHON_RUN = bash -lc 'if [ -f "$(VENV_ACTIVATE)" ]; then . "$(VENV_ACTIVATE)"; fi; $(PYTHON) "$$@"' --

TARGET_BENCH ?= bench_transforms
TARGET_BENCH_STD ?= bench_transforms_std
TARGET_BENCH_X86_SIMD ?= bench_transforms_x86_simd
SRCS_BENCH := bench_transforms.cpp
TARGET_BENCH_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH)

CC := gcc
CXX := g++
STRIP := strip
OBJDUMP := objdump
WORKSPACE_X86 ?= /home/zoomin/workspace
X86_INSTALL_ROOT ?= $(WORKSPACE_X86)/x86
PCL_SOURCE_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../..)
PCL_INSTALL_ROOT := $(X86_INSTALL_ROOT)/pcl
EIGEN_ROOT := $(X86_INSTALL_ROOT)/eigen-3.3.9
BOOST_ROOT := $(X86_INSTALL_ROOT)/boost-1.88.0
GTEST_ROOT ?=

LIB_DIRS_LIST := $(PCL_INSTALL_ROOT)/lib $(BOOST_ROOT)/lib
ifneq ($(strip $(GTEST_ROOT)),)
LIB_DIRS_LIST += $(GTEST_ROOT)/lib
endif
empty :=
space := $(empty) $(empty)
LIB_PATH_VAL := $(subst $(space),:,$(LIB_DIRS_LIST))

INCLUDES := \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include
ifneq ($(strip $(GTEST_ROOT)),)
INCLUDES += -I$(GTEST_ROOT)/include
endif

CXXFLAGS_ARCH := -march=native -mtune=native \
	-fopt-info-vec-missed=$(LOG_FILE) \
	-DPCL_SILENCE_MALLOC_WARNING=1
LDFLAGS := $(foreach dir,$(LIB_DIRS_LIST),-L$(dir) -Wl,-rpath-link=$(dir))
RUN_CMD := LD_LIBRARY_PATH=$(LIB_PATH_VAL):$$LD_LIBRARY_PATH
EXTRA_CXXFLAGS ?=
EXTRA_LDFLAGS ?=
CXXFLAGS := -std=c++17 -O3 -g $(CXXFLAGS_ARCH) -DPCL_NO_PRECOMPILE $(INCLUDES) $(EXTRA_CXXFLAGS)
LIBS_BENCH := -lpcl_common -lm

$(LOG_DIR):
	mkdir -p $(LOG_DIR)
$(LOG_VEC_MISS_DIR): $(LOG_DIR)
	mkdir -p $(LOG_VEC_MISS_DIR)
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)
$(OUTPUT_DIR_X86): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_X86)
$(BUILD_DIR)/$(ARCH):
	mkdir -p $(BUILD_DIR)/$(ARCH)

$(TARGET_BENCH_BIN): $(SRCS_BENCH) | $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Benchmark ($(ARCH))..."
	$(CXX) $(CXXFLAGS) $(SRCS_BENCH) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_BENCH) -o $(TARGET_BENCH_BIN)

run_bench: clean_bench $(TARGET_BENCH_BIN)
	@echo "[RUN] Benchmark on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_BENCH_BIN)

clean_bench:
	rm -f $(TARGET_BENCH_BIN)
clean_bench_std:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD)
clean_bench_simd:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_X86_SIMD)
clean:
	rm -rf $(BUILD_DIR)/$(ARCH) $(LOG_DIR)

.PHONY: run_bench clean_bench clean_bench_std clean_bench_simd clean

else

TOPIC  := transforms
MODULE := common

FOCUS_DIR      := pcl/common/include/pcl/common/impl/transforms.hpp
FILTER_REPORT  := log/filtered_transforms.log
ANALYZE_REPORT := log/analyze_transforms.log

TARGET_BENCH     := bench_transforms
TARGET_BENCH_STD := bench_transforms_std
TARGET_BENCH_RVV := bench_transforms_rvv
TARGET_BENCH_X86_SIMD ?= bench_transforms_x86_simd
SRCS_BENCH       := bench_transforms.cpp

TARGET_TEST     := test_transforms
TARGET_TEST_STD := test_transforms_std
TARGET_TEST_RVV := test_transforms_rvv
BOARD_TARGET_TEST := test_transforms
SRCS_TEST       := test_transforms.cpp

TARGET_UPSTREAM_TEST := test_transforms_upstream
TARGET_UPSTREAM_TEST_STD := test_transforms_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_transforms_upstream_rvv
SRCS_UPSTREAM_TEST = $(PCL_SOURCE_ROOT)/test/common/test_transforms.cpp

OUTPUT_DIR_X86 ?= $(OUTPUT_DIR)/x86
REMOTE_DIR ?= ~/pcl-test/common/transforms

LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_UPSTREAM_TEST := $(LIBS_TEST)

include ../../mk/rvv-topic.mk

X86_STD_EXTRA_CXXFLAGS ?= -U__SSE__ -U__SSE2__ -U__SSE3__ -U__SSSE3__ -U__SSE4_1__ -U__SSE4_2__ -U__AVX__ -U__AVX2__ -U__AVX512F__ -fno-tree-vectorize
X86_SIMD_EXTRA_CXXFLAGS ?=

$(OUTPUT_DIR_X86): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_X86)

run_bench_x86_std: | $(OUTPUT_DIR_X86)
	@$(MAKE) -C $(CURDIR) ARCH=x86 clean_bench_std
	@$(MAKE) -C $(CURDIR) ARCH=x86 run_bench USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_BENCH_STD) EXTRA_CXXFLAGS='$(X86_STD_EXTRA_CXXFLAGS)' 2>&1 | tee $(OUTPUT_DIR_X86)/run_bench_std.log

run_bench_x86_simd: | $(OUTPUT_DIR_X86)
	@$(MAKE) -C $(CURDIR) ARCH=x86 clean_bench_simd
	@$(MAKE) -C $(CURDIR) ARCH=x86 run_bench USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_BENCH_X86_SIMD) EXTRA_CXXFLAGS='$(X86_SIMD_EXTRA_CXXFLAGS)' 2>&1 | tee $(OUTPUT_DIR_X86)/run_bench_simd.log

analyze_bench_x86_compare: $(OUTPUT_DIR_X86)/run_bench_std.log $(OUTPUT_DIR_X86)/run_bench_simd.log | $(OUTPUT_DIR_X86)
	@$(PYTHON_RUN) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(OUTPUT_DIR_X86)/run_bench_std.log --rvv-log $(OUTPUT_DIR_X86)/run_bench_simd.log \
		--heading "PCL transforms.hpp Benchmark (x86 Std vs SIMD)" --variant-label SIMD --variant-log-label "SIMD log" \
		2>&1 | tee $(OUTPUT_DIR_X86)/analyze_bench_compare.log

run_bench_x86_compare: run_bench_x86_std run_bench_x86_simd analyze_bench_x86_compare

.PHONY: run_bench_x86_std run_bench_x86_simd analyze_bench_x86_compare run_bench_x86_compare

endif
