# =============================================================================
#  PCL common/common diagnostic Makefile
#
# RISC-V build, QEMU and board deploy rules are provided by
# test-rvv/mk/rvv-topic.mk. The approximation parameter scripts and standalone
# math tests remain common/common-specific extensions. ARCH=x86 is kept as a
# local compatibility path because rvv-topic.mk is intentionally RVV-only.
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
SCRIPT_DIR := script
MAKEFILE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
UV_BIN ?= uv
VENV_DIR := $(MAKEFILE_DIR)/.venv
VENV_PYTHON := $(VENV_DIR)/bin/python
VENV_DEPS_STAMP := $(VENV_DIR)/.deps-stamp

TARGET_BENCH ?= bench_common
SRCS_BENCH := bench_common.cpp
TARGET_TEST ?= test_common
SRCS_TEST := test_common.cpp
TARGET_BENCH_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH)
TARGET_TEST_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST)

TARGET_ATAN2_TEST := atan2_test
SRCS_ATAN2_TEST := atan2_test.cpp
TARGET_ACOS_TEST := acos_test
SRCS_ACOS_TEST := acos_test.cpp
TARGET_EXPF_TEST := expf_test
SRCS_EXPF_TEST := expf_test.cpp
TARGET_EXPF_REMEZ_VS_TAYLOR := expf_remez_vs_taylor
SRCS_EXPF_REMEZ_VS_TAYLOR := expf_remez_vs_taylor.cpp
TARGET_LOGF_TEST := logf_test
SRCS_LOGF_TEST := logf_test.cpp
TARGET_ATAN2_TEST_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_ATAN2_TEST)
TARGET_ACOS_TEST_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_ACOS_TEST)
TARGET_EXPF_TEST_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_EXPF_TEST)
TARGET_EXPF_REMEZ_VS_TAYLOR_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_EXPF_REMEZ_VS_TAYLOR)
TARGET_LOGF_TEST_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_LOGF_TEST)

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
MATH_TEST_CXXFLAGS := -std=c++17 -O3 -g $(INCLUDES)

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

$(VENV_PYTHON):
	@command -v $(UV_BIN) >/dev/null 2>&1 || (echo "缺少 uv，请先安装 uv" >&2; exit 1)
	@$(UV_BIN) venv $(VENV_DIR)

$(VENV_DEPS_STAMP): $(VENV_PYTHON)
	@$(UV_BIN) pip install --python "$(VENV_PYTHON)" numpy scipy
	@touch "$(VENV_DEPS_STAMP)"

$(TARGET_BENCH_BIN): $(SRCS_BENCH) | $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Benchmark ($(ARCH))..."
	$(CXX) $(CXXFLAGS) $(SRCS_BENCH) $(LDFLAGS) $(LIBS_BENCH) -o $(TARGET_BENCH_BIN)

$(TARGET_TEST_BIN): $(SRCS_TEST) | $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Unit Test ($(ARCH))..."
	$(CXX) $(CXXFLAGS) $(SRCS_TEST) $(LDFLAGS) $(LIBS_TEST) -o $(TARGET_TEST_BIN)

$(TARGET_ATAN2_TEST_BIN): $(SRCS_ATAN2_TEST) | $(BUILD_DIR)/$(ARCH)
	$(CXX) $(MATH_TEST_CXXFLAGS) $(SRCS_ATAN2_TEST) -o $(TARGET_ATAN2_TEST_BIN) -lm
$(TARGET_ACOS_TEST_BIN): $(SRCS_ACOS_TEST) | $(BUILD_DIR)/$(ARCH)
	$(CXX) $(MATH_TEST_CXXFLAGS) $(SRCS_ACOS_TEST) -o $(TARGET_ACOS_TEST_BIN) -lm
$(TARGET_EXPF_TEST_BIN): $(SRCS_EXPF_TEST) | $(BUILD_DIR)/$(ARCH)
	$(CXX) $(MATH_TEST_CXXFLAGS) $(SRCS_EXPF_TEST) -o $(TARGET_EXPF_TEST_BIN) -lm
$(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN): $(SRCS_EXPF_REMEZ_VS_TAYLOR) | $(BUILD_DIR)/$(ARCH)
	$(CXX) $(MATH_TEST_CXXFLAGS) $(SRCS_EXPF_REMEZ_VS_TAYLOR) -o $(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) -lm
$(TARGET_LOGF_TEST_BIN): $(SRCS_LOGF_TEST) | $(BUILD_DIR)/$(ARCH)
	$(CXX) $(MATH_TEST_CXXFLAGS) $(SRCS_LOGF_TEST) -o $(TARGET_LOGF_TEST_BIN) -lm

