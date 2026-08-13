# =============================================================================
#  PCL registration/transformation_estimation_point_to_plane_lls RVV diagnostic Makefile
# =============================================================================

TOPIC  := transformation_estimation_point_to_plane_lls
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp
FILTER_REPORT  := log/filtered_transformation_estimation_point_to_plane_lls.log
ANALYZE_REPORT := log/analyze_transformation_estimation_point_to_plane_lls.log

TARGET_BENCH     := bench_transformation_estimation_point_to_plane_lls
TARGET_BENCH_STD := bench_transformation_estimation_point_to_plane_lls_std
TARGET_BENCH_RVV := bench_transformation_estimation_point_to_plane_lls_rvv
SRCS_BENCH       := src/bench_teptpl.cpp

TARGET_TEST     := test_transformation_estimation_point_to_plane_lls
TARGET_TEST_STD := test_transformation_estimation_point_to_plane_lls_std
TARGET_TEST_RVV := test_transformation_estimation_point_to_plane_lls_rvv
SRCS_TEST       := \
	src/test_teptpl_public_semantics.cpp \
	src/test_teptpl_candidates.cpp \
	src/test_teptpl_production_direct.cpp \
	src/test_teptpl_row_sources.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/registration/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include

LIBS_CORE := \
	-lpcl_registration \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk
