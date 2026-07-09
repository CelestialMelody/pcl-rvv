# =============================================================================
#  test-rvv/app RVV pipeline Makefile
#
# The primary pipeline_app std/RVV benchmark uses test-rvv/mk/rvv-topic.mk.
# DAG, hot-pipeline, read_vlen and board comparison orchestration stay here
# because app is a multi-binary benchmark workload rather than a single test
# topic.
# =============================================================================

TOPIC  := app
MODULE := app

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

TARGET_BENCH     := pipeline_app
TARGET_BENCH_STD := pipeline_app_std
TARGET_BENCH_RVV := pipeline_app_rvv
SRCS_BENCH       := pcl_pipeline_app.cpp
HAS_TEST         := 0

PIPELINE_STD_OUTPUT_FILE  := output/qemu/run_pipeline_std.log
PIPELINE_RVV_OUTPUT_FILE  := output/qemu/run_pipeline_rvv.log
PIPELINE_COMPARE_OUTPUT_FILE := output/qemu/analyze_pipeline_compare.log
BENCH_STD_OUTPUT_FILE     := $(PIPELINE_STD_OUTPUT_FILE)
BENCH_RVV_OUTPUT_FILE     := $(PIPELINE_RVV_OUTPUT_FILE)
BENCH_COMPARE_OUTPUT_FILE := $(PIPELINE_COMPARE_OUTPUT_FILE)

TARGET_DAG_STD ?= pipeline_dag_std
TARGET_DAG_RVV ?= pipeline_dag_rvv
TARGET_DAG     ?= pipeline_dag_app
SRCS_DAG       := pcl_pipeline_dag.cpp

TARGET_HOT_STD ?= pipeline_hot_std
TARGET_HOT_RVV ?= pipeline_hot_rvv
TARGET_HOT     ?= pipeline_hot_app
SRCS_HOT       := pcl_pipeline_hot_rvv.cpp

DAG_STD_OUTPUT_FILE  := output/qemu/run_dag_std.log
DAG_RVV_OUTPUT_FILE  := output/qemu/run_dag_rvv.log
DAG_COMPARE_OUTPUT_FILE := output/qemu/analyze_dag_compare.log
HOT_STD_OUTPUT_FILE  := output/qemu/run_hot_std.log
HOT_RVV_OUTPUT_FILE  := output/qemu/run_hot_rvv.log
HOT_COMPARE_OUTPUT_FILE := output/qemu/analyze_hot_compare.log

REMOTE_DIR ?= ~/pcl-test/app

INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/2d/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/sample_consensus/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(HDF5_ROOT)/include \
	-I$(ZLIB_ROOT)/include \
	-I$(LIBPNG_ROOT)/include

LIBS_APP := \
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
LIBS_BENCH := $(LIBS_APP)

include ../mk/rvv-topic.mk

.DEFAULT_GOAL := build

# The app board runner expects compare scripts in app/../script.
REMOTE_SCRIPT_DIR := $(REMOTE_DIR)/../script

PIPELINE_EXE := $(TARGET_BENCH_BIN)
DAG_EXE      := $(BUILD_DIR)/$(ARCH)/$(TARGET_DAG)
HOT_EXE      := $(BUILD_DIR)/$(ARCH)/$(TARGET_HOT)

READ_VLEN_SRC := $(TEST_RVV_SHARED_SCRIPT_DIR)/riscv_read_vlen.c
READ_VLEN_BIN := $(BUILD_DIR)/$(ARCH)/read_vlen
CFLAGS_READ_VLEN ?= -std=c11 -O2 -Wall -Wextra -march=rv64gcv -mabi=lp64d

build: $(PIPELINE_EXE) $(DAG_EXE)
build_hot: $(HOT_EXE)
build_all: $(PIPELINE_EXE) $(DAG_EXE) $(HOT_EXE)

run_pipeline: run_bench
run_pipeline_std: run_bench_std
run_pipeline_rvv: run_bench_rvv
run_pipeline_compare: run_bench_compare
analyze_pipeline_compare: analyze_bench_compare

$(DAG_EXE): $(SRCS_DAG) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] pipeline_dag USE_PCL_RVV10=$(USE_PCL_RVV10) -> $(DAG_EXE)"
	$(CXX) $(CXXFLAGS) $(SRCS_DAG) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_APP) -o $(DAG_EXE)

$(HOT_EXE): $(SRCS_HOT) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] pipeline_hot USE_PCL_RVV10=$(USE_PCL_RVV10) -> $(HOT_EXE)"
	$(CXX) $(CXXFLAGS) $(SRCS_HOT) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_APP) -o $(HOT_EXE)

run_dag: clean_dag_one $(DAG_EXE)
	@echo "[RUN] $(DAG_EXE) (USE_PCL_RVV10=$(USE_PCL_RVV10))"
	$(RUN_CMD) ./$(DAG_EXE)

run_dag_std: clean_dag_std | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_dag USE_PCL_RVV10=0 TARGET_DAG=$(TARGET_DAG_STD) 2>&1 | tee $(DAG_STD_OUTPUT_FILE)

run_dag_rvv: clean_dag_rvv | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_dag USE_PCL_RVV10=1 TARGET_DAG=$(TARGET_DAG_RVV) 2>&1 | tee $(DAG_RVV_OUTPUT_FILE)

run_dag_compare: run_dag_std run_dag_rvv analyze_dag_compare

analyze_dag_compare: $(DAG_STD_OUTPUT_FILE) $(DAG_RVV_OUTPUT_FILE) | $(OUTPUT_DIR_QEMU)
	$(PYTHON_RUN) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(DAG_STD_OUTPUT_FILE) \
		--rvv-log $(DAG_RVV_OUTPUT_FILE) \
		--device "QEMU rv64" \
		2>&1 | tee $(DAG_COMPARE_OUTPUT_FILE)

