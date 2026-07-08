# =============================================================================
# Board-side parameters for test-rvv/filters/convolution.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_convolution_std
REMOTE_BENCH_RVV := bench_convolution_rvv
REMOTE_TEST      := test_convolution_rvv
REMOTE_BENCH_ARGS := 20 full

REMOTE_DIR := /root/pcl-test/filters/convolution

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
