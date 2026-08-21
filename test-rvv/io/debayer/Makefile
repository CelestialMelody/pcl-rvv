# =============================================================================
#  PCL io/debayer RVV diagnostic Makefile
#
#  本 topic 先建立 production-shaped diagnostic（生产形态诊断）：测试专用
#  helper 复刻 DeBayer::debayerBilinear() 的内区 2x2 Bayer stencil（邻域模板）
#  语义。QEMU 只用于 correctness（正确性）和路径 / 日志形状；性能结论必须来自板卡。
# =============================================================================

TOPIC  := debayer
MODULE := io

FOCUS_DIR      := pcl/io/src/debayer.cpp
FILTER_REPORT  := log/filtered_debayer.log
ANALYZE_REPORT := log/analyze_debayer.log

TARGET_TEST     := test_debayer
TARGET_TEST_STD := test_debayer_std
TARGET_TEST_RVV := test_debayer_rvv
SRCS_TEST       := src/test_debayer.cpp ../../../io/src/debayer.cpp

TARGET_BENCH     := bench_debayer
TARGET_BENCH_STD := bench_debayer_std
TARGET_BENCH_RVV := bench_debayer_rvv
SRCS_BENCH       := src/bench_debayer.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/io/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
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

LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lm

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare run_bench_rvv

run_board_debayer_bilinear_inner:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter bilinear_inner_640x480 --iterations 20 --warmup-iterations 3"

run_board_debayer_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 10 --warmup-iterations 2"

.PHONY: run_qemu_smoke run_board_debayer_bilinear_inner run_board_debayer_smoke