run_hot: clean_hot_one $(HOT_EXE)
	@echo "[RUN] $(HOT_EXE) (USE_PCL_RVV10=$(USE_PCL_RVV10))"
	$(RUN_CMD) ./$(HOT_EXE)

run_hot_std: clean_hot_std | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_hot USE_PCL_RVV10=0 TARGET_HOT=$(TARGET_HOT_STD) 2>&1 | tee $(HOT_STD_OUTPUT_FILE)

run_hot_rvv: clean_hot_rvv | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_hot USE_PCL_RVV10=1 TARGET_HOT=$(TARGET_HOT_RVV) 2>&1 | tee $(HOT_RVV_OUTPUT_FILE)

run_hot_compare: run_hot_std run_hot_rvv analyze_hot_compare

analyze_hot_compare: $(HOT_STD_OUTPUT_FILE) $(HOT_RVV_OUTPUT_FILE) | $(OUTPUT_DIR_QEMU)
	$(PYTHON_RUN) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(HOT_STD_OUTPUT_FILE) \
		--rvv-log $(HOT_RVV_OUTPUT_FILE) \
		--device "QEMU rv64" \
		2>&1 | tee $(HOT_COMPARE_OUTPUT_FILE)

clean_dag_one:
	rm -f $(DAG_EXE)
clean_dag_std:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_DAG_STD)
clean_dag_rvv:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_DAG_RVV)
clean_hot_one:
	rm -f $(HOT_EXE)
clean_hot_std:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_HOT_STD)
clean_hot_rvv:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_HOT_RVV)
clean_app_bins:
	rm -f $(BUILD_DIR)/$(ARCH)/pipeline_app* $(BUILD_DIR)/$(ARCH)/pipeline_dag* $(BUILD_DIR)/$(ARCH)/pipeline_hot* $(READ_VLEN_BIN)
clean: clean_app_bins

$(READ_VLEN_BIN): $(READ_VLEN_SRC) | $(BUILD_DIR)/$(ARCH)
	$(CC) $(CFLAGS_READ_VLEN) -o $@ $<

info:
	@echo "PCL_INSTALL_ROOT=$(PCL_INSTALL_ROOT)"
	@echo "make run_hot_compare | run_pipeline_compare | run_dag_compare"
	@echo "make deploy_board | deploy_read_vlen | deploy_*"

define DEPLOY_APP_BIN
	@$(STRIP) -s $(BUILD_DIR)/$(ARCH)/$(1) -o $(BUILD_DIR)/$(ARCH)/$(1).stripped
	@rsync -e "$(RSYNC_SSH)" -avzP $(BUILD_DIR)/$(ARCH)/$(1).stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(1)
	@rm -f $(BUILD_DIR)/$(ARCH)/$(1).stripped
endef

deploy_pipeline_std: deploy_bench_std
deploy_pipeline_rvv: deploy_bench_rvv

deploy_dag_std: deploy_files clean_dag_std
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=0 TARGET_DAG=$(TARGET_DAG_STD) $(BUILD_DIR)/$(ARCH)/$(TARGET_DAG_STD)
	$(call DEPLOY_APP_BIN,$(TARGET_DAG_STD))

deploy_dag_rvv: deploy_files clean_dag_rvv
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_DAG=$(TARGET_DAG_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_DAG_RVV)
	$(call DEPLOY_APP_BIN,$(TARGET_DAG_RVV))

deploy_hot_std: deploy_files clean_hot_std
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=0 TARGET_HOT=$(TARGET_HOT_STD) $(BUILD_DIR)/$(ARCH)/$(TARGET_HOT_STD)
	$(call DEPLOY_APP_BIN,$(TARGET_HOT_STD))

deploy_hot_rvv: deploy_files clean_hot_rvv
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_HOT=$(TARGET_HOT_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_HOT_RVV)
	$(call DEPLOY_APP_BIN,$(TARGET_HOT_RVV))

deploy_compare_script: deploy_files

deploy_read_vlen: check_board_ssh $(READ_VLEN_BIN)
	@$(STRIP) -s $(READ_VLEN_BIN) -o $(READ_VLEN_BIN).stripped
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_DIR)"
	@rsync -e "$(RSYNC_SSH)" -avzP $(READ_VLEN_BIN).stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/read_vlen
	@rm -f $(READ_VLEN_BIN).stripped

deploy_pipeline: deploy_pipeline_std deploy_pipeline_rvv deploy_read_vlen deploy_compare_script
deploy_hot: deploy_hot_std deploy_hot_rvv deploy_read_vlen deploy_compare_script
deploy_dag: deploy_dag_std deploy_dag_rvv deploy_read_vlen deploy_compare_script
deploy_board: deploy_dag_std deploy_dag_rvv deploy_hot_std deploy_hot_rvv deploy_read_vlen deploy_compare_script
	@echo "[deploy_board] bins + compare script -> $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/ & $(REMOTE_SCRIPT_DIR)/"

.PHONY: build build_all build_hot run_pipeline run_pipeline_std run_pipeline_rvv run_pipeline_compare \
	run_dag run_dag_std run_dag_rvv run_dag_compare \
	run_hot run_hot_std run_hot_rvv run_hot_compare \
	analyze_pipeline_compare analyze_dag_compare analyze_hot_compare \
	clean_dag_one clean_dag_std clean_dag_rvv clean_hot_one clean_hot_std clean_hot_rvv clean_app_bins \
	info deploy_pipeline_std deploy_pipeline_rvv deploy_dag_std deploy_dag_rvv deploy_hot_std deploy_hot_rvv \
	deploy_compare_script deploy_read_vlen deploy_pipeline deploy_hot deploy_dag
