# =============================================================================
#  PCL filters/conditional_removal RVV test/bench Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := conditional_removal
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/conditional_removal.hpp
FILTER_REPORT  := log/filtered_conditional_removal.log
ANALYZE_REPORT := log/analyze_conditional_removal.log

TARGET_BENCH     := bench_conditional_removal
TARGET_BENCH_STD := bench_conditional_removal_std
TARGET_BENCH_RVV := bench_conditional_removal_rvv
SRCS_BENCH       := bench_conditional_removal.cpp

TARGET_TEST     := test_conditional_removal
TARGET_TEST_STD := test_conditional_removal_std
TARGET_TEST_RVV := test_conditional_removal_rvv
SRCS_TEST       := test_conditional_removal.cpp

TARGET_UPSTREAM_TEST     := test_conditional_removal_upstream
TARGET_UPSTREAM_TEST_STD := test_conditional_removal_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_conditional_removal_upstream_rvv
SRCS_UPSTREAM_TEST       = $(PCL_SOURCE_ROOT)/test/filters/test_filters.cpp
UPSTREAM_TEST_ARGS       ?= $(PCL_SOURCE_ROOT)/test/bun0.pcd $(PCL_SOURCE_ROOT)/test/milk_cartoon_all_small_clorox.pcd

LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm
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
	-lgtest -lgtest_main -lpthread \
	-lz -lhdf5 -llz4 -lpng -lm

include ../../mk/rvv-topic.mk