parms:
	$(VENV_PYTHON) $(SCRIPT_DIR)/parms.py
parms_expf parms_atan2 parms_log1p parms_acos: $(VENV_DEPS_STAMP)
	$(VENV_PYTHON) $(SCRIPT_DIR)/$@.py

run_bench: clean_bench $(TARGET_BENCH_BIN)
	@echo "[RUN] Benchmark on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_BENCH_BIN)
run_test: clean_test $(TARGET_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] Unit Test on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_test.log
run_atan2_test: $(TARGET_ATAN2_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(TARGET_ATAN2_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_atan2_test.log
run_acos_test: $(TARGET_ACOS_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(TARGET_ACOS_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_acos_test.log
run_expf_test: $(TARGET_EXPF_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(TARGET_EXPF_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_expf_test.log
run_expf_remez_vs_taylor: $(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_expf_remez_vs_taylor.log
run_logf_test: $(TARGET_LOGF_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(TARGET_LOGF_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_logf_test.log
run_math: run_atan2_test run_acos_test run_expf_test run_logf_test

clean_bench:
	rm -f $(TARGET_BENCH_BIN)
clean_test:
	rm -f $(TARGET_TEST_BIN)
clean_math:
	rm -f $(TARGET_ATAN2_TEST_BIN) $(TARGET_ACOS_TEST_BIN) $(TARGET_EXPF_TEST_BIN) $(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) $(TARGET_LOGF_TEST_BIN)
clean: clean_bench clean_test clean_math
	rm -rf $(LOG_VEC_MISS_DIR)

.PHONY: run_bench run_test run_atan2_test run_acos_test run_expf_test run_expf_remez_vs_taylor run_logf_test run_math \
	clean_bench clean_test clean_math clean parms parms_expf parms_atan2 parms_log1p parms_acos

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

TARGET_ATAN2_TEST := atan2_test
SRCS_ATAN2_TEST := atan2_test.cpp
TARGET_ACOS_TEST := acos_test
SRCS_ACOS_TEST := acos_test.cpp
TARGET_EXPF_TEST := expf_test
SRCS_EXPF_TEST := expf_test.cpp
TARGET_EXPF_REMEZ_VS_TAYLOR := expf_remez_vs_taylor
SRCS_EXPF_REMEZ_VS_TAYLOR := expf_remez_vs_taylor.cpp
TARGET_LOGF_TEST := logf_test
SRCS_LOGF_TEST := logf_test.cpp

REMOTE_DIR ?= ~/pcl-test/common/common

LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm

include ../../mk/rvv-topic.mk

MAKEFILE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SCRIPT_DIR := script
UV_BIN ?= uv
VENV_DIR := $(MAKEFILE_DIR)/.venv
VENV_PYTHON := $(VENV_DIR)/bin/python
VENV_DEPS_STAMP := $(VENV_DIR)/.deps-stamp

TARGET_ATAN2_TEST_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_ATAN2_TEST)
TARGET_ACOS_TEST_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_ACOS_TEST)
TARGET_EXPF_TEST_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_EXPF_TEST)
TARGET_EXPF_REMEZ_VS_TAYLOR_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_EXPF_REMEZ_VS_TAYLOR)
TARGET_LOGF_TEST_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_LOGF_TEST)

MATH_TEST_CXXFLAGS := -std=c++17 -O3 -g $(INCLUDES) -march=rv64gcv -mabi=lp64d -D__RVV10__ $(EIGEN_RVV_FLAGS)

$(VENV_PYTHON):
	@command -v $(UV_BIN) >/dev/null 2>&1 || (echo "缺少 uv，请先安装 uv" >&2; exit 1)
	@echo "[VENV] create/update $(VENV_DIR)"
	@$(UV_BIN) venv $(VENV_DIR)

$(VENV_DEPS_STAMP): $(VENV_PYTHON)
	@echo "[VENV] install numpy/scipy for parms scripts"
	@$(UV_BIN) pip install --python "$(VENV_PYTHON)" numpy scipy
	@touch "$(VENV_DEPS_STAMP)"

$(TARGET_ATAN2_TEST_BIN): $(SRCS_ATAN2_TEST) $(TEST_RVV_SHARED_SCRIPT_DIR)/term_table.hpp | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling atan2_test ($(ARCH))..."
	$(CXX) $(MATH_TEST_CXXFLAGS) $(SRCS_ATAN2_TEST) -o $(TARGET_ATAN2_TEST_BIN) -lm

$(TARGET_ACOS_TEST_BIN): $(SRCS_ACOS_TEST) $(TEST_RVV_SHARED_SCRIPT_DIR)/term_table.hpp | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling acos_test ($(ARCH))..."
	$(CXX) $(MATH_TEST_CXXFLAGS) $(SRCS_ACOS_TEST) -o $(TARGET_ACOS_TEST_BIN) -lm

$(TARGET_EXPF_TEST_BIN): $(SRCS_EXPF_TEST) | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling expf_test ($(ARCH))..."
	$(CXX) $(MATH_TEST_CXXFLAGS) $(SRCS_EXPF_TEST) -o $(TARGET_EXPF_TEST_BIN) -lm

$(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN): $(SRCS_EXPF_REMEZ_VS_TAYLOR) | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling expf_remez_vs_taylor ($(ARCH))..."
	$(CXX) $(MATH_TEST_CXXFLAGS) $(SRCS_EXPF_REMEZ_VS_TAYLOR) -o $(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) -lm

$(TARGET_LOGF_TEST_BIN): $(SRCS_LOGF_TEST) | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling logf_test ($(ARCH))..."
	$(CXX) $(MATH_TEST_CXXFLAGS) $(SRCS_LOGF_TEST) -o $(TARGET_LOGF_TEST_BIN) -lm

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

run_atan2_test: $(TARGET_ATAN2_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] atan2 approximation vs std::atan2..."
	$(RUN_CMD) ./$(TARGET_ATAN2_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_atan2_test.log

run_acos_test: $(TARGET_ACOS_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] acos approximation vs std::acos..."
	$(RUN_CMD) ./$(TARGET_ACOS_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_acos_test.log

run_expf_test: $(TARGET_EXPF_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] expf approximation vs std::expf..."
	$(RUN_CMD) ./$(TARGET_EXPF_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_expf_test.log

run_expf_remez_vs_taylor: $(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] expf Remez/Taylor vs std::expf..."
	$(RUN_CMD) ./$(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_expf_remez_vs_taylor.log

run_logf_test: $(TARGET_LOGF_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] logf Remez+RVV vs std::logf..."
	$(RUN_CMD) ./$(TARGET_LOGF_TEST_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_logf_test.log

clean_math:
	rm -f $(TARGET_ATAN2_TEST_BIN) $(TARGET_ACOS_TEST_BIN) $(TARGET_EXPF_TEST_BIN) $(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) $(TARGET_LOGF_TEST_BIN)

deploy_atan2_test: $(TARGET_ATAN2_TEST_BIN) deploy_files
	@$(STRIP) -s ./$(TARGET_ATAN2_TEST_BIN) -o ./$(TARGET_ATAN2_TEST)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_ATAN2_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_ATAN2_TEST)
	@rm -f ./$(TARGET_ATAN2_TEST)_stripped

deploy_acos_test: $(TARGET_ACOS_TEST_BIN) deploy_files
	@$(STRIP) -s ./$(TARGET_ACOS_TEST_BIN) -o ./$(TARGET_ACOS_TEST)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_ACOS_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_ACOS_TEST)
	@rm -f ./$(TARGET_ACOS_TEST)_stripped

deploy_expf_test: $(TARGET_EXPF_TEST_BIN) deploy_files
	@$(STRIP) -s ./$(TARGET_EXPF_TEST_BIN) -o ./$(TARGET_EXPF_TEST)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_EXPF_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_EXPF_TEST)
	@rm -f ./$(TARGET_EXPF_TEST)_stripped

deploy_expf_remez_vs_taylor: $(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) deploy_files
	@$(STRIP) -s ./$(TARGET_EXPF_REMEZ_VS_TAYLOR_BIN) -o ./$(TARGET_EXPF_REMEZ_VS_TAYLOR)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_EXPF_REMEZ_VS_TAYLOR)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_EXPF_REMEZ_VS_TAYLOR)
	@rm -f ./$(TARGET_EXPF_REMEZ_VS_TAYLOR)_stripped

deploy_logf_test: $(TARGET_LOGF_TEST_BIN) deploy_files
	@$(STRIP) -s ./$(TARGET_LOGF_TEST_BIN) -o ./$(TARGET_LOGF_TEST)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_LOGF_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_LOGF_TEST)
	@rm -f ./$(TARGET_LOGF_TEST)_stripped

deploy_math: deploy_atan2_test deploy_acos_test deploy_expf_test deploy_expf_remez_vs_taylor deploy_logf_test
run_math: run_atan2_test run_acos_test run_expf_test run_logf_test

clean: clean_math

.PHONY: parms parms_expf parms_atan2 parms_log1p parms_acos \
	run_atan2_test run_acos_test run_expf_test run_expf_remez_vs_taylor run_logf_test run_math \
	clean_math deploy_atan2_test deploy_acos_test deploy_expf_test deploy_expf_remez_vs_taylor deploy_logf_test deploy_math

endif
