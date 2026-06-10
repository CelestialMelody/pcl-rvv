# =============================================================================
#  PCL filters/bilateral RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := bilateral
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/bilateral.hpp
FILTER_REPORT  := log/filtered_bilateral.log
ANALYZE_REPORT := log/analyze_bilateral.log

TARGET_BENCH     := bench_bilateral
TARGET_BENCH_STD := bench_bilateral_std
TARGET_BENCH_RVV := bench_bilateral_rvv
SRCS_BENCH       := bench_bilateral.cpp

TARGET_TEST     := test_bilateral
TARGET_TEST_STD := test_bilateral_std
TARGET_TEST_RVV := test_bilateral_rvv
SRCS_TEST       := test_bilateral.cpp

TARGET_UPSTREAM_TEST     := test_bilateral_upstream
TARGET_UPSTREAM_TEST_STD := test_bilateral_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_bilateral_upstream_rvv
SRCS_UPSTREAM_TEST       = $(PCL_SOURCE_ROOT)/test/filters/test_bilateral.cpp
UPSTREAM_TEST_ARGS       = $(PCL_SOURCE_ROOT)/test/milk_cartoon_all_small_clorox.pcd --gtest_filter=FastBilateralFilter.Filters_Bilateral

EXTRA_CXXFLAGS := -DPCL_BILATERAL_RVV_DIAGNOSTIC
LIBS_CORE := \
	-lpcl_filters \
	-lpcl_io \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_octree \
	-lpcl_common \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lz -lhdf5 -llz4 -lpng -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread
LIBS_UPSTREAM_TEST := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk
