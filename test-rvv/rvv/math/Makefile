# =============================================================================
# RVV math helper/vectorization tests（RVV 数学 helper / 向量化测试）
#
# sincos 中文执行地图：
#   - parms_sincos：运行系数和 range reduction（范围约化）参数脚本。
#   - run_sincos_test：运行 dense（密集网格）、adversarial（边界对抗点）、
#     special（特殊值）、signed-zero（带符号零）、scalar/RVV same-chain
#     （标量/RVV 同构对拍）和可选 --bench gate（失败会返回非 0 的验收条件）。
#   - run_sincos_range_smoke：运行 RangeImageSpherical-shaped local xyz smoke
#     （调用方形态冒烟测试）；这是下游证据，不是 production caller（生产调用方）
#     接入。
#   - run_sincos_range_image_spherical_integration_test：只给该目标追加
#     prototype_include（测试专用 include shadow），验证 RangeImageSpherical
#     test-only calculate3DPoint 原型能命中 RVV path（实际 RVV 链路）。
#     该目标不纳入默认 run_math_all。
#   - deploy_sincos_test / deploy_sincos_range_smoke：只复制 scratch（二进制实验）
#     可执行文件和 board.mk 到板卡。它们只在显式调用或 deploy_math_all 中运行；
#     默认目标不会自动跑 sincos。
# =============================================================================

SHELL := /bin/bash
.SHELLFLAGS := -o pipefail -c

ARCH ?= riscv
MODULE := rvv
TOPIC  := math
USE_PCL_RVV10 ?= 1

MAKEFILE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
TEST_RVV_ROOT ?= $(abspath $(MAKEFILE_DIR)/../..)

ifeq ($(ARCH),x86)
CC := gcc
CXX := g++
PCL_SOURCE_ROOT ?= $(abspath $(MAKEFILE_DIR)/../../..)
TEST_RVV_SHARED_SCRIPT_DIR ?= $(TEST_RVV_ROOT)/script
LOG_DIR ?= log
OUTPUT_DIR ?= output
OUTPUT_DIR_QEMU ?= $(OUTPUT_DIR)/qemu
OUTPUT_DIR_BOARD ?= $(OUTPUT_DIR)/board
BUILD_DIR ?= build
RUN_CMD :=
STRIP ?= strip
SSH_OPTS ?= -F $(HOME)/.ssh/config
SSH_CMD ?= ssh $(SSH_OPTS)
RSYNC_SSH ?= ssh $(SSH_OPTS)
REMOTE_USER ?=
REMOTE_IP ?=
REMOTE_DIR ?= /root/pcl-test/rvv/math
MATH_INCLUDES := \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/build/include \
	-I$(PCL_SOURCE_ROOT) \
	-I/usr/include/eigen3
MATH_TEST_CXXFLAGS := -std=c++17 -O3 -g -march=native -mtune=native $(MATH_INCLUDES)
LDFLAGS ?=
PCL_COMMON_LINK := -L$(PCL_SOURCE_ROOT)/build/lib -Wl,-rpath,$(PCL_SOURCE_ROOT)/build/lib -lpcl_common
else
include ../../mk/rvv-env.mk
LOG_DIR ?= log
OUTPUT_DIR ?= output
OUTPUT_DIR_QEMU ?= $(OUTPUT_DIR)/qemu
OUTPUT_DIR_BOARD ?= $(OUTPUT_DIR)/board
BUILD_DIR ?= build
REMOTE_DIR ?= /root/pcl-test/rvv/math
MATH_INCLUDES := \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include
MATH_TEST_CXXFLAGS := -std=c++17 -O3 -g -march=rv64gcv -mabi=lp64d -D__RVV10__ $(EIGEN_RVV_FLAGS) $(MATH_INCLUDES)
PCL_COMMON_LINK := -lpcl_common
endif

SCRIPT_DIR := script
SINCOS_PROTOTYPE_INCLUDE := sincos/prototype_include
UV_BIN ?= uv
VENV_DIR := $(MAKEFILE_DIR)/.venv
VENV_PYTHON := $(VENV_DIR)/bin/python
VENV_DEPS_STAMP := $(VENV_DIR)/.deps-stamp

ACOS_SRC := acos/acos_test.cpp
ATAN2_SRC := atan2/atan2_test.cpp
EXPF_SRC := expf/expf_test.cpp
EXPF_REMEZ_SRC := expf/expf_remez_vs_taylor.cpp
LOGF_SRC := logf/logf_test.cpp
SINCOS_SRC := sincos/sincos_test.cpp
SINCOS_RANGE_SRC := sincos/sincos_range_smoke.cpp
SINCOS_RANGE_IMAGE_SPHERICAL_INTEGRATION_SRC := sincos/sincos_range_image_spherical_integration_test.cpp
SINCOS_RANGE_IMAGE_SPHERICAL_PROTOTYPE_HEADER := $(SINCOS_PROTOTYPE_INCLUDE)/pcl/range_image/impl/range_image_spherical.hpp
SINCOS_HEADER := sincos/sincos_finite_domain.hpp

ACOS_BIN := $(BUILD_DIR)/$(ARCH)/acos/acos_test
ATAN2_BIN := $(BUILD_DIR)/$(ARCH)/atan2/atan2_test
EXPF_BIN := $(BUILD_DIR)/$(ARCH)/expf/expf_test
EXPF_REMEZ_BIN := $(BUILD_DIR)/$(ARCH)/expf/expf_remez_vs_taylor
LOGF_BIN := $(BUILD_DIR)/$(ARCH)/logf/logf_test
SINCOS_BIN := $(BUILD_DIR)/$(ARCH)/sincos/sincos_test
SINCOS_RANGE_BIN := $(BUILD_DIR)/$(ARCH)/sincos/sincos_range_smoke
SINCOS_RANGE_IMAGE_SPHERICAL_INTEGRATION_BIN := $(BUILD_DIR)/$(ARCH)/sincos/sincos_range_image_spherical_integration_test

MATH_BINS := $(ACOS_BIN) $(ATAN2_BIN) $(EXPF_BIN) $(EXPF_REMEZ_BIN) $(LOGF_BIN) $(SINCOS_BIN) $(SINCOS_RANGE_BIN) $(SINCOS_RANGE_IMAGE_SPHERICAL_INTEGRATION_BIN)
COMMON_TERM_TABLE := $(TEST_RVV_SHARED_SCRIPT_DIR)/term_table.hpp

.DEFAULT_GOAL := help

help:
	@echo "RVV math helper tests"
	@echo "  parms parms_acos parms_atan2 parms_expf parms_log1p parms_sincos"
	@echo "  run_acos_test run_atan2_test run_expf_test run_expf_remez_vs_taylor run_logf_test"
	@echo "  run_sincos_test run_sincos_range_smoke run_sincos_range_image_spherical_integration_test run_sincos_bench"
	@echo "  run_math_all deploy_math_all clean_math"

$(LOG_DIR) $(OUTPUT_DIR) $(OUTPUT_DIR_QEMU) $(OUTPUT_DIR_BOARD):
	mkdir -p $@

$(BUILD_DIR)/$(ARCH)/acos $(BUILD_DIR)/$(ARCH)/atan2 $(BUILD_DIR)/$(ARCH)/expf $(BUILD_DIR)/$(ARCH)/logf $(BUILD_DIR)/$(ARCH)/sincos:
	mkdir -p $@

$(OUTPUT_DIR_QEMU)/acos $(OUTPUT_DIR_QEMU)/atan2 $(OUTPUT_DIR_QEMU)/expf $(OUTPUT_DIR_QEMU)/logf $(OUTPUT_DIR_QEMU)/sincos:
	mkdir -p $@

$(VENV_PYTHON):
	@command -v $(UV_BIN) >/dev/null 2>&1 || (echo "Missing uv; install uv or set UV_BIN" >&2; exit 1)
	@$(UV_BIN) venv $(VENV_DIR)

$(VENV_DEPS_STAMP): $(VENV_PYTHON)
	@$(UV_BIN) pip install --python "$(VENV_PYTHON)" numpy scipy
	@touch "$(VENV_DEPS_STAMP)"

$(ACOS_BIN): $(ACOS_SRC) $(COMMON_TERM_TABLE) | $(BUILD_DIR)/$(ARCH)/acos
	$(CXX) $(MATH_TEST_CXXFLAGS) $< $(LDFLAGS) -o $@ -lm

$(ATAN2_BIN): $(ATAN2_SRC) $(COMMON_TERM_TABLE) | $(BUILD_DIR)/$(ARCH)/atan2
	$(CXX) $(MATH_TEST_CXXFLAGS) $< $(LDFLAGS) -o $@ -lm

$(EXPF_BIN): $(EXPF_SRC) $(COMMON_TERM_TABLE) | $(BUILD_DIR)/$(ARCH)/expf
	$(CXX) $(MATH_TEST_CXXFLAGS) $< $(LDFLAGS) -o $@ -lm

$(EXPF_REMEZ_BIN): $(EXPF_REMEZ_SRC) | $(BUILD_DIR)/$(ARCH)/expf
	$(CXX) $(MATH_TEST_CXXFLAGS) $< $(LDFLAGS) -o $@ -lm

