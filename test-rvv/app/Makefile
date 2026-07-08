# =============================================================================
#  test-rvv/app — RVV 综合管线（链接 libpcl_* 多组件 + Boost/Flann/Zlib/HDF5 等 RUN_QEMU 时需完整 LD_LIBRARY_PATH）
#
# 用法：
#   make / make build_all     — app + dag + pipeline_hot（见下；默认可执行名沿用 RVV 命名）
#   make build_hot            — 仅编 pipeline_hot_std/rvv（common 热区对比）
#   make run_pipeline_compare / run_dag_compare / run_hot_compare
#   make deploy_board | deploy_read_vlen | deploy_* | deploy_hot_* | deploy_compare_script — strip + rsync
# 说明：
# - pcl_pipeline_app：多支线并联热点；源码头列有「未 RVV」段落；见 pcl_pipeline_hot_rvv 对照实验。
# - pcl_pipeline_dag：单条点云 DAG。
# - pcl_pipeline_hot_rvv：去掉若干未 RVV 阶段后仅剩 common+L1/L2 等（见源码）。
# - 调用方是否 -D__RVV10__ 决定头文件内 RVV 分发。
# =============================================================================

OUTPUT_DIR        = output
OUTPUT_DIR_QEMU   = $(OUTPUT_DIR)/qemu
OUTPUT_DIR_BOARD  = $(OUTPUT_DIR)/board
BUILD_DIR         = build/riscv
LOG_DIR           = log
LOG_VEC_MISS_DIR  = $(LOG_DIR)/vec_missed_log

TEST_RVV_SHARED_SCRIPT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../script)
BENCH_COMPARE_SCRIPT  = $(TEST_RVV_SHARED_SCRIPT_DIR)/analyze_bench_compare.py

PIPELINE_STD_OUTPUT_FILE  = $(OUTPUT_DIR_QEMU)/run_pipeline_std.log
PIPELINE_RVV_OUTPUT_FILE  = $(OUTPUT_DIR_QEMU)/run_pipeline_rvv.log
BENCH_COMPARE_OUTPUT_FILE = $(OUTPUT_DIR_QEMU)/analyze_pipeline_compare.log

SRCS      = pcl_pipeline_app.cpp
SRCS_DAG  = pcl_pipeline_dag.cpp

TARGET_PIPELINE_STD ?= pipeline_app_std
TARGET_PIPELINE_RVV ?= pipeline_app_rvv
TARGET_PIPELINE ?= pipeline_app
PIPELINE_EXE = $(BUILD_DIR)/$(TARGET_PIPELINE)

TARGET_DAG_STD ?= pipeline_dag_std
TARGET_DAG_RVV ?= pipeline_dag_rvv
TARGET_DAG ?= pipeline_dag_app
DAG_EXE = $(BUILD_DIR)/$(TARGET_DAG)

SRCS_HOT = pcl_pipeline_hot_rvv.cpp
TARGET_HOT_STD ?= pipeline_hot_std
TARGET_HOT_RVV ?= pipeline_hot_rvv
TARGET_HOT ?= pipeline_hot_app
HOT_EXE = $(BUILD_DIR)/$(TARGET_HOT)

READ_VLEN_SRC := $(TEST_RVV_SHARED_SCRIPT_DIR)/riscv_read_vlen.c
READ_VLEN_BIN := $(BUILD_DIR)/read_vlen
CFLAGS_READ_VLEN ?= -std=c11 -O2 -Wall -Wextra -march=rv64gcv -mabi=lp64d

HOT_STD_OUTPUT_FILE  = $(OUTPUT_DIR_QEMU)/run_hot_std.log
HOT_RVV_OUTPUT_FILE  = $(OUTPUT_DIR_QEMU)/run_hot_rvv.log
HOT_COMPARE_OUTPUT_FILE = $(OUTPUT_DIR_QEMU)/analyze_hot_compare.log

PYTHON ?= python3

# 与 test-rvv/Makefile 一致，可按环境覆盖
REMOTE_USER     ?=
REMOTE_IP       ?=
REMOTE_APP_DIR  ?= ~/pcl-test/app
# 板卡上与 app 同级的 script/，供 analyze_bench_compare.py（board.mk 的 SCRIPT_DIR 应对齐此路径）
REMOTE_SCRIPT_DIR ?= $(REMOTE_APP_DIR)/../script

USE_PCL_RVV10 ?= 1

EIGEN_RVV_FLAGS ?= -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl
MODULE ?= app
TOPIC ?= app
include ../mk/rvv-env.mk

INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/2d/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/sample_consensus/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(HDF5_ROOT)/include \
	-I$(ZLIB_ROOT)/include \
	-I$(LIBPNG_ROOT)/include

EXTRA_CXXFLAGS ?=
EXTRA_LDFLAGS  ?=

# fopt-info vec-missed 可选；需要时取消下行注释并把 LOG_VEC_MISS_DIR 拷到别处
CXXFLAGS_VEC_MISSED = -fopt-info-vec-missed=$(LOG_VEC_MISS_DIR)/vec_missed_$(shell date +%Y-%m-%d_%H-%M-%S).log

CXXFLAGS = -std=c++17 -O3 -g $(CXXFLAGS_ARCH) -DPCL_NO_PRECOMPILE $(INCLUDES) $(EXTRA_CXXFLAGS)

# 链入 2d/features/filters/io/search/kdtree/octree/sample_consensus + 依赖（与 test-rvv/2d & sample_consensus 板级布局一致）
LIBS = \
	-lpcl_features \
	-lpcl_filters \
	-lpcl_sample_consensus \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_octree \
	-lpcl_io \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lflann_cpp \
	-lz -lhdf5 -llz4 -lpng \
	-lm

# 2d 头文件来自源码树；若安装根无 libpcl_2d，不必链接（卷积等依赖 libpcl_filters）。

# 若链接器报找不到 -lpcl_octree，请核对安装目录下实际 .so 名称。

RUN_CMD = LD_LIBRARY_PATH=$(LIB_PATH_VAL):$$LD_LIBRARY_PATH qemu-riscv64 -L $(RISCV_SYSROOT) -cpu rv64,v=true,vlen=256,elen=64

# 避免出现「默认目标竟是 $(BUILD_DIR) 目录规则」而导致 plain `make` 不编可执行文件
.DEFAULT_GOAL := build

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)
$(OUTPUT_DIR_QEMU): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_QEMU)
$(OUTPUT_DIR_BOARD): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_BOARD)
$(LOG_DIR):
	mkdir -p $(LOG_DIR)
$(LOG_VEC_MISS_DIR): $(LOG_DIR)
	mkdir -p $(LOG_VEC_MISS_DIR)

$(PIPELINE_EXE): $(SRCS) | $(BUILD_DIR) $(LOG_DIR) $(LOG_VEC_MISS_DIR)
	@echo "[BUILD] pipeline USE_PCL_RVV10=$(USE_PCL_RVV10) -> $(PIPELINE_EXE)"
	$(CXX) $(CXXFLAGS) $(SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS) -o $@

$(DAG_EXE): $(SRCS_DAG) | $(BUILD_DIR) $(LOG_DIR) $(LOG_VEC_MISS_DIR)
	@echo "[BUILD] pipeline_dag USE_PCL_RVV10=$(USE_PCL_RVV10) -> $(DAG_EXE)"
	$(CXX) $(CXXFLAGS) $(SRCS_DAG) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS) -o $@

$(HOT_EXE): $(SRCS_HOT) | $(BUILD_DIR) $(LOG_DIR) $(LOG_VEC_MISS_DIR)
	@echo "[BUILD] pipeline_hot USE_PCL_RVV10=$(USE_PCL_RVV10) -> $(HOT_EXE)"
	$(CXX) $(CXXFLAGS) $(SRCS_HOT) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS) -o $@

# 默认构建：并联基准 + DAG 各一（均为当前 USE_PCL_RVV10，默认 RVV）
build: $(PIPELINE_EXE) $(DAG_EXE)

build_hot: $(HOT_EXE)

build_all: $(PIPELINE_EXE) $(DAG_EXE) $(HOT_EXE)

run_pipeline: clean_one $(PIPELINE_EXE) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] $(PIPELINE_EXE) (USE_PCL_RVV10=$(USE_PCL_RVV10))"
	$(RUN_CMD) ./$(PIPELINE_EXE)

run_pipeline_std: clean_std
	@$(MAKE) run_pipeline USE_PCL_RVV10=0 TARGET_PIPELINE=$(TARGET_PIPELINE_STD)

run_pipeline_rvv: clean_rvv
	@$(MAKE) run_pipeline USE_PCL_RVV10=1 TARGET_PIPELINE=$(TARGET_PIPELINE_RVV)

DAG_STD_OUTPUT_FILE  = $(OUTPUT_DIR_QEMU)/run_dag_std.log
DAG_RVV_OUTPUT_FILE  = $(OUTPUT_DIR_QEMU)/run_dag_rvv.log
DAG_COMPARE_OUTPUT_FILE = $(OUTPUT_DIR_QEMU)/analyze_dag_compare.log

