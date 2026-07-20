# =============================================================================
#  PCL registration/transformation_estimation_point_to_plane_lls_weighted RVV diagnostic Makefile
# =============================================================================

TOPIC  := transformation_estimation_point_to_plane_lls_weighted
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp
FILTER_REPORT  := log/filtered_transformation_estimation_point_to_plane_lls_weighted.log
ANALYZE_REPORT := log/analyze_transformation_estimation_point_to_plane_lls_weighted.log

TARGET_BENCH     := bench_transformation_estimation_point_to_plane_lls_weighted
TARGET_BENCH_STD := bench_transformation_estimation_point_to_plane_lls_weighted_std
TARGET_BENCH_RVV := bench_transformation_estimation_point_to_plane_lls_weighted_rvv
SRCS_BENCH       := bench_transformation_estimation_point_to_plane_lls_weighted.cpp

TARGET_TEST     := test_transformation_estimation_point_to_plane_lls_weighted
TARGET_TEST_STD := test_transformation_estimation_point_to_plane_lls_weighted_std
TARGET_TEST_RVV := test_transformation_estimation_point_to_plane_lls_weighted_rvv
SRCS_TEST       := test_transformation_estimation_point_to_plane_lls_weighted.cpp

INCLUDES ?= \
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
