# =============================================================================
#  PCL common/centroid RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := centroid
MODULE := common

FOCUS_DIR      := pcl/common/include/pcl/common/impl/centroid.hpp
FILTER_REPORT  := log/filtered_centroid.log
ANALYZE_REPORT := log/analyze_centroid.log

TARGET_BENCH     := bench_centroid
TARGET_BENCH_STD := bench_centroid_std
TARGET_BENCH_RVV := bench_centroid_rvv
SRCS_BENCH       := bench_centroid.cpp

TARGET_TEST     := test_centroid
TARGET_TEST_STD := test_centroid_std
TARGET_TEST_RVV := test_centroid_rvv
BOARD_TARGET_TEST := test_centroid
SRCS_TEST       := test_centroid.cpp

TEST_BUN0_PCD := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/pcd/bun0.pcd)
TEST_ARGS     := $(TEST_BUN0_PCD)

BENCH_COMPARE_OUTPUT_FILE = $(OUTPUT_DIR_QEMU)/bench_compare.log

REMOTE_DIR ?= ~/pcl-test/common/centroid
DEPLOY_EXTRA_FILES := $(TEST_BUN0_PCD)
DEPLOY_EXTRA_REMOTE_DIR := $(REMOTE_DIR)/pcd

LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_io -lboost_iostreams -lpcl_common -lz -lpng -lgtest -lpthread -lm

include ../../mk/rvv-topic.mk
