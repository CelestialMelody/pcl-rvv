# =============================================================================
#  PCL filters/shadowpoints RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := shadowpoints
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/shadowpoints.hpp
FILTER_REPORT  := log/filtered_shadowpoints.log
ANALYZE_REPORT := log/analyze_shadowpoints.log

TARGET_BENCH     := bench_shadowpoints
TARGET_BENCH_STD := bench_shadowpoints_std
TARGET_BENCH_RVV := bench_shadowpoints_rvv
SRCS_BENCH       := bench_shadowpoints.cpp

TARGET_TEST     := test_shadowpoints
TARGET_TEST_STD := test_shadowpoints_std
TARGET_TEST_RVV := test_shadowpoints_rvv
SRCS_TEST       := test_shadowpoints.cpp

TARGET_UPSTREAM_TEST     := test_shadowpoints_upstream
TARGET_UPSTREAM_TEST_STD := test_shadowpoints_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_shadowpoints_upstream_rvv
SRCS_UPSTREAM_TEST       = \
	$(PCL_SOURCE_ROOT)/test/filters/test_filters.cpp \
	$(PCL_SOURCE_ROOT)/sample_consensus/src/sac_model_ellipse3d.cpp
UPSTREAM_TEST_ARGS       = $(PCL_SOURCE_ROOT)/test/bun0.pcd $(PCL_SOURCE_ROOT)/test/milk_cartoon_all_small_clorox.pcd --gtest_filter=ShadowPoints.Filters

INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/filters/include \
	-I$(PCL_SOURCE_ROOT)/features/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
	-I$(PCL_SOURCE_ROOT)/octree/include \
	-I$(PCL_SOURCE_ROOT)/sample_consensus/include \
	-I$(PCL_SOURCE_ROOT)/io/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(HDF5_ROOT)/include \
	-I$(ZLIB_ROOT)/include \
	-I$(LIBPNG_ROOT)/include

EXTRA_CXXFLAGS := -DPCL_SHADOWPOINTS_RVV_BENCH_ONLY
LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_common -lm
LIBS_UPSTREAM_TEST := \
	-lpcl_filters \
	-lpcl_features \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_octree \
	-lpcl_io \
	-lpcl_sample_consensus \
	-lpcl_common \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lgtest -lgtest_main -lpthread \
	-lz -lhdf5 -llz4 -lpng -lm

include ../../mk/rvv-topic.mk
