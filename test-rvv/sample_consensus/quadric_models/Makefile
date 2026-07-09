# =============================================================================
#  PCL sample_consensus/quadric_models RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk. This topic is test-only.
# =============================================================================

TOPIC  := quadric_models
MODULE := sample_consensus

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/sample_consensus/include/pcl/sample_consensus
FILTER_REPORT  := log/filtered_quadric_models.log
ANALYZE_REPORT := log/analyze_quadric_models.log

TARGET_TEST     := rvv_sac_quadric_test
TARGET_TEST_STD := rvv_sac_quadric_test_std
TARGET_TEST_RVV := rvv_sac_quadric_test_rvv
BOARD_TARGET_TEST := rvv_sac_quadric_test
SRCS_TEST       := test_sample_consensus_quadric_models.cpp

REMOTE_DIR ?= ~/pcl-test/sample_consensus/quadric_models

USE_PCL_SOURCE_HEADERS ?= 1
PCL_SOURCE_INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT)/sample_consensus/include
ifeq ($(USE_PCL_SOURCE_HEADERS),1)
PCL_INCLUDE = $(PCL_SOURCE_INCLUDES) -I$(PCL_INSTALL_ROOT)/include/pcl-1.15
else
PCL_INCLUDE = -I$(PCL_INSTALL_ROOT)/include/pcl-1.15
endif

INCLUDES = \
	$(PCL_INCLUDE) \
	-I$(PCL_SOURCE_ROOT) \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(HDF5_ROOT)/include \
	-I$(ZLIB_ROOT)/include \
	-I$(LIBPNG_ROOT)/include

LIBS_TEST := \
	-lpcl_sample_consensus \
	-lpcl_common \
	-lpcl_io \
	-lpcl_search \
	-lpcl_kdtree \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lgtest -lgtest_main -lpthread \
	-lz -lhdf5 -llz4 -lpng -lm

include ../../mk/rvv-topic.mk

all: $(TARGET_TEST_BIN)

.PHONY: all
