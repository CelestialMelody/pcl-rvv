# =============================================================================
#  PCL filters/extract_indices RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := extract_indices
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/extract_indices.hpp
FILTER_REPORT  := log/filtered_extract_indices.log
ANALYZE_REPORT := log/analyze_extract_indices.log

TARGET_BENCH     := bench_extract_indices
TARGET_BENCH_STD := bench_extract_indices_std
TARGET_BENCH_RVV := bench_extract_indices_rvv
SRCS_BENCH       := bench_extract_indices.cpp

TARGET_TEST     := test_extract_indices
TARGET_TEST_STD := test_extract_indices_std
TARGET_TEST_RVV := test_extract_indices_rvv
SRCS_TEST       := test_extract_indices.cpp

TARGET_UPSTREAM_TEST     := test_extract_indices_upstream
TARGET_UPSTREAM_TEST_STD := test_extract_indices_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_extract_indices_upstream_rvv
SRCS_UPSTREAM_TEST       = $(PCL_SOURCE_ROOT)/test/filters/test_filters.cpp
UPSTREAM_TEST_ARGS       = $(PCL_SOURCE_ROOT)/test/bun0.pcd $(PCL_SOURCE_ROOT)/test/milk_cartoon_all_small_clorox.pcd

EXTRA_CXXFLAGS := -DPCL_EXTRACT_INDICES_RVV_DIAGNOSTIC
LIBS_BENCH := -lpcl_filters -lpcl_common -lm
LIBS_TEST  := -lpcl_filters -lpcl_common -lgtest -lgtest_main -lpthread -lm

include ../../mk/rvv-topic.mk
