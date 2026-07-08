# =============================================================================
#  PCL registration/correspondence_estimation_organized_projection RVV diagnostic Makefile
# =============================================================================

TOPIC  := correspondence_estimation_organized_projection
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp
FILTER_REPORT  := log/filtered_correspondence_estimation_organized_projection.log
ANALYZE_REPORT := log/analyze_correspondence_estimation_organized_projection.log

TARGET_BENCH     := bench_correspondence_estimation_organized_projection
TARGET_BENCH_STD := bench_correspondence_estimation_organized_projection_std
TARGET_BENCH_RVV := bench_correspondence_estimation_organized_projection_rvv
SRCS_BENCH       := bench_correspondence_estimation_organized_projection.cpp

TARGET_TEST     := test_correspondence_estimation_organized_projection
TARGET_TEST_STD := test_correspondence_estimation_organized_projection_std
TARGET_TEST_RVV := test_correspondence_estimation_organized_projection_rvv
SRCS_TEST       := test_correspondence_estimation_organized_projection.cpp

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
	-lpcl_registration \
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
