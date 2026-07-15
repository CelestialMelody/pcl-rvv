# =============================================================================
# PCL common/common diagnostic Makefile
#
# This topic owns common.hpp benchmark/unit-test coverage only. RVV math helper
# parameter scripts and standalone approximation tests live in test-rvv/rvv/math.
# =============================================================================

ARCH ?= riscv

ifeq ($(ARCH),x86)

TOPIC := common
MODULE := common
LOG_DIR ?= log
TIMESTAMP := $(shell date +%Y-%m-%d_%H-%M-%S)
LOG_VEC_MISS_DIR ?= $(LOG_DIR)/vec_missed_log
LOG_FILE ?= $(LOG_VEC_MISS_DIR)/vec_missed_$(TIMESTAMP).log
OUTPUT_DIR ?= output
OUTPUT_DIR_QEMU ?= $(OUTPUT_DIR)/qemu
BUILD_DIR ?= build

TARGET_BENCH ?= bench_common
SRCS_BENCH := bench_common.cpp
TARGET_TEST ?= test_common
SRCS_TEST := test_common.cpp
TARGET_BENCH_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH)
TARGET_TEST_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST)

CC := gcc
CXX := g++
X86_INSTALL_DIR ?= /home/zoomin/workspace/x86
PCL_SOURCE_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../..)
PCL_INSTALL_ROOT := $(X86_INSTALL_DIR)/pcl
EIGEN_ROOT := $(X86_INSTALL_DIR)/eigen-3.3.9
BOOST_ROOT := $(X86_INSTALL_DIR)/boost-1.88.0
GTEST_ROOT ?= /usr

LIB_DIRS_LIST := $(PCL_INSTALL_ROOT)/lib $(BOOST_ROOT)/lib
empty :=
space := $(empty) $(empty)
LIB_PATH_VAL := $(subst $(space),:,$(LIB_DIRS_LIST))

INCLUDES := \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include

CXXFLAGS_ARCH := -march=native -mtune=native -fopt-info-vec-missed=$(LOG_FILE)
LDFLAGS := $(foreach dir,$(LIB_DIRS_LIST),-L$(dir) -Wl,-rpath-link=$(dir))
RUN_CMD := LD_LIBRARY_PATH=$(LIB_PATH_VAL):$$LD_LIBRARY_PATH
CXXFLAGS := -std=c++17 -O3 -g $(CXXFLAGS_ARCH) -DPCL_NO_PRECOMPILE $(INCLUDES)
LIBS_BENCH := -lpcl_common -lm
LIBS_TEST := -lpcl_common -lgtest -lgtest_main -lpthread -lm

$(LOG_DIR):
	mkdir -p $(LOG_DIR)
$(LOG_VEC_MISS_DIR): $(LOG_DIR)
	mkdir -p $(LOG_VEC_MISS_DIR)
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)
$(OUTPUT_DIR_QEMU): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_QEMU)
$(BUILD_DIR)/$(ARCH):
	mkdir -p $(BUILD_DIR)/$(ARCH)

$(TARGET_BENCH_BIN): $(SRCS_BENCH) | $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling common benchmark ($(ARCH))..."
	$(CXX) $(CXXFLAGS) $(SRCS_BENCH) $(LDFLAGS) $(LIBS_BENCH) -o $(TARGET_BENCH_BIN)

$(TARGET_TEST_BIN): $(SRCS_TEST) | $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling common unit test ($(ARCH))..."
	$(CXX) $(CXXFLAGS) $(SRCS_TEST) $(LDFLAGS) $(LIBS_TEST) -o $(TARGET_TEST_BIN)

run_bench: clean_bench $(TARGET_BENCH_BIN)
	@echo "[RUN] common benchmark on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_BENCH_BIN)

run_test: clean_test $(TARGET_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] common unit test on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_test.log

clean_bench:
	rm -f $(TARGET_BENCH_BIN)

clean_test:
	rm -f $(TARGET_TEST_BIN)

clean: clean_bench clean_test
	rm -rf $(LOG_VEC_MISS_DIR)

.PHONY: run_bench run_test clean_bench clean_test clean

else

TOPIC  := common
MODULE := common

FOCUS_DIR      := pcl/common/include/pcl/common
FILTER_REPORT  := log/filtered_common.log
ANALYZE_REPORT := log/analyze_common.log

TARGET_BENCH     := bench_common
TARGET_BENCH_STD := bench_common_std
TARGET_BENCH_RVV := bench_common_rvv
SRCS_BENCH       := bench_common.cpp

TARGET_TEST     := test_common
TARGET_TEST_STD := test_common_std
TARGET_TEST_RVV := test_common_rvv
BOARD_TARGET_TEST := test_common
SRCS_TEST       := test_common.cpp

REMOTE_DIR ?= ~/pcl-test/common/common

LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm

include ../../mk/rvv-topic.mk

endif