$(LOGF_BIN): $(LOGF_SRC) $(COMMON_TERM_TABLE) | $(BUILD_DIR)/$(ARCH)/logf
	$(CXX) $(MATH_TEST_CXXFLAGS) $< $(LDFLAGS) -o $@ -lm

$(SINCOS_BIN): $(SINCOS_SRC) $(SINCOS_HEADER) | $(BUILD_DIR)/$(ARCH)/sincos
	$(CXX) $(MATH_TEST_CXXFLAGS) $< $(LDFLAGS) -o $@ -lm

$(SINCOS_RANGE_BIN): $(SINCOS_RANGE_SRC) $(SINCOS_HEADER) | $(BUILD_DIR)/$(ARCH)/sincos
	$(CXX) $(MATH_TEST_CXXFLAGS) $< $(LDFLAGS) -o $@ -lm

$(SINCOS_RANGE_IMAGE_SPHERICAL_INTEGRATION_BIN): $(SINCOS_RANGE_IMAGE_SPHERICAL_INTEGRATION_SRC) $(SINCOS_HEADER) $(SINCOS_RANGE_IMAGE_SPHERICAL_PROTOTYPE_HEADER) $(PCL_SOURCE_ROOT)/common/include/pcl/common/impl/rvv_math.hpp | $(BUILD_DIR)/$(ARCH)/sincos
	$(CXX) -I$(SINCOS_PROTOTYPE_INCLUDE) $(MATH_TEST_CXXFLAGS) $< $(LDFLAGS) $(PCL_COMMON_LINK) -o $@ -lm

parms:
	python3 script/parms.py

parms_acos: $(VENV_DEPS_STAMP)
	$(VENV_PYTHON) acos/script/parms_acos.py

parms_atan2: $(VENV_DEPS_STAMP)
	$(VENV_PYTHON) atan2/script/parms_atan2.py

parms_expf: $(VENV_DEPS_STAMP)
	$(VENV_PYTHON) expf/script/parms_expf.py

parms_log1p: $(VENV_DEPS_STAMP)
	$(VENV_PYTHON) logf/script/parms_log1p.py

parms_sincos: $(VENV_DEPS_STAMP)
	$(VENV_PYTHON) sincos/script/parms_sincos.py

