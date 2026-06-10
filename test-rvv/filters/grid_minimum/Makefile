# =============================================================================
#  PCL filters/grid_minimum RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := grid_minimum
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/grid_minimum.hpp
FILTER_REPORT  := log/filtered_grid_minimum.log
ANALYZE_REPORT := log/analyze_grid_minimum.log

TARGET_BENCH     := bench_grid_minimum
TARGET_BENCH_STD := bench_grid_minimum_std
TARGET_BENCH_RVV := bench_grid_minimum_rvv
SRCS_BENCH       := bench_grid_minimum.cpp

TARGET_TEST     := test_grid_minimum
TARGET_TEST_STD := test_grid_minimum_std
TARGET_TEST_RVV := test_grid_minimum_rvv
SRCS_TEST       := test_grid_minimum.cpp

TARGET_UPSTREAM_TEST     := test_grid_minimum_upstream
TARGET_UPSTREAM_TEST_STD := test_grid_minimum_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_grid_minimum_upstream_rvv
SRCS_UPSTREAM_TEST       = $(PCL_SOURCE_ROOT)/test/filters/test_grid_minimum.cpp

EXTRA_CXXFLAGS := -DPCL_GRID_MINIMUM_RVV_DIAGNOSTIC
LIBS_BENCH := -lpcl_filters -lpcl_common -lm
LIBS_TEST  := -lpcl_filters -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_UPSTREAM_TEST := -lpcl_filters -lpcl_common -lgtest -lgtest_main -lpthread -lm

include ../../mk/rvv-topic.mk
