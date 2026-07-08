# =============================================================================
# Board-side parameters for test-rvv/filters/fast_bilateral.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_fast_bilateral_std
REMOTE_BENCH_RVV := bench_fast_bilateral_rvv
REMOTE_TEST      := test_fast_bilateral_rvv

REMOTE_DIR := /root/pcl-test/filters/fast_bilateral

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
