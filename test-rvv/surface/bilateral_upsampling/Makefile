# =============================================================================
#  PCL surface/bilateral_upsampling RVV topic Makefile
#
#  本 topic 构建 test-rvv 下的 correctness、bench、asm 和 board 证据入口。
#  生产源码的实际 RVV patch 仍在 surface/include/pcl/surface/impl/
#  bilateral_upsampling.hpp 中维护；本 Makefile 只负责测试和证据采集。
# =============================================================================

TOPIC  := bilateral_upsampling
MODULE := surface

FOCUS_DIR      := pcl/surface/include/pcl/surface/impl/bilateral_upsampling.hpp
FILTER_REPORT  := log/filtered_bilateral_upsampling.log
ANALYZE_REPORT := log/analyze_bilateral_upsampling.log

TARGET_BENCH     := bench_bilateral_upsampling
TARGET_BENCH_STD := bench_bilateral_upsampling_std
TARGET_BENCH_RVV := bench_bilateral_upsampling_rvv
SRCS_BENCH       := src/bench_bilateral_upsampling.cpp

TARGET_TEST     := test_bilateral_upsampling
TARGET_TEST_STD := test_bilateral_upsampling_std
TARGET_TEST_RVV := test_bilateral_upsampling_rvv
SRCS_TEST       := src/test_bilateral_upsampling.cpp

INCLUDES = \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/surface/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include

LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm

include ../../mk/rvv-topic.mk
