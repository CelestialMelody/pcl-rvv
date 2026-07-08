# =============================================================================
#  PCL filters/fast_bilateral_omp RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := fast_bilateral_omp
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/fast_bilateral_omp.hpp
FILTER_REPORT  := log/filtered_fast_bilateral_omp.log
ANALYZE_REPORT := log/analyze_fast_bilateral_omp.log

TARGET_BENCH     := bench_fast_bilateral_omp
TARGET_BENCH_STD := bench_fast_bilateral_omp_std
TARGET_BENCH_RVV := bench_fast_bilateral_omp_rvv
SRCS_BENCH       := bench_fast_bilateral_omp.cpp

TARGET_TEST     := test_fast_bilateral_omp
TARGET_TEST_STD := test_fast_bilateral_omp_std
TARGET_TEST_RVV := test_fast_bilateral_omp_rvv
SRCS_TEST       := test_fast_bilateral_omp.cpp

TARGET_UPSTREAM_TEST     := test_fast_bilateral_omp_upstream
TARGET_UPSTREAM_TEST_STD := test_fast_bilateral_omp_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_fast_bilateral_omp_upstream_rvv
SRCS_UPSTREAM_TEST       = $(PCL_SOURCE_ROOT)/test/filters/test_bilateral.cpp
UPSTREAM_TEST_ARGS       = $(PCL_SOURCE_ROOT)/test/milk_cartoon_all_small_clorox.pcd --gtest_filter=FastBilateralFilterOMP.Filters_Bilateral

LIBS_CORE := \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_octree \
	-lpcl_common \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lz -lhdf5 -llz4 -lpng -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lgomp -lpthread
LIBS_UPSTREAM_TEST := \
	-lpcl_filters \
	-lpcl_io \
	-lpcl_sample_consensus \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_octree \
	-lpcl_common \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lgtest -lgtest_main -lgomp -lpthread \
	-lz -lhdf5 -llz4 -lpng -lm

include ../../mk/rvv-topic.mk
