# =============================================================================
#  PCL surface/organized_fast_mesh RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := organized_fast_mesh
MODULE := surface

FOCUS_DIR      := pcl/surface/include/pcl/surface/impl/organized_fast_mesh.hpp
FILTER_REPORT  := log/filtered_organized_fast_mesh.log
ANALYZE_REPORT := log/analyze_organized_fast_mesh.log

TARGET_BENCH     := bench_organized_fast_mesh
TARGET_BENCH_STD := bench_organized_fast_mesh_std
TARGET_BENCH_RVV := bench_organized_fast_mesh_rvv
SRCS_BENCH       := src/bench_organized_fast_mesh.cpp

TARGET_TEST     := test_organized_fast_mesh
TARGET_TEST_STD := test_organized_fast_mesh_std
TARGET_TEST_RVV := test_organized_fast_mesh_rvv
SRCS_TEST       := src/test_organized_fast_mesh.cpp

LIBS_CORE := \
	-lpcl_surface \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-llz4 -lz \
	-lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/surface/include \
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

include ../../mk/rvv-topic.mk
