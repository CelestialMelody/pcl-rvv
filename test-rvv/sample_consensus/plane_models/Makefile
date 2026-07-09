# =============================================================================
#  PCL sample_consensus/plane_models RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk. The load-compare benchmark remains
# a plane_models-specific extension.
# =============================================================================

TOPIC  := plane_models
MODULE := sample_consensus
THIS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/sample_consensus/include/pcl/sample_consensus
FILTER_REPORT  := log/filtered_plane_models.log
ANALYZE_REPORT := log/analyze_plane_models.log

TARGET_TEST     := rvv_sac_plane_test
TARGET_TEST_STD := rvv_sac_plane_test_std
TARGET_TEST_RVV := rvv_sac_plane_test_rvv
BOARD_TARGET_TEST := rvv_sac_plane_test
SRCS_TEST       := test_sample_consensus_plane_models.cpp

TARGET_BENCH     := bench_sac_normal_plane
TARGET_BENCH_STD := bench_sac_normal_plane_std
TARGET_BENCH_RVV := bench_sac_normal_plane_rvv
SRCS_BENCH       := bench_sac_normal_plane.cpp

TARGET_BENCH_LOAD ?= bench_sac_normal_plane_load_compare
SRCS_BENCH_LOAD   := bench_sac_normal_plane_load_compare.cpp

PLANE_PCD := $(THIS_DIR)/pcd/sac_plane_test.pcd
TEST_ARGS := $(PLANE_PCD)
BENCH_ARGS := $(PLANE_PCD)

REMOTE_DIR ?= ~/pcl-test/sample_consensus/plane_models
DEPLOY_EXTRA_FILES := $(PLANE_PCD)
DEPLOY_EXTRA_REMOTE_DIR := $(REMOTE_DIR)/pcd

USE_PCL_SOURCE_HEADERS ?= 1
PCL_SOURCE_INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT)/sample_consensus/include
ifeq ($(USE_PCL_SOURCE_HEADERS),1)
PCL_INCLUDE = $(PCL_SOURCE_INCLUDES) -I$(PCL_INSTALL_ROOT)/include/pcl-1.15
else
PCL_INCLUDE = -I$(PCL_INSTALL_ROOT)/include/pcl-1.15
endif

INCLUDES = \
	$(PCL_INCLUDE) \
	-I$(PCL_SOURCE_ROOT) \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(HDF5_ROOT)/include \
	-I$(ZLIB_ROOT)/include \
	-I$(LIBPNG_ROOT)/include

LIBS_SAMPLE_CONSENSUS := \
	-lpcl_sample_consensus \
	-lpcl_common \
	-lpcl_io \
	-lpcl_search \
	-lpcl_kdtree \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lz -lhdf5 -llz4 -lpng -lm
LIBS_BENCH := $(LIBS_SAMPLE_CONSENSUS)
LIBS_TEST  := $(LIBS_SAMPLE_CONSENSUS) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk

TARGET_BENCH_LOAD_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_LOAD)

all: $(TARGET_TEST_BIN) $(TARGET_BENCH_BIN) $(TARGET_BENCH_LOAD_BIN)

$(TARGET_BENCH_LOAD_BIN): $(SRCS_BENCH_LOAD) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling load-compare benchmark ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_BENCH_LOAD) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_BENCH) -o $(TARGET_BENCH_LOAD_BIN)

run_bench_load: clean_bench_load $(TARGET_BENCH_LOAD_BIN)
	@echo "[RUN] NormalPlane RVV load-compare on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_BENCH_LOAD_BIN) $(PLANE_PCD) 200

clean_bench_load:
	rm -f $(TARGET_BENCH_LOAD_BIN)

deploy_bench: deploy_bench_rvv

deploy_bench_load: deploy_files clean_bench_load
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH_LOAD=$(TARGET_BENCH_LOAD) $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_LOAD)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_LOAD) -o ./$(TARGET_BENCH_LOAD)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_BENCH_LOAD)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_BENCH_LOAD)
	@rm -f ./$(TARGET_BENCH_LOAD)_stripped

clean: clean_bench_load

.PHONY: all run_bench_load clean_bench_load deploy_bench deploy_bench_load
