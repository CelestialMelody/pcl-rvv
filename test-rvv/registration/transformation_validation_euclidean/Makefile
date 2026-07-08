# =============================================================================
#  PCL registration/transformation_validation_euclidean RVV diagnostic Makefile
# =============================================================================

TOPIC  := transformation_validation_euclidean
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/transformation_validation_euclidean.hpp
FILTER_REPORT  := log/filtered_transformation_validation_euclidean.log
ANALYZE_REPORT := log/analyze_transformation_validation_euclidean.log

TARGET_BENCH     := bench_transformation_validation_euclidean
TARGET_BENCH_STD := bench_transformation_validation_euclidean_std
TARGET_BENCH_RVV := bench_transformation_validation_euclidean_rvv
SRCS_BENCH       := bench_transformation_validation_euclidean.cpp

TARGET_TEST     := test_transformation_validation_euclidean
TARGET_TEST_STD := test_transformation_validation_euclidean_std
TARGET_TEST_RVV := test_transformation_validation_euclidean_rvv
SRCS_TEST       := test_transformation_validation_euclidean.cpp

INCLUDES ?= \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/registration/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
	-I$(PCL_SOURCE_ROOT)/octree/include \
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

LIBS_CORE := \
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
