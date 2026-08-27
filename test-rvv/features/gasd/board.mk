# =============================================================================
# Board-side Makefile for features/gasd RVV diagnostic.
# =============================================================================

# =============================================================================
# Board-side parameters for test-rvv/features/gasd.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_TEST := test_gasd_rvv
REMOTE_BENCH_STD := bench_gasd_std
REMOTE_BENCH_RVV := bench_gasd_rvv

REMOTE_DIR := /root/pcl-test/features/gasd

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