run_pipeline_compare: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) run_pipeline_std 2>&1 | tee $(PIPELINE_STD_OUTPUT_FILE)
	@$(MAKE) run_pipeline_rvv 2>&1 | tee $(PIPELINE_RVV_OUTPUT_FILE)
	@$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(PIPELINE_STD_OUTPUT_FILE) \
		--rvv-log $(PIPELINE_RVV_OUTPUT_FILE) \
		--device "QEMU rv64" \
		2>&1 | tee $(BENCH_COMPARE_OUTPUT_FILE)

run_dag: clean_dag_one $(DAG_EXE) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] $(DAG_EXE) (USE_PCL_RVV10=$(USE_PCL_RVV10))"
	$(RUN_CMD) ./$(DAG_EXE)

run_dag_std: clean_dag_std
	@$(MAKE) run_dag USE_PCL_RVV10=0 TARGET_DAG=$(TARGET_DAG_STD)

run_dag_rvv: clean_dag_rvv
	@$(MAKE) run_dag USE_PCL_RVV10=1 TARGET_DAG=$(TARGET_DAG_RVV)

run_dag_compare: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) run_dag_std 2>&1 | tee $(DAG_STD_OUTPUT_FILE)
	@$(MAKE) run_dag_rvv 2>&1 | tee $(DAG_RVV_OUTPUT_FILE)
	@$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(DAG_STD_OUTPUT_FILE) \
		--rvv-log $(DAG_RVV_OUTPUT_FILE) \
		--device "QEMU rv64" \
		2>&1 | tee $(DAG_COMPARE_OUTPUT_FILE)

analyze_dag_compare: $(DAG_STD_OUTPUT_FILE) $(DAG_RVV_OUTPUT_FILE) | $(OUTPUT_DIR_QEMU)
	$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(DAG_STD_OUTPUT_FILE) \
		--rvv-log $(DAG_RVV_OUTPUT_FILE) \
		2>&1 | tee $(DAG_COMPARE_OUTPUT_FILE)

analyze_pipeline_compare: $(PIPELINE_STD_OUTPUT_FILE) $(PIPELINE_RVV_OUTPUT_FILE) | $(OUTPUT_DIR_QEMU)
	$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(PIPELINE_STD_OUTPUT_FILE) \
		--rvv-log $(PIPELINE_RVV_OUTPUT_FILE) \
		2>&1 | tee $(BENCH_COMPARE_OUTPUT_FILE)

run_hot: clean_hot_one $(HOT_EXE) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] $(HOT_EXE) (USE_PCL_RVV10=$(USE_PCL_RVV10))"
	$(RUN_CMD) ./$(HOT_EXE)

run_hot_std: clean_hot_std
	@$(MAKE) run_hot USE_PCL_RVV10=0 TARGET_HOT=$(TARGET_HOT_STD)

run_hot_rvv: clean_hot_rvv
	@$(MAKE) run_hot USE_PCL_RVV10=1 TARGET_HOT=$(TARGET_HOT_RVV)

run_hot_compare: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) run_hot_std 2>&1 | tee $(HOT_STD_OUTPUT_FILE)
	@$(MAKE) run_hot_rvv 2>&1 | tee $(HOT_RVV_OUTPUT_FILE)
	@$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(HOT_STD_OUTPUT_FILE) \
		--rvv-log $(HOT_RVV_OUTPUT_FILE) \
		--device "QEMU rv64" \
		2>&1 | tee $(HOT_COMPARE_OUTPUT_FILE)

analyze_hot_compare: $(HOT_STD_OUTPUT_FILE) $(HOT_RVV_OUTPUT_FILE) | $(OUTPUT_DIR_QEMU)
	$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(HOT_STD_OUTPUT_FILE) \
		--rvv-log $(HOT_RVV_OUTPUT_FILE) \
		2>&1 | tee $(HOT_COMPARE_OUTPUT_FILE)

clean_one:
	rm -f $(PIPELINE_EXE)

clean_dag_one:
	rm -f $(DAG_EXE)

clean_std:
	rm -f $(BUILD_DIR)/$(TARGET_PIPELINE_STD)

clean_rvv:
	rm -f $(BUILD_DIR)/$(TARGET_PIPELINE_RVV)

clean_dag_std:
	rm -f $(BUILD_DIR)/$(TARGET_DAG_STD)

clean_dag_rvv:
	rm -f $(BUILD_DIR)/$(TARGET_DAG_RVV)

clean_hot_one:
	rm -f $(HOT_EXE)

