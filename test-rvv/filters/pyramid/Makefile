# =============================================================================
#  PCL filters/pyramid RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := pyramid
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/pyramid.hpp
FILTER_REPORT  := log/filtered_pyramid.log
ANALYZE_REPORT := log/analyze_pyramid.log

TARGET_BENCH     := bench_pyramid
TARGET_BENCH_STD := bench_pyramid_std
TARGET_BENCH_RVV := bench_pyramid_rvv
SRCS_BENCH       := bench_pyramid.cpp

TARGET_TEST     := test_pyramid
TARGET_TEST_STD := test_pyramid_std
TARGET_TEST_RVV := test_pyramid_rvv
SRCS_TEST       := test_pyramid.cpp

TARGET_UPSTREAM_TEST     := test_pyramid_upstream
TARGET_UPSTREAM_TEST_STD := test_pyramid_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_pyramid_upstream_rvv
SRCS_UPSTREAM_TEST       = $(PCL_SOURCE_ROOT)/test/filters/test_filters.cpp
UPSTREAM_TEST_OUTPUT_FILE :=

REMOTE_DIR ?= ~/pcl-test/filters/pyramid

LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_UPSTREAM_TEST := \
	-lpcl_filters \
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
