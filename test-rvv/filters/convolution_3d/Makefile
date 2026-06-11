# =============================================================================
#  PCL filters/convolution_3d RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := convolution_3d
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/convolution_3d.hpp
FILTER_REPORT  := log/filtered_convolution_3d.log
ANALYZE_REPORT := log/analyze_convolution_3d.log

TARGET_BENCH     := bench_convolution_3d
TARGET_BENCH_STD := bench_convolution_3d_std
TARGET_BENCH_RVV := bench_convolution_3d_rvv
SRCS_BENCH       := bench_convolution_3d.cpp

TARGET_TEST     := test_convolution_3d
TARGET_TEST_STD := test_convolution_3d_std
TARGET_TEST_RVV := test_convolution_3d_rvv
SRCS_TEST       := test_convolution_3d.cpp

EXTRA_CXXFLAGS := -DPCL_CONVOLUTION_3D_RVV_DIAGNOSTIC
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
LIBS_TEST  := $(LIBS_CORE)

include ../../mk/rvv-topic.mk