run_acos_test: $(ACOS_BIN) | $(OUTPUT_DIR_QEMU)/acos
	$(RUN_CMD) ./$(ACOS_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/acos/run_acos_test.log

run_atan2_test: $(ATAN2_BIN) | $(OUTPUT_DIR_QEMU)/atan2
	$(RUN_CMD) ./$(ATAN2_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/atan2/run_atan2_test.log

run_expf_test: $(EXPF_BIN) | $(OUTPUT_DIR_QEMU)/expf
	$(RUN_CMD) ./$(EXPF_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/expf/run_expf_test.log

run_expf_remez_vs_taylor: $(EXPF_REMEZ_BIN) | $(OUTPUT_DIR_QEMU)/expf
	$(RUN_CMD) ./$(EXPF_REMEZ_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/expf/run_expf_remez_vs_taylor.log

run_logf_test: $(LOGF_BIN) | $(OUTPUT_DIR_QEMU)/logf
	$(RUN_CMD) ./$(LOGF_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/logf/run_logf_test.log

# sincos 数学专项 gate。验证近似误差、特殊值、带符号零和 scalar/RVV
# same-chain（标量/RVV 同构对拍）。当前 lp-abs-hi-lo 路径会调用 rvv_math.hpp
# 中的受限 integration prototype（受限接入原型），但不接任何 RangeImage
# production caller（生产调用方）。
run_sincos_test: $(SINCOS_BIN) | $(OUTPUT_DIR_QEMU)/sincos
	$(RUN_CMD) ./$(SINCOS_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/sincos/run_sincos_test.log

# microbench（微基准）入口。二进制内部会先跑 correctness gates（正确性验收），
# 再进入计时段。QEMU timing（QEMU 计时）不是性能证据；板卡计时才是。
run_sincos_bench: $(SINCOS_BIN) | $(OUTPUT_DIR_QEMU)/sincos
	$(RUN_CMD) ./$(SINCOS_BIN) --bench 2>&1 | tee $(OUTPUT_DIR_QEMU)/sincos/run_sincos_bench.log

# caller-shaped smoke（调用方形态冒烟测试）。镜像 RangeImageSpherical local xyz
# 数学形态，使用 scratch helper（实验 helper），仍与 production RangeImage 代码隔离。
run_sincos_range_smoke: $(SINCOS_RANGE_BIN) | $(OUTPUT_DIR_QEMU)/sincos
	$(RUN_CMD) ./$(SINCOS_RANGE_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/sincos/run_sincos_range_smoke.log

# test-only 接入原型验证。该目标通过 prototype_include 覆盖 impl header，只在
# 测试中复刻 RangeImageSpherical::calculate3DPoint 的接入方式；不修改
# production header，不接 base RangeImage，也不纳入 run_math_all。
run_sincos_range_image_spherical_integration_test: $(SINCOS_RANGE_IMAGE_SPHERICAL_INTEGRATION_BIN) | $(OUTPUT_DIR_QEMU)/sincos
	$(RUN_CMD) ./$(SINCOS_RANGE_IMAGE_SPHERICAL_INTEGRATION_BIN) 2>&1 | tee $(OUTPUT_DIR_QEMU)/sincos/run_sincos_range_image_spherical_integration_test.log

run_math_all: run_acos_test run_atan2_test run_expf_test run_expf_remez_vs_taylor run_logf_test run_sincos_test run_sincos_range_smoke

clean_acos:
	rm -rf $(BUILD_DIR)/$(ARCH)/acos $(OUTPUT_DIR_QEMU)/acos $(LOG_DIR)/acos

clean_atan2:
	rm -rf $(BUILD_DIR)/$(ARCH)/atan2 $(OUTPUT_DIR_QEMU)/atan2 $(LOG_DIR)/atan2

clean_expf:
	rm -rf $(BUILD_DIR)/$(ARCH)/expf $(OUTPUT_DIR_QEMU)/expf $(LOG_DIR)/expf

clean_logf:
	rm -rf $(BUILD_DIR)/$(ARCH)/logf $(OUTPUT_DIR_QEMU)/logf $(LOG_DIR)/logf

clean_sincos:
	rm -rf $(BUILD_DIR)/$(ARCH)/sincos $(OUTPUT_DIR_QEMU)/sincos $(LOG_DIR)/sincos

clean_math: clean_acos clean_atan2 clean_expf clean_logf clean_sincos
	rm -rf $(VENV_DIR)

check_board_ssh:
	@test -n "$(REMOTE_USER)" || (echo "[config] REMOTE_USER is not set" >&2; exit 1)
	@test -n "$(REMOTE_IP)" || (echo "[config] REMOTE_IP is not set" >&2; exit 1)
	@$(SSH_CMD) -o BatchMode=yes -o ConnectTimeout=5 $(REMOTE_USER)@$(REMOTE_IP) true

deploy_board_file: check_board_ssh
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_DIR)"
	@rsync -e "$(RSYNC_SSH)" -avzP board.mk $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/board.mk

define DEPLOY_RULE
$(1): check_board_ssh deploy_board_file $(2)
	@echo "[DEPLOY] $(notdir $(2)) -> $(REMOTE_DIR)"
	@$(STRIP) -s ./$(2) -o ./$(notdir $(2))_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(notdir $(2))_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(notdir $(2))
	@rm -f ./$(notdir $(2))_stripped
endef

$(eval $(call DEPLOY_RULE,deploy_acos_test,$(ACOS_BIN)))
$(eval $(call DEPLOY_RULE,deploy_atan2_test,$(ATAN2_BIN)))
$(eval $(call DEPLOY_RULE,deploy_expf_test,$(EXPF_BIN)))
$(eval $(call DEPLOY_RULE,deploy_expf_remez_vs_taylor,$(EXPF_REMEZ_BIN)))
$(eval $(call DEPLOY_RULE,deploy_logf_test,$(LOGF_BIN)))
$(eval $(call DEPLOY_RULE,deploy_sincos_test,$(SINCOS_BIN)))
$(eval $(call DEPLOY_RULE,deploy_sincos_range_smoke,$(SINCOS_RANGE_BIN)))
$(eval $(call DEPLOY_RULE,deploy_sincos_range_image_spherical_integration_test,$(SINCOS_RANGE_IMAGE_SPHERICAL_INTEGRATION_BIN)))

deploy_math_all: deploy_acos_test deploy_atan2_test deploy_expf_test deploy_expf_remez_vs_taylor deploy_logf_test deploy_sincos_test deploy_sincos_range_smoke

.PHONY: help parms parms_acos parms_atan2 parms_expf parms_log1p parms_sincos \
	run_acos_test run_atan2_test run_expf_test run_expf_remez_vs_taylor run_logf_test \
	run_sincos_test run_sincos_bench run_sincos_range_smoke run_sincos_range_image_spherical_integration_test run_math_all \
	clean_acos clean_atan2 clean_expf clean_logf clean_sincos clean_math \
	check_board_ssh deploy_board_file deploy_acos_test deploy_atan2_test deploy_expf_test \
	deploy_expf_remez_vs_taylor deploy_logf_test deploy_sincos_test deploy_sincos_range_smoke \
	deploy_sincos_range_image_spherical_integration_test deploy_math_all