clean_hot_std:
	rm -f $(BUILD_DIR)/$(TARGET_HOT_STD)

clean_hot_rvv:
	rm -f $(BUILD_DIR)/$(TARGET_HOT_RVV)

clean:
	rm -f $(BUILD_DIR)/pipeline_app* $(BUILD_DIR)/$(TARGET_PIPELINE_STD) $(BUILD_DIR)/$(TARGET_PIPELINE_RVV)
	rm -f $(BUILD_DIR)/pipeline_dag* $(BUILD_DIR)/$(TARGET_DAG_STD) $(BUILD_DIR)/$(TARGET_DAG_RVV)
	rm -f $(BUILD_DIR)/pipeline_hot* $(BUILD_DIR)/$(TARGET_HOT_STD) $(BUILD_DIR)/$(TARGET_HOT_RVV)
	rm -f $(READ_VLEN_BIN)

$(READ_VLEN_BIN): $(READ_VLEN_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS_READ_VLEN) -o $@ $<

info:
	@echo "PCL_INSTALL_ROOT=$(PCL_INSTALL_ROOT)"
	@echo "make run_hot_compare | run_pipeline_compare | run_dag_compare"
	@echo "make deploy_board | deploy_read_vlen REMOTE_IP=... | deploy_compare_script REMOTE_IP=..."

# 远端目录（每台目标一条：先 make 再 strip/rsync；需 REMOTE_IP）
_DEPLOY_PREP = @test -n "$(REMOTE_IP)" || (echo "[deploy] 请设置 REMOTE_IP" >&2; exit 1); \
	ssh $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_APP_DIR)"

# 将单款二进制发布到 REMOTE_APP_DIR（需 ssh/rsync，板卡已部署与 test-rvv/Makefile deploy_lib 一致的 lib）
deploy_pipeline_std:
	$(_DEPLOY_PREP)
	@$(MAKE) USE_PCL_RVV10=0 TARGET_PIPELINE=$(TARGET_PIPELINE_STD) $(BUILD_DIR)/$(TARGET_PIPELINE_STD)
	@$(STRIP) -s $(BUILD_DIR)/$(TARGET_PIPELINE_STD) -o $(BUILD_DIR)/$(TARGET_PIPELINE_STD).stripped
	rsync -avzP $(BUILD_DIR)/$(TARGET_PIPELINE_STD).stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/$(TARGET_PIPELINE_STD)
	@rm -f $(BUILD_DIR)/$(TARGET_PIPELINE_STD).stripped
	@echo "[deploy] $(TARGET_PIPELINE_STD) -> $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/"

deploy_pipeline_rvv:
	$(_DEPLOY_PREP)
	@$(MAKE) USE_PCL_RVV10=1 TARGET_PIPELINE=$(TARGET_PIPELINE_RVV) $(BUILD_DIR)/$(TARGET_PIPELINE_RVV)
	@$(STRIP) -s $(BUILD_DIR)/$(TARGET_PIPELINE_RVV) -o $(BUILD_DIR)/$(TARGET_PIPELINE_RVV).stripped
	rsync -avzP $(BUILD_DIR)/$(TARGET_PIPELINE_RVV).stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/$(TARGET_PIPELINE_RVV)
	@rm -f $(BUILD_DIR)/$(TARGET_PIPELINE_RVV).stripped
	@echo "[deploy] $(TARGET_PIPELINE_RVV) -> $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/"

deploy_dag_std:
	$(_DEPLOY_PREP)
	@$(MAKE) USE_PCL_RVV10=0 TARGET_DAG=$(TARGET_DAG_STD) $(BUILD_DIR)/$(TARGET_DAG_STD)
	@$(STRIP) -s $(BUILD_DIR)/$(TARGET_DAG_STD) -o $(BUILD_DIR)/$(TARGET_DAG_STD).stripped
	rsync -avzP $(BUILD_DIR)/$(TARGET_DAG_STD).stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/$(TARGET_DAG_STD)
	@rm -f $(BUILD_DIR)/$(TARGET_DAG_STD).stripped
	@echo "[deploy] $(TARGET_DAG_STD) -> $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/"

deploy_dag_rvv:
	$(_DEPLOY_PREP)
	@$(MAKE) USE_PCL_RVV10=1 TARGET_DAG=$(TARGET_DAG_RVV) $(BUILD_DIR)/$(TARGET_DAG_RVV)
	@$(STRIP) -s $(BUILD_DIR)/$(TARGET_DAG_RVV) -o $(BUILD_DIR)/$(TARGET_DAG_RVV).stripped
	rsync -avzP $(BUILD_DIR)/$(TARGET_DAG_RVV).stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/$(TARGET_DAG_RVV)
	@rm -f $(BUILD_DIR)/$(TARGET_DAG_RVV).stripped
	@echo "[deploy] $(TARGET_DAG_RVV) -> $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/"

