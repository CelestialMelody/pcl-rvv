# =============================================================================
#  PCL filters/covariance_sampling RVV diagnostic Makefile
# =============================================================================

TOPIC  := covariance_sampling
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/covariance_sampling.hpp
FILTER_REPORT  := log/filtered_covariance_sampling.log
ANALYZE_REPORT := log/analyze_covariance_sampling.log

TARGET_BENCH     := bench_covariance_sampling
TARGET_BENCH_STD := bench_covariance_sampling_std
TARGET_BENCH_RVV := bench_covariance_sampling_rvv
SRCS_BENCH       := bench_covariance_sampling.cpp

TARGET_TEST     := test_covariance_sampling
TARGET_TEST_STD := test_covariance_sampling_std
TARGET_TEST_RVV := test_covariance_sampling_rvv
SRCS_TEST       := test_covariance_sampling.cpp

EXTRA_CXXFLAGS := -DPCL_COVARIANCE_SAMPLING_RVV_DIAGNOSTIC
LIBS_CORE := \
	-lpcl_filters \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lz -lhdf5 -llz4 -lpng -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk
