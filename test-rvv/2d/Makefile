# =============================================================================
#  PCL 2d RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk. The edge-store micro-benchmark and
# common/math forwarding targets remain 2d-specific.
# =============================================================================

TOPIC  := 2d
MODULE := 2d
THIS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/2d/include/pcl/2d/impl/edge.hpp
FILTER_REPORT  := log/filtered_report.log
ANALYZE_REPORT := log/analyzed_vec_report.log

TARGET_BENCH     := bench_2d
TARGET_BENCH_STD := bench_2d_std
TARGET_BENCH_RVV := bench_2d_rvv
SRCS_BENCH       := bench_2d.cpp

TARGET_TEST     := test_2d
TARGET_TEST_STD := test_2d_std
TARGET_TEST_RVV := test_2d_rvv
BOARD_TARGET_TEST := test_2d
SRCS_TEST       := test_2d.cpp

TARGET_EDGE_STORE_TEST ?= test_edge_store_bench
SRCS_EDGE_STORE_TEST   := test_edge_store_bench.cpp

PCD_FILES_DIR ?= $(THIS_DIR)/pcd
PCD_FILES := \
	$(PCD_FILES_DIR)/lena.pcd \
	$(PCD_FILES_DIR)/gauss_smooth.pcd \
	$(PCD_FILES_DIR)/erosion.pcd \
	$(PCD_FILES_DIR)/dilation.pcd \
	$(PCD_FILES_DIR)/opening.pcd \
	$(PCD_FILES_DIR)/closing.pcd \
	$(PCD_FILES_DIR)/erosion_binary.pcd \
	$(PCD_FILES_DIR)/dilation_binary.pcd \
	$(PCD_FILES_DIR)/opening_binary.pcd \
	$(PCD_FILES_DIR)/closing_binary.pcd \
	$(PCD_FILES_DIR)/canny.pcd

TEST_ARGS := $(PCD_FILES)

REMOTE_DIR ?= /root/pcl-test/2d
DEPLOY_EXTRA_FILES := $(PCD_FILES)
DEPLOY_EXTRA_REMOTE_DIR := $(REMOTE_DIR)/pcds

INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/2d/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(HDF5_ROOT)/include \
	-I$(ZLIB_ROOT)/include \
	-I$(LIBPNG_ROOT)/include

LIBS_2D := \
	-lpcl_filters \
	-lpcl_io \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lz -lhdf5 -lpng -llz4 -lm
LIBS_BENCH := $(LIBS_2D) -lgtest -lgtest_main -lpthread
LIBS_TEST  := $(LIBS_2D) -lgtest -lgtest_main -lpthread

include ../mk/rvv-topic.mk

TARGET_EDGE_STORE_TEST_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_EDGE_STORE_TEST)

run_bench: generate_vec_report

$(TARGET_EDGE_STORE_TEST_BIN): $(SRCS_EDGE_STORE_TEST) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling edge-store micro-benchmark ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_EDGE_STORE_TEST) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_TEST) -o $(TARGET_EDGE_STORE_TEST_BIN)

run_edge_store_test: clean_edge_store_test $(TARGET_EDGE_STORE_TEST_BIN)
	@echo "[RUN] Edge-store micro-benchmark on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_EDGE_STORE_TEST_BIN)

clean_edge_store_test:
	rm -f $(TARGET_EDGE_STORE_TEST_BIN)

deploy_edge_store_test: check_board_ssh clean_edge_store_test
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_EDGE_STORE_TEST=$(TARGET_EDGE_STORE_TEST) $(BUILD_DIR)/$(ARCH)/$(TARGET_EDGE_STORE_TEST)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_EDGE_STORE_TEST) -o ./$(TARGET_EDGE_STORE_TEST)_stripped
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_DIR)"
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_EDGE_STORE_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_EDGE_STORE_TEST)
	@rm -f ./$(TARGET_EDGE_STORE_TEST)_stripped

dump_bench: dump_bench_rvv

COMMON_COMMON_DIR := ../common/common

parms:
	@$(MAKE) -C $(COMMON_COMMON_DIR) ARCH=$(ARCH) parms

parms_expf:
	@$(MAKE) -C $(COMMON_COMMON_DIR) ARCH=$(ARCH) parms_expf

parms_atan2:
	@$(MAKE) -C $(COMMON_COMMON_DIR) ARCH=$(ARCH) parms_atan2

parms_acos:
	@$(MAKE) -C $(COMMON_COMMON_DIR) ARCH=$(ARCH) parms_acos

run_atan2_test:
	@$(MAKE) -C $(COMMON_COMMON_DIR) ARCH=$(ARCH) run_atan2_test

run_acos_test:
	@$(MAKE) -C $(COMMON_COMMON_DIR) ARCH=$(ARCH) run_acos_test

run_expf_test:
	@$(MAKE) -C $(COMMON_COMMON_DIR) ARCH=$(ARCH) run_expf_test

run_expf_remez_vs_taylor:
	@$(MAKE) -C $(COMMON_COMMON_DIR) ARCH=$(ARCH) run_expf_remez_vs_taylor

clean: clean_edge_store_test

.PHONY: run_edge_store_test clean_edge_store_test deploy_edge_store_test dump_bench \
	parms parms_expf parms_atan2 parms_acos run_atan2_test run_acos_test \
	run_expf_test run_expf_remez_vs_taylor