# 发布 analyze_bench_compare.py，使板卡上 make -f board.mk run_*_compare 能找到脚本
deploy_compare_script:
	@test -n "$(REMOTE_IP)" || (echo "[deploy] 请设置 REMOTE_IP" >&2; exit 1)
	@test -f "$(BENCH_COMPARE_SCRIPT)" || (echo "[deploy_compare_script] missing $(BENCH_COMPARE_SCRIPT)" >&2; exit 1)
	ssh $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_APP_DIR) $(REMOTE_SCRIPT_DIR)"
	rsync -avzP $(BENCH_COMPARE_SCRIPT) $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_SCRIPT_DIR)/
	@echo "[deploy_compare_script] -> $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_SCRIPT_DIR)/"

deploy_read_vlen:
	$(_DEPLOY_PREP)
	@$(MAKE) $(READ_VLEN_BIN)
	@$(STRIP) -s $(READ_VLEN_BIN) -o $(READ_VLEN_BIN).stripped
	rsync -avzP $(READ_VLEN_BIN).stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/read_vlen
	@rm -f $(READ_VLEN_BIN).stripped
	@echo "[deploy_read_vlen] -> $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/read_vlen"

deploy_hot_std:
	$(_DEPLOY_PREP)
	@$(MAKE) USE_PCL_RVV10=0 TARGET_HOT=$(TARGET_HOT_STD) $(BUILD_DIR)/$(TARGET_HOT_STD)
	@$(STRIP) -s $(BUILD_DIR)/$(TARGET_HOT_STD) -o $(BUILD_DIR)/$(TARGET_HOT_STD).stripped
	rsync -avzP $(BUILD_DIR)/$(TARGET_HOT_STD).stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/$(TARGET_HOT_STD)
	@rm -f $(BUILD_DIR)/$(TARGET_HOT_STD).stripped
	@echo "[deploy] $(TARGET_HOT_STD) -> $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/"

deploy_hot_rvv:
	$(_DEPLOY_PREP)
	@$(MAKE) USE_PCL_RVV10=1 TARGET_HOT=$(TARGET_HOT_RVV) $(BUILD_DIR)/$(TARGET_HOT_RVV)
	@$(STRIP) -s $(BUILD_DIR)/$(TARGET_HOT_RVV) -o $(BUILD_DIR)/$(TARGET_HOT_RVV).stripped
	rsync -avzP $(BUILD_DIR)/$(TARGET_HOT_RVV).stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/$(TARGET_HOT_RVV)
	@rm -f $(BUILD_DIR)/$(TARGET_HOT_RVV).stripped
	@echo "[deploy] $(TARGET_HOT_RVV) -> $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/"

deploy_pipeline: deploy_pipeline_std deploy_pipeline_rvv deploy_read_vlen deploy_compare_script

deploy_hot: deploy_hot_std deploy_hot_rvv deploy_read_vlen deploy_compare_script

deploy_dag: deploy_dag_std deploy_dag_rvv deploy_read_vlen deploy_compare_script

# 四款二进制 + read_vlen（板载 CSR vlenb） + 对比脚本（board.mk 依赖 ../script/analyze_bench_compare.py）
deploy_board: deploy_pipeline_std deploy_pipeline_rvv deploy_hot_std deploy_hot_rvv deploy_dag_std deploy_dag_rvv deploy_read_vlen deploy_compare_script
	@echo "[deploy_board] bins + compare script -> $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_APP_DIR)/ & $(REMOTE_SCRIPT_DIR)/"

.PHONY: build build_all build_hot run_pipeline run_pipeline_std run_pipeline_rvv run_pipeline_compare \
	run_dag run_dag_std run_dag_rvv run_dag_compare \
	run_hot run_hot_std run_hot_rvv run_hot_compare \
	analyze_pipeline_compare analyze_dag_compare analyze_hot_compare \
	clean clean_one clean_dag_one clean_hot_one clean_std clean_rvv clean_dag_std clean_dag_rvv clean_hot_std clean_hot_rvv 	info \
	deploy_board deploy_pipeline_std deploy_pipeline_rvv deploy_dag_std deploy_dag_rvv deploy_hot_std deploy_hot_rvv deploy_read_vlen deploy_compare_script
