# =============================================================================
# Board-side parameters for test-rvv/surface/bilateral_upsampling.
# Shared board rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_bilateral_upsampling_std
REMOTE_BENCH_RVV := bench_bilateral_upsampling_rvv
REMOTE_TEST      := test_bilateral_upsampling_rvv

REMOTE_DIR := /root/pcl-test/surface/bilateral_upsampling

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)

