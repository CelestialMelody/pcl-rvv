# =============================================================================
#  PCL filters/radius_outlier_removal RVV diagnostic Makefile
# =============================================================================

TOPIC  := radius_outlier_removal
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/radius_outlier_removal.hpp
FILTER_REPORT  := log/filtered_radius_outlier_removal.log
ANALYZE_REPORT := log/analyze_radius_outlier_removal.log

TARGET_BENCH     := bench_radius_outlier_removal
TARGET_BENCH_STD := bench_radius_outlier_removal_std
TARGET_BENCH_RVV := bench_radius_outlier_removal_rvv
SRCS_BENCH       := bench_radius_outlier_removal.cpp

TARGET_TEST     := test_radius_outlier_removal
TARGET_TEST_STD := test_radius_outlier_removal_std
TARGET_TEST_RVV := test_radius_outlier_removal_rvv
SRCS_TEST       := test_radius_outlier_removal.cpp

EXTRA_CXXFLAGS := -DPCL_RADIUS_OUTLIER_REMOVAL_RVV_DIAGNOSTIC
LIBS_CORE := \
	-lpcl_filters \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_octree \
	-lpcl_common \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lz -lhdf5 -llz4 -lpng -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk
